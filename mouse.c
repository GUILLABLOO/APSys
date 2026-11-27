/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
mouse - APSys 00.01 */

#include "mouse.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define PS2_DATA 0x60
#define PS2_STAT 0x64
#define PS2_CMD  0x64
#define ST_OUT 0x01
#define ST_IN  0x02
#define ST_AUX 0x20

static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline u8   inb (u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}

static int wait_in(void){
    for(int i=0;i<200000;++i){ if(!(inb(PS2_STAT)&ST_IN)) return 1; __asm__ volatile("pause"); }
    return 0;
}
static int wait_out(void){
    for(int i=0;i<200000;++i){ if(inb(PS2_STAT)&ST_OUT) return 1; __asm__ volatile("pause"); }
    return 0;
}
static void pcmd(u8 c){ if(wait_in()) outb(PS2_CMD,c); }
static void pwrd(u8 v){ if(wait_in()) outb(PS2_DATA,v); }
static int  prdd(u8 *v){ if(!wait_out()) return 0; *v=inb(PS2_DATA); return 1; }
static void pflush(void){ for(int i=0;i<64 && (inb(PS2_STAT)&ST_OUT);++i) (void)inb(PS2_DATA); }

static int m_cmd(u8 c){
    pcmd(0xD4); pwrd(c);
    u8 r=0;
    for(int i=0;i<4;++i){
        if(!prdd(&r)) return 0;
        if(r==0xFA) return 1;
        if(r==0xFE) return 0;
    }
    return 0;
}
static int m_arg(u8 v){
    pcmd(0xD4); pwrd(v);
    u8 r=0;
    if(!prdd(&r)) return 0;
    return r==0xFA;
}

static int present=0;
static int px=0, py=0, bw=1, bh=1, btn=0;
static int press_mask=0, rel_mask=0, moved_flag=0;
static int btn_mask=MOUSE_B1|MOUSE_B2|MOUSE_B3;
static int acc_dx=0, acc_dy=0;
static int frozen=0;
static u8  pkt[3];
static int pkt_i=0;

static void hw_poll(void){
    press_mask=0; rel_mask=0; moved_flag=0;
    if(!present) return;
    for(int guard=0; guard<64; ++guard){
        u8 st=inb(PS2_STAT);
        if(!(st & ST_OUT)) break;
        if(!(st & ST_AUX)) break;
        u8 b=inb(PS2_DATA);
        if(pkt_i==0){
            if(!(b & 0x08)) continue;
            pkt[0]=b; pkt_i=1; continue;
        }
        pkt[pkt_i++]=b;
        if(pkt_i<3) continue;
        pkt_i=0;
        if(pkt[0] & 0xC0) continue;

        int nb=0;
        if(pkt[0]&1) nb|=MOUSE_B1;
        if(pkt[0]&2) nb|=MOUSE_B3;
        if(pkt[0]&4) nb|=MOUSE_B2;

        int dx=(int)pkt[1] - ((pkt[0]&0x10)?256:0);
        int dy=(int)pkt[2] - ((pkt[0]&0x20)?256:0);

        if(dx||dy){
            moved_flag=1;
            acc_dx+=dx; acc_dy+=dy;
            if(!frozen){
                px+=dx; py-=dy;
                if(px<0) px=0;
                if(py<0) py=0;
                if(px>=bw) px=bw-1;
                if(py>=bh) py=bh-1;
            }
        }

        press_mask |= nb & ~btn;
        rel_mask   |= btn & ~nb;
        btn=nb;
    }
}

static mouse_video_t V;
static int have_video=0;

#define CONS_COLS 128
#define CONS_ROWS 40

static char g_ch[CONS_ROWS][CONS_COLS];
static u8   g_cr[CONS_ROWS][CONS_COLS];
static u8   g_cg[CONS_ROWS][CONS_COLS];
static u8   g_cb[CONS_ROWS][CONS_COLS];
static u8   g_hl[CONS_ROWS][CONS_COLS];
static u8   g_want[CONS_ROWS][CONS_COLS];
static int  g_on=0, g_rows=0, g_cols=0;
static int  g_top=80, g_cw=8, g_chh=18;

