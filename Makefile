# RIVET — bare-metal C99 kernel/firmware framework
#
# Layout
#   include/   runtime headers (header-only library)
#   src/       tool sources + per-arch asm/disas backends
#   examples/  example firmware/kernel C sources + .s asm files
#   build/     compiled toolchain executables
#   bin/       assembled .bin outputs, linked images, hex files
#
# Targets
#   make            build all tools + assemble every example .s into bin/
#   make tools      build host toolchain only (rivet-as, -disas, -ld, ...)
#   make examples   assemble all example .s files into bin/
#   make objects    compile-check all example .c files (freestanding)
#   make clean      remove build/ and bin/

CC          := gcc
CSTD        := -std=c99
CFLAGS_TOOL := $(CSTD) -Wall -Wextra -O2 -no-pie
CFLAGS_FW   := $(CSTD) -ffreestanding -nostdlib -Wall -Wextra \
               -fno-stack-protector -Iinclude

BUILD       := build
BIN         := bin

# ---------- toolchain ------------------------------------------------------
TOOL_ARCH_AS    := src/arch/riscv32_as.c    src/arch/x86_64_as.c
TOOL_ARCH_DISAS := src/arch/riscv32_disas.c src/arch/x86_64_disas.c

TOOLS := $(BUILD)/rivet-as       \
         $(BUILD)/rivet-disas    \
         $(BUILD)/rivet-ld       \
         $(BUILD)/rivet-objcopy  \
         $(BUILD)/rivet-img      \
         $(BUILD)/rivet-strings  \
         $(BUILD)/rivet-size

.PHONY: all tools examples objects clean dirs

all: tools examples objects

dirs:
	@mkdir -p $(BUILD) $(BIN)

tools: dirs $(TOOLS)

$(BUILD)/rivet-as: src/rivet-as.c $(TOOL_ARCH_AS) | dirs
	$(CC) $(CFLAGS_TOOL) $^ -o $@

$(BUILD)/rivet-disas: src/rivet-disas.c $(TOOL_ARCH_DISAS) | dirs
	$(CC) $(CFLAGS_TOOL) $^ -o $@

$(BUILD)/rivet-ld: src/rivet-ld.c | dirs
	$(CC) $(CFLAGS_TOOL) $< -o $@

$(BUILD)/rivet-objcopy: src/rivet-objcopy.c | dirs
	$(CC) $(CFLAGS_TOOL) $< -o $@

$(BUILD)/rivet-img: src/rivet-img.c | dirs
	$(CC) $(CFLAGS_TOOL) $< -o $@

$(BUILD)/rivet-strings: src/rivet-strings.c | dirs
	$(CC) $(CFLAGS_TOOL) $< -o $@

$(BUILD)/rivet-size: src/rivet-size.c | dirs
	$(CC) $(CFLAGS_TOOL) $< -o $@

