SHELL := /bin/sh

# Toolchain commands may be overridden on the command line, for example:
#   make CC=x86_64-linux-gnu-gcc
CC      := gcc
LD      := ld
OBJCOPY := objcopy
QEMU    := qemu-system-x86_64

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

# Recursively enumerate sources without requiring Makefile edits for new files.
rwildcard = $(foreach entry,$(wildcard $1/*),$(call rwildcard,$(entry),$2) $(filter $2,$(entry)))

BOOT_C_SRCS   := $(call rwildcard,boot,%.c)
BOOT_ASM_SRCS := $(call rwildcard,boot,%.S)
KERNEL_SRCS   := $(call rwildcard,kernel,%.c) $(call rwildcard,kernel,%.S)
LIBC_SRCS     := $(call rwildcard,libc,%.c) $(call rwildcard,libc,%.S)
UTILS_SRCS    := $(call rwildcard,utils,%.c) $(call rwildcard,utils,%.S)

BOOT_OBJS := $(patsubst %,$(BUILD_DIR)/%.o,$(basename $(BOOT_C_SRCS) $(BOOT_ASM_SRCS)))

# Each component owns its flags. These are intentionally kept separate even
# while only the bootloader has a final link target.
COMMON_FREESTANDING_CFLAGS := -ffreestanding -fno-stack-protector -fno-stack-check \
	-fno-strict-aliasing -Wall -Wextra -Werror -MMD -MP
BOOT_CPPFLAGS := -I$(GNU_EFI_INC_DIR) -I$(GNU_EFI_INC_DIR)/x86_64
BOOT_CFLAGS   := $(COMMON_FREESTANDING_CFLAGS) -fpic -fshort-wchar -mno-red-zone \
	-maccumulate-outgoing-args
BOOT_ASFLAGS  := $(BOOT_CFLAGS)
BOOT_LDFLAGS  := -nostdlib -Wl,--no-undefined -Wl,-znocombreloc \
	-Wl,-T,$(GNU_EFI_LDS) -Wl,-shared -Wl,-Bsymbolic

KERNEL_CPPFLAGS :=
KERNEL_CFLAGS   := $(COMMON_FREESTANDING_CFLAGS) -mno-red-zone
KERNEL_ASFLAGS  := $(KERNEL_CFLAGS)
KERNEL_LDFLAGS  := -nostdlib
LIBC_CPPFLAGS   :=
LIBC_CFLAGS     := $(COMMON_FREESTANDING_CFLAGS)
LIBC_ASFLAGS    := $(LIBC_CFLAGS)
LIBC_LDFLAGS    := -nostdlib
UTILS_CPPFLAGS  :=
UTILS_CFLAGS    := -Wall -Wextra -Werror -MMD -MP
UTILS_ASFLAGS   := $(UTILS_CFLAGS)
UTILS_LDFLAGS  :=

BOOT_SO  := $(BUILD_DIR)/boot/skern.so
BOOT_EFI := $(BUILD_DIR)/esp/EFI/BOOT/BOOTX64.EFI
OVMF_VARS_COPY := $(BUILD_DIR)/ovmf/OVMF_VARS.fd
DEPS := $(BOOT_OBJS:.o=.d)

.DEFAULT_GOAL := all

.PHONY: all run debug clean help check-gnu-efi check-qemu check-ovmf

all: $(BOOT_EFI)

$(BUILD_DIR)/boot/%.o: boot/%.c | check-gnu-efi
	@mkdir -p $(@D)
	$(CC) $(BOOT_CPPFLAGS) $(CPPFLAGS) $(BOOT_CFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot/%.o: boot/%.S | check-gnu-efi
	@mkdir -p $(@D)
	$(CC) $(BOOT_CPPFLAGS) $(CPPFLAGS) $(BOOT_ASFLAGS) $(ASFLAGS) -c $< -o $@

$(BOOT_SO): $(BOOT_OBJS) | check-gnu-efi
	@mkdir -p $(@D)
	$(CC) $(BOOT_LDFLAGS) $(LDFLAGS) $(GNU_EFI_CRT) $^ \
		-L$(GNU_EFI_LIB_DIR) -lefi -lgnuefi -o $@

$(BOOT_EFI): $(BOOT_SO)
	@mkdir -p $(@D)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
		-j .rel -j .rela -j '.rel.*' -j '.rela.*' -j .reloc \
		--target=efi-app-x86_64 $< $@

$(OVMF_VARS_COPY): $(OVMF_VARS) | check-ovmf
	@mkdir -p $(@D)
	cp $< $@

run: $(BOOT_EFI) $(OVMF_VARS_COPY) | check-qemu check-ovmf
	$(QEMU) -machine q35 -m 256M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS_COPY) \
		-drive format=raw,file=fat:rw:$(BUILD_DIR)/esp

debug: $(BOOT_EFI) $(OVMF_VARS_COPY) | check-qemu check-ovmf
	$(QEMU) -machine q35 -m 256M -S -s \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS_COPY) \
		-drive format=raw,file=fat:rw:$(BUILD_DIR)/esp

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
	@echo "  all (default)  Build all implemented components"
	@echo "  run            Build and boot the UEFI application in QEMU"
	@echo "  debug          Start QEMU paused with a GDB server on :1234"
	@echo "  clean          Remove generated files"
	@echo "  help           Show this help"

-include $(DEPS)
