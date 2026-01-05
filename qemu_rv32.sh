make clean
make CONFIG_RISCV32_BAREMETAL=y
timeout --foreground 5 qemu-system-riscv32 -machine virt -cpu rv32 -nographic -bios none -kernel ./mqjs_baremetal.elf  -m 4M  -serial mon:stdio
