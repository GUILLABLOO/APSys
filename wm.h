/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
wm - APSys 00.01 */

#ifndef WM_H
#define WM_H
#include "gui.h"

#define WM_MAX_WINDOWS 8
#define WM_TITLE_H     22
#define WM_BORDER      3
#define WM_TITLE_MAX   24

#define WM_EDGE_L 1
#define WM_EDGE_R 2
#define WM_EDGE_T 4
#define WM_EDGE_B 8

typedef struct wm_window_s {
    int  id, x, y, w, h;
    char title[WM_TITLE_MAX];
    int  visible, focused, alive;
    void *user;
    void (*content)(gui_gfx_t *g, struct wm_window_s *W);
} wm_window_t;

typedef void (*wm_content_cb)(gui_gfx_t *g, wm_window_t *W);

void wm_set_content_cb(wm_content_cb cb);
int  wm_create(const char *title, int x, int y, int w, int h, void *user);
void wm_destroy(int id);
void wm_focus(int id);
void wm_toggle_min(int id);
void wm_move_delta(int id, int dx, int dy);
void wm_resize_edges(int id, int edges, int dx, int dy);
void wm_set_skip(int id);
int  wm_count(void);
wm_window_t *wm_get(int k);
wm_window_t *wm_find(int id);
wm_window_t *wm_top(void);
int  wm_hit(int vx, int vy, int *part);
int  wm_hit_border(int vx, int vy, int *edges);
void wm_client_rect(wm_window_t *W, int *x, int *y, int *w, int *h);
void wm_draw(gui_gfx_t *g);
void wm_rubber(gui_gfx_t *g, wm_window_t *W);

#endif
