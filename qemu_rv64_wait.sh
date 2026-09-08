#!/bin/sh
# Firmware first, bytecode via a fixed-address mailbox slot.
set -e
make -f Makefile.riscv CONFIG_RISCV64_BAREMETAL=y CONFIG_BYTECODE_WAIT=y
QEMU="${QEMU:-qemu-system-riscv64}"
trap 'rm -f riscv/ready.bin' EXIT INT TERM
timeout --foreground 8 "$QEMU" -machine virt -cpu rv64 -nographic -bios none \
  -kernel ./mqjs_baremetal.elf -m 4M -serial mon:stdio \
  -device loader,file=riscv/test_code.slot,addr=0x80100000,force-raw=on
