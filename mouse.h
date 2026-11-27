/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
mouse - APSys 00.01 */

#ifndef MOUSE_H
#define MOUSE_H

#define MOUSE_B1 1
#define MOUSE_B2 2
#define MOUSE_B3 4

typedef struct {
    volatile unsigned char *vram;
    unsigned int  pitch;
    unsigned int  hw_w, hw_h;
    unsigned char bpp;
    unsigned int  virt_w, virt_h;
    unsigned char bg_r, bg_g, bg_b;
    void (*rect)(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
    void (*glyph)(unsigned char c, int x, int y, unsigned char r, unsigned char g, unsigned char b);
} mouse_video_t;

int  mouse_init(const mouse_video_t *video);
void mouse_cons_setup(int top, int cell_w, int cell_h);
void mouse_enable_buttons(int mask);
void mouse_note_char(unsigned char c, int x, int y, unsigned char r, unsigned char g, unsigned char b);
void mouse_note_clear(int x, int y, int w, int h);

int  mouse_frame(void);
int  mouse_take_exec(char *out, int max);
void mouse_hide(void);
void mouse_show(void);
void mouse_draw_plain(void);
void mouse_set_frozen(int f);
void mouse_take_delta(int *dx, int *dy);
int  mouse_pressed(void);
int  mouse_released(void);
int  mouse_moved(void);
int  mouse_present(void);
void mouse_update(void);
void mouse_get(int *x, int *y, int *buttons);

#endif
