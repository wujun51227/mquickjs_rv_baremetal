# RISC-V Baremetal User Guide

## Overview

MQuickJS supports baremetal operation on RISC-V 32-bit and 64-bit processors without requiring an operating system. This allows MQuickJS to run directly on embedded RISC-V devices with minimal memory requirements.

## Requirements

### Essential Tools

- **RISC-V GCC Cross-Compilation Toolchain**:
  - 32-bit: `riscv32-unknown-elf-gcc`
  - 64-bit: `riscv64-unknown-elf-gcc`

- **QEMU Emulator** (for testing and development):
  - 32-bit: `qemu-system-riscv32`
  - 64-bit: `qemu-system-riscv64`

### Installing the Toolchain

#### Ubuntu/Debian

```bash
# Install RISC-V 64-bit toolchain
sudo apt-get install gcc-riscv64-unknown-elf qemu-system-riscv64

# Install RISC-V 32-bit toolchain
sudo apt-get install gcc-riscv32-unknown-elf qemu-system-riscv32
```

#### Building from Source

If you need the latest version of the toolchain, you can obtain it from the official RISC-V repository:

```bash
git clone https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain
./configure --prefix=/opt/riscv --enable-multilib
make
```

## Building RISC-V Baremetal Version

### RISC-V 64-bit Version

```bash
# Clean previous build
make -f Makefile.riscv clean

# Build 64-bit baremetal version
make -f Makefile.riscv CONFIG_RISCV64_BAREMETAL=y

# Output file: mqjs_baremetal.elf
```

### RISC-V 32-bit Version

```bash
# Clean previous build
make -f Makefile.riscv clean

# Build 32-bit baremetal version
make -f Makefile.riscv CONFIG_RISCV32_BAREMETAL=y

# Output file: mqjs_baremetal.elf
```

### Precompiled bytecode (optional)

Default firmware still `JS_Eval()`s the embedded `test_code` source on the MCU.
The bytecode region must be writable RAM because `JS_RelocateBytecode()` patches pointers in place.

**Embedded** (`CONFIG_BYTECODE=y`): host compiles `riscv/test_code.js` and links the `.bin` into the ELF at `JSBYTECODE_ADDR` (default `0x80100000`). The MCU runs it immediately.

**Mailbox / late load** (`CONFIG_BYTECODE_WAIT=y`, implies `CONFIG_BYTECODE`): the ELF only reserves a NOBITS slot of `JSBYTECODE_SIZE` (default 8192) at that address. Firmware starts first, polls a 16-byte header, then runs the image after a loader writes `riscv/test_code.slot`.

Slot layout at `JSBYTECODE_ADDR`:

| Offset | Field | Notes |
|--------|-------|--------|
| 0 | `magic` | `0x3142534A` (`JSB1`) |
| 4 | `length` | size of the raw `.bin` |
| 8 | `checksum` | sum of image bytes |
| 12 | `ready` | write **last**: 0 empty, 1 ready, 2 taken, 3 done, 4 error |
| 16 | `image` | raw `test_code.bin` |

```bash
# Embedded (bin is inside the ELF)
make -f Makefile.riscv clean
make -f Makefile.riscv CONFIG_RISCV32_BAREMETAL=y CONFIG_BYTECODE=y

# Mailbox: firmware first, bytecode later
make -f Makefile.riscv clean
make -f Makefile.riscv CONFIG_RISCV32_BAREMETAL=y CONFIG_BYTECODE_WAIT=y
# outputs mqjs_baremetal.elf and riscv/test_code.slot

qemu-system-riscv32 -machine virt -cpu rv32 -nographic -bios none \
  -kernel ./mqjs_baremetal.elf -m 4M -serial mon:stdio \
  -device loader,file=riscv/test_code.slot,addr=0x80100000
```

The generated QEMU slot contains `ready=1` because QEMU loads it before the CPU starts. On real hardware, write the complete slot with `ready=0`, then write `1` to `JSBYTECODE_ADDR + 12` as the final operation.

### Build Options

The following options can be configured in the Makefile:

