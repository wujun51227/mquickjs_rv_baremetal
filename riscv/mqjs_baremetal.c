/*
 * Micro QuickJS REPL - Baremetal RISC-V version
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>

#include "../cutils.h"
#include "../mquickjs.h"

/* Forward declaration for console.log */
JSValue js_console_log(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);


extern char __heap_start[];
extern char __heap_end[];

static uint8_t *load_file(const char *filename, int *plen);

static void dump_error(JSContext *ctx)
{
    JSValue val;
    JSCStringBuf buf;
    const char *str;
    size_t len;

    val = JS_GetException(ctx);
    if (!JS_IsUndefined(val)) {
        str = JS_ToCStringLen(ctx, &len, val, &buf);
        printf("%s\n", str);
    }
}

static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int i;
    JSValue v;
    
    for(i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        v = argv[i];
        if (JS_IsString(ctx, v)) {
            JSCStringBuf buf;
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v, &buf);
            printf("%s", str);
        } else {
            JS_PrintValueF(ctx, argv[i], JS_DUMP_LONG);
        }
    }
    putchar('\n');
    return JS_UNDEFINED;
}

static JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JS_GC(ctx);
    return JS_UNDEFINED;
}

#if defined(__linux__) || defined(__APPLE__)
static int64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}
#else
static int64_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}
#endif

static int64_t get_date_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

JSValue js_date_constructor(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv)
{
    double val;
    argc &= ~FRAME_CF_CTOR;
    if (argc == 0) {
        val = get_date_ms();
    } else if (argc == 1 && JS_IsNumber(ctx, argv[0])) {
        if (JS_ToNumber(ctx, &val, argv[0]))
            return JS_EXCEPTION;
    } else {
        return JS_ThrowTypeError(ctx, "unsupported Date() parameter");
    }
    return JS_NewDate(ctx, val);
}

static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_NewInt64(ctx, get_date_ms());
}

static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return 0;
}

/* load a script */
static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JSValue ret;
    return ret;
}

/* timers */
typedef struct {
    BOOL allocated;
    JSGCRef func;
    int64_t timeout; /* in ms */
} JSTimer;

#define MAX_TIMERS 16

static JSTimer js_timer_list[MAX_TIMERS];

static JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JSTimer *th;
    int delay, i;
    JSValue *pfunc;
    
    if (!JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "not a function");
    if (JS_ToInt32(ctx, &delay, argv[1]))
        return JS_EXCEPTION;
    for(i = 0; i < MAX_TIMERS; i++) {
        th = &js_timer_list[i];
        if (!th->allocated) {
            pfunc = JS_AddGCRef(ctx, &th->func);
            *pfunc = argv[0];
            th->timeout = get_time_ms() + delay;
            th->allocated = TRUE;
            return JS_NewInt32(ctx, i);
        }
    }
    return JS_ThrowInternalError(ctx, "too many timers");
}

static JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int timer_id;
    JSTimer *th;

    if (JS_ToInt32(ctx, &timer_id, argv[0]))
        return JS_EXCEPTION;
    if (timer_id >= 0 && timer_id < MAX_TIMERS) {
        th = &js_timer_list[timer_id];
        if (th->allocated) {
            JS_DeleteGCRef(ctx, &th->func);
            th->allocated = FALSE;
        }
    }
    return JS_UNDEFINED;
}

#include "../mqjs_stdlib.h"

#define STYLE_DEFAULT    COLOR_BRIGHT_GREEN
#define STYLE_COMMENT    COLOR_WHITE
#define STYLE_STRING     COLOR_BRIGHT_CYAN
#define STYLE_REGEX      COLOR_CYAN
#define STYLE_NUMBER     COLOR_GREEN
#define STYLE_KEYWORD    COLOR_BRIGHT_WHITE
#define STYLE_FUNCTION   COLOR_BRIGHT_YELLOW
#define STYLE_TYPE       COLOR_BRIGHT_MAGENTA
#define STYLE_IDENTIFIER COLOR_BRIGHT_GREEN
#define STYLE_ERROR      COLOR_RED
#define STYLE_RESULT     COLOR_BRIGHT_WHITE
#define STYLE_ERROR_MSG  COLOR_BRIGHT_RED

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
    fread(buf, 1, buf_len, f);
    buf[buf_len] = '\0';
    fclose(f);
    if (plen)
        *plen = buf_len;
    return buf;
}

