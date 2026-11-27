/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
colors - APSys 00.01 */

#ifndef COLORS_H
#define COLORS_H
#include "gui.h"

void colors_draw(gui_gfx_t *g, int x, int y, int w, int h, int frame);
int colors_run_cli(gui_gfx_t *g);

#endif
