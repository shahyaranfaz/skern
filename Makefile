SHELL := /bin/sh

# Toolchain commands may be overridden on the command line.
CC      := gcc
CXX     := g++
LD      := ld
OBJCOPY := objcopy
QEMU    := qemu-system-x86_64
PYTHON  := python3

BUILD_DIR := build

# GNU-EFI installs these files in different directories across distributions.
GNU_EFI_CRT ?= $(firstword $(wildcard \
	/usr/lib/crt0-efi-x86_64.o \
	/usr/lib64/crt0-efi-x86_64.o \
	/usr/lib/x86_64-linux-gnu/gnuefi/crt0-efi-x86_64.o \
	/usr/lib/gnuefi/crt0-efi-x86_64.o))
GNU_EFI_LDS ?= $(firstword $(wildcard \
	/usr/lib/elf_x86_64_efi.lds \
	/usr/lib64/elf_x86_64_efi.lds \
	/usr/lib/x86_64-linux-gnu/gnuefi/elf_x86_64_efi.lds \
	/usr/lib/gnuefi/elf_x86_64_efi.lds))
GNU_EFI_LIB_DIR ?= $(dir $(GNU_EFI_CRT))
GNU_EFI_INC_DIR ?= /usr/include/efi

OVMF_CODE ?= $(firstword $(wildcard \
	/usr/share/OVMF/OVMF_CODE_4M.fd \
	/usr/share/OVMF/OVMF_CODE.fd \
	/usr/share/edk2/x64/OVMF_CODE.fd \
	/usr/share/edk2/ovmf/OVMF_CODE.fd))
OVMF_VARS ?= $(firstword $(wildcard \
	/usr/share/OVMF/OVMF_VARS_4M.fd \
	/usr/share/OVMF/OVMF_VARS.fd \
	/usr/share/edk2/x64/OVMF_VARS.fd \
	/usr/share/edk2/ovmf/OVMF_VARS.fd))

