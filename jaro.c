/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
jaro - APSys 00.01
Обычный текстовик для .txt и .asm */

#include "jaro.h"
#include "key.h"
#include "fatwr.h"
typedef unsigned char u8;
typedef unsigned long ulong;
static void j_scopy(char *d, const char *s, int max) {
int i = 0;
while (s && s[i] && i < max - 1) { d[i] = s[i]; i++; }
d[i] = 0;
}
static int  j_len(const char *s) { int n = 0; while (s && s[n]) n++; return n; }
static char j_low(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static void j_status(jaro_t *J, const char *s) { j_scopy(J->status, s, 64); }
static int  j_num(char *b, int p, int v) {
char t[8]; int n = 0;
if (v == 0) t[n++] = '0';
while (v && n < 8) { t[n++] = (char)('0' + v % 10); v /= 10; }
while (n > 0) b[p++] = t[--n];
return p;
}
int jaro_ext_ok(const char *name) {
    int n = j_len(name);
    if (n < 3) return 0;
    const char *dot2 = name + (n - 2);
    if (dot2[0] == '.') {
        char a = j_low(dot2[1]);
        if (a == 'c') return 1;
        if (a == 'h') return 1;
    }
    if (n >= 5) {
        const char *dot3 = name + (n - 4);
        if (dot3[0] == '.') {
            char a = j_low(dot3[1]), b = j_low(dot3[2]), c = j_low(dot3[3]);
            if (a == 'a' && b == 's' && c == 'm') return 1;
            if (a == 't' && b == 'x' && c == 't') return 1;
        }
    }
    return 0;
}
static int line_len(jaro_t *J, int r) { return j_len(J->lines[r]); }
static void jaro_view(jaro_t *J) {
int rows = J->vrows, cols = J->vcols;
if (J->crow < J->vtop) J->vtop = J->crow;
if (J->crow >= J->vtop + rows) J->vtop = J->crow - rows + 1;
if (J->ccol < J->vleft) J->vleft = J->ccol;
if (J->ccol >= J->vleft + cols) J->vleft = J->ccol - cols + 1;
if (J->vtop < 0) J->vtop = 0;
if (J->vleft < 0) J->vleft = 0;
if (J->vtop >= J->nlines) J->vtop = J->nlines - 1;
}
static void j_load(jaro_t *J) {
static char buf[JARO_MAX_LINES * JARO_LINE_MAX];
char full[160]; int p = 0;
for (int i = 0; J->path[i] && p < 120; ++i) full[p++] = J->path[i];
if (p && J->path[0]) full[p++] = '/';
for (int i = 0; J->name[i] && p < 150; ++i) full[p++] = J->name[i];
full[p] = 0;
long n = fw_read(J->vol, full, (u8*)buf, sizeof(buf) - 1);
J->nlines = 1; J->lines[0][0] = 0;
if (n <= 0) { j_status(J, n < 0 ? "NEW FILE" : "EMPTY FILE"); return; }
buf[n] = 0;
int li = 0, ci = 0;
for (long i = 0; i < n && li < JARO_MAX_LINES; ++i) {
char c = buf[i];
if (c == '\n') { J->lines[li][ci] = 0; li++; ci = 0; continue; }
if (c == '\r') continue;
if (ci < JARO_LINE_MAX - 1) J->lines[li][ci++] = c;
}
if (ci > 0 && li < JARO_MAX_LINES) { J->lines[li][ci] = 0; li++; }
if (li == 0) li = 1;
J->nlines = li;
j_status(J, "LOADED FROM DISK");
}
static int j_save(jaro_t *J) {
static char buf[JARO_MAX_LINES * JARO_LINE_MAX];
int p = 0, cap = (int)sizeof(buf) - 1;
for (int i = 0; i < J->nlines; ++i) {
if (i && p < cap) buf[p++] = '\n';
for (int c = 0; J->lines[i][c] && p < cap; ++c) buf[p++] = J->lines[i][c];
}
if (!fw_write(J->vol, J->path, J->name, (u8*)buf, (ulong)p)) {
j_status(J, fw_err());
return 0;
}
J->dirty = 0;
j_status(J, "SAVED TO DISK");
return 1;
}
int jaro_open(jaro_t *J, int vol, const char *path, const char *name) {
J->nlines = 1; J->lines[0][0] = 0;
J->crow = 0; J->ccol = 0;
J->vtop = 0; J->vleft = 0;
J->vrows = 20; J->vcols = 60;
J->dirty = 0; J->pending = 0;
J->vol = vol;
j_scopy(J->path, path, JARO_LINE_MAX);
j_scopy(J->name, name, 32);
j_status(J, "");
if (!jaro_ext_ok(name)) { j_status(J, "ONLY .C .H .ASM .TXT"); return 0; }
if (!fw_mountable(vol)) { j_status(J, fw_err()); return 0; }
j_load(J);
return 1;
}
static void ins_char(jaro_t *J, char c) {
char *L = J->lines[J->crow];
int n = line_len(J, J->crow);
if (n >= JARO_LINE_MAX - 1) { j_status(J, "LINE FULL"); return; }
for (int i = n; i >= J->ccol; --i) L[i + 1] = L[i];
L[J->ccol] = c;
J->ccol++; J->dirty = 1;
}
static void do_enter(jaro_t *J) {
if (J->nlines >= JARO_MAX_LINES) { j_status(J, "TOO MANY LINES"); return; }
char *L = J->lines[J->crow];
int n = line_len(J, J->crow);
for (int i = J->nlines; i > J->crow + 1; --i)
j_scopy(J->lines[i], J->lines[i - 1], JARO_LINE_MAX);
char *N = J->lines[J->crow + 1];
int p = 0;
for (int i = J->ccol; i <= n; ++i) N[p++] = L[i];
J->nlines++; J->crow++; J->ccol = 0; J->dirty = 1;
}
static void do_backspace(jaro_t *J) {
char *L = J->lines[J->crow];
if (J->ccol > 0) {
int n = line_len(J, J->crow);
for (int i = J->ccol - 1; i <= n; ++i) L[i] = L[i + 1];
J->ccol--; J->dirty = 1;
} else if (J->crow > 0) {
int plen = line_len(J, J->crow - 1);
int n = line_len(J, J->crow);
if (plen + n >= JARO_LINE_MAX) { j_status(J, "LINES TOO LONG TO JOIN"); return; }
char *P = J->lines[J->crow - 1];
for (int i = 0; i <= n; ++i) P[plen + i] = L[i];
for (int i = J->crow; i < J->nlines - 1; ++i)
j_scopy(J->lines[i], J->lines[i + 1], JARO_LINE_MAX);
J->nlines--; J->crow--; J->ccol = plen; J->dirty = 1;
}
}
static void do_del_line(jaro_t *J) {
if (J->nlines <= 1) {
J->lines[0][0] = 0; J->ccol = 0; J->dirty = 1;
j_status(J, "BUFFER CLEARED");
return;
}
for (int i = J->crow; i < J->nlines - 1; ++i)
j_scopy(J->lines[i], J->lines[i + 1], JARO_LINE_MAX);
J->nlines--;
if (J->crow >= J->nlines) J->crow = J->nlines - 1;
int n = line_len(J, J->crow);
if (J->ccol > n) J->ccol = n;
J->dirty = 1;
j_status(J, "LINE DELETED");
}
static void do_trunc(jaro_t *J) {
J->nlines = 1; J->lines[0][0] = 0;
J->crow = 0; J->ccol = 0;
J->dirty = 1;
j_status(J, "BUFFER CLEARED");
}
static int quit_arm = 0;
static int quit_flow(jaro_t *J) {
if (J->dirty && !quit_arm) {
quit_arm = 1;
j_status(J, "DIRTY: C-S SAVE, C-Q/C-L AGAIN TO LOSE");
return JARO_ACT_NONE;
}
quit_arm = 0;
return JARO_ACT_QUIT;
}
int jaro_key(jaro_t *J, int key, int ctrl, int shift) {
(void)shift;
if (ctrl) {
if (key == 's' || key == 'S') { j_save(J); quit_arm = 0; jaro_view(J); return JARO_ACT_NONE; }
if (key == 'q' || key == 'Q' || key == 'l' || key == 'L') return quit_flow(J);
if (key == 'd' || key == 'D') { do_del_line(J); jaro_view(J); return JARO_ACT_NONE; }
if (key == 't' || key == 'T') { do_trunc(J);    jaro_view(J); return JARO_ACT_NONE; }
return JARO_ACT_NONE;
}
switch (key) {
case KEY_LEFT:
if (J->ccol > 0) J->ccol--;
else if (J->crow > 0) { J->crow--; J->ccol = line_len(J, J->crow); }
break;
case KEY_RIGHT: {
int n = line_len(J, J->crow);
if (J->ccol < n) J->ccol++;
else if (J->crow < J->nlines - 1) { J->crow++; J->ccol = 0; }
break;
}
case KEY_UP:
if (J->crow > 0) { J->crow--; int n = line_len(J, J->crow); if (J->ccol > n) J->ccol = n; }
break;
case KEY_DOWN:
if (J->crow < J->nlines - 1) { J->crow++; int n = line_len(J, J->crow); if (J->ccol > n) J->ccol = n; }
break;
case KEY_ENTER:     do_enter(J); break;
case KEY_BACKSPACE: do_backspace(J); break;
default:
if (key >= 32 && key <= 126) ins_char(J, (char)key);
break;
}
jaro_view(J);
return JARO_ACT_NONE;
}
void jaro_draw(jaro_t *J, gui_gfx_t *g, int cx, int cy, int cw, int ch) {
g->rect(cx, cy, cw, ch, 12, 14, 20);
int rows = (ch - 20) / 18;
int cols = (cw - 8) / 8;
if (rows < 1) rows = 1;
if (cols < 1) cols = 1;
J->vrows = rows; J->vcols = cols;
jaro_view(J);
for (int r = 0; r < rows; ++r) {
int li = J->vtop + r;
int y = cy + 2 + r * 18;
if (li < J->nlines) {
const char *L = J->lines[li];
for (int c = 0; c < cols; ++c) {
char s = L[J->vleft + c];
if (!s) break;
g->ch((u8)s, cx + 4 + c * 8, y, 220, 220, 220);
}
}
if (li == J->crow) {
int c = J->ccol - J->vleft;
if (c >= 0 && c < cols) {
char cur = J->lines[li][J->ccol];
g->rect(cx + 4 + c * 8, y, 8, 16, 230, 230, 230);
if (cur) g->ch((u8)cur, cx + 4 + c * 8, y, 12, 14, 20);
}
}
}
int sy = cy + ch - 18;
g->rect(cx, sy, cw, 1, 0, 0, 0);
g->rect(cx, sy + 1, cw, 17, 255, 255, 255);
char st[120]; int p = 0;
for (int i = 0; J->name[i] && p < 100; ++i) st[p++] = J->name[i];
if (J->dirty && p < 100) st[p++] = '*';
if (J->status[0]) {
if (p < 110) st[p++] = ' ';
if (p < 110) st[p++] = '|';
if (p < 110) st[p++] = ' ';
for (int i = 0; J->status[i] && p < 110; ++i) st[p++] = J->status[i];
}
st[p] = 0;
g->str(st, cx + 4, sy + 2, 10, 10, 10);
char lc[24]; int q = 0;
q = j_num(lc, q, J->crow + 1); lc[q++] = ':'; q = j_num(lc, q, J->ccol + 1); lc[q] = 0;
g->str(lc, cx + cw - 4 - q * 8, sy + 2, 10, 10, 10);
}
void jaro_run_cli(gui_gfx_t *g, int vol, const char *path, const char *name) {
static jaro_t J;
if (!jaro_open(&J, vol, path, name)) {
g->rect(0, 0, (int)g->virt_w, (int)g->virt_h, 10, 12, 16);
g->str("JARO: CANNOT OPEN", 16, 16, 240, 110, 110);
g->str(J.status, 16, 34, 220, 220, 220);
g->str("C-S SAVE  C-Q/C-L EXIT  C-D DEL LINE  C-T CLEAR", 16, 60, 150, 150, 170);
g->present();
while (key_poll() == KEY_NONE) __asm__ volatile("pause");
return;
}
int need = 1;
for (;;) {
if (need) {
jaro_draw(&J, g, 0, 0, (int)g->virt_w, (int)g->virt_h);
g->present();
need = 0;
}
int k = key_poll();
if (k == KEY_NONE) { __asm__ volatile("pause"); continue; }
key_modifiers_t m;
key_get_modifiers(&m);
int act = jaro_key(&J, k, m.ctrl, m.shift);
need = 1;
if (act == JARO_ACT_QUIT) break;
}
}
