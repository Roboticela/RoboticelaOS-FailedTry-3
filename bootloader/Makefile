# robobootloader - RoboticelaOS Stage 01 bootloader
#
# Targets:
#   make / make release   build build/BOOTX64.EFI
#   make debug            build with serial diagnostics enabled
#   make testkernel       build build/robokernel (test ELF)
#   make test             build & run host unit tests (build/host/*)
#   make image            assemble build/robo-disk.img (ESP w/ boot files)
#   make run              launch QEMU on the disk image
#   make screenshot       capture build/screen.png from a running QEMU
#   make clean            remove all build outputs
#
# Toolchain: clang + LLD (produces UEFI PE/COFF without GNU binutils).
# Override:  CLANG=... LD=... NASM=... QEMU=...

CLANG ?= clang
LD    ?= ld.lld
NASM  ?= nasm
PYTHON ?= python
ifneq ($(shell command -v python 2>/dev/null),)
PYTHON := python
else ifneq ($(shell command -v python3 2>/dev/null),)
PYTHON := python3
endif
LOGO_WIDTH ?= 1024

# Locate clang/lld if not already on PATH. Support both Windows and Linux LLVM
# installs (for example MSYS2/Cygwin and distro packages such as
# /usr/lib/llvm-*/bin), while avoiding false positives from unrelated ld.exe.
CLANG_DIRS := /c/CLang/clang+llvm-18.1.8-x86_64-pc-windows-msvc/bin \
              /usr/lib/llvm-*/bin /usr/local/opt/llvm/bin /usr/local/bin /usr/bin \
              $(HOME)/.local/bin

CLANG_BIN := $(firstword $(foreach d,$(CLANG_DIRS),$(wildcard $(d)/clang) $(wildcard $(d)/clang-*) $(wildcard $(d)/clang++)))
ifeq ($(CLANG_BIN),)
CLANG := clang
else
CLANG := $(CLANG_BIN)
endif

LLD_BIN := $(firstword $(foreach d,$(CLANG_DIRS),$(wildcard $(d)/lld-link) $(wildcard $(d)/ld.lld) $(wildcard $(d)/ld.lld-*)))
ifeq ($(LLD_BIN),)
LLD_LINK := lld-link
else
LLD_LINK := $(LLD_BIN)
endif

ifeq ($(shell command -v $(NASM) 2>/dev/null),)
NASM := /c/msys64/usr/bin/nasm
endif

UEFI_CC := $(CLANG) -target x86_64-unknown-windows
UEFI_LD := $(LLD_LINK)

BUILD   := build
GEN     := $(BUILD)/generated
OBJ     := $(BUILD)/obj

# Native (Windows) compilers need a Windows-style temp dir; derive one.
WIN_TMP := $(shell cygpath -m "$${TEMP:-$${TMP:-/tmp}}" 2>/dev/null || echo /tmp)
export TMP  := $(WIN_TMP)
export TEMP := $(WIN_TMP)

SRC := src
CFLAGS := -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check \
          -fshort-wchar -fno-strict-aliasing -mno-red-zone \
          -Wall -Wextra -O2 -I$(SRC) -I$(GEN)

DEBUG ?= 0
ifeq ($(DEBUG),1)
CFLAGS += -DROBO_DEBUG_BUILD=1 -O1
LDFLAGS_DBG := -debug
endif

# Rebuild everything when the debug flavor changes.
FLAVOR_FILE := $(BUILD)/.flavor
FLAVOR := release$(DEBUG)
ifneq ($(shell cat $(FLAVOR_FILE) 2>/dev/null),$(FLAVOR))
MISSING_FLAVOR := yes
endif

LDFLAGS := -subsystem:efi_application -nodefaultlib -entry:efi_main \
           -filealign:32 $(LDFLAGS_DBG)

UEFI_SRCS := $(SRC)/main.c \
             $(SRC)/crt.c \
             $(SRC)/lib.c \
             $(SRC)/debug.c \
             $(SRC)/boot.c \
             $(SRC)/uefi/uefi.c \
             $(SRC)/memory/memory.c \
             $(SRC)/graphics/framebuffer.c \
             $(SRC)/graphics/renderer.c \
             $(SRC)/graphics/progress.c \
             $(SRC)/keyboard/keyboard.c \
             $(SRC)/menu/menu.c \
             $(SRC)/font/robofont.c \
             $(SRC)/image/bmp.c \
             $(SRC)/config/config.c \
             $(SRC)/elf/elf_loader.c \
             $(SRC)/bootinfo/bootinfo.c

UEFI_OBJS := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(filter %.c,$(UEFI_SRCS))) \
             $(OBJ)/logo_embed.o

# Boot image needs the generated logo BMP.
IMAGE_FILES := $(BUILD)/BOOTX64.EFI testkernel $(LOGO_BMP) $(BUILD)/boot.cfg

ifneq ($(MISSING_FLAVOR),)
$(shell rm -rf $(OBJ))
endif

.PHONY: all release debug test testkernel image run screenshot clean host-tests

all: release
	@echo $(FLAVOR) > $(FLAVOR_FILE)

release: $(BUILD)/BOOTX64.EFI
	@echo $(FLAVOR) > $(FLAVOR_FILE)

debug:
	$(MAKE) DEBUG=1 release

# ------------------------------------------------------------ UEFI build

$(BUILD)/BOOTX64.EFI: $(UEFI_OBJS) | $(BUILD)
	$(UEFI_LD) $(LDFLAGS) -out:$@ $(UEFI_OBJS)

$(OBJ)/%.o: $(SRC)/%.c | $(GEN)
	@mkdir -p $(dir $@)
	$(UEFI_CC) $(CFLAGS) -c $< -o $@

