# code_review

Pi returned follow-up steps:

已完成只读审查。下面按文件职责、问题与文档差异给出结论；未改任何文件。

## 各文件职责

| 文件 | 作用 |
|------|------|
| `riscv/mqjs_baremetal.c` | 裸机入口：建 JS 上下文、跑内嵌源码或字节码/mailbox，打印测试结果后退出 |
| `riscv/baremetal_syscall_rv32.c` / `baremetal_syscall_rv64.c` | 无 libc 时的 UART/`printf`/`malloc`/时间/`setjmp`/字符串与部分 libgcc 桩 |
| `riscv/start_riscv32.s` / `start_riscv64.s` | `_start`：关中断、设 `gp`、清 BSS、设栈、调 `main`，返回后 `ecall`+`wfi` |
| `riscv/shift_rv32.s` | RV32 的 64 位移位 `__ashldi3`/`__lshrdi3`/`__ashrdi3` |
| `riscv/riscv32_baremetal.ld` / `riscv64_baremetal.ld` | 单块 RAM：`0x80000000` 起 4MB，heap 在 BSS 后，栈从 RAM 顶向下，可选 `.jsbytecode` |
| `riscv/jsbytecode_slot.h` | mailbox 16 字节头、`fence`、字节和校验 |
| `riscv/js_to_bc.c` | 主机把 `test_code.js` 编成 `.bin`，或 `--wrap-slot` 打 mailbox |
| `riscv/test_code.js` | 默认测试脚本（应与 `mqjs_baremetal.c` 内嵌字符串同步） |
| `Makefile.riscv` | RV32/64 交叉编译、软浮点、heap/UART/bytecode 选项、host `js_to_bc` |
| `qemu_rv32.sh` / `qemu_rv64.sh` | clean + 源码路径 QEMU（5s timeout） |
| `qemu_rv32_wait.sh` / `qemu_rv64_wait.sh` | `CONFIG_BYTECODE_WAIT` + `-device loader` 加载 slot |

当前仓库里的 `mqjs_baremetal.elf` 是 **RV64、无 bytecode**：`.text≈97KB`，heap `0x8001fb00–0x80023b00`（16KB），栈顶 `0x80400000`。

---

## 正确性 / 缺陷

### 1. RV32 64 位移位实现是错的（高）

`shift_rv32.s` 在移位量 `< 32` 时用减法代替「跨半字移位」，`srl/sll …, 0` 是空操作，像未写完的草稿。

```32:40:riscv/shift_rv32.s
    mv t0, a0
    sll a1, a1, a2
    sub t1, t0, a2
    srl t1, t1, 0
    sll t0, t0, a2
    or a1, a1, t1
    mv a0, t0
    ret
```

`__lshrdi3`（约 57–67 行）和 `__ashrdi3`（约 94–113 行）同样错误。正确公式应为：

- `<< b`（b<32）：`hi = (hi<<b) | (lo>>(32-b)); lo <<= b`
- `>> b`：`lo = (lo>>b) | (hi<<(32-b)); hi >>= b`

影响：RV32 上 `gettimeofday()` 的 `__udivdi3` 含 `(udi_int)1 << i` 和 `x >> i`，会走到这些 helper；JS 的 64 位数/软浮点位操作也可能坏。应改成标准 libgcc 算法或直接链 `-lgcc`。

### 2. `setjmp`/`longjmp` 用 C 内联汇编，ABI 脆弱（高）

解析器依赖 `setjmp`（`mquickjs.c:11745`）。实现写在 syscall 里，例如：

```397:426:riscv/baremetal_syscall_rv32.c
int setjmp(jmp_buf env) {
    asm volatile(
        "sw ra, 0(%0)\n"
        ...
        : : "r"(env) : "memory"
    );
    return 0;
}
```

问题：

- 不是 naked/纯汇编叶子；一旦 GCC 给 `setjmp` 开栈帧，`longjmp` 恢复的是 **setjmp 的 sp** 而不是调用者的 sp。
- `jmp_buf` 只在 `.c` 里 typedef，`mquickjs.c` 用的是工具链 `<setjmp.h>`，**大小可能不一致**，会写爆 `JSParseState`。
- 未标 `returns_twice`（声明侧靠头文件碰运气）。
- 默认路径是 `JS_Eval`，会走 parser/`setjmp`。字节码路径可能绕开，从而掩盖问题。

建议改成独立 `.S`，只存 `ra/sp/s0–s11`，与 newlib 的 `jmp_buf` 布局一致。

