/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
gui - APSys 00.01 */

#ifndef GUI_H
#define GUI_H

typedef unsigned char gu8;
typedef unsigned int  gu32;

typedef struct {
    volatile unsigned char *vram;
    unsigned int  pitch, hw_w, hw_h;
    unsigned char bpp;
    unsigned int  virt_w, virt_h;
    void (*rect)(int x, int y, int w, int h, gu8 r, gu8 g, gu8 b);
    void (*ch)(gu8 c, int x, int y, gu8 r, gu8 g, gu8 b);
    void (*str)(const char *s, int x, int y, gu8 r, gu8 g, gu8 b);
    void (*wallpaper)(void);
    void (*enable_bb)(void);
    void (*disable_bb)(void);
    void (*present)(void);
} gui_gfx_t;

#endif
