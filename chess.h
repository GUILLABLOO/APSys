/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
chess - APSys 00.01 */

#ifndef CHESS_H
#define CHESS_H
#include "gui.h"

typedef struct {
    unsigned char board[64];
    int turn;
    int sel;
    int cursor;
    int n_mv;
    int mv_to[64];
    float yaw, dist, height;
    int over;
    char msg[48];
    int sqx[64][4], sqy[64][4];
} chess_t;

void chess_init(chess_t *C);
int  chess_key(chess_t *C, int key, int ctrl);
void chess_click(chess_t *C, int vx, int vy);
void chess_draw(chess_t *C, gui_gfx_t *g, int cx, int cy, int cw, int ch);
void chess_run_cli(gui_gfx_t *g);

#endif
