/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
key - APSys 00.01 */

#include "key.h"

static inline uint8_t kbd_inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static key_modifiers_t mods = {0, 0, 0, 0};
static int e0_prefix = 0;

typedef struct {
    char map[128];
} kbd_map_t;

static const kbd_map_t scancode_normal = {{
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0,
    0,0,0,0,0,0,0,0,0,0,
    0, 0, 0,0,0,'-',0,0,0,'+',0,0,0,0,0,0,0,0,0,0
}};

static const kbd_map_t scancode_shift = {{
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0,
    0,0,0,0,0,0,0,0,0,0,
    0, 0, 0,0,0,'-',0,0,0,'+',0,0,0,0,0,0,0,0,0,0
}};

void key_init(void) {
    mods.shift = 0;
    mods.ctrl = 0;
    mods.alt = 0;
    mods.capslock = 0;
    e0_prefix = 0;
}

void key_get_modifiers(key_modifiers_t *out_mods) {
    if (out_mods) {
        *out_mods = mods;
    }
}

int key_poll(void) {
    uint8_t st = kbd_inb(0x64);
    if (!(st & 0x01)) return KEY_NONE;
    if (st & 0x20) return KEY_NONE;

    uint8_t sc = kbd_inb(0x60);

    if (sc == 0xE0) {
        e0_prefix = 1;
        return KEY_NONE;
    }

    int released = (sc & 0x80) != 0;
    uint8_t code = sc & 0x7F;

    if (e0_prefix) {
        e0_prefix = 0;
        if (code == 0x1D) { mods.ctrl = !released; return KEY_NONE; }
        if (code == 0x38) { mods.alt  = !released; return KEY_NONE; }
        if (released) return KEY_NONE;
        if (code == 0x48) return KEY_UP;
        if (code == 0x50) return KEY_DOWN;
        if (code == 0x4B) return KEY_LEFT;
        if (code == 0x4D) return KEY_RIGHT;
        return KEY_NONE;
    }

    if (code == 0x2A || code == 0x36) { mods.shift = !released; return KEY_NONE; }
    if (code == 0x1D) { mods.ctrl = !released; return KEY_NONE; }
    if (code == 0x38) { mods.alt  = !released; return KEY_NONE; }
    if (code == 0x3A && !released) { mods.capslock = !mods.capslock; return KEY_NONE; }
    if (released) return KEY_NONE;

    if (code >= 0x3B && code <= 0x44) return KEY_F1 + (code - 0x3B);
    if (code == 0x57) return KEY_F11;
    if (code == 0x58) return KEY_F12;
    if (code == 0x01) return KEY_ESC;
    if (code == 0x0E) return KEY_BACKSPACE;
    if (code == 0x0F) return KEY_TAB;
    if (code == 0x1C) return KEY_ENTER;
    if (code == 0x39) return KEY_SPACE;

    if (code < 128) {
        const char *tbl = mods.shift ? scancode_shift.map : scancode_normal.map;
        char ch = *(tbl + code);
        if (ch >= 'a' && ch <= 'z' && mods.capslock) {
            ch -= 32;
        } else if (ch >= 'A' && ch <= 'Z' && mods.capslock && mods.shift) {
            ch += 32;
        }
        return (int)(uint8_t)ch;
    }
    return KEY_NONE;
}
