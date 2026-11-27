/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
colors - APSys 00.01 */

#include "colors.h"
#include "key.h"

typedef unsigned char u8;
typedef unsigned int  u32;

#define GRID_W 16
#define GRID_H 12

static u32 grid_colors[GRID_W * GRID_H];

static u32 random_color(void) {
    static u32 seed = 12345;
    seed = seed * 1103515245 + 12345;
    u32 r = (seed >> 16) & 0xFF;
    u32 g = (seed >> 8) & 0xFF;
    u32 b = seed & 0xFF;
    return (r << 16) | (g << 8) | b;
}

static void randomize_colors(void) {
    for (int i = 0; i < GRID_W * GRID_H; ++i)
        grid_colors[i] = random_color();
}

void colors_draw(gui_gfx_t *g, int x, int y, int w, int h, int frame) {
    if (frame % 120 == 0) randomize_colors();
    int cell_w = w / GRID_W;
    int cell_h = h / GRID_H;
    if (cell_w < 1) cell_w = 1;
    if (cell_h < 1) cell_h = 1;
    for (int gy = 0; gy < GRID_H; ++gy) {
        for (int gx = 0; gx < GRID_W; ++gx) {
            u32 c = grid_colors[gy * GRID_W + gx];
            g->rect(x + gx * cell_w, y + gy * cell_h, cell_w, cell_h,
                    (u8)((c >> 16) & 0xFF), (u8)((c >> 8) & 0xFF), (u8)(c & 0xFF));
        }
    }
}

int colors_run_cli(gui_gfx_t *g) {
    randomize_colors();
    int frame = 0;
    while (1) {
        colors_draw(g, 0, 0, (int)g->virt_w, (int)g->virt_h, frame);
        g->present();
        int k = key_poll();
        key_modifiers_t m;
        key_get_modifiers(&m);
        if (k == 27 || (m.ctrl && (k == 'l' || k == 'L'))) return 0;
        frame++;
        for (int i = 0; i < 100000; ++i) __asm__ volatile("pause");
    }
}