### 3. RV32 `copysign` 把 `double` 当 32 位（中高）

```340:345:riscv/baremetal_syscall_rv32.c
double copysign(double x, double y) {
    unsigned int xbits = *(unsigned int *)&x;
    unsigned int ybits = *(unsigned int *)&y;
    xbits = (xbits & 0x7FFFFFFF) | (ybits & 0x80000000);
    return *(double *)&xbits;
}
```

LE 下低 32 位是尾数，符号在高 32 位。应像 RV64 那样用 `uint64_t` union。现有 ELF 里已有 `copysign` 符号，说明有引用。

### 4. `malloc` 与 JS heap 重叠；对齐/realloc 错误（中高）

```398:405:riscv/mqjs_baremetal.c
#ifdef HEAP_SIZE
    mem_size = HEAP_SIZE;
#else
    mem_size = 16*1024;
#endif
    mem_buf = (uint8_t *)__heap_start;
```

```281:301:riscv/baremetal_syscall_rv32.c
    ptr = heap_ptr;
    heap_ptr = (char *)(((unsigned int)heap_ptr + 3) & ~3u);
    heap_ptr += size;
```

- JS 与 bump `malloc` 都从 `__heap_start` 起，**无隔离**。当前测试可能没调 `malloc`，但是地雷。
- 返回的是对齐**之前**的 `ptr`，对齐的是下一次。
- RV32/64 的 `realloc` 不拷贝（syscall 约 306–311 / 303–308 行）。
- `calloc(nmemb * size)` 可溢出。

C 堆应放到 `__heap_end` 之后、栈之前，或禁止 libc `malloc`，让 JS 独占该区。

### 5. `js_load` 返回未初始化值（中）

```141:144:riscv/mqjs_baremetal.c
static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JSValue ret;
    return ret;
}
```

stdlib 导出了 `load()`。syscall 里另有一份全局 `js_load` 抛 TypeError，但 stdlib 在本 TU 里绑定的是 **static** 版本。JS 一调 `load()` 就是 UB。`js_performance_now`（133–136 行）恒返回 `0`，与 README「Timer」不符。

### 6. JS 辅助函数双份实现（中，可维护性）

`mqjs_baremetal.c` 里 `js_print`/`js_gc`/`js_setTimeout` 等是 `static`，syscall 里又有全局同名函数。链接能过（现 ELF：`t` + `T` 各一份），但行为不一致：static 版会 `JS_PrintValueF`，全局版非字符串直接丢掉。`js_console_log` 是死代码——stdlib 的 `console.log` 绑的是 `js_print`（`mqjs_stdlib.c:320`）。

### 7. 启动与异常（中）

```7:8:riscv/start_riscv32.s
    li t0, 0
    csrw mstatus, t0
```

RV32 把整个 `mstatus` 清零；RV64 只用 `csrci mstatus, 0x8`。都未设 `mtvec`，未清/传 `argc/argv`（`main` 未用，但仍是垃圾）。返回后：

```29:36:riscv/start_riscv32.s
        fence
        li gp, 1
        li a7, 93
        li a0, 0
        ecall
    wfi
```

`-bios none` 的 M-mode `ecall`（Linux `exit`）会陷到 `mtvec=0`。QEMU 脚本靠 `timeout` 收尸，真机上是未定义。应设 `mtvec`、用 `a0=main返回值` 停机，不要假 Linux syscall。

BSS：RV32 按 4 字节、RV64 按 8 字节清，链接脚本有 ALIGN，当前没问题；无溢出检测。

### 8. UART（中，真机）

```45:47:riscv/baremetal_syscall_rv32.c
void putchar(char c) {
    volatile char *uart = (volatile char *)UART_BASE_ADDR;
    *uart = c;
}
```

不初始化 16550（LCR/除数/FIFO），不等 LSR THR 空。QEMU virt 常能出字；真机易丢字节。`print_char` 把 `\n` 变成 `\r\n`，`putchar`/`write()`/`puts` 不会。无 `getchar`，REPL 不存在。README 写 115200，固件从未设波特率。

---

## 内存 / 链接布局

默认（与现 ELF 一致）：

- RAM `0x80000000`，`LENGTH=4M`（`riscv64_baremetal.ld:30`）
- `.text/.rodata/.data/.bss` → `.heap` 16KB → 空洞 → 栈 8KB 在 `0x80400000` 向下
- `CONFIG_BYTECODE` 时 `--section-start=.jsbytecode=0x80100000`

