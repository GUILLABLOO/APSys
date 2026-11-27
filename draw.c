/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
draw - APSys 00.01
РИСОВАЛОЧКАААААААААААААААА :D, маленькая */

#include "draw.h"
#include "key.h"
typedef unsigned char u8;

static u8 canvas[DRAW_CANVAS_W * DRAW_CANVAS_H];
static const u8 PAL[8][3] = {
    {20,20,20}, {255,255,255}, {200,40,40}, {40,160,60},
    {40,90,200}, {220,200,40}, {60,180,200}, {200,80,180}
};

void draw_init(draw_t *D) {
    for (int i = 0; i < DRAW_CANVAS_W * DRAW_CANVAS_H; i++) canvas[i] = 1;
    D->color = 0; D->drawing = 0; D->lx = -1; D->ly = -1;
}
static void put_px(draw_t *D, int x, int y) {
    if (x < 0 || y < 0 || x >= DRAW_CANVAS_W || y >= DRAW_CANVAS_H) return;
    canvas[y * DRAW_CANVAS_W + x] = (u8)D->color;
}
static void brush(draw_t *D, int x, int y) {
    for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++) put_px(D, x + dx, y + dy);
}
void draw_plot(draw_t *D, int x, int y) { brush(D, x - 1, y - 1); }
void draw_drag_begin(draw_t *D, int x, int y) {
    D->drawing = 1; D->lx = x; D->ly = y;
    draw_plot(D, x, y);
}
void draw_drag_to(draw_t *D, int x, int y) {
    if (!D->drawing) { draw_drag_begin(D, x, y); return; }
    int dx = x - D->lx, dy = y - D->ly;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int steps = ax > ay ? ax : ay;
    if (steps < 1) steps = 1;
    if (steps > 256) steps = 256;
    for (int i = 1; i <= steps; i++)
        draw_plot(D, D->lx + dx * i / steps, D->ly + dy * i / steps);
    D->lx = x; D->ly = y;
}
int draw_key(draw_t *D, int key) {
    if (key == KEY_ESC) return DRAW_ACT_QUIT;
    if (key >= '1' && key <= '8') { D->color = key - '1'; return DRAW_ACT_NONE; }
    if (key == 'c' || key == 'C') { draw_init(D); return DRAW_ACT_NONE; }
    return DRAW_ACT_NONE;
}

int draw_press(draw_t *D, int cx, int cy, int cw, int ch, int vx, int vy) {
    (void)cw; (void)ch;
    int x = vx - cx, y = vy - cy;
    if (y < 20) {
        if (x >= 0 && x < 8 * 24) D->color = x / 24;
        return 0;
    }
    return 1;
}
void draw_draw(draw_t *D, gui_gfx_t *g, int cx, int cy, int cw, int ch) {
    g->rect(cx, cy, cw, 20, 200, 200, 200);
    for (int i = 0; i < 8; i++) {
        g->rect(cx + 2 + i * 24, cy + 2, 22, 16, PAL[i][0], PAL[i][1], PAL[i][2]);
        if (i == D->color) g->rect(cx + 2 + i * 24, cy + 2, 22, 2, 20, 20, 20);
    }
    int H = ch - 38; if (H > DRAW_CANVAS_H) H = DRAW_CANVAS_H;
    int W = cw;      if (W > DRAW_CANVAS_W) W = DRAW_CANVAS_W;
    g->rect(cx, cy + 20, cw, ch - 38, 255, 255, 255);
    for (int y = 0; y < H; y++) {
        int x = 0;
        while (x < W) {
            u8 c = canvas[y * DRAW_CANVAS_W + x];
            int x2 = x + 1;
            while (x2 < W && canvas[y * DRAW_CANVAS_W + x2] == c) x2++;
            g->rect(cx + x, cy + 20 + y, x2 - x, 1, PAL[c][0], PAL[c][1], PAL[c][2]);
            x = x2;
        }
    }
    g->rect(cx, cy + ch - 18, cw, 18, 255, 255, 255);
    g->str("1-8 COLOR  C CLEAR  LMB DRAW  ESC EXIT", cx + 6, cy + ch - 16, 10, 10, 10);
}
