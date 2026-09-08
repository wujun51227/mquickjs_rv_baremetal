#!/bin/sh
# Firmware first, bytecode via a fixed-address mailbox slot.
# All artifacts stay inside the isolated build/riscv64-wait tree.
set -eu
make -f Makefile.riscv CONFIG_RISCV64_BAREMETAL=y CONFIG_BYTECODE_WAIT=y
QEMU="${QEMU:-qemu-system-riscv64}"
timeout --foreground 8 "$QEMU" -machine virt -cpu rv64 -nographic -bios none \
  -kernel build/riscv64-wait/mqjs_baremetal.elf -m 4M -serial mon:stdio \
  -device loader,file=build/riscv64-wait/test_code.slot,addr=0x80100000,force-raw=on