问题：

1. `.jsbytecode` **没有 `(NOLOAD)`**（`riscv32_baremetal.ld:73-79`），WAIT 模式注释/README 说 NOBITS，实现可能是 PROGBITS 零填充。QEMU 先加载 ELF 再 loader 时还能工作；若改成真正 NOBITS，未初始化 RAM 可能偶然 `ready==1`。
2. `gp = .text末 + 0x800`（ld:38），小数据实际在 `.data/.srodata`（现 ELF 约 `0x8001f8xx`），**不在 gp±2KB**。`medany` 多半靠 PC 相对，但 gp 松弛是隐患。应按 sdata 中点设 `__global_pointer$`。
3. README 说把 QEMU `-m` 加到 8M 能缓解内存不足（README 约「Insufficient Memory」），**链接脚本仍是 4M，栈仍在 `0x80400000`**。必须同步改 `LENGTH(RAM)`。
4. 代码+rodata 已约 128KB，再加 16KB heap + 8KB 栈。FAQ「10KB 即可」严重不符。
5. `HEAP_SIZE`（C）与 `__heap_size`（ld）靠 Makefile 同时传；只改一边会静默不同步。应 `ASSERT(__heap_size == HEAP_SIZE)` 或只保留一处。

---

## Bytecode mailbox 协议

头布局与 `jsbytecode_slot.h:8-16`、README 表基本一致。实现要点：

```280:295:riscv/mqjs_baremetal.c
    if (h->ready != JSBC_SLOT_READY)
        return 0;
    jsbc_slot_fence();
    if (h->magic != JSBC_SLOT_MAGIC)
        return 0;
    n = h->length;
    if (n < sizeof(JSBytecodeHeader) || n > cap)
        return 0;
#if CONFIG_BYTECODE_CHECKSUM
    return jsbc_slot_checksum(image, n) == h->checksum;
```

```327:344:riscv/mqjs_baremetal.c
    while (!slot_is_valid(...)) {
        spins++;
        if (BYTECODE_WAIT_SPINS && spins >= BYTECODE_WAIT_SPINS) {
            ...
            h->ready = JSBC_SLOT_ERROR;
```

**ready 原子性**

- `ready` 是对齐 `uint32_t` + `volatile`，单 hart 对齐 32-bit 存取可视为原子。
- 有 `fence rw, rw`，先读 ready 再读其余，符合「ready 最后写」的 acquire。
- **没用 A 扩展 AMO**，也没有 `_Atomic`。多 hart / DMA / 非一致缓存不够。
- 校验通过后再次读 `length`、置 `TAKEN`，**不重新校验**。`JS_RelocateBytecode` 原地打补丁，存在 TOCTOU。应：fence → 置 TAKEN → fence → 再校验 magic/length/checksum → 再 relocate。
- `ready==1` 但 magic/checksum 失败时被当成「未就绪」死等。应把「已就绪但无效」标 `ERROR` 并退出。
- `JSBC_SLOT_ERROR=4`（`jsbytecode_slot.h:29`）README 表只写了 0–3。

**checksum**

- 字节和，不含 header，易碰撞，不是完整性/认证。
- `CONFIG_BYTECODE_CHECKSUM`：C 在 WAIT 下默认 1（`mqjs_baremetal.c:268-270`）；Makefile 只有用户显式设置才 `-D`（`Makefile.riscv:107-109`）。与 README「默认 1」一致，但 Makefile 注释易让人以为 make 变量默认是 1。
- `CONFIG_BYTECODE_CHECKSUM=0` 时只信 `ready`，外部写 0x80100000 就能执行。

**timeout**

- `BYTECODE_WAIT_SPINS` 是忙等次数，不是墙钟；`0` 永远等。依赖 CPU 频率和 `-Os`。
- 忙等无 `wfi`/`pause`，真机耗电。
- `spins` 为 `uint32_t`，永远等时会回绕，偶尔多打一行 `still waiting...`。

**其它**

- `js_to_bc.c:203`：`wrap_slot` 直接 `ready=JSBC_SLOT_READY`，符合「QEMU 在 CPU 启动前加载」；README 对真机「先 ready=0 再写 1」的说明是对的，但工具没有「ready=0 的 slot」生成选项。
- `wrap_slot` 不检查 `header+bin <= JSBYTECODE_SIZE`（默认 8192）。
- 32 位 `JS_PrepareBytecode64to32` 已把 `base_addr=0`、`version=JS_BYTECODE_VERSION_32`（与 RV32 上 `JS_BYTECODE_VERSION` 相同），MCU 侧 `JS_RelocateBytecode` 合理。这条不是 bug。
- 嵌入式路径（无 WAIT）是原始 `.bin`，无 16 字节头，与 mailbox 不同，这点是对的。