- `CONFIG_RISCV64_BAREMETAL=y`: Enable RISC-V 64-bit baremetal mode
- `CONFIG_RISCV32_BAREMETAL=y`: Enable RISC-V 32-bit baremetal mode
- `CONFIG_SMALL=y`: Optimize for code size (enabled by default)
- `HEAP_SIZE`: Heap size (default 16384 bytes)
- `MCOUNTER_FREQ_HZ`: `mcycle` counter frequency in Hz (default 10000000). The resulting `gettimeofday()` value is monotonic time relative to the counter origin, not Unix epoch time.
- `CONFIG_WERROR=y`: Treat warnings as errors (for development)
- `CONFIG_BYTECODE=y`: Host-compile `riscv/test_code.js` and run the bytecode from `JSBYTECODE_ADDR` (default `0x80100000`).
- `CONFIG_BYTECODE_WAIT=y`: Same address, but firmware waits for an external loader to write a mailbox slot (`JSBYTECODE_SIZE`, default 8192). Implies `CONFIG_BYTECODE`.
- `BYTECODE_WAIT_SPINS`: Optional wait-loop limit; `0` waits forever, while a nonzero value reports `JSBC_SLOT_ERROR` on timeout.
- `CONFIG_BYTECODE_CHECKSUM`: Enable slot checksum validation (default `1`). Set to `0` to skip checksum calculation and reduce startup time when the loader and memory are trusted.

## Running and Testing

### Running with QEMU

#### RISC-V 64-bit

```bash
# Use the provided script
./qemu_rv64.sh

# Or run manually
qemu-system-riscv64 \
  -machine virt \
  -cpu rv64 \
  -nographic \
  -bios none \
  -kernel ./mqjs_baremetal.elf \
  -m 4M \
  -serial mon:stdio
```

#### RISC-V 32-bit

```bash
# Use the provided script
./qemu_rv32.sh

# Or run manually
qemu-system-riscv32 \
  -machine virt \
  -cpu rv32 \
  -nographic \
  -bios none \
  -kernel ./mqjs_baremetal.elf \
  -m 4M \
  -serial mon:stdio
```

### QEMU Parameter Description

- `-machine virt`: Use QEMU's virtual machine type
- `-cpu rv64/rv32`: Specify CPU type
- `-nographic`: Disable GUI, use serial console
- `-bios none`: Do not load BIOS, load kernel directly
- `-kernel`: ELF file to load
- `-m 4M`: Allocate 4MB memory
- `-serial mon:stdio`: Redirect serial to standard I/O

## Memory Configuration

### Default Memory Configuration

- **Heap Size**: 16 KB (16384 bytes), placed immediately after BSS
- **Stack**: grows down from the end of RAM (default reserved 8 KB)
- **Total Memory**: 4 MB (QEMU `LENGTH(RAM)` / `-m 4M`)

### Modifying Heap Size

Modify the `HEAP_SIZE` variable in the Makefile:

```makefile
HEAP_SIZE = 32768  # 32 KB
```

Then rebuild:

```bash
make -f Makefile.riscv clean
make -f Makefile.riscv CONFIG_RISCV64_BAREMETAL=y
```

## System Call Implementation

The MQuickJS RISC-V baremetal version implements the following system calls:

### Basic Output

- `putchar()`: Character output
- `puts()`: String output
- `printf()`: Formatted output

### Memory Management

- Custom memory allocator, does not rely on standard C library's malloc/free

### Time Functions

- `gettimeofday()`: Get system time (simulated)

## Hardware Support

### RISC-V Instruction Sets

- **RV32I**: RISC-V 32-bit integer instruction set
- **RV64I**: RISC-V 64-bit integer instruction set
- **M Extension**: Multiply/divide instructions (optional)

### Peripherals

- **UART**: For serial communication and REPL interaction
- **Timer**: For garbage collection and performance measurement

## Running on Real Hardware

### Preparation

1. Ensure your RISC-V development board supports the following features:
   - Sufficient RAM (recommended at least 64 KB)
   - UART serial port
   - Bootloader supports ELF format

2. Obtain the board's boot configuration and memory mapping information

3. Modify the linker script (`riscv*_baremetal.ld`) to match your hardware

### Loading the Program

Use the board's toolchain to load `mqjs_baremetal.elf` onto the hardware:

```bash
# Example: Using OpenOCD
openocd -f board/your_board.cfg -c "program mqjs_baremetal.elf verify reset exit"

# Or use other tools like J-Link, GDB, etc.
```

### Serial Connection

Connect to the development board using a serial terminal:

```bash
# Linux
screen /dev/ttyUSB0 115200

# Or use minicom
minicom -D /dev/ttyUSB0 -b 115200
```

## Troubleshooting

### Compilation Errors

