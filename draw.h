/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
draw - APSys 00.01 */

#ifndef DRAW_H
#define DRAW_H
#include "gui.h"
#ifndef ASYXBOX_SIGNAL_OPEN_DRAW
#define ASYXBOX_SIGNAL_OPEN_DRAW 13
#endif
#define DRAW_ACT_NONE 0
#define DRAW_ACT_QUIT 1
#define DRAW_CANVAS_W 640
#define DRAW_CANVAS_H 440
typedef struct {
    int color;
    int drawing, lx, ly;
} draw_t;
void draw_init(draw_t *D);
int  draw_key(draw_t *D, int key);
void draw_draw(draw_t *D, gui_gfx_t *g, int cx, int cy, int cw, int ch);
int  draw_press(draw_t *D, int cx, int cy, int cw, int ch, int vx, int vy);
void draw_drag_begin(draw_t *D, int x, int y);
void draw_drag_to(draw_t *D, int x, int y);
#endif