void mouse_note_char(unsigned char c,int x,int y,unsigned char r,unsigned char g,unsigned char b){
    if(!g_on) return;
    if(y < g_top || x < 0) return;
    if((y - g_top) % g_chh) return;
    if(x % g_cw) return;
    int row=(y-g_top)/g_chh, col=x/g_cw;
    if(row<0||row>=g_rows||col<0||col>=g_cols) return;
    g_ch[row][col]=(char)c;
    g_cr[row][col]=r; g_cg[row][col]=g; g_cb[row][col]=b;
}

void mouse_note_clear(int x,int y,int w,int h){
    if(!g_on) return;
    for(int r=0;r<g_rows;++r){
        int cy=g_top+r*g_chh;
        if(cy+g_chh<=y || cy>=y+h) continue;
        for(int c=0;c<g_cols;++c){
            int cx=c*g_cw;
            if(cx+g_cw<=x || cx>=x+w) continue;
            g_ch[r][c]=0;
            g_hl[r][c]=0;
        }
    }
}

void mouse_cons_setup(int top,int cell_w,int cell_h){
    g_top=top; g_cw=cell_w; g_chh=cell_h;
    if(!have_video || g_cw<=0 || g_chh<=0){ g_on=0; return; }
    g_rows=((int)V.virt_h - g_top)/g_chh;
    g_cols=(int)V.virt_w/g_cw;
    if(g_rows>CONS_ROWS) g_rows=CONS_ROWS;
    if(g_cols>CONS_COLS) g_cols=CONS_COLS;
    if(g_rows<0) g_rows=0;
    if(g_cols<0) g_cols=0;
    for(int r=0;r<CONS_ROWS;++r)
        for(int c=0;c<CONS_COLS;++c){
            g_ch[r][c]=0; g_hl[r][c]=0; g_want[r][c]=0;
            g_cr[r][c]=255; g_cg[r][c]=255; g_cb[r][c]=255;
        }
    g_on=1;
}

static void cell_invert(int row,int col){
    if(!have_video || !V.vram || !V.virt_w || !V.virt_h) return;
    int vx=col*g_cw, vy=g_top+row*g_chh;
    int rx=vx*(int)V.hw_w/(int)V.virt_w;
    int ry=vy*(int)V.hw_h/(int)V.virt_h;
    int rw=g_cw*(int)V.hw_w/(int)V.virt_w;
    int rh=g_chh*(int)V.hw_h/(int)V.virt_h;
    if(rw<1) rw=1;
    if(rh<1) rh=1;
    u32 bpp=V.bpp/8;
    for(int y=ry; y<ry+rh; ++y){
        if(y<0 || y>=(int)V.hw_h) continue;
        u32 base=(u32)y*V.pitch;
        for(int x=rx; x<rx+rw; ++x){
            if(x<0 || x>=(int)V.hw_w) continue;
            u32 off=base+(u32)x*bpp;
            V.vram[off+0]=(u8)(255-V.vram[off+0]);
            V.vram[off+1]=(u8)(255-V.vram[off+1]);
            V.vram[off+2]=(u8)(255-V.vram[off+2]);
        }
    }
}

static int sel_on=0, sel_ar=0, sel_ac=0, sel_r=0, sel_c=0, exec_pending=0;

static void sel_bounds(int *r0,int *c0,int *r1,int *c1){
    *r0=sel_ar; *c0=sel_ac; *r1=sel_r; *c1=sel_c;
    if(*r1<*r0 || (*r1==*r0 && *c1<*c0)){
        int t=*r0; *r0=*r1; *r1=t;
        t=*c0; *c0=*c1; *c1=t;
    }
}

