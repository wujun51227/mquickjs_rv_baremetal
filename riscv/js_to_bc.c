/*
 * Host-side JS -> mQuickJS bytecode compiler for RISC-V baremetal images.
 *
 * Usage: js_to_bc [-m32] -o FILE.jsbc input.js
 *
 * Always compiles with JS_EVAL_RETVAL so the last expression is returned,
 * matching the original mqjs_baremetal.c JS_Eval(..., JS_EVAL_RETVAL) path.
 */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "../cutils.h"
#include "../mquickjs.h"
#include "jsbytecode_slot.h"

static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_UNDEFINED;
}

static JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_UNDEFINED;
}

JSValue js_date_constructor(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv)
{
    return JS_ThrowTypeError(ctx, "unsupported Date() during compile");
}

static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_NewInt32(ctx, 0);
}

static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_NewInt32(ctx, 0);
}

static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_ThrowTypeError(ctx, "load() is not available during compile");
}

static JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_ThrowTypeError(ctx, "setTimeout() is not available during compile");
}

static JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_UNDEFINED;
}

#include "mqjs_stdlib.h"

static uint8_t *load_file(const char *filename, int *plen)
{
    FILE *f;
    uint8_t *buf;
    int buf_len;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    buf_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(buf_len + 1);
    if (!buf) {
        fprintf(stderr, "Could not allocate %d bytes for %s\n", buf_len, filename);
        exit(1);
    }
    if (fread(buf, 1, buf_len, f) != (size_t)buf_len) {
        fprintf(stderr, "Could not read %s\n", filename);
        exit(1);
    }
    buf[buf_len] = '\0';
    fclose(f);
    if (plen)
        *plen = buf_len;
    return buf;
}

static void js_log_func(void *opaque, const void *buf, size_t buf_len)
{
    fwrite(buf, 1, buf_len, stderr);
}

static void dump_error(JSContext *ctx)
{
    JSValue val;
    JSCStringBuf cbuf;
    const char *str;
    size_t len;

    val = JS_GetException(ctx);
    if (!JS_IsUndefined(val)) {
        str = JS_ToCStringLen(ctx, &len, val, &cbuf);
        fprintf(stderr, "%s\n", str);
    }
}

static int compile_file(const char *filename, const char *outfilename,
                        BOOL force_32bit)
{
    uint8_t *mem_buf;
    size_t mem_size;
    JSContext *ctx;
    char *eval_str;
    JSValue val;
    union {
        JSBytecodeHeader hdr;
#if JSW == 8
        JSBytecodeHeader32 hdr32;
#endif
    } hdr_buf;
    int hdr_len;
    const uint8_t *data_buf;
    uint32_t data_len;
    FILE *f;

    mem_size = 2 << 20;
    mem_buf = malloc(mem_size);
    if (!mem_buf) {
        fprintf(stderr, "Could not allocate compiler heap\n");
        return 1;
    }

    /* prepare_compilation: stdlib objects are not instantiated; only
       atoms are needed so the parser can intern identifiers. */
    ctx = JS_NewContext2(mem_buf, mem_size, &js_stdlib, TRUE);
    if (!ctx) {
        fprintf(stderr, "Could not create JS context\n");
        free(mem_buf);
        return 1;
    }
    JS_SetLogFunc(ctx, js_log_func);

    eval_str = (char *)load_file(filename, NULL);
    val = JS_Parse(ctx, eval_str, strlen(eval_str), filename, JS_EVAL_RETVAL);
    free(eval_str);
    if (JS_IsException(val)) {
        dump_error(ctx);
        JS_FreeContext(ctx);
        free(mem_buf);
        return 1;
    }

#if JSW == 8
    if (force_32bit) {
        if (JS_PrepareBytecode64to32(ctx, &hdr_buf.hdr32, &data_buf, &data_len, val)) {
            fprintf(stderr, "Could not convert the bytecode from 64 to 32 bits\n");
            JS_FreeContext(ctx);
            free(mem_buf);
            return 1;
        }
        hdr_len = sizeof(JSBytecodeHeader32);
    } else
#endif
    {
        JS_PrepareBytecode(ctx, &hdr_buf.hdr, &data_buf, &data_len, val);
        /* Relocate to zero so the image is position-independent until
           the MCU calls JS_RelocateBytecode() at the load address. */
        JS_RelocateBytecode2(ctx, &hdr_buf.hdr, (uint8_t *)data_buf, data_len, 0, FALSE);
        hdr_len = sizeof(JSBytecodeHeader);
    }

    f = fopen(outfilename, "wb");
    if (!f) {
        perror(outfilename);
        JS_FreeContext(ctx);
        free(mem_buf);
        return 1;
    }
    if (fwrite(&hdr_buf, 1, hdr_len, f) != (size_t)hdr_len ||
        fwrite(data_buf, 1, data_len, f) != data_len) {
        fprintf(stderr, "Could not write %s\n", outfilename);
        fclose(f);
        JS_FreeContext(ctx);
        free(mem_buf);
        return 1;
    }
    fclose(f);

    JS_FreeContext(ctx);
    free(mem_buf);
    return 0;
}

static int wrap_slot(const char *in_filename, const char *out_filename)
{
    uint8_t *buf;
    int buf_len;
    JsBcSlotHeader hdr;
    FILE *f;

    buf = load_file(in_filename, &buf_len);
    if (buf_len <= 0) {
        fprintf(stderr, "Error: input file %s is empty\n", in_filename);
        free(buf);
        return 1;
    }
    hdr.magic = JSBC_SLOT_MAGIC;
    hdr.length = (uint32_t)buf_len;
    hdr.checksum = jsbc_slot_checksum(buf, (uint32_t)buf_len);
    hdr.ready = JSBC_SLOT_READY;

    f = fopen(out_filename, "wb");
    if (!f) {
        perror(out_filename);
        free(buf);
        return 1;
    }
    if (fwrite(&hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
        fwrite(buf, 1, buf_len, f) != (size_t)buf_len) {
        fprintf(stderr, "Could not write %s\n", out_filename);
        fclose(f);
        free(buf);
        return 1;
    }
    fclose(f);
    free(buf);
    return 0;
}

static void usage(void)
{
    fprintf(stderr,
            "usage: js_to_bc [-m32] -o FILE.jsbc input.js\n"
            "       js_to_bc --wrap-slot -o FILE.slot input.bin\n"
            "  -m32         emit 32-bit bytecode (for RV32 targets, 64-bit host)\n"
            "  --wrap-slot  wrap a .bin as a mailbox slot (header + image)\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *out_filename;
    const char *in_filename;
    BOOL force_32bit, do_wrap_slot;
    int i;

    out_filename = NULL;
    in_filename = NULL;
    force_32bit = FALSE;
    do_wrap_slot = FALSE;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-m32")) {
            force_32bit = TRUE;
        } else if (!strcmp(argv[i], "--wrap-slot")) {
            do_wrap_slot = TRUE;
        } else if (!strcmp(argv[i], "-o")) {
            if (i + 1 >= argc)
                usage();
            out_filename = argv[++i];
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage();
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown option '%s'\n", argv[i]);
            usage();
        } else if (!in_filename) {
            in_filename = argv[i];
        } else {
            usage();
        }
    }

    if (!out_filename || !in_filename)
        usage();

    if (do_wrap_slot)
        return wrap_slot(in_filename, out_filename);

#if JSW != 8
    if (force_32bit) {
        fprintf(stderr, "-m32 is only supported on a 64-bit host\n");
        return 1;
    }
#endif

    return compile_file(in_filename, out_filename, force_32bit);
}
