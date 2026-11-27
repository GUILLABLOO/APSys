/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
task - APSys 00.01 */

#ifndef TASK_H
#define TASK_H
#include "gui.h"

void task_init(gui_gfx_t *g, int scr_w, int scr_h);
void task_draw(void);
int  task_second(void);

int  task_button_hit(int vx, int vy);
int  task_curtain_hit(int vx, int vy, int *idx);
void task_set_curtain(int open);
int  task_curtain_open(void);
void task_set_curtain_sel(int idx);
int  task_curtain_sel(void);
int  task_curtain_id(int idx);

#endif
