/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
balls - APSys 00.01 */

#ifndef BALLS_H
#define BALLS_H
#include "gui.h"

#define BALLS_ACT_NONE 0
#define BALLS_ACT_QUIT 1

#define BALLS_MAX_CHAIN 64
#define BALLS_MAX_PTS   24

typedef struct {
    float px[BALLS_MAX_PTS], py[BALLS_MAX_PTS], cum[BALLS_MAX_PTS];
    int   npts;
    float total_len;
    float hole_x, hole_y;
    float can_x, can_y, aim_x, aim_y;
    int   type[BALLS_MAX_CHAIN];
    float s[BALLS_MAX_CHAIN];
    int   n;
    int   fly_on; float fx, fy, fvx, fvy; int fly_type;
    int   loaded;
    int   paused, over;      /* over: 0 игра, 1 проиграли, 2 уровень чист */
    int   level, score;
    int   vw, vh, rx, ry, gen;
    unsigned long long last_tsc;
    unsigned long rnd;
} balls_t;

void balls_init(balls_t *B);
void balls_tick(balls_t *B);
int  balls_key(balls_t *B, int key, int ctrl);
void balls_click(balls_t *B, int lx, int ly);
void balls_draw(balls_t *B, gui_gfx_t *g, int cx, int cy, int cw, int ch);
void balls_run_cli(gui_gfx_t *g);

#endif
