#!/bin/sh
# Build the isolated RV64 output tree, then boot the fresh ELF.
# Objects live in build/riscv64, so switching architectures/configs never
# reuses stale artifacts and no `make clean` is needed between runs.
set -eu
make -f Makefile.riscv CONFIG_RISCV64_BAREMETAL=y
timeout --foreground 5 qemu-system-riscv64 -machine virt -cpu rv64 -nographic -bios none \
  -kernel build/riscv64/mqjs_baremetal.elf -m 4M -serial mon:stdio
