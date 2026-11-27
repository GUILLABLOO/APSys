/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
settings - APSys 00.01 */

#ifndef SETTINGS_H
#define SETTINGS_H
#include "gui.h"
#ifndef ASYXBOX_SIGNAL_OPEN_SETTINGS
#define ASYXBOX_SIGNAL_OPEN_SETTINGS 12
#endif
#define SET_ACT_NONE 0
#define SET_ACT_QUIT 1
typedef struct {
    int sel;
    int page_sel;
    int wp;
    int clock_mode;
    int invert;
    char status[48];
} settings_t;
const settings_t* settings_cfg(void);
void settings_load_cfg(void);
void settings_save_cfg(void);
void settings_init(settings_t *S);
int  settings_key(settings_t *S, int key, int ctrl);
void settings_draw(settings_t *S, gui_gfx_t *g, int cx, int cy, int cw, int ch);
int  settings_hit(settings_t *S, int cx, int cy, int cw, int ch, int vx, int vy);
void settings_press(settings_t *S, int idx);
void settings_wallpaper(gui_gfx_t *g);
void settings_draw_clock(gui_gfx_t *g, int x, int y);
int  settings_panel_clock_width(void);
void settings_draw_panel_clock(gui_gfx_t *g, int x, int y, int h);
#endif
