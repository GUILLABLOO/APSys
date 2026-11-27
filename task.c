/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
task - APSys 00.01 */

#include "task.h"
#include "wm.h"
#include "settings.h"

typedef unsigned char u8;
typedef unsigned short u16;

static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline u8   inb (u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}

#define COL_EDGE 60
#define COL_FILL 255
#define COL_TEXT 40

#define PANEL_W 110
#define PANEL_H 28
#define CURT_W  160
#define CURT_H  22
#define CLOCK_W 84

static gui_gfx_t G;
static int scr_w=0, scr_h=0;
static int curtain_open=0, curtain_sel=-1;
static int curt_id[WM_MAX_WINDOWS];
static int curt_n=0;

void task_init(gui_gfx_t *g,int w,int h){ G=*g; scr_w=w; scr_h=h; }

static u8 cmos(u8 reg){ outb(0x70,reg); return inb(0x71); }
static u8 bcd(u8 v){ return (u8)((v&0x0F)+((v>>4)*10)); }

static void read_hms(int *hh,int *mm,int *ss){
    u8 b=cmos(0x0B);
    u8 h=cmos(0x04), m=cmos(0x02), s=cmos(0x00);
    u8 pm=h&0x80; h&=0x7F;
    if(!(b&4)){ h=bcd(h); m=bcd(m); s=bcd(s); }
    if(!(b&2)){ if(pm&&h!=12)h+=12; if(!pm&&h==12)h=0; }
    if(h>23)h=23; if(m>59)m=59; if(s>59)s=59;
    *hh=h; *mm=m; *ss=s;
}
int task_second(void){ int h,m,s; read_hms(&h,&m,&s); return s; }
static void clock_str(char *out){
    int h,m,s; read_hms(&h,&m,&s);
    out[0]=(char)('0'+h/10); out[1]=(char)('0'+h%10); out[2]=':';
    out[3]=(char)('0'+m/10); out[4]=(char)('0'+m%10); out[5]=':';
    out[6]=(char)('0'+s/10); out[7]=(char)('0'+s%10); out[8]=0;
}

/* раскраска */
static void box(int x,int y,int w,int h){
    G.rect(x,y,w,h, COL_EDGE,COL_EDGE,COL_EDGE);
    G.rect(x+2,y+2,w-4,h-4, COL_FILL,COL_FILL,COL_FILL);
}

void task_set_curtain(int o){ curtain_open=o; if(!o) curtain_sel=-1; }
int  task_curtain_open(void){ return curtain_open; }
void task_set_curtain_sel(int i){ curtain_sel=i; }
int  task_curtain_sel(void){ return curtain_sel; }
int  task_curtain_id(int i){ return (i>=0&&i<curt_n)?curt_id[i]:-1; }

void task_draw(void){
    int cy=scr_h-PANEL_H-8;

int cw = settings_panel_clock_width();
if (cw > 0) {
    box(scr_w-cw-8, cy, cw, PANEL_H);
    settings_draw_panel_clock(&G, scr_w-cw-8+4, cy+2, PANEL_H-4);
}

    box(8, cy, PANEL_W, PANEL_H);
    G.str("ARTHE", 16, cy+8, COL_TEXT,COL_TEXT,COL_TEXT);
    int n=wm_count(); if(n>9) n=9;
    char cn[2]; cn[0]=(char)('0'+n); cn[1]=0;
    G.str(cn, 84, cy+8, COL_TEXT,COL_TEXT,COL_TEXT);

    curt_n=0;
    if(curtain_open){
        for(int k=wm_count()-1;k>=0;--k){
            wm_window_t *W=wm_get(k);
            if(!W) continue;
            if(curt_n<WM_MAX_WINDOWS) curt_id[curt_n++]=W->id;
        }
        for(int i=0;i<curt_n;++i){
            int ey=cy-(i+1)*CURT_H;
            wm_window_t *W=wm_find(curt_id[i]);
            const char *t=W?W->title:"?";
            if(i==curtain_sel){
                G.rect(8, ey, CURT_W, CURT_H-2, COL_EDGE,COL_EDGE,COL_EDGE);
                G.str(t, 14, ey+4, 255,255,255);
            } else {
                box(8, ey, CURT_W, CURT_H-2);
                G.str(t, 14, ey+4, COL_TEXT,COL_TEXT,COL_TEXT);
            }
        }
    }
}

int task_button_hit(int vx,int vy){
    int cy=scr_h-PANEL_H-8;
    return (vx>=8 && vx<8+PANEL_W && vy>=cy && vy<cy+PANEL_H);
}

int task_curtain_hit(int vx,int vy,int *idx){
    *idx=-1;
    if(!curtain_open) return 0;
    int cy=scr_h-PANEL_H-8;
    if(vx<8 || vx>=8+CURT_W) return 0;
    for(int i=0;i<curt_n;++i){
        int ey=cy-(i+1)*CURT_H;
        if(vy>=ey && vy<ey+CURT_H-2){ *idx=i; return 1; }
    }
    return 0;
}
