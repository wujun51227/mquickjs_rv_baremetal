make -f Makefile.riscv clean
make -f Makefile.riscv CONFIG_RISCV64_BAREMETAL=y
timeout --foreground 5 qemu-system-riscv64 -machine virt -cpu rv64 -nographic -bios none -kernel ./mqjs_baremetal.elf  -m 4M -serial mon:stdio