$(OBJ)/logo_embed.o: $(GEN)/logo_embed.S | $(GEN)
	$(UEFI_CC) -c $< -o $@

$(GEN)/logo_embed.S: $(LOGO_BMP) tools/embedlogo.py | $(GEN)
	$(PYTHON) tools/embedlogo.py

$(GEN):
	mkdir -p $(GEN) $(OBJ)

$(BUILD):
	mkdir -p $(BUILD)

# Logo: generated from the SVG vector master (SVG -> 32-bit BMP).
# The SVG lives in bootloader/assets/logo.svg (repo-root assets/logo.svg is
# the canonical copy; assets/logo.svg is a synced duplicate).
LOGO_SVG  := assets/logo.svg
LOGO_BMP  := assets/logo.bmp
LOGO_GEN  := tools/generate-logo.py

.PHONY: logo
logo: $(LOGO_BMP)

# Regenerate when the SVG, the tool, or the configured raster width changes.
$(LOGO_BMP): $(LOGO_SVG) $(LOGO_GEN)
	$(PYTHON) $(LOGO_GEN) --svg $(LOGO_SVG) --out $(LOGO_BMP) --width $(LOGO_WIDTH)

# ---------------------------------------------------------- test kernel

TESTKERN_OBJS := $(OBJ)/testkernel-kernel.o

testkernel: $(BUILD)/robokernel

ifeq ($(shell command -v ld.lld 2>/dev/null),)
LD := $(firstword $(foreach d,$(CLANG_DIRS),$(wildcard $(d)/ld.lld) $(wildcard $(d)/ld.lld.exe)))
ifneq ($(LD),)
LD := $(LD)
else
LD := ld.lld
endif
else
LD := $(shell command -v ld.lld)
endif

$(BUILD)/robokernel: $(TESTKERN_OBJS) testkernel/linker.ld | $(BUILD)
	$(LD) -m elf_x86_64 -T testkernel/linker.ld -static -nostdlib \
	    -o $@ $(TESTKERN_OBJS)

$(OBJ)/testkernel-kernel.o: testkernel/kernel.c | $(GEN)
	$(CLANG) -target x86_64-unknown-linux-gnu -ffreestanding -fno-builtin \
	    -fno-stack-protector -fno-pie -O2 -Wall -Wextra -I$(SRC) -c $< -o $@

# ---------------------------------------------------------- host tests

HOST_CC := $(CLANG)
HOST_TESTS := cfg_test bmp_test elf_test progress_test

host-tests: $(patsubst %,$(BUILD)/host/%.run,$(HOST_TESTS))

$(BUILD)/host/%.run: $(BUILD)/host/%
	$<

$(BUILD)/host/cfg_test: tests/config_test.c $(SRC)/config/config.c $(SRC)/lib.c
	@mkdir -p $(BUILD)/host
	$(HOST_CC) -O1 -Wall -Wextra -I$(SRC) $^ -o $@

$(BUILD)/host/bmp_test: tests/bmp_test.c $(SRC)/image/bmp.c $(SRC)/lib.c
	@mkdir -p $(BUILD)/host
	$(HOST_CC) -O1 -Wall -Wextra -I$(SRC) $^ -o $@

$(BUILD)/host/elf_test: tests/elf_test.c $(SRC)/elf/elf_loader.c $(SRC)/lib.c $(SRC)/debug.c
	@mkdir -p $(BUILD)/host
	$(HOST_CC) -O1 -Wall -Wextra -I$(SRC) $^ -o $@

$(BUILD)/host/progress_test: tests/progress_test.c $(SRC)/graphics/progress.c
	@mkdir -p $(BUILD)/host
	$(HOST_CC) -O1 -Wall -Wextra -I$(SRC) $^ -o $@

test: host-tests testkernel

# ---------------------------------------------------------- image / run

image: $(IMAGE_FILES)
	$(PYTHON) tools/mkfat32.py --output $(BUILD)/robo-disk.img --size-mb 64 \
	    --file EFI/BOOT/BOOTX64.EFI:$(BUILD)/BOOTX64.EFI \
	    --file RoboBoot/robokernel:$(BUILD)/robokernel \
	    --file RoboBoot/logo.bmp:assets/logo.bmp \
	    --file RoboBoot/boot.cfg:$(BUILD)/boot.cfg

# Second disk for testing the Alt boot picker: same content, different FAT
# volume label so the picker shows a distinct name.
image-menu: $(IMAGE_FILES)
	$(PYTHON) tools/mkfat32.py --output $(BUILD)/robo-disk2.img --size-mb 64 \
	    --label ROBOTEST2 \
	    --file EFI/BOOT/BOOTX64.EFI:$(BUILD)/BOOTX64.EFI \
	    --file RoboBoot/robokernel:$(BUILD)/robokernel \
	    --file RoboBoot/logo.bmp:assets/logo.bmp \
	    --file RoboBoot/boot.cfg:$(BUILD)/boot.cfg

CFG_DEBUG ?= 0

$(BUILD)/boot.cfg: | $(BUILD)
	printf 'kernel=RoboBoot/robokernel\nlogo=RoboBoot/logo.bmp\ntimeout=3\nmenu=alt\ntest=1\ndebug=$(CFG_DEBUG)\n' > $@

run: image
	@HEADLESS=0 bash tools/run-qemu.sh $(BUILD)/robo-disk.img

run-headless:
	@HEADLESS=1 bash tools/run-qemu.sh $(BUILD)/robo-disk.img

screenshot:
	bash tools/screendump.sh $(BUILD)/screen.png

clean:
	rm -rf $(BUILD)

.PHONY: host-tests run-headless