---

## ISA / 软浮点 / 编译选项

- RV32：`-march=rv32imac_zicsr`（`Makefile.riscv:81`）
- RV64：`-march=rv64imac` **无 `zicsr`**（74 行）。启动和 `csrr mcycle` 在较新 GCC 上会失败。
- 始终 `-DUSE_SOFTFLOAT`，`libm.c` 提供 `__adddf3` 等；现 ELF 已链上这些符号，**未链 `-lgcc`** 目前能过，但移位/其他 runtime 仍缺。
- FAQ 写「只支持 RV32I/RV64I，M 可选」，实际 `imac`。A 在 march 里却不用原子指令。
- 无 `-msoft-float`（无 F 时通常不必）；`CONFIG_SOFTFLOAT` 与 RISC-V 路径是两套。
- `QEMU -cpu rv32/rv64` 可能比 `imac` 更宽，掩盖非法指令。
- 无 `-ffunction-sections -Wl,--gc-sections`；`CONFIG_ASAN` 若打开会进裸机链接。

---

## 真机可移植性

必须改、文档却写得很轻描淡写的部分：

1. `LENGTH(RAM)` / `ORIGIN`、UART 基址、波特率、16550 初始化与 LSR 轮询
2. `mtvec`、PMP（UART `0x10000000` 与 RAM）、可能的 `mtime` 而非乱填的 `MCOUNTER_FREQ_HZ=10MHz`
3. 启动：不要 Linux `ecall`；按 bootloader 要求设 `a0/a1`（hartid/dtb）
4. mailbox：cache 属性、多核 fence、先加载固件再写 slot；ELF 是否把 slot 清零
5. 代码体积 ≫ 64KB「建议」；需要约 **≥160KB RAM + XIP 或更大 RAM**
6. 无看门狗，JS 死循环即死机

---

## JS eval / 字节码安全

`mquickjs.h:378-379` 写明：`JS_LoadBytecode` **不做校验，必须来自可信源**。

- 默认：内嵌源码，可信。
- `CONFIG_BYTECODE`：镜像在可写 RAM，relocate 原地打补丁。
- `CONFIG_BYTECODE_WAIT`：能写 `0x80100000` 就能注入；校验和不是签名。
- stdlib 仍有全局 `eval`（`mqjs_stdlib.c:369`）。
- 无时间配额、无 WDT、无 PMP 把 mailbox 与代码隔开。

嵌入式场景应：关 `eval`、校验和改为 CRC32 或签名、mailbox 只执行一次并锁 PMP、限制循环/内存。

---

## 与 `README_RISCV.md` 的不一致