# ---------- example assembly (.s -> bin/*.bin) -----------------------------
# Source layout:
#   examples/asm/riscv32/*.s    RV32 sources
#   examples/asm/x86_64/*.s     x86_64 sources
RV_ASM       := $(wildcard examples/asm/riscv32/*.s)
X86_ASM      := $(wildcard examples/asm/x86_64/*.s)

RV_BINS      := $(patsubst examples/asm/riscv32/%.s,$(BIN)/%.bin,$(RV_ASM))
X86_BINS     := $(patsubst examples/asm/x86_64/%.s,$(BIN)/%.bin,$(X86_ASM))

# Standalone RV32 firmware demo (runs on the QEMU virt board).
FW_RV_BINS   := $(BIN)/rv32_blink.bin $(BIN)/rv32_sensor.bin

examples: tools $(RV_BINS) $(X86_BINS) $(FW_RV_BINS)

$(BIN)/%.bin: examples/asm/riscv32/%.s | dirs $(BUILD)/rivet-as
	$(BUILD)/rivet-as -m riscv32 $< -o $@ -b 0x80000000

$(BIN)/rv32_blink.bin: examples/firmware/rv32_blink.s | dirs $(BUILD)/rivet-as
	$(BUILD)/rivet-as -m riscv32 $< -o $@ -b 0x80000000

$(BIN)/rv32_sensor.bin: examples/firmware/rv32_sensor.s | dirs $(BUILD)/rivet-as
	$(BUILD)/rivet-as -m riscv32 $< -o $@ -b 0x80000000

$(BIN)/%.bin: examples/asm/x86_64/%.s | dirs $(BUILD)/rivet-as
	$(BUILD)/rivet-as -m x86_64 $< -o $@ -b 0x200000

# x86_64 .bin demos become bootable by wrapping them with a 32->64
# multiboot trampoline (lm64_launch.S). The .bin payload is embedded
# into the .payload section of an ELF32 via objcopy.
OBJCOPY  ?= objcopy
X86_ELFS := $(patsubst examples/asm/x86_64/%.s,$(BIN)/%.elf,$(X86_ASM))

examples: $(X86_ELFS)

$(BUILD)/lm64_launch.o: examples/qemu/lm64_launch.S | dirs
	$(CC) -m32 -c $< -o $@

$(BUILD)/%_blob.o: $(BIN)/%.bin
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 \
	    --rename-section .data=.payload \
	    $< $@

$(BIN)/%.elf: $(BUILD)/lm64_launch.o $(BUILD)/%_blob.o \
              examples/qemu/lm64_launch.ld
	$(LD) -m elf_i386 -no-pie -T examples/qemu/lm64_launch.ld \
	    $(BUILD)/lm64_launch.o $(BUILD)/$*_blob.o -o $@

# ---------- example C compile-check ---------------------------------------
# Source layout:
#   examples/firmware/*.c       firmware-style demos
#   examples/kernel/*.c         kernel-style demos
EX_FIRMWARE  := $(wildcard examples/firmware/*.c)
EX_KERNEL    := $(wildcard examples/kernel/*.c)
EX_CSRC      := $(EX_FIRMWARE) $(EX_KERNEL)
EX_COBJ      := $(patsubst examples/firmware/%.c,$(BUILD)/obj/firmware/%.o,$(EX_FIRMWARE)) \
                $(patsubst examples/kernel/%.c,$(BUILD)/obj/kernel/%.o,$(EX_KERNEL))

objects: $(EX_COBJ)

$(BUILD)/obj/firmware/%.o: examples/firmware/%.c | dirs
	@mkdir -p $(BUILD)/obj/firmware
	$(CC) $(CFLAGS_FW) -c $< -o $@

$(BUILD)/obj/kernel/%.o: examples/kernel/%.c | dirs
	@mkdir -p $(BUILD)/obj/kernel
	$(CC) $(CFLAGS_FW) -c $< -o $@

# ---------- bootable multiboot kernel for QEMU -----------------------------
# 32-bit ELF that boots under `qemu-system-x86_64 -kernel`. Prints to
# the QEMU debug console (port 0xE9), then exits via isa-debug-exit.
#
# Run with:
#   make qemu-run
MB_CFLAGS := -m32 -std=c99 -ffreestanding -nostdlib -Wall -Wextra \
             -fno-stack-protector -fno-pic -fno-pie -Iinclude

# Kernel-style C demos are linked as 32-bit multiboot1 ELFs since
# QEMU's -kernel only accepts ELF32 for the multiboot path. The runtime
# auto-selects the 32-bit x86 backend in arch.h when -m32 is used.
KERN_CFLAGS := -m32 -std=c99 -ffreestanding -nostdlib -Wall -Wextra \
               -fno-stack-protector -fno-pic -fno-pie -Iinclude

# Compile the multiboot1 trampoline once; reused by every kernel ELF.
$(BUILD)/kern_boot.o: examples/qemu/multiboot.S | dirs
	$(CC) -m32 -c $< -o $@

# Pattern rule: bin/<name>.elf <- examples/kernel/<name>.c
# Use gcc as the linker driver so libgcc helpers (__udivdi3 etc) resolve.
$(BIN)/%.elf: examples/kernel/%.c $(BUILD)/kern_boot.o examples/qemu/multiboot.ld | dirs
	$(CC) $(KERN_CFLAGS) -c $< -o $(BUILD)/k_$*.o
	$(CC) -m32 -nostdlib -no-pie -Wl,-T,examples/qemu/multiboot.ld \
	      $(BUILD)/kern_boot.o $(BUILD)/k_$*.o -lgcc -o $@

$(BIN)/multiboot.elf: examples/qemu/multiboot.S examples/qemu/kmain.c \
                      examples/qemu/multiboot.ld | dirs
	$(CC) -m32 -c examples/qemu/multiboot.S -o $(BUILD)/multiboot_boot.o
	$(CC) $(MB_CFLAGS) -c examples/qemu/kmain.c -o $(BUILD)/multiboot_kmain.o
	$(LD) -m elf_i386 -no-pie -T examples/qemu/multiboot.ld \
	      $(BUILD)/multiboot_boot.o $(BUILD)/multiboot_kmain.o -o $@

.PHONY: qemu-build qemu-run
qemu-build: $(BIN)/multiboot.elf

qemu-run: $(BIN)/multiboot.elf
	qemu-system-x86_64 -kernel $< -no-reboot \
	    -debugcon stdio -serial null -display none \
	    -device isa-debug-exit,iobase=0xF4,iosize=0x04 ; true

# Boot each kernel-style demo under qemu-system-x86_64; debugcon output
# is the only visible channel because we run with -display none.
.PHONY: qemu-run-kernel qemu-run-full qemu-run-panic qemu-run-init qemu-run-all
qemu-run-kernel: $(BIN)/kernel.elf
	timeout 4 qemu-system-x86_64 -kernel $< -no-reboot \
	    -debugcon stdio -serial null -display none \
	    -device isa-debug-exit,iobase=0xF4,iosize=0x04 ; true
qemu-run-full: $(BIN)/kernel_full.elf
	timeout 4 qemu-system-x86_64 -kernel $< -no-reboot \
	    -debugcon stdio -serial null -display none \
	    -device isa-debug-exit,iobase=0xF4,iosize=0x04 ; true
qemu-run-panic: $(BIN)/panic_test.elf
	timeout 4 qemu-system-x86_64 -kernel $< -no-reboot \
	    -debugcon stdio -serial null -display none \
	    -device isa-debug-exit,iobase=0xF4,iosize=0x04 ; true
qemu-run-init: $(BIN)/x86_init.elf
	timeout 5 qemu-system-x86_64 -kernel $< -no-reboot \
	    -debugcon stdio -serial null -display none \
	    -device isa-debug-exit,iobase=0xF4,iosize=0x04 ; true

.PHONY: qemu-run-branch qemu-run-hello qemu-run-memops
qemu-run-branch: $(BIN)/branch.elf
	timeout 4 qemu-system-x86_64 -kernel $< -no-reboot \
	    -debugcon stdio -serial null -display none \
	    -device isa-debug-exit,iobase=0xF4,iosize=0x04 ; true
qemu-run-hello: $(BIN)/hello.elf
	timeout 4 qemu-system-x86_64 -kernel $< -no-reboot \
	    -debugcon stdio -serial null -display none \
	    -device isa-debug-exit,iobase=0xF4,iosize=0x04 ; true
qemu-run-memops: $(BIN)/memops.elf
	timeout 4 qemu-system-x86_64 -kernel $< -no-reboot \
	    -debugcon stdio -serial null -display none \
	    -device isa-debug-exit,iobase=0xF4,iosize=0x04 ; true

qemu-run-all: qemu-run qemu-run-rv32 qemu-run-rv32-blink \
              qemu-run-kernel qemu-run-full qemu-run-panic qemu-run-init \
              qemu-run-branch qemu-run-hello qemu-run-memops

# RV32 hello-to-UART demo. Boots on qemu-system-riscv32's virt machine,
# prints to the 16550 UART at 0x10000000, then enters a spin loop.
.PHONY: qemu-run-rv32 qemu-run-rv32-blink
qemu-run-rv32: $(BIN)/uart_hello.bin
	timeout 2 qemu-system-riscv32 -machine virt -bios none \
	    -kernel $< -nographic -no-reboot ; true

qemu-run-rv32-blink: $(BIN)/rv32_blink.bin
	timeout 3 qemu-system-riscv32 -machine virt -bios none \
	    -kernel $< -nographic -no-reboot ; true

.PHONY: qemu-run-rv32-sensor
qemu-run-rv32-sensor: $(BIN)/rv32_sensor.bin
	timeout 3 qemu-system-riscv32 -machine virt -bios none \
	    -kernel $< -nographic -no-reboot ; true

# RV32 instruction-trace smoke test for the older compute-only demos.
.PHONY: qemu-trace-rv32
qemu-trace-rv32: $(BIN)/boot.bin
	@echo "--- in_asm trace of bin/boot.bin under qemu-system-riscv32 ---"
	timeout 1 qemu-system-riscv32 -machine virt -bios none -kernel $< \
	    -nographic -no-reboot -no-shutdown \
	    -d in_asm,nochain -D /tmp/rivet_rv32.log 2>/dev/null ; true
	@grep -E '0x8000[0-9a-f]+' /tmp/rivet_rv32.log

# ---------- housekeeping ---------------------------------------------------
clean:
	rm -rf $(BUILD) $(BIN)

# Show what the build will produce.
list:
	@echo "Tools:    $(TOOLS)"
	@echo "RV bins:  $(RV_BINS)"
	@echo "x86 bins: $(X86_BINS)"
	@echo "Objects:  $(EX_COBJ)"