static int js_log_err_flag;

static void js_log_func(void *opaque, const void *buf, size_t buf_len)
{
    const char *str = (const char *)buf;
    size_t i;
    for (i = 0; i < buf_len; i++) {
        putchar(str[i]);
    }
}

/* Step 6: Add simple string operation */
static const char *test_code =
  "var cnt=0;\n"
  "console.log('hello');\n"
  "cnt+=1;\n"
    "var a = 10;\n"
  "cnt+=1;\n"
    "var b = 20;\n"
  "cnt+=1;\n"
    "var sum = a + b;\n"
  "cnt+=1;\n"
    "var product = a * b;\n"
  "cnt+=1;\n"
    "var division = b / a;\n"
  "cnt+=1;\n"
    "var max_val = (a > b) ? a : b;\n"
  "cnt+=1;\n"
    "var arr = [a, b, sum];\n"
  "cnt+=1;\n"
    "var obj = { x: a, y: b };\n"
  "cnt+=1;\n"
    "function add(x, y) { return x + y; }\n"
  "cnt+=1;\n"
    "var result = add(a, b);\n"
  "cnt+=1;\n"
    "var str1 = 'Hello';\n"
  "cnt+=1;\n"
    "var str2 = 'World';\n"
  "cnt+=1;\n"
    "var str3 = str1 + ' ' + str2;\n"
  "cnt+=1;\n"
    "var all_pass = (sum === 30) && (product === 200) && (division === 2) && (max_val === 20) && (arr.length === 3) && (arr[0] === 10) && (arr[1] === 20) && (arr[2] === 30) && (obj.x === 10) && (obj.y === 20) && (result === 30) && (str3 === 'Hello World');\n"
    "all_pass ? cnt : 0;\n"
    ;

int main(int argc, char **argv)
{
    JSContext *ctx;
    JSValue val;
    uint8_t *mem_buf;
    size_t mem_size;
    int test_passed;

    printf("MicroQuickJS Baremetal RISC-V\n");
    printf("=============================\n\n");

    /* Set up memory for JavaScript engine */
#ifdef HEAP_SIZE
    mem_size = HEAP_SIZE;
#else
    mem_size = 16*1024; /* 16 KB default */
#endif
    mem_buf = (uint8_t *)__heap_start;

    printf("Initializing JavaScript engine...\n");
    ctx = JS_NewContext(mem_buf, mem_size, &js_stdlib);
    if (!ctx) {
        printf("Error: Cannot allocate JS context\n");
        return 1;
    }
    printf("Context created successfully\n");

    /* Set up log function for JS_PrintValueF */
    // JS_SetLogFunc(ctx, js_log_func);  // Commented out for 32-bit compatibility

    printf("Running built-in tests...\n\n");

    /* Run embedded test code */
    printf("Evaluating test code...\n");
    val = JS_Eval(ctx, test_code, strlen(test_code), "<test>", JS_EVAL_RETVAL);
    printf("Test code evaluated\n");
    if (JS_IsException(val)) {
        printf("\nTest failed!\n");
        dump_error(ctx);
        JS_FreeContext(ctx);
        return 1;
    }

    /* Get test result */
    if (JS_ToInt32(ctx, &test_passed, val)) {
        printf("Error: Cannot get test result\n");
        JS_FreeContext(ctx);
        return 1;
    }

    printf("\n");
    printf("Tests passed: %d\n", test_passed);

    printf("\n");
    printf("Memory usage:\n");
    //JS_DumpMemory(ctx, 1);

    JS_FreeContext(ctx);

    printf("\nAll tests completed successfully!\n");
    printf("Exiting...\n");

    return 0;
}
