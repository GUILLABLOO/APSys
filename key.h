/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
key - APSys 00.01 */

#ifndef KEY_H
#define KEY_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

#define KEY_NONE       0
#define KEY_ESC        27
#define KEY_BACKSPACE  8
#define KEY_TAB        9
#define KEY_ENTER      10
#define KEY_SPACE      32

#define KEY_UP         128
#define KEY_DOWN       129
#define KEY_LEFT       130
#define KEY_RIGHT      131

#define KEY_F1         140
#define KEY_F2         141
#define KEY_F3         142
#define KEY_F4         143
#define KEY_F5         144
#define KEY_F6         145
#define KEY_F7         146
#define KEY_F8         147
#define KEY_F9         148
#define KEY_F10        149
#define KEY_F11        150
#define KEY_F12        151

typedef struct {
    int shift;
    int ctrl;
    int alt;
    int capslock;
} key_modifiers_t;

void key_init(void);
int  key_poll(void);
void key_get_modifiers(key_modifiers_t *out_mods);

#endif
