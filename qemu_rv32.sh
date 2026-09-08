#!/bin/sh
# Build the isolated RV32 output tree, then boot the fresh ELF.
# Objects live in build/riscv32, so switching architectures/configs never
# reuses stale artifacts and no `make clean` is needed between runs.
set -eu
make -f Makefile.riscv CONFIG_RISCV32_BAREMETAL=y
timeout --foreground 5 qemu-system-riscv32 -machine virt -cpu rv32 -nographic -bios none \
  -kernel build/riscv32/mqjs_baremetal.elf -m 4M -serial mon:stdio