rwildcard = $(foreach entry,$(wildcard $1/*),$(call rwildcard,$(entry),$2) $(filter $2,$(entry)))

BOOT_C_SRCS     := $(call rwildcard,bootloader,%.c)
BOOT_ASM_SRCS   := $(call rwildcard,bootloader,%.S)
KERNEL_CPP_SRCS := $(call rwildcard,kernel,%.cpp)
KERNEL_ASM_SRCS := $(call rwildcard,kernel,%.S)

BOOT_C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.c.o,$(BOOT_C_SRCS))
BOOT_ASM_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.S.o,$(BOOT_ASM_SRCS))
KERNEL_CPP_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.cpp.o,$(KERNEL_CPP_SRCS))
KERNEL_ASM_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.S.o,$(KERNEL_ASM_SRCS))

BOOT_OBJS := $(BOOT_C_OBJS) $(BOOT_ASM_OBJS)
KERNEL_OBJS := $(KERNEL_CPP_OBJS) $(KERNEL_ASM_OBJS)

COMMON_WARNINGS := -Wall -Wextra -Werror
COMMON_DEPFLAGS := -MMD -MP

BOOT_CPPFLAGS := -Iinclude -I$(GNU_EFI_INC_DIR) -I$(GNU_EFI_INC_DIR)/x86_64
BOOT_CFLAGS   := -ffreestanding -fno-stack-protector -fno-stack-check -fno-strict-aliasing \
	-fpic -fshort-wchar -mno-red-zone -maccumulate-outgoing-args \
	$(COMMON_WARNINGS) $(COMMON_DEPFLAGS)
BOOT_ASFLAGS  := $(BOOT_CFLAGS)
BOOT_LDFLAGS  := -nostdlib -Wl,--no-undefined -Wl,-znocombreloc \
	-Wl,-T,$(GNU_EFI_LDS) -Wl,-shared -Wl,-Bsymbolic

KERNEL_CPPFLAGS := -Iinclude
KERNEL_CXXFLAGS := -std=c++20 -ffreestanding -fno-exceptions -fno-rtti \
	-fno-threadsafe-statics -fno-use-cxa-atexit -fno-stack-protector -fno-stack-check \
	-fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel \
	-mno-mmx -mno-sse -mno-sse2 $(COMMON_WARNINGS) $(COMMON_DEPFLAGS)
KERNEL_ASFLAGS := -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
	-m64 -mno-red-zone $(COMMON_WARNINGS) $(COMMON_DEPFLAGS)
KERNEL_LDFLAGS := -nostdlib -static --build-id=none -z max-page-size=0x1000 \
	-T kernel/linker.ld

BOOT_SO    := $(BUILD_DIR)/bootloader/skern.so
BOOT_EFI   := $(BUILD_DIR)/esp/EFI/BOOT/BOOTX64.EFI
KERNEL_ELF := $(BUILD_DIR)/kernel/kernel.elf
KERNEL_ESP := $(BUILD_DIR)/esp/skern/kernel.elf
OVMF_VARS_COPY := $(BUILD_DIR)/ovmf/OVMF_VARS.fd
DEPS := $(BOOT_OBJS:.o=.d) $(KERNEL_OBJS:.o=.d)

.DEFAULT_GOAL := all

.PHONY: all run debug test clean help check-gnu-efi check-qemu check-ovmf

all: $(BOOT_EFI) $(KERNEL_ESP)

$(BUILD_DIR)/bootloader/%.c.o: bootloader/%.c | check-gnu-efi
	@mkdir -p $(@D)
	$(CC) $(BOOT_CPPFLAGS) $(CPPFLAGS) $(BOOT_CFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/bootloader/%.S.o: bootloader/%.S | check-gnu-efi
	@mkdir -p $(@D)
	$(CC) $(BOOT_CPPFLAGS) $(CPPFLAGS) $(BOOT_ASFLAGS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/%.cpp.o: kernel/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(KERNEL_CPPFLAGS) $(CPPFLAGS) $(KERNEL_CXXFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/%.S.o: kernel/%.S
	@mkdir -p $(@D)
	$(CC) $(KERNEL_CPPFLAGS) $(CPPFLAGS) $(KERNEL_ASFLAGS) $(ASFLAGS) -c $< -o $@

$(BOOT_SO): $(BOOT_OBJS) | check-gnu-efi
	@mkdir -p $(@D)
	$(CC) $(BOOT_LDFLAGS) $(LDFLAGS) $(GNU_EFI_CRT) $^ \
		-L$(GNU_EFI_LIB_DIR) -lefi -lgnuefi -o $@

$(BOOT_EFI): $(BOOT_SO)
	@mkdir -p $(@D)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
		-j .rel -j .rela -j '.rel.*' -j '.rela.*' -j .reloc \
		--target=efi-app-x86_64 $< $@

$(KERNEL_ELF): $(KERNEL_OBJS) kernel/linker.ld
	@mkdir -p $(@D)
	$(LD) $(KERNEL_LDFLAGS) $(LDFLAGS) $(KERNEL_OBJS) -o $@

$(KERNEL_ESP): $(KERNEL_ELF)
	@mkdir -p $(@D)
	cp $< $@

$(OVMF_VARS_COPY): $(OVMF_VARS) | check-ovmf
	@mkdir -p $(@D)
	cp $< $@

run: all $(OVMF_VARS_COPY) | check-qemu check-ovmf
	$(QEMU) -machine q35 -m 256M \
		-debugcon stdio -global isa-debugcon.iobase=0xe9 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS_COPY) \
		-drive format=raw,file=fat:rw:$(BUILD_DIR)/esp

debug: all $(OVMF_VARS_COPY) | check-qemu check-ovmf
	$(QEMU) -machine q35 -m 256M -S -s \
		-debugcon stdio -global isa-debugcon.iobase=0xe9 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS_COPY) \
		-drive format=raw,file=fat:rw:$(BUILD_DIR)/esp

test: all
	$(PYTHON) -m unittest discover -s tests -v

check-gnu-efi:
	@test -n "$(GNU_EFI_CRT)" -a -f "$(GNU_EFI_CRT)" || \
		{ echo "error: GNU-EFI startup object not found (set GNU_EFI_CRT)" >&2; exit 1; }
	@test -n "$(GNU_EFI_LDS)" -a -f "$(GNU_EFI_LDS)" || \
		{ echo "error: GNU-EFI linker script not found (set GNU_EFI_LDS)" >&2; exit 1; }
	@test -d "$(GNU_EFI_INC_DIR)" || \
		{ echo "error: GNU-EFI headers not found (set GNU_EFI_INC_DIR)" >&2; exit 1; }

check-qemu:
	@command -v $(QEMU) >/dev/null 2>&1 || \
		{ echo "error: QEMU not found (set QEMU)" >&2; exit 1; }

check-ovmf:
	@test -n "$(OVMF_CODE)" -a -f "$(OVMF_CODE)" || \
		{ echo "error: OVMF code image not found (set OVMF_CODE)" >&2; exit 1; }
	@test -n "$(OVMF_VARS)" -a -f "$(OVMF_VARS)" || \
		{ echo "error: OVMF variables image not found (set OVMF_VARS)" >&2; exit 1; }

clean:
	rm -rf -- $(BUILD_DIR)

help:
	@echo "skern build targets:"
	@echo "  all (default)  Build and package the bootloader and kernel"
	@echo "  run            Build and boot skern in QEMU"
	@echo "  debug          Start QEMU paused with a GDB server on :1234"
	@echo "  test           Run host and QEMU integration tests"
	@echo "  clean          Remove generated files"
	@echo "  help           Show this help"

-include $(DEPS)
