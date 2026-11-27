/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
jaro - APSys 00.01 */

#ifndef JARO_H
#define JARO_H
#include "gui.h"
#define JARO_MAX_LINES 128
#define JARO_LINE_MAX  80
#define JARO_ACT_NONE  0
#define JARO_ACT_QUIT  1
typedef struct {
    char lines[JARO_MAX_LINES][JARO_LINE_MAX];
    int  nlines;
    int  crow, ccol;
    int  vtop, vleft;
    int  vrows, vcols;
    int  vol;
    char path[JARO_LINE_MAX];
    char name[32];
    int  dirty;
    int  pending;
    char status[64];
} jaro_t;
int  jaro_open(jaro_t *J, int vol, const char *path, const char *name);
int  jaro_ext_ok(const char *name);
int  jaro_key(jaro_t *J, int key, int ctrl, int shift);
void jaro_draw(jaro_t *J, gui_gfx_t *g, int cx, int cy, int cw, int ch);
void jaro_run_cli(gui_gfx_t *g, int vol, const char *path, const char *name);
#endif
