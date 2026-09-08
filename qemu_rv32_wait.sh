#!/bin/sh
# Firmware first, bytecode via a fixed-address mailbox slot.
# -device loader writes the slot before the CPU starts; the firmware still
# goes through the handshake (ready/magic/length/checksum).
# All artifacts stay inside the isolated build/riscv32-wait tree.
set -eu
make -f Makefile.riscv CONFIG_RISCV32_BAREMETAL=y CONFIG_BYTECODE_WAIT=y
QEMU="${QEMU:-qemu-system-riscv32}"
timeout --foreground 8 "$QEMU" -machine virt -cpu rv32 -nographic -bios none \
  -kernel build/riscv32-wait/mqjs_baremetal.elf -m 4M -serial mon:stdio \
  -device loader,file=build/riscv32-wait/test_code.slot,addr=0x80100000,force-raw=on
