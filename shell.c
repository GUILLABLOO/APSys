/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
shell - APSys 00.01 */

#include "shell.h"
#include "wm.h"
#include "task.h"
#include "mouse.h"
#include "key.h"
#include "asyxbox.h"
#include "cube.h"
#include "jaro.h"
#include "settings.h"
#include "draw.h"
#define APSYS_HAS_CALC  1
#define APSYS_HAS_CHESS 1
#define APSYS_HAS_BALLS 1
#define APSYS_HAS_ASM   1
#if APSYS_HAS_CALC
#include "calc.h"
#endif
#if APSYS_HAS_CHESS
#include "chess.h"
#endif
#if APSYS_HAS_BALLS
#include "balls.h"
#endif
typedef unsigned char u8;
typedef unsigned int  u32;
#define TERM_LINES 64
#define TERM_COLS  96
#define MAX_TERMS  10
typedef struct {
char lines[TERM_LINES][TERM_COLS];
int  count;
char input[TERM_COLS];
int  len, cursor;
int  colors_mode, colors_frame;
int  cube_mode, cube_frame, cube_is_4d;
int  jaro_mode;
jaro_t j;
int  settings_mode;
settings_t st;
int draw_mode;
draw_t dr;
#if APSYS_HAS_CALC
int  calc_mode;
calc_t c;
#endif
#if APSYS_HAS_CHESS
int  chess_mode;
chess_t ch;
#endif
#if APSYS_HAS_BALLS
int  balls_mode;
balls_t b;
#endif
#if APSYS_HAS_ASM
int  asm_mode;
#endif
} term_t;
static term_t terms[MAX_TERMS];
static term_t *exec_term = 0;
static asyxbox_ctx_t tctx;
static gui_gfx_t *shell_g = 0;
static int kill_mode = 0;
static int ctx_mode = 0, ctx_sel = -1, ctx_x = 0, ctx_y = 0;
#define CTX_W   180
#define CTX_ROW 22
#define CTX_H   (CTX_ROW * 4 + 4)
static const char *ctx_items[4] = {"SPAWN CONSOLE","EXIT ARTHE","REBOOT","SHUTDOWN"};
static void ctx_menu_draw(gui_gfx_t *g, int x, int y, int sel) {
g->rect(x+4, y+4, CTX_W, CTX_H, 25, 25, 28);
g->rect(x,   y,   CTX_W, CTX_H, 60, 60, 60);
g->rect(x+2, y+2, CTX_W-4, CTX_H-4, 255, 255, 255);
for (int i=0;i<4;++i) {
int iy = y + 2 + i*CTX_ROW;
if (i == sel) {
g->rect(x+2, iy, CTX_W-4, CTX_ROW, 60, 60, 60);
g->str(ctx_items[i], x+10, iy+4, 255, 255, 255);
} else {
g->str(ctx_items[i], x+10, iy+4, 40, 40, 40);
}
}
}
static void term_reset(term_t *T) {
for (int i=0;i<TERM_LINES;++i) T->lines[i][0]=0;
T->count=0; T->len=0; T->cursor=0;
T->colors_mode=0; T->colors_frame=0;
T->cube_mode=0; T->cube_frame=0; T->cube_is_4d=0;
T->jaro_mode=0;
T->settings_mode=0;
T->draw_mode=0;
#if APSYS_HAS_CALC
T->calc_mode=0;
#endif
#if APSYS_HAS_CHESS
T->chess_mode=0;
#endif
#if APSYS_HAS_BALLS
T->balls_mode=0;
#endif
#if APSYS_HAS_ASM
T->asm_mode=0;
#endif
}
static void term_append(term_t *T, const char *s) {
if (T->count >= TERM_LINES) {
for (int i=0;i<TERM_LINES-1;++i)
for (int c=0;c<TERM_COLS;++c) T->lines[i][c]=T->lines[i+1][c];
T->count = TERM_LINES-1;
}
char *dst=T->lines[T->count];
int p=0;
while (*s && p<TERM_COLS-1) dst[p++]=*s++;
dst[p]=0;
T->count++;
}
static void term_print_line(const char *t, u8 r, u8 g, u8 b) {
(void)r;(void)g;(void)b;
if (exec_term) term_append(exec_term, t);
}
static void term_print_raw(const char *t, u8 r, u8 g, u8 b) {
(void)r;(void)g;(void)b;
if (!exec_term) return;
if (exec_term->count==0) term_append(exec_term,"");
char *dst=exec_term->lines[exec_term->count-1];
int n=0; while(dst[n]&&n<TERM_COLS-1)n++;
while(*t&&n<TERM_COLS-1) dst[n++]=*t++;
dst[n]=0;
}
static void term_pump(void) {
if (!shell_g) return;
mouse_update();
wm_draw(shell_g);
task_draw();
shell_g->present();
mouse_draw_plain();
}
static int tn_dec(char *b, int p, u32 v) {
char tmp[12]; int n=0;
if (v==0) tmp[n++]='0';
while (v && n<12) { tmp[n++]=(char)('0'+v%10); v/=10; }
while (n>0) b[p++]=tmp[--n];
return p;
}
static void term_print_num(const char *pre, u32 v, const char *suf, u8 r, u8 g, u8 b) {
char buf[96]; int p=0;
while (*pre && p<90) buf[p++]=*pre++;
p=tn_dec(buf,p,v);
while (*suf && p<90) buf[p++]=*suf++;
buf[p]=0;
term_print_line(buf,r,g,b);
}
static void term_echo(term_t *T) {
char buf[TERM_COLS]; int p=0;
const char *pr="asya->";
while (*pr && p<TERM_COLS-1) buf[p++]=*pr++;
for (int i=0;i<T->len && p<TERM_COLS-1;++i) buf[p++]=T->input[i];
buf[p]=0;
term_append(T,buf);
}
static void term_draw_colors(gui_gfx_t *g, term_t *T, int cx, int cy, int cw, int ch) {
const int GW=16, GH=12;
int cwi=cw/GW, chi=ch/GH;
if (cwi<1) cwi=1;
if (chi<1) chi=1;
u32 base = 12345u + (u32)T->colors_frame * 2654435761u;
for (int gy=0;gy<GH;++gy)
for (int gx=0;gx<GW;++gx) {
u32 s = base + (u32)(gy*GW+gx)*1103515245u;
s = s*1103515245u + 12345u;
u8 r=(u8)((s>>16)&0xFF), gr=(u8)((s>>8)&0xFF), b=(u8)(s&0xFF);
int x=cx+gx*cwi, y=cy+gy*chi;
int w=cwi, h=chi;
if (x+w>cx+cw) w=cx+cw-x;
if (y+h>cy+ch) h=cy+ch-y;
if (w>0 && h>0) g->rect(x,y,w,h,r,gr,b);
}
T->colors_frame++;
}
static void shell_paint_bg(gui_gfx_t *g){
settings_wallpaper(g);
}
static void shell_present(gui_gfx_t *g){
g->present();
const settings_t *C = settings_cfg();
if (!C->invert) return;
u32 bpp = g->bpp / 8;
volatile u8 *v = g->vram;
for (u32 y = 0; y < g->hw_h; y++) {
volatile u8 *row = v + (unsigned long)y * g->pitch;
for (u32 x = 0; x < g->hw_w * bpp; x += bpp) {
row[x] = (u8)~row[x]; row[x+1] = (u8)~row[x+1]; row[x+2] = (u8)~row[x+2];
}
}
}
static void term_draw(gui_gfx_t *g, wm_window_t *W) {
term_t *T=(term_t*)W->user;
if (!T) return;
int cx,cy,cw,chh;
wm_client_rect(W,&cx,&cy,&cw,&chh);
if (cw<8 || chh<8) return;
if (T->draw_mode) { draw_draw(&T->dr, g, cx, cy, cw, chh); return; }
if (T->settings_mode) { settings_draw(&T->st, g, cx, cy, cw, chh); return; }
#if APSYS_HAS_ASM
if (T->asm_mode) {
g->rect(cx, cy, cw, chh, 10, 12, 16);
g->str("ASM MODE - press ESC to exit", cx+10, cy+20, 200, 200, 200);
return;
}
#endif
#if APSYS_HAS_BALLS
if (T->balls_mode) { balls_draw(&T->b, g, cx, cy, cw, chh); return; }
#endif
#if APSYS_HAS_CHESS
if (T->chess_mode) { chess_draw(&T->ch, g, cx, cy, cw, chh); return; }
#endif
#if APSYS_HAS_CALC
if (T->calc_mode)  { calc_draw(&T->c, g, cx, cy, cw, chh); return; }
#endif
if (T->jaro_mode)  { jaro_draw(&T->j, g, cx, cy, cw, chh); return; }
if (T->colors_mode){ term_draw_colors(g,T,cx,cy,cw,chh); return; }
if (T->cube_mode) {
if (T->cube_is_4d) cube_draw_4d_window(g,cx,cy,cw,chh,&T->cube_frame);
else               cube_draw_window   (g,cx,cy,cw,chh,&T->cube_frame);
return;
}
g->rect(cx,cy,cw,chh,255,255,255);
int cols=cw/8, rows=(chh-18)/18;
if (cols>TERM_COLS) cols=TERM_COLS;
if (rows<1) rows=1;
int start=T->count-rows;
if (start<0) start=0;
int y=cy;
for (int i=start;i<T->count;++i) {
for (int c=0;c<cols;++c) {
char ch=T->lines[i][c];
if (!ch) break;
g->ch((u8)ch, cx+c*8, y, 25,25,25);
}
y+=18;
}
g->rect(cx, y, cw, 18, 255,255,255);
g->str("asya->", cx, y, 0,110,0);
for (int c=0;c<T->len && c<cols-8;++c)
g->ch((u8)T->input[c], cx+48+c*8, y, 25,25,25);
g->ch('_', cx+48+T->cursor*8, y, 25,25,25);
}
static int term_slot_free(int i) {
for (int k=0;k<wm_count();++k) {
wm_window_t *W=wm_get(k);
if (W && W->user==&terms[i]) return 0;
}
return 1;
}
static void spawn_console(void) {
int idx=-1;
for (int i=0;i<MAX_TERMS;++i) if (term_slot_free(i)) { idx=i; break; }
if (idx<0) {
wm_window_t *W=wm_top();
term_t *T=W?(term_t*)W->user:0;
if (T) {
term_append(T, "TERMINAL LIMIT: 10 WINDOWS MAX");
term_append(T, "CLOSE ONE (ESC + CLICK) AND RETRY");
}
return;
}
term_reset(&terms[idx]);
int offx=(idx%5)*48;
int offy=(idx/5)*48;
wm_create("TERMINAL", 60+offx, 40+offy, 480, 320, &terms[idx]);
}
static int term_is_anim(term_t *T) {
if (T->colors_mode || T->cube_mode) return 1;
if (T->settings_mode) return 1;
#if APSYS_HAS_BALLS
if (T->balls_mode) return 1;
#endif
return 0;
}
static int any_demo(int skip) {
for (int k=0;k<wm_count();++k) {
wm_window_t *W=wm_get(k);
if (!W || !W->visible || !W->user) continue;
if (W->id == skip) continue;
if (term_is_anim((term_t*)W->user)) return 1;
}
return 0;
}
static void redraw_demos(gui_gfx_t *g, int skip) {
for (int k=0;k<wm_count();++k) {
wm_window_t *W=wm_get(k);
if (!W || !W->visible || !W->user) continue;
if (W->id == skip) continue;
term_t *T=(term_t*)W->user;
if (!term_is_anim(T)) continue;
#if APSYS_HAS_BALLS
if (T->balls_mode) balls_tick(&T->b);
#endif
term_draw(g,W);
}
}
static int win_overlap(wm_window_t *A, wm_window_t *B) {
if (A->x + A->w + 6 <= B->x) return 0;
if (B->x + B->w + 6 <= A->x) return 0;
if (A->y + A->h + 6 <= B->y) return 0;
if (B->y + B->h + 6 <= A->y) return 0;
return 1;
}
static int demo_covered(int skip) {
int n = wm_count();
for (int k = 0; k < n; ++k) {
wm_window_t *W = wm_get(k);
if (!W || !W->visible || !W->user || W->id == skip) continue;
if (!term_is_anim((term_t*)W->user)) continue;
for (int j = k + 1; j < n; ++j) {
wm_window_t *U = wm_get(j);
if (!U || !U->visible || U->id == skip) continue;
if (win_overlap(W, U)) return 1;
}
}
return 0;
}
static void redraw_anim(gui_gfx_t *g, int skip) {
if (demo_covered(skip)) { wm_draw(g); return; }
redraw_demos(g, skip);
}
int shell_run(gui_gfx_t *g, u32 mem_lo, u32 mem_hi) {
mouse_video_t mv;
mv.vram=g->vram; mv.pitch=g->pitch;
mv.hw_w=g->hw_w; mv.hw_h=g->hw_h; mv.bpp=g->bpp;
mv.virt_w=g->virt_w; mv.virt_h=g->virt_h;
mv.bg_r=24; mv.bg_g=26; mv.bg_b=32;
mv.rect=g->rect; mv.glyph=g->ch;
mouse_init(&mv);
mouse_enable_buttons(MOUSE_B1|MOUSE_B2|MOUSE_B3);
shell_g = g;
settings_load_cfg();
tctx.mem_lower_kb=mem_lo; tctx.mem_upper_kb=mem_hi;
tctx.virt_w=g->virt_w; tctx.virt_h=g->virt_h;
tctx.hw_w=g->hw_w;     tctx.hw_h=g->hw_h;
tctx.bpp=g->bpp;
tctx.print_line=term_print_line;
tctx.print_num =term_print_num;
tctx.print_raw =term_print_raw;
tctx.pump      =term_pump;
tctx.signal=0;
for (int i=0;i<MAX_TERMS;++i) term_reset(&terms[i]);
g->enable_bb();
wm_set_content_cb(term_draw);
task_init(g,(int)g->virt_w,(int)g->virt_h);
task_set_curtain(0);
spawn_console();
int structural=1, task_dirty=0;
int mode=0, mode_id=-1, mode_edges=0;
int curtain_mode=0;
int last_sec=task_second();
shell_paint_bg(g); wm_draw(g); task_draw(); shell_present(g); mouse_draw_plain();
for (;;) {
mouse_update();
int pressed=mouse_pressed(), released=mouse_released(), moved=mouse_moved();
int mx,my,mb; mouse_get(&mx,&my,&mb);
int vx=mx*(int)g->virt_w/(int)g->hw_w;
int vy=my*(int)g->virt_h/(int)g->hw_h;
int key=key_poll();
int sec=task_second();
int sec_changed=(sec!=last_sec);
int anim=any_demo(mode ? mode_id : -1);
if (sec_changed) last_sec=sec;
int dx=0,dy=0;
mouse_take_delta(&dx,&dy);
if (!pressed && !released && !moved && !key && !sec_changed && !anim && !dx && !dy) {
__asm__ volatile("pause");
continue;
}
if (pressed) {
if (kill_mode && (pressed & MOUSE_B1)) {
int part=0, wid=wm_hit(vx,vy,&part);
if (wid>=0) {
wm_destroy(wid);
if (mode_id==wid) { mode=0; mouse_set_frozen(0); wm_set_skip(-1); }
}
kill_mode=0;
structural=1;
} else if (task_button_hit(vx,vy)) {
if (pressed & MOUSE_B3) {
curtain_mode=1;
task_set_curtain(1);
task_set_curtain_sel(-1);
task_dirty=1;
}
} else {
int part=0, wid=wm_hit(vx,vy,&part);
if (wid>=0) {
wm_focus(wid);
wm_window_t *FW = wm_find(wid);
term_t *FT = FW ? (term_t*)FW->user : 0;
if (FT && (pressed & MOUSE_B1)) {
int ccx,ccy,ccw,cch;
wm_client_rect(FW,&ccx,&ccy,&ccw,&cch);
if (vx>=ccx && vx<ccx+ccw && vy>=ccy && vy<ccy+cch) {
int used=0;
if (FT->draw_mode) {
    if (draw_press(&FT->dr, ccx, ccy, ccw, cch, vx, vy)) {
        draw_drag_begin(&FT->dr, vx - ccx, vy - ccy - 20);
    }
    used = 1;
}
if (FT->settings_mode) {
int idx = settings_hit(&FT->st, ccx, ccy, ccw, cch, vx, vy);
if (idx >= 0) settings_press(&FT->st, idx);
used = 1;
}
#if APSYS_HAS_CALC
if (FT->calc_mode) {
int idx=calc_hit(&FT->c, ccx,ccy,ccw,cch, vx,vy);
if (idx>=0){ FT->c.sel=idx; calc_press(&FT->c,idx); }
used=1;
}
#endif
#if APSYS_HAS_CHESS
if (FT->chess_mode) { chess_click(&FT->ch, vx, vy); used=1; }
#endif
#if APSYS_HAS_BALLS
if (FT->balls_mode) { balls_click(&FT->b, vx-ccx, vy-ccy); used=1; }
#endif
if (used) structural=1;
}
}
int edges=0;
if ((pressed & MOUSE_B2) && wm_hit_border(vx,vy,&edges)==wid) {
mode=2; mode_id=wid; mode_edges=edges;
wm_set_skip(wid);
mouse_take_delta(&dx,&dy);
} else if (pressed & MOUSE_B3) {
mode=1; mode_id=wid;
wm_set_skip(wid);
mouse_set_frozen(1);
mouse_take_delta(&dx,&dy);
}
structural=1;
} else if (pressed & MOUSE_B3) {
ctx_mode=1;
ctx_x=vx; ctx_y=vy;
if (ctx_x+CTX_W>(int)g->virt_w) ctx_x=(int)g->virt_w-CTX_W;
if (ctx_y+CTX_H>(int)g->virt_h) ctx_y=(int)g->virt_h-CTX_H;
if (ctx_x<0) ctx_x=0;
if (ctx_y<0) ctx_y=0;
ctx_sel=-1;
structural=1;
}
}
}
if (curtain_mode) {
int idx=-1;
task_curtain_hit(vx,vy,&idx);
if (idx!=task_curtain_sel()) { task_set_curtain_sel(idx); task_dirty=1; }
}
if (ctx_mode) {
int s=-1;
if (vx>=ctx_x && vx<ctx_x+CTX_W && vy>=ctx_y && vy<ctx_y+CTX_H) {
s=(vy-(ctx_y+2))/CTX_ROW;
if (s<0||s>3) s=-1;
}
if (s!=ctx_sel) { ctx_sel=s; structural=1; }
}
if (mode==1 && (dx||dy)) wm_move_delta(mode_id,dx,dy);
if (mode==2 && (dx||dy)) wm_resize_edges(mode_id,mode_edges,dx,dy);
if ((mb & MOUSE_B1) && !mode && !kill_mode) {
    wm_window_t *TW = wm_top();
    term_t *TT = TW ? (term_t*)TW->user : 0;
    if (TT && TT->draw_mode && TT->dr.drawing) {
        int ccx, ccy, ccw, cch;
        wm_client_rect(TW, &ccx, &ccy, &ccw, &cch);
        if (vx >= ccx && vx < ccx + ccw && vy >= ccy + 20 && vy < ccy + cch - 18) {
            draw_drag_to(&TT->dr, vx - ccx, vy - ccy - 20);
            structural = 1;
        } else {
            TT->dr.drawing = 0;
        }
    }
}
if (released) {
if (released & MOUSE_B1) {
    wm_window_t *TW = wm_top();
    term_t *TT = TW ? (term_t*)TW->user : 0;
    if (TT && TT->draw_mode) TT->dr.drawing = 0;
}
if (curtain_mode && (released & MOUSE_B3)) {
curtain_mode=0;
int id=task_curtain_id(task_curtain_sel());
if (id>=0) {
wm_window_t *W=wm_find(id);
if (W) { W->visible=1; wm_focus(id); structural=1; }
}
task_set_curtain(0);
task_dirty=1;
}
if (ctx_mode && (released & MOUSE_B3)) {
ctx_mode=0; structural=1;
if      (ctx_sel==0) { spawn_console(); }
else if (ctx_sel==1) { return 1; }
else if (ctx_sel==2) { asyxbox_exec("rb",2,&tctx); }
else if (ctx_sel==3) { asyxbox_exec("sn",2,&tctx); }
}
if (mode==1 && (released & MOUSE_B3)) { mode=0; mouse_set_frozen(0); wm_set_skip(-1); structural=1; }
if (mode==2 && (released & MOUSE_B2)) { mode=0; wm_set_skip(-1); structural=1; }
}
if (key) {
wm_window_t *W=wm_top();
term_t *T=W?(term_t*)W->user:0;
int demo = T && (T->colors_mode || T->cube_mode);
int handled = 0;
#if APSYS_HAS_BALLS
if (!handled && T && T->balls_mode) {
key_modifiers_t mm; key_get_modifiers(&mm);
if (balls_key(&T->b, key, mm.ctrl)==BALLS_ACT_QUIT) T->balls_mode=0;
handled=1;
}
#endif
#if APSYS_HAS_CHESS
if (!handled && T && T->chess_mode) {
key_modifiers_t mm; key_get_modifiers(&mm);
if (chess_key(&T->ch, key, mm.ctrl)) T->chess_mode=0;
handled=1;
}
#endif
#if APSYS_HAS_CALC
if (!handled && T && T->calc_mode) {
key_modifiers_t mm; key_get_modifiers(&mm);
if (calc_key(&T->c, key, mm.ctrl)==CALC_ACT_QUIT) T->calc_mode=0;
handled=1;
}
#endif
#if APSYS_HAS_ASM
if (!handled && T && T->asm_mode) {
if (key==KEY_ESC) { T->asm_mode=0; handled=1; }
}
#endif
if (!handled && T && T->draw_mode) {
    if (draw_key(&T->dr, key) == DRAW_ACT_QUIT) T->draw_mode = 0;
    handled = 1;
}
if (!handled && T && T->settings_mode) {
key_modifiers_t mm; key_get_modifiers(&mm);
if (settings_key(&T->st, key, mm.ctrl) == SET_ACT_QUIT) T->settings_mode = 0;
handled = 1;
}
if (!handled && T && T->jaro_mode) {
key_modifiers_t mm; key_get_modifiers(&mm);
if (jaro_key(&T->j, key, mm.ctrl, mm.shift)==JARO_ACT_QUIT) T->jaro_mode=0;
handled=1;
}
if (handled) {
structural=1;
} else if (demo) {
T->colors_mode=0; T->cube_mode=0; T->cube_is_4d=0;
structural=1;
} else if (key==KEY_ESC) {
kill_mode=!kill_mode;
structural=1;
} else if (key==KEY_F1) {
spawn_console();
structural=1;
} else if (T) {
if (key==KEY_ENTER) {
term_echo(T);
T->input[T->len]=0;
tctx.signal=0;
exec_term=T;
asyxbox_exec(T->input,T->len,&tctx);
int sig=tctx.signal;
tctx.signal=0;
exec_term=0;
for (int i=0;i<TERM_COLS;++i) T->input[i]=0;
T->len=0; T->cursor=0;
if (sig==ASYXBOX_SIGNAL_SWITCH_TERMINAL) return 1;
if (sig==ASYXBOX_SIGNAL_SWITCH_ARTHE) {
while (wm_count()>0) {
wm_window_t *X=wm_get(0);
if (!X) break;
wm_destroy(X->id);
}
spawn_console();
}
#if APSYS_HAS_ASM
if (sig==ASYXBOX_SIGNAL_OPEN_ASM) T->asm_mode=1;
#endif
if (sig==ASYXBOX_SIGNAL_CLEAR) T->count=0;
if (sig==ASYXBOX_SIGNAL_OPEN_COLORS) { T->colors_mode=1; T->colors_frame=0; }
if (sig==ASYXBOX_SIGNAL_OPEN_CUBE)   { T->cube_mode=1; T->cube_frame=0; T->cube_is_4d=0; }
if (sig==ASYXBOX_SIGNAL_OPEN_CUBE_4D){ T->cube_mode=1; T->cube_frame=0; T->cube_is_4d=1; }
if (sig==ASYXBOX_SIGNAL_OPEN_EDITOR) {
if (jaro_open(&T->j, tctx.edit_vol, tctx.edit_path, tctx.edit_name)) T->jaro_mode=1;
else { term_append(T,"JARO: CANNOT OPEN"); term_append(T,T->j.status); }
}
#if APSYS_HAS_CALC
if (sig==ASYXBOX_SIGNAL_OPEN_CALC) { calc_init(&T->c); T->calc_mode=1; }
#endif
#if APSYS_HAS_CHESS
if (sig==ASYXBOX_SIGNAL_OPEN_CHESS) { chess_init(&T->ch); T->chess_mode=1; }
#endif
#if APSYS_HAS_BALLS
if (sig==ASYXBOX_SIGNAL_OPEN_BALLS) { balls_init(&T->b); T->balls_mode=1; }
#endif
if (sig==ASYXBOX_SIGNAL_OPEN_SETTINGS) { settings_init(&T->st); T->settings_mode=1; }
if (sig == ASYXBOX_SIGNAL_OPEN_DRAW) { draw_init(&T->dr); T->draw_mode = 1; }
structural=1;
} else if (key==KEY_BACKSPACE) {
if (T->cursor>0) {
for (int i=T->cursor-1;i<T->len-1;++i) T->input[i]=T->input[i+1];
T->len--; T->cursor--;
}
structural=1;
} else if (key==KEY_LEFT) {
if (T->cursor>0) T->cursor--;
structural=1;
} else if (key==KEY_RIGHT) {
if (T->cursor<T->len) T->cursor++;
structural=1;
} else if (key>=32 && key<=126 && T->len<TERM_COLS-1) {
key_modifiers_t mm; key_get_modifiers(&mm);
if (!mm.ctrl) {
for (int i=T->len;i>T->cursor;--i) T->input[i]=T->input[i-1];
T->input[T->cursor]=(char)key;
T->len++; T->cursor++;
structural=1;
}
}
}
}
if (structural) {
shell_paint_bg(g);
wm_draw(g);
task_draw();
if (ctx_mode) ctx_menu_draw(g,ctx_x,ctx_y,ctx_sel);
structural=0; task_dirty=0;
} else if (anim) {
redraw_anim(g, mode ? mode_id : -1);
if (sec_changed || task_dirty) { task_draw(); task_dirty=0; }
} else if (sec_changed || task_dirty) {
shell_paint_bg(g);
wm_draw(g);
task_draw();
task_dirty=0;
}
shell_present(g);
if (mode) {
wm_window_t *W=wm_find(mode_id);
if (W) wm_rubber(g,W);
}
mouse_draw_plain();
if (kill_mode) {
int sw=12, sh=19;
if (mx+sw>(int)g->hw_w) sw=(int)g->hw_w-mx;
if (my+sh>(int)g->hw_h) sh=(int)g->hw_h-my;
u32 bpp=g->bpp/8;
for (int r=0;r<sh;++r)
for (int c=0;c<sw;++c) {
int on=(r==c)||(r+1==sw-c);
if (r==sh/2||c==sw/2) on=1;
if (!on) continue;
u32 off=(u32)(my+r)*g->pitch+(u32)(mx+c)*bpp;
g->vram[off+0]=(u8)(255-g->vram[off+0]);
g->vram[off+1]=(u8)(255-g->vram[off+1]);
g->vram[off+2]=(u8)(255-g->vram[off+2]);
}
}
__asm__ volatile("pause");
}
return 0;
}
