/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
cube - APSys 00.01 */

#ifndef CUBE_H
#define CUBE_H
#include "gui.h"

void cube_run_cli(gui_gfx_t *g);
void cube_run_cli_4d(gui_gfx_t *g);
void cube_draw_window(gui_gfx_t *g, int cx, int cy, int cw, int ch, int *frame);
void cube_draw_4d_window(gui_gfx_t *g, int cx, int cy, int cw, int ch, int *frame);

#endif
