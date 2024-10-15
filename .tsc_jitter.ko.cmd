cmd_/tsc/tsc_jitter.ko := ld -r -m elf_x86_64 -T ./scripts/module-common.lds --build-id  -o /tsc/tsc_jitter.ko /tsc/tsc_jitter.o /tsc/tsc_jitter.mod.o