1. **不是 REPL**。无 `getchar`/readline；跑完测试就退出。Hardware Support 写「REPL interaction」不实。
2. 32-bit 构建代码块（约 44–51 行）**少闭合 \`\`\`**。
3. 工程结构（约 375–385 行）缺 `jsbytecode_slot.h`、`js_to_bc.c`、`test_code.js`。
4. FAQ：ISA 只有 I；Makefile 是 `imac` + 软浮点。
5. 「10KB RAM」vs 实际 ~128KB 镜像 + 16KB heap + 8KB 栈。
6. 「增加 `-m 8M`」不改 linker `LENGTH`。
7. 「UART 初始化」——代码里没有初始化。
8. 系统调用「模拟 gettimeofday」——实际是 `mcycle`，文档后部有 `MCOUNTER_FREQ_HZ`，前面仍像 Unix 时间。
9. mailbox 表缺 `ready=4 ERROR`。
10. 示例 QEMU 无 `force-raw=on`（wait 脚本有）。
11. 「把系统调用注册到 `mqjs_baremetal.c`」——实际靠生成的 stdlib + 同名 C 函数。
12. `qemu_rv32.sh`/`qemu_rv64.sh` 无 shebang，且 `make clean`；wait 脚本删已经不用的 `riscv/ready.bin`。
13. `HEAP_SIZE = 32768` 示例与 `HEAP_SIZE?=16384` 的覆盖关系未说清。
14. Limitations 写需要 FPU 或软浮点——构建已强制软浮点。

---

## 建议优先级

1. 重写 `shift_rv32.s`，或 `-lgcc` 并删错误 helper。
2. 用汇编实现符合 ABI 的 `setjmp`/`longjmp`，与 `<setjmp.h>` 一致。
3. 分开 JS heap 与 C heap；修好 malloc 对齐/`realloc`。
4. 修好 RV32 `copysign`；`js_load` 返回明确错误。
5. mailbox：ready=1 且非法则 `ERROR`；TAKEN 后复检；CRC；文档补 `ERROR=4`。
6. UART：LSR + 初始化；RV64 `march` 加 `_zicsr`；设 `mtvec`；去掉假 `ecall`。
7. 按真实体积改 README；删 REPL 说法；补文件列表与 4M linker 约束。

未修改任何文件。

### Steps
- ⬜ `step-1` **修复 RV32 64 位移位** <span class="step-edit-btn" data-step-id="step-1" title="Edit step">✏️</span>
  - 重写 riscv/shift_rv32.s 的 __ashldi3/__lshrdi3/__ashrdi3，或改为链接 libgcc 并删除错误实现；用已知移位量做交叉测试。
- ⬜ `step-2` **修复 setjmp 与堆隔离** <span class="step-edit-btn" data-step-id="step-2" title="Edit step">✏️</span>
  - 用汇编实现与 <setjmp.h> 布局一致的 setjmp/longjmp；把 C malloc 移出 JS __heap_start 区域并修正对齐与 realloc。
- ⬜ `step-3` **强化 mailbox 与 UART/启动** <span class="step-edit-btn" data-step-id="step-3" title="Edit step">✏️</span>
  - ready=1 且非法则置 ERROR；TAKEN 后复检；考虑 CRC/AMO；UART 加 LSR 与初始化；RV64 加 zicsr；设置 mtvec 并去掉假 ecall。
- ⬜ `step-4` **同步 README_RISCV.md** <span class="step-edit-btn" data-step-id="step-4" title="Edit step">✏️</span>
  - 纠正 REPL、RAM 下限、ERROR=4、工程文件列表、ISA、QEMU -m 与 linker LENGTH、缺闭合代码块等问题。

### Diagnostics
- [info] Preferences MCP config detected (977 chars).
- [info] Pi session MCP directory prepared: /home/wood/.config/markzen/pi-agent-sessions/session-persistent
- [info] Pi MCP adapter is configured for this session via package: npm:pi-mcp-adapter
- [error] `riscv/shift_rv32.s` 64 位左移/逻辑右移/算术右移在 shift<32 时用减法+移位0，结果错误；RV32 的 __udivdi3 会调用它们。
- [error] `riscv/baremetal_syscall_rv32.c` copysign 仅改写 double 的低 32 位，符号位在高半字。
- [error] `riscv/mqjs_baremetal.c` js_load 返回未初始化 JSValue；stdlib 绑定的是该 static 版本。
- [warning] `riscv/baremetal_syscall_rv32.c` malloc 与 JS heap 同起点；先返回未对齐指针再对齐下一次；realloc 不拷贝。
- [warning] `riscv/baremetal_syscall_rv32.c` setjmp/longjmp 非 ABI 叶子函数，jmp_buf 不共享头文件，解析器 longjmp 可能坏栈。
- [warning] `riscv/mqjs_baremetal.c` mailbox：ready==1 但校验失败会空转；TAKEN 后不复检；超时按自旋次数而非时间。
- [warning] `Makefile.riscv` RV64 -march=rv64imac 无 zicsr，与 RV32 的 rv32imac_zicsr 不一致。
- [warning] `riscv/start_riscv32.s` 无 mtvec；main 返回后 M-mode ecall SYS_exit，-bios none 下行为未定义。

### Warnings
- UART 只写 THR、无 LSR/初始化，真机易丢字符且无输入，不是 REPL。
- checksum 为字节和，CONFIG_BYTECODE_CHECKSUM=0 时固定地址 mailbox 可被注入；JS_LoadBytecode 明确要求可信源，stdlib 仍导出 eval。
- .jsbytecode 未标 NOLOAD，与 README 的 NOBITS 描述不符。
- gp 不在小数据中点；HEAP_SIZE 与 linker __heap_size 可能不同步。
- 代码+rodata 约 128KB，FAQ 的 10KB 最低 RAM 不成立。
- qemu_rv32.sh/qemu_rv64.sh 无 shebang；wait 脚本仍清理已废弃的 ready.bin。