**Problem**: Cannot find `riscv64-unknown-elf-gcc`

**Solution**: Install RISC-V toolchain or set PATH:

```bash
export PATH=/opt/riscv/bin:$PATH
```

### QEMU Startup Failure

**Problem**: QEMU cannot start

**Solution**: Check QEMU version and architecture support:

```bash
qemu-system-riscv64 --version
qemu-system-riscv64 --machine help | grep virt
```

### Insufficient Memory

**Problem**: Program runs out of memory

**Solution**: Increase QEMU memory allocation:

```bash
qemu-system-riscv64 -m 8M ...  # Increase to 8MB
```

Or increase heap size in the Makefile.

### No Serial Output

**Problem**: No output after startup

**Solution**:
1. Check UART initialization code
2. Verify QEMU serial parameters are correct
3. Check memory mapping in the linker script

## Performance Optimization

### Code Size Optimization

```bash
# Enable code size optimization
make -f Makefile.riscv CONFIG_RISCV64_BAREMETAL=y CONFIG_SMALL=y
```

### Compiler Optimization

You can adjust compiler optimization level in the Makefile:

```makefile
CFLAGS += -Os  # Optimize for code size
# Or
CFLAGS += -O2  # Balanced optimization
```

## Development and Debugging

### Debugging with GDB

```bash
# Start QEMU and wait for GDB connection
qemu-system-riscv64 -machine virt -cpu rv64 -nographic -bios none \
  -kernel ./mqjs_baremetal.elf -m 4M -s -S

# Start GDB in another terminal
riscv64-unknown-elf-gdb ./mqjs_baremetal.elf

# Connect in GDB
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

### Adding Debug Information

Add debug symbols in the Makefile:

```makefile
CFLAGS += -g
```

## Project Structure

```
riscv/
├── mqjs_baremetal.c           # Baremetal JavaScript execution main entry
├── baremetal_syscall_rv32.c   # RISC-V 32-bit system calls (UART, time, math, memory)
├── baremetal_syscall_rv64.c   # RISC-V 64-bit system calls
├── start_riscv32.s            # RISC-V 32-bit startup code (_start, trap, exit)
├── start_riscv64.s            # RISC-V 64-bit startup code
├── setjmp_rv32.S              # ABI-compliant setjmp/longjmp for RV32
├── setjmp_rv64.S              # ABI-compliant setjmp/longjmp for RV64
├── shift_rv32.s               # 32-bit 64-bit shift helper functions (__ashldi3, etc.)
├── jsbytecode_slot.h          # Bytecode mailbox slot header definition & checksum
├── js_to_bc.c                 # Host tool: JS compiler to bytecode and slot packager
├── test_code.js               # Test JavaScript script
├── test_shift_rv32.c          # Unit tests for RV32 shift helpers
├── riscv32_baremetal.ld       # 32-bit linker script
└── riscv64_baremetal.ld       # 64-bit linker script
```

## Limitations and Notes

1. **Standard Library Limitations**: The baremetal version does not include complete file system support
2. **Floating Point Operations**: Uses software floating point library support (`USE_SOFTFLOAT`)
3. **Multithreading**: Does not support multithreading
4. **Networking**: No networking capabilities
5. **Execution Mode**: Non-interactive batch execution (evaluates embedded script or mailbox bytecode)

## FAQ

### Q: Can it run on real hardware?

A: Yes, but you need to adapt the startup code, UART base address / baud rate, and linker script to match your hardware memory map.

### Q: Which RISC-V extensions are supported?

A: The default configurations use `rv32imac_zicsr` (RV32) and `rv64imac_zicsr` (RV64) with soft-float.

### Q: How do I add custom system calls?

A: Add system call implementations in `baremetal_syscall_rv*.c` and expose any desired JS bindings in `mqjs_baremetal.c`.

### Q: What is the minimum memory requirement?

A: The compiled baremetal firmware image is ~120 KB (`.text` + `.rodata`), plus 16 KB JS heap and 8 KB C stack. At least 160 KB RAM (or ROM + RAM for XIP) is required.

## Related Resources

- [QuickJS Official Website](https://bellard.org/quickjs/)
- [MQuickJS Official Website](https://bellard.org/mquickjs/)
- [RISC-V Official Website](https://riscv.org/)
- [QEMU Documentation](https://www.qemu.org/docs/master/)

## License

MIT License

Copyright (c) 2017-2025 Fabrice Bellard and Charlie Gordon
