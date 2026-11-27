; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (c) 2026 GUILLABLOO
; boot - APSys 00.01 */

bits 32

MODULEALIGN equ 1<<0
MEMINFO     equ 1<<1
VIDEOMODE   equ 1<<2
FLAGS       equ MODULEALIGN | MEMINFO | VIDEOMODE
MAGIC       equ 0x1BADB002
CHECKSUM    equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0
    dd 0
    dd 1024
    dd 768
    dd 32

section .text
global _start
extern aes_kernel_main

_start:
    cli
    mov esp, stack_top
    and esp, 0xFFFFFFF0

    push ebx
    push eax

    finit

    mov eax, cr0
    and eax, 0xFFFFFFFB
    or  eax, 2
    mov cr0, eax
    mov eax, cr4
    or  eax, 0x200
    or  eax, 0x400
    mov cr4, eax

    call aes_kernel_main
    add esp, 8

.halt_loop:
    cli
    hlt
    jmp .halt_loop

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:
