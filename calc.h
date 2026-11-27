/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
calc - APSys 00.01 */

#ifndef CALC_H
#define CALC_H
#include "gui.h"

#define CALC_ACT_NONE 0
#define CALC_ACT_QUIT 1

typedef struct {
    char   disp[24];
    int    has_entry;
    int    just_eq;
    int    err;
    char   pend;
    double acc;
    int    sel;
} calc_t;

void calc_init(calc_t *K);
int  calc_key(calc_t *K, int key, int ctrl);
int  calc_hit(calc_t *K, int cx, int cy, int cw, int ch, int vx, int vy);
void calc_press(calc_t *K, int idx);
void calc_draw(calc_t *K, gui_gfx_t *g, int cx, int cy, int cw, int ch);
void calc_run_cli(gui_gfx_t *g);

#endif