static void want_build(void){
    for(int r=0;r<g_rows;++r)
        for(int c=0;c<g_cols;++c) g_want[r][c]=0;
    if(!sel_on) return;
    int r0,c0,r1,c1;
    sel_bounds(&r0,&c0,&r1,&c1);
    for(int r=r0;r<=r1;++r){
        if(r<0||r>=g_rows) continue;
        int cs=(r==r0)?c0:0;
        int ce=(r==r1)?c1:g_cols-1;
        for(int c=cs;c<=ce;++c)
            if(c>=0&&c<g_cols) g_want[r][c]=1;
    }
}

static void want_apply(void){
    for(int r=0;r<g_rows;++r)
        for(int c=0;c<g_cols;++c)
            if(g_want[r][c]!=g_hl[r][c]){
                cell_invert(r,c);
                g_hl[r][c]=g_want[r][c];
            }
}

static int sel_text(char *out,int max){
    if(!sel_on || max<2) return 0;
    int r0,c0,r1,c1;
    sel_bounds(&r0,&c0,&r1,&c1);
    int n=0;
    for(int r=r0;r<=r1 && n<max-1;++r){
        if(r<0||r>=g_rows) continue;
        int cs=(r==r0)?c0:0;
        int ce=(r==r1)?c1:g_cols-1;
        if(ce>=g_cols) ce=g_cols-1;
        for(int c=cs;c<=ce && n<max-1;++c){
            if(c<0) continue;
            char ch=g_ch[r][c];
            out[n++]= ch?ch:' ';
        }
        if(r<r1 && n<max-1) out[n++]=' ';
    }
    out[n]=0;
    int s=0; while(s<n && out[s]==' ') s++;
    int e=n; while(e>s && out[e-1]==' ') e--;
    int m=0;
    for(int i=s;i<e;++i) out[m++]=out[i];
    out[m]=0;
    return m;
}

#define CUR_W 12
#define CUR_H 19

static const u8 cur_shape[CUR_H][CUR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,1,1,1,1,0},
    {1,2,2,2,1,1,1,0,0,0,0,0},
    {1,2,1,1,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
};

static u8  cur_save[CUR_H][CUR_W][3];
static int cur_x=0, cur_y=0, cur_vis=0, cur_sw=0, cur_sh=0;

void mouse_hide(void){
    if(!cur_vis || !have_video || !V.vram) return;
    u32 bpp=V.bpp/8;
    for(int r=0;r<cur_sh;++r)
        for(int c=0;c<cur_sw;++c){
            u32 off=(u32)(cur_y+r)*V.pitch + (u32)(cur_x+c)*bpp;
            V.vram[off+0]=cur_save[r][c][0];
            V.vram[off+1]=cur_save[r][c][1];
            V.vram[off+2]=cur_save[r][c][2];
        }
    cur_vis=0;
}

void mouse_show(void){
    if(!present || !have_video || !V.vram) return;
    cur_x=px; cur_y=py;
    cur_sw=CUR_W; cur_sh=CUR_H;
    if(cur_x+cur_sw>(int)V.hw_w) cur_sw=(int)V.hw_w-cur_x;
    if(cur_y+cur_sh>(int)V.hw_h) cur_sh=(int)V.hw_h-cur_y;
    if(cur_sw<=0||cur_sh<=0){ cur_vis=0; return; }
    u32 bpp=V.bpp/8;
    for(int r=0;r<cur_sh;++r)
        for(int c=0;c<cur_sw;++c){
            u32 off=(u32)(cur_y+r)*V.pitch + (u32)(cur_x+c)*bpp;
            cur_save[r][c][0]=V.vram[off+0];
            cur_save[r][c][1]=V.vram[off+1];
            cur_save[r][c][2]=V.vram[off+2];
            u8 s=cur_shape[r][c];
            if(s==1){ V.vram[off+0]=0;   V.vram[off+1]=0;   V.vram[off+2]=0; }
            else if(s==2){ V.vram[off+0]=255; V.vram[off+1]=255; V.vram[off+2]=255; }
        }
    cur_vis=1;
}

