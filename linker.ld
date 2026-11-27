/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
linker - APSys 00.01 */

ENTRY(_start)

SECTIONS {
    . = 1M;

    .multiboot 0x100000 : AT(0x100000) {
        *(.multiboot)
    }

    .text ALIGN(4K) : {
        _kernel_entry = .;
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) : {
        *(.data)
        *(.data.*)
    }
    _flat_data_end = .;

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    _flat_end = .;
}
