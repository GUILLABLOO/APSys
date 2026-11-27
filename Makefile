# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 GUILLABLOO
# Makefile - APSys 00.01
# MakeFile APSys, ничего больше ;)
CC       = clang
ASM      = nasm
LD       = ld
GRUB_MK  = grub-mkrescue

TARGET_FLAGS = -target i686-pc-elf -m32 -ffreestanding -nostdinc
INCLUDES     = -I.
OPT_FLAGS    = -Os -Wall -Wextra -fno-builtin -fno-stack-protector -mno-red-zone

CFLAGS   = $(TARGET_FLAGS) $(OPT_FLAGS) $(INCLUDES)
LDFLAGS  = -m elf_i386 -T linker.ld -z max-page-size=0x1000
ASMFLAGS = -f elf32

BUILD_DIR   = build
ISO_DIR     = iso
KERNEL_BIN  = $(BUILD_DIR)/apsys_kernel.bin
ISO_IMAGE   = APSys0001.iso

SRC_ASM = boot.asm
SRC_C   = kernel.c key.c asyxbox.c sound.c hda.c colors.c roomfs.c fatwr.c asb.c draw.c settings.c fsc.c asmc.c asmxbox.c jaro.c calc.c balls.c chess.c mouse.c wm.c cube.c task.c shell.c satasys.c diskmgr.c fss.c
OBJ_ASM = $(BUILD_DIR)/boot.o
OBJ_C   = $(BUILD_DIR)/kernel.o $(BUILD_DIR)/key.o $(BUILD_DIR)/asyxbox.o \
          $(BUILD_DIR)/sound.o $(BUILD_DIR)/hda.o $(BUILD_DIR)/mouse.o \
          $(BUILD_DIR)/wm.o $(BUILD_DIR)/task.o $(BUILD_DIR)/shell.o \
          $(BUILD_DIR)/satasys.o $(BUILD_DIR)/diskmgr.o $(BUILD_DIR)/fss.o \
          $(BUILD_DIR)/colors.o $(BUILD_DIR)/cube.o $(BUILD_DIR)/roomfs.o \
          $(BUILD_DIR)/fatwr.o $(BUILD_DIR)/jaro.o $(BUILD_DIR)/calc.o $(BUILD_DIR)/chess.o \
          $(BUILD_DIR)/balls.o $(BUILD_DIR)/asmxbox.o $(BUILD_DIR)/asmc.o $(BUILD_DIR)/asb.o \
          $(BUILD_DIR)/fsc.o $(BUILD_DIR)/settings.o $(BUILD_DIR)/draw.o

.PHONY: all clean run run-hda iso dirs

all: dirs $(KERNEL_BIN) iso

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub

$(OBJ_ASM): $(SRC_ASM) | dirs
	$(ASM) $(ASMFLAGS) $< -o $@

build/mbr.bin: mbr.asm
	nasm -f bin mbr.asm -o build/mbr.bin

build/stage1.raw: stage1.asm
	nasm -f bin stage1.asm -o build/stage1.raw

build/stage1.bin: build/stage1.raw
	dd if=build/stage1.raw of=build/stage1.bin bs=8192 conv=sync status=none

bootblobs.h: build/mbr.bin build/stage1.bin
	@{ echo "static const unsigned char mbr_blob[512] = {"; \
	   od -An -v -tu1 build/mbr.bin | awk '{for(i=1;i<=NF;i++){printf "%s,",$$i; if(++c%16==0)printf "\n"}} END{if(c%16)printf "\n"}'; \
	   echo "};"; \
	   echo "static const unsigned char stage1_blob[8192] = {"; \
	   od -An -v -tu1 build/stage1.bin | awk '{for(i=1;i<=NF;i++){printf "%s,",$$i; if(++c%16==0)printf "\n"}} END{if(c%16)printf "\n"}'; \
	   echo "};"; } > bootblobs.h

build/install.o: bootblobs.h

$(BUILD_DIR)/%.o: %.c key.h asyxbox.h sound.h hda.h settings.h mouse.h gui.h wm.h task.h draw.h shell.h fsc.h balls.h asb.h asmc.h asmxbox.h fss.h calc.h chess.h jaro.h colors.h diskmgr.h cube.h | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJ_ASM) $(OBJ_C) linker.ld | dirs
	$(LD) $(LDFLAGS) $(OBJ_ASM) $(OBJ_C) -o $@

$(ISO_DIR)/boot/grub/grub.cfg: Makefile | dirs
	@echo 'set timeout=1' > $@
	@echo 'set default=0' >> $@
	@echo 'insmod all_video' >> $@
	@echo 'set gfxpayload=1024x768x32' >> $@
	@echo '' >> $@
	@echo 'menuentry "ApSys (Asya oPerating System)" {' >> $@
	@echo '    multiboot /boot/apsys_kernel.bin' >> $@
	@echo '    boot' >> $@
	@echo '}' >> $@

$(ISO_DIR)/boot/apsys_kernel.bin: $(KERNEL_BIN) | dirs
	cp $< $@

#Вот тут создаётся ISO-образ APSys VVV (если что запускается и на флешке и на CD дисках, советую использовать dd для записи на носитель, где будет лежать APSys!)
iso: $(ISO_DIR)/boot/grub/grub.cfg $(ISO_DIR)/boot/apsys_kernel.bin
	$(GRUB_MK) -o $(ISO_IMAGE) $(ISO_DIR)
	@echo "Поздравляю, вы создали ISO-образ APSys! -> $(ISO_IMAGE)"

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) $(ISO_IMAGE)