void mouse_draw_plain(void){
    if(!present || !have_video || !V.vram) return;
    int x=px, y=py;
    int sw=CUR_W, sh=CUR_H;
    if(x+sw>(int)V.hw_w) sw=(int)V.hw_w-x;
    if(y+sh>(int)V.hw_h) sh=(int)V.hw_h-y;
    if(sw<=0||sh<=0) return;
    u32 bpp=V.bpp/8;
    for(int r=0;r<sh;++r)
        for(int c=0;c<sw;++c){
            u8 s=cur_shape[r][c];
            if(!s) continue;
            u32 off=(u32)(y+r)*V.pitch + (u32)(x+c)*bpp;
            if(s==1){ V.vram[off+0]=0;   V.vram[off+1]=0;   V.vram[off+2]=0; }
            else    { V.vram[off+0]=255; V.vram[off+1]=255; V.vram[off+2]=255; }
        }
}

void mouse_set_frozen(int f){ frozen=f; }

void mouse_take_delta(int *dx,int *dy){
    if(dx)*dx=acc_dx;
    if(dy)*dy=acc_dy;
    acc_dx=0; acc_dy=0;
}

int mouse_init(const mouse_video_t *video){
    if(!video) return 0;
    V=*video;
    have_video=1;
    bw=(int)V.hw_w; bh=(int)V.hw_h;
    if(bw<1) bw=1;
    if(bh<1) bh=1;
    px=bw/2; py=bh/2;
    pkt_i=0; btn=0; present=0; frozen=0; acc_dx=0; acc_dy=0;
    btn_mask=MOUSE_B1|MOUSE_B2|MOUSE_B3;

    pcmd(0xA8);
    pcmd(0x20);
    u8 cb=0;
    if(!prdd(&cb)) return 0;
    cb &= (u8)~0x10;
    cb &= (u8)~0x08;
    cb &= (u8)~0x03;
    cb |= 0x20;
    pcmd(0x60); pwrd(cb);

    pflush();
    m_cmd(0xF6);
    m_cmd(0xF3); m_arg(100);
    if(!m_cmd(0xF4)) return 0;
    pflush();

    present=1;
    return 1;
}

void mouse_enable_buttons(int mask){ btn_mask=mask; }

int mouse_frame(void){
    hw_poll();
    if(!present || !have_video) return 0;
    if(!press_mask && !rel_mask && !moved_flag) return 0;

    int hr=-1, hc=-1;
    if(g_on && V.hw_w && V.hw_h){
        int vx=px*(int)V.virt_w/(int)V.hw_w;
        int vy=py*(int)V.virt_h/(int)V.hw_h;
        if(vy>=g_top){
            hr=(vy-g_top)/g_chh;
            hc=vx/g_cw;
            if(hr>=g_rows||hc>=g_cols){ hr=-1; hc=-1; }
        }
    }

    int pe   = press_mask & btn_mask;
    int held = btn & btn_mask;

    mouse_hide();
    if((pe & MOUSE_B1) && hr>=0){ sel_on=1; sel_ar=sel_r=hr; sel_ac=sel_c=hc; }
    if((held & MOUSE_B1) && sel_on && hr>=0){ sel_r=hr; sel_c=hc; }
    if(pe & MOUSE_B3) sel_on=0;
    if(pe & MOUSE_B2) exec_pending=1;
    if(g_on){ want_build(); want_apply(); }
    mouse_show();
    return 1;
}

int mouse_take_exec(char *out,int max){
    if(!exec_pending) return 0;
    exec_pending=0;
    if(!sel_on) return 0;
    int n=sel_text(out,max);
    mouse_hide();
    sel_on=0;
    if(g_on){ want_build(); want_apply(); }
    mouse_show();
    return n;
}

int  mouse_pressed(void){ return press_mask; }
int  mouse_released(void){ return rel_mask; }
int  mouse_moved(void){ return moved_flag; }
int  mouse_present(void){ return present; }
void mouse_update(void){ hw_poll(); }
void mouse_get(int *x,int *y,int *b){ if(x)*x=px; if(y)*y=py; if(b)*b=btn; }
