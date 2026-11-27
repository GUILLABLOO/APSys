/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (c) 2026 GUILLABLOO */
/* settings - APSys 00.01 */

#include "settings.h"
#include "key.h"
#include "fatwr.h"
#include "roomfs.h"
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

static settings_t g_cfg;
static int cfg_loaded = 0;
const settings_t* settings_cfg(void){ return &g_cfg; }

/* тут время */
static inline void s_outb(u16 p, u8 v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline u8   s_inb (u16 p){ u8 r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }
static u8 cmos(u8 r){ s_outb(0x70, r); return s_inb(0x71); }
static u8 bcd(u8 v){ return (u8)((v & 0x0F) + ((v >> 4) * 10)); }
typedef struct { int h, m, s, dd, mo; } stime_t;
static stime_t now_time(void){
    stime_t t;
    u8 rb = cmos(0x0B);
    u8 h = cmos(0x04), m = cmos(0x02), s = cmos(0x00);
    u8 dd = cmos(0x07), mo = cmos(0x08);
    if (!(rb & 4)) { h = bcd(h); m = bcd(m); s = bcd(s); dd = bcd(dd); mo = bcd(mo); }
    if (!(rb & 2)) { if (h & 0x80) { h &= 0x7F; if (h != 12) h += 12; } else if (h == 12) h = 0; }
    if (h > 23) h = 23; if (m > 59) m = 59; if (s > 59) s = 59;
    if (dd < 1) dd = 1; if (dd > 31) dd = 31; if (mo < 1) mo = 1; if (mo > 12) mo = 12;
    t.h = h; t.m = m; t.s = s; t.dd = dd; t.mo = mo;
    return t;
}
static int put2(char *b, int p, int v){
    if (v < 0) v = 0; if (v > 99) v = 99;
    b[p++] = (char)('0' + v / 10); b[p++] = (char)('0' + v % 10); return p;
}
static void time_str(char *b){
    stime_t t = now_time(); int p = 0;
    p = put2(b, p, t.h); b[p++] = ':'; p = put2(b, p, t.m); b[p++] = ':'; p = put2(b, p, t.s); b[p] = 0;
}

/* сохранение / загрузка */
static int find_num(const char *s, const char *key){
    int kl = 0; while (key[kl]) kl++;
    for (int i = 0; s[i]; i++) {
        if (i > 0 && s[i-1] != ' ' && s[i-1] != '\n') continue;
        int ok = 1;
        for (int k = 0; k < kl; k++) if (s[i+k] != key[k]) { ok = 0; break; }
        if (!ok) continue;
        int j = i + kl;
        if (s[j] != ' ') continue;
        j++;
        int v = 0;
        while (s[j] >= '0' && s[j] <= '9') { v = v * 10 + (s[j] - '0'); j++; }
        return v;
    }
    return -1;
}
void settings_load_cfg(void){
    if (cfg_loaded) return;
    cfg_loaded = 1;
    g_cfg.wp = 0; g_cfg.clock_mode = 1; g_cfg.invert = 0;
    int vol = roomfs_system_vol();
    if (vol < 0) return;
    static u8 buf[256];
    long n = fw_read(vol, "SETTINGS.CFG", buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = 0;
    int v;
    v = find_num((char*)buf, "WP"); if (v >= 0 && v <= 4) g_cfg.wp = v;
    v = find_num((char*)buf, "CK"); if (v >= 0 && v <= 2) g_cfg.clock_mode = v;
    v = find_num((char*)buf, "IV"); if (v == 0 || v == 1) g_cfg.invert = v;
}
void settings_save_cfg(void){
    int vol = roomfs_system_vol();
    if (vol < 0) return;
    char b[96]; int p = 0;
    const char *hdr = "APSYSCFG2\nWP ";
    while (*hdr) b[p++] = *hdr++;
    p = put2(b, p, g_cfg.wp); b[p++] = '\n';
    b[p++] = 'C'; b[p++] = 'K'; b[p++] = ' ';
    p = put2(b, p, g_cfg.clock_mode); b[p++] = '\n';
    b[p++] = 'I'; b[p++] = 'V'; b[p++] = ' ';
    p = put2(b, p, g_cfg.invert); b[p++] = '\n';
    fw_write(vol, "", "SETTINGS.CFG", (u8*)b, (unsigned long)p);
}
static void apply_and_save(settings_t *S){
    g_cfg.wp = S->wp; g_cfg.clock_mode = S->clock_mode; g_cfg.invert = S->invert;
    settings_save_cfg();
}
void settings_init(settings_t *S){
    settings_load_cfg();
    *S = g_cfg;
    S->sel = 0; S->page_sel = 0;
    const char *st = "UP/DN MENU  LEFT/RIGHT CHANGE  ESC EXIT";
    int i = 0; while (st[i] && i < 47) { S->status[i] = st[i]; i++; } S->status[i] = 0;
}

/* обои */
void settings_wallpaper(gui_gfx_t *g){
    int W = (int)g->virt_w, H = (int)g->virt_h;
    int wp = g_cfg.wp;
    if (wp == 0) {
        for (int y = 0; y < H; y += 8)
            for (int x = 0; x < W; x += 8) {
                u8 c = (u8)((((y / 8) + (x / 8)) & 1) ? 165 : 235);
                g->rect(x, y, 8, 8, c, c, c);
            }
    } else if (wp == 1) {
        for (int y = 0; y < H; y++) {
            int t = H > 1 ? y * 255 / (H - 1) : 0;
            g->rect(0, y, W, 1, (u8)(16 + t / 5), (u8)(30 + t / 3), (u8)(80 + t / 2));
        }
    } else if (wp == 2) {
        for (int y = 0; y < H; y += 16) {
            u8 c = (u8)(40 + ((y / 16) & 3) * 26);
            g->rect(0, y, W, 16, c, c, (u8)(c + 8));
        }
    } else if (wp == 3) {
        g->rect(0, 0, W, H, 10, 12, 24);
        u32 s = 12345u;
        for (int i = 0; i < 220; i++) {
            s = s * 1103515245u + 12345u;
            int x = (int)((s >> 8) % (u32)W);
            s = s * 1103515245u + 12345u;
            int y = (int)((s >> 8) % (u32)H);
            u8 b = (u8)(120 + ((s >> 16) & 127));
            g->rect(x, y, 2, 2, b, b, b);
        }
    } else {
        g->rect(0, 0, W, H, 24, 26, 32);
    }
}

/* часики, стрелки, табличка углов, панель и многое другое, мне лень всё писать, лол :P */
static const int SIN60[60] = {
0,105,208,309,407,500,588,669,743,809,
866,914,951,978,995,1000,995,978,951,914,
866,809,743,669,588,500,407,309,208,105,
0,-105,-208,-309,-407,-500,-588,-669,-743,-809,
-866,-914,-951,-978,-995,-1000,-995,-978,-951,-914,
-866,-809,-743,-669,-588,-500,-407,-309,-208,-105 };
static int sin60(int k){ k %= 60; if (k < 0) k += 60; return SIN60[k]; }
static int cos60(int k){ return sin60(k + 15); }
static int isq(int v){ if (v <= 0) return 0; int r = 1; while ((r+1)*(r+1) <= v) r++; return r; }
static void hand(gui_gfx_t *g, int x, int y, int len, int k, u8 r, u8 gg, u8 b){
    int s = sin60(k), c = cos60(k);
    for (int i = 0; i <= len; i++) {
        int px = x + (s * i) / 1000, py = y - (c * i) / 1000;
        g->rect(px, py, 2, 2, r, gg, b);
    }
}
static void dial(gui_gfx_t *g, int cx0, int cy0, int R){
    for (int dy = -R-2; dy <= R+2; dy++) {
        int w = isq((R+2)*(R+2) - dy*dy);
        g->rect(cx0-w, cy0+dy, 2*w, 1, 40, 44, 60);
    }
    for (int dy = -R; dy <= R; dy++) {
        int w = isq(R*R - dy*dy);
        g->rect(cx0-w, cy0+dy, 2*w, 1, 235, 238, 245);
    }
    for (int q = 0; q < 12; q++) {
        int k = q * 5;
        for (int rr = R-4; rr <= R-1; rr++) {
            int px = cx0 + (sin60(k)*rr)/1000, py = cy0 - (cos60(k)*rr)/1000;
            g->rect(px, py, 2, 2, 30, 30, 40);
        }
    }
    stime_t t = now_time();
    hand(g, cx0, cy0, R-13, (t.h % 12) * 5 + t.m / 12, 20, 20, 30);
    hand(g, cx0, cy0, R-6,  t.m, 20, 20, 30);
    hand(g, cx0, cy0, R-3,  t.s, 190, 60, 60);
    g->rect(cx0-1, cy0-1, 3, 3, 20, 20, 30);
}
void settings_draw_clock(gui_gfx_t *g, int x, int y){
    stime_t t = now_time();
    char buf[16]; int p = 0;
    p = put2(buf, p, t.h); buf[p++] = ':'; p = put2(buf, p, t.m); buf[p++] = ':'; p = put2(buf, p, t.s); buf[p] = 0;
    char dte[16]; int q = 0;
    q = put2(dte, q, t.dd); dte[q++] = '.'; q = put2(dte, q, t.mo); dte[q] = 0;
    dial(g, x + 34, y + 34, 30);
    g->str(buf, x + 78, y + 26, 25, 25, 25);
    g->str(dte, x + 78, y + 44, 110, 110, 120);
}
int settings_panel_clock_width(void){
    if (g_cfg.clock_mode == 0) return 0;
    if (g_cfg.clock_mode == 1) return 84;
    return 128;
}
void settings_draw_panel_clock(gui_gfx_t *g, int x, int y, int h){
    if (g_cfg.clock_mode == 0) return;
    char buf[16]; time_str(buf);
    int tx = x;
    if (g_cfg.clock_mode == 2) {
        int r = h / 2 - 2;
        int cx0 = x + r + 2, cy0 = y + h / 2;
        for (int dy = -r; dy <= r; dy++) {
            int w = isq(r*r - dy*dy);
            g->rect(cx0-w, cy0+dy, 2*w, 1, 235, 238, 245);
        }
        for (int q = 0; q < 12; q += 3) {
            int k = q * 5;
            int px = cx0 + (sin60(k)*(r-2))/1000, py = cy0 - (cos60(k)*(r-2))/1000;
            g->rect(px, py, 1, 1, 30, 30, 40);
        }
        stime_t t = now_time();
        hand(g, cx0, cy0, r-4, (t.h % 12) * 5 + t.m / 12, 20, 20, 30);
        hand(g, cx0, cy0, r-2, t.m, 20, 20, 30);
        tx = cx0 + r + 6;
    }
    g->str(buf, tx, y + (h - 16) / 2, 40, 40, 40);
}

/* Пункты, необычные вещи(к сожелению есть, мне лень это убрать) и про саму APSys! */
static const char *menu_names[4] = { "WALLPAPER", "CLOCK", "UNUSUAL", "ABOUT" };
static const char *wp_names[5]  = { "CHECKER", "GRADIENT", "STRIPES", "STARS", "SOLID" };
static const char *ck_names[3]  = { "OFF", "DIGITAL", "ANALOG + DIGITAL" };
#define SET_LX 8
#define SET_LW 132
#define SET_Y0 10
#define SET_RH 24
#define SET_RX 152

void settings_draw(settings_t *S, gui_gfx_t *g, int cx, int cy, int cw, int ch){
    g->rect(cx, cy, cw, ch, 255, 255, 255);
    for (int i = 0; i < 4; i++) {
        int y = cy + SET_Y0 + i * SET_RH;
        if (i == S->sel) { g->rect(cx + SET_LX, y, SET_LW, SET_RH - 2, 60, 60, 60);
            g->str(menu_names[i], cx + SET_LX + 8, y + 4, 255, 255, 255); }
        else g->str(menu_names[i], cx + SET_LX + 8, y + 4, 25, 25, 25);
    }
    int rx = cx + SET_RX, ry = cy + SET_Y0;
    if (S->sel == 0) {
        for (int o = 0; o < 5; o++) {
            int y = ry + o * 22;
            char line[40]; int p = 0;
            line[p++] = (o == S->wp) ? '*' : ' '; line[p++] = ' ';
            for (int i = 0; wp_names[o][i] && p < 38; i++) line[p++] = wp_names[o][i];
            line[p] = 0;
            g->str(line, rx, y, (o == S->wp) ? 0 : 25, (o == S->wp) ? 110 : 25, (o == S->wp) ? 0 : 25);
        }
    } else if (S->sel == 1) {
        for (int o = 0; o < 3; o++) {
            int y = ry + o * 22;
            char line[40]; int p = 0;
            line[p++] = (o == S->clock_mode) ? '*' : ' '; line[p++] = ' ';
            for (int i = 0; ck_names[o][i] && p < 38; i++) line[p++] = ck_names[o][i];
            line[p] = 0;
            g->str(line, rx, y, (o == S->clock_mode) ? 0 : 25, (o == S->clock_mode) ? 110 : 25, (o == S->clock_mode) ? 0 : 25);
        }
        settings_draw_clock(g, rx + 10, ry + 80);
    } else if (S->sel == 2) {
        char line[48]; int p = 0;
        for (int i = 0; i < 14 && i < 40; i++) line[p++] = "INVERT DESKTOP"[i];
        line[p++] = ' '; line[p++] = ':'; line[p++] = ' ';
        const char *vv = S->invert ? "ON" : "OFF";
        for (int i = 0; vv[i] && p < 46; i++) line[p++] = vv[i];
        line[p] = 0;
        g->str(line, rx, ry, S->invert ? 150 : 25, S->invert ? 110 : 25, S->invert ? 0 : 25);
        g->str("LEFT/RIGHT OR CLICK: ON/OFF", rx, ry + 120, 150, 150, 170);
    } else {
        const char *about[6] = {
            "APSYS v00.01",
            "Creator : GUILLABLOO",
            "---Special thanks---",
            "Tinwo , aero_wx.2, raz3r_hype",
            "and monitor_shinco_y198_hp8",
            "(TikTok : @GUILLABLOO )"
        };
        for (int i = 0; i < 6; i++)
            g->str(about[i], rx, ry + i * 20, i == 0 ? 0 : 25, i == 0 ? 90 : 25, i == 0 ? 160 : 25);
    }
    g->rect(cx, cy + ch - 18, cw, 18, 255, 255, 255);
    g->str(S->status, cx + 6, cy + ch - 16, 10, 10, 10);
}
int settings_hit(settings_t *S, int cx, int cy, int cw, int ch, int vx, int vy){
    (void)S; (void)cw; (void)ch;
    int lx = cx + SET_LX, y0 = cy + SET_Y0;
    if (vx >= lx && vx < lx + SET_LW) {
        int i = (vy - y0) / SET_RH;
        if (i >= 0 && i < 4) return i;
        return -1;
    }
    if (vx >= cx + SET_RX) {
        int o = (vy - y0) / 22;
        int cnt = (S->sel == 0) ? 5 : (S->sel == 1) ? 3 : (S->sel == 2) ? 1 : 0;
        if (o >= 0 && o < cnt) return 100 + o;
    }
    return -1;
}
void settings_press(settings_t *S, int idx){
    if (idx >= 0 && idx < 4) { S->sel = idx; S->page_sel = 0; return; }
    if (idx < 100) return;
    int o = idx - 100;
    if (S->sel == 0 && o < 5) { S->wp = o; apply_and_save(S); }
    else if (S->sel == 1 && o < 3) { S->clock_mode = o; apply_and_save(S); }
    else if (S->sel == 2 && o == 0) { S->invert = !S->invert; apply_and_save(S); }
}
int settings_key(settings_t *S, int key, int ctrl){
    (void)ctrl;
    if (key == KEY_ESC) return SET_ACT_QUIT;
    if (key == KEY_UP)   { S->sel = (S->sel + 3) & 3; return SET_ACT_NONE; }
    if (key == KEY_DOWN) { S->sel = (S->sel + 1) & 3; return SET_ACT_NONE; }
    if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_ENTER) {
        int d = (key == KEY_LEFT) ? -1 : 1;
        if (S->sel == 0) { S->wp = (S->wp + d + 5) % 5; apply_and_save(S); }
        else if (S->sel == 1) { S->clock_mode = (S->clock_mode + d + 3) % 3; apply_and_save(S); }
        else if (S->sel == 2) {
            if (key == KEY_ENTER) S->invert = !S->invert;
            else S->invert = (d > 0) ? 1 : 0;
            apply_and_save(S);
        }
    }
    return SET_ACT_NONE;
}
