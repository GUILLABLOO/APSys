;SPDX-License-Identifier: GPL-3.0-or-later
;Copyright (c) 2026 GUILLABLOO
;asmc - APSys 00.01

[BITS 16]
[ORG 0x7C00]
start:
    cli
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [drive], dl
    mov word [0x700], 0x0010
    mov word [0x702], 16
    mov word [0x704], 0x7E00
    mov word [0x706], 0x0000
    mov dword [0x708], 1
    mov dword [0x70C], 0
    mov ah, 0x42
    mov dl, [drive]
    mov si, 0x700
    int 0x13
    jc fail
    jmp 0x0000:0x7E00
fail:
    mov si, msg
.fp:  lodsb
    test al, al
    jz .h
    mov ah, 0x0E
    mov bx, 7
    int 0x10
    jmp .fp
.h:   hlt
    jmp .h
msg db "MBR LOAD FAIL", 0
drive db 0x80
times 446-($-$$) db 0
times 510-($-$$) db 0
db 0x55, 0xAA
