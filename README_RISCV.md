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
make clean

# Build 64-bit baremetal version
make CONFIG_RISCV64_BAREMETAL=y

# Output file: mqjs_baremetal.elf
```

### RISC-V 32-bit Version

```bash
# Clean previous build
make clean

# Build 32-bit baremetal version
make CONFIG_RISCV32_BAREMETAL=y

# Output file: mqjs_baremetal.elf
```

### Build Options

The following options can be configured in the Makefile:

- `CONFIG_RISCV64_BAREMETAL=y`: Enable RISC-V 64-bit baremetal mode
- `CONFIG_RISCV32_BAREMETAL=y`: Enable RISC-V 32-bit baremetal mode
- `CONFIG_SMALL=y`: Optimize for code size (enabled by default)
- `HEAP_SIZE`: Heap size (default 16384 bytes)
- `CONFIG_WERROR=y`: Treat warnings as errors (for development)

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

- **Heap Size**: 16 KB (16384 bytes)
- **Total Memory**: 4 MB (QEMU configuration)

### Modifying Heap Size

Modify the `HEAP_SIZE` variable in the Makefile:

```makefile
HEAP_SIZE = 32768  # 32 KB
```

Then rebuild:

```bash
make clean
make CONFIG_RISCV64_BAREMETAL=y
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
make CONFIG_RISCV64_BAREMETAL=y CONFIG_SMALL=y
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
├── mqjs_baremetal.c           # Baremetal REPL main program
├── baremetal_syscall_rv32.c   # RISC-V 32-bit system calls
├── baremetal_syscall_rv64.c   # RISC-V 64-bit system calls
├── start_riscv32.s            # RISC-V 32-bit startup code
├── start_riscv64.s            # RISC-V 64-bit startup code
├── shift_rv32.s               # 32-bit shift helper functions
├── riscv32_baremetal.ld       # 32-bit linker script
└── riscv64_baremetal.ld       # 64-bit linker script
```

## Limitations and Notes

1. **Standard Library Limitations**: The baremetal version does not include complete file system support
2. **Floating Point Operations**: Requires hardware FPU or software floating point library support
3. **Multithreading**: Does not support multithreading
4. **Networking**: No networking capabilities
5. **Dynamic Loading**: Does not support dynamic module loading

## FAQ

### Q: Can it run on real hardware?

A: Yes, but you need to adapt the startup code and linker script to match your hardware configuration.

### Q: Which RISC-V extensions are supported?

A: The base version only supports RV32I/RV64I. For other extensions (such as M, A, F, D), you need to modify the code accordingly.

### Q: How do I add custom system calls?

A: Add system call implementations in `baremetal_syscall_rv*.c` and register them in `mqjs_baremetal.c`.

### Q: What is the minimum memory requirement?

A: Theoretically, 10 KB RAM is sufficient to run, but at least 16-32 KB is recommended for better performance.

## Related Resources

- [QuickJS Official Website](https://bellard.org/quickjs/)
- [MQuickJS Official Website](https://bellard.org/mquickjs/)
- [RISC-V Official Website](https://riscv.org/)
- [QEMU Documentation](https://www.qemu.org/docs/master/)

## License

MIT License

Copyright (c) 2017-2025 Fabrice Bellard and Charlie Gordon
