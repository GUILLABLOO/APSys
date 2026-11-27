/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
balls - APSys 00.01 */

#include "balls.h"
#include "key.h"
#include "mouse.h"

typedef unsigned char u8;

#define BALL_R 11
#define SP     (2*BALL_R)
#define FLY    340.0f
#define CLOSE_SPEED 140.0f
#define POP_TIME 0.28f

static unsigned long long b_rdtsc(void){
    unsigned lo,hi;
    __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    return ((unsigned long long)hi<<32)|lo;
}
static unsigned long long tsc_per_ms = 2000000ULL;
static inline void b_outb(unsigned short p, unsigned char v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline unsigned char b_inb(unsigned short p){ unsigned char v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static void tsc_calibrate(void){
    unsigned char old = b_inb(0x61);
    b_outb(0x43, 0xB0);
    b_outb(0x42, 0x9C);
    b_outb(0x42, 0x2E);
    b_outb(0x61, (unsigned char)((old & 0xFD) | 0x01));
    unsigned long long t0 = b_rdtsc();
    for(int guard=0; guard<20000000; ++guard){
        if(b_inb(0x61) & 0x20) break;
    }
    unsigned long long t1 = b_rdtsc();
    b_outb(0x61, old);
    unsigned long long d = (t1 - t0) / 10ULL;
    if(d > 100000ULL && d < 10000000ULL) tsc_per_ms = d;
}
static float f_mod(float x, float y){ while(x>=y)x-=y; while(x<=-y)x+=y; return x; }
static float f_sin(float x){
    const float PI=3.14159265f, TWO=6.28318530f;
    x=f_mod(x,TWO);
    if(x> PI)x-=TWO;
    if(x<-PI)x+=TWO;
    float x2=x*x, t=x, s=t;
    t*=-x2/(2.0f*3.0f); s+=t;
    t*=-x2/(4.0f*5.0f); s+=t;
    t*=-x2/(6.0f*7.0f); s+=t;
    t*=-x2/(8.0f*9.0f); s+=t;
    return s;
}
static float f_cos(float x){ return f_sin(x+1.57079633f); }
static float b_sqrt(float v){
    if(v<=0.0f)return 0.0f;
    float x=v*0.5f+0.5f;
    for(int i=0;i<24;++i)x=(x+v/x)*0.5f;
    return x;
}
static int b_irnd(balls_t*B,int n){
    B->rnd=B->rnd*1103515245u+12345u;
    return (int)((B->rnd>>8)%(unsigned)n);
}
static int b_num(char*b,int p,int v){
    char t[8]; int n=0;
    if(v==0)t[n++]='0';
    while(v&&n<8){t[n++]=(char)('0'+v%10);v/=10;}
    while(n>0)b[p++]=t[--n];
    return p;
}
static int str_app_hud(char*d,int p,const char*s){ while(*s&&p<78)d[p++]=*s++; return p; }

static const u8 CA[3][3]={{60,180,80},{240,130,180},{45,45,45}};
static const u8 CB[3][3]={{150,80,200},{120,200,240},{240,240,240}};
static void fill_circle(gui_gfx_t*g,int cx,int cy,int r,u8 cr,u8 cg,u8 cb,
                        int x0,int y0,int x1,int y1){
    for(int dy=-r;dy<=r;++dy){
        int hw=(int)b_sqrt((float)(r*r-dy*dy));
        int y=cy+dy; if(y<y0||y>y1)continue;
        int xa=cx-hw, xb=cx+hw;
        if(xa<x0)xa=x0; if(xb>x1)xb=x1;
        if(xb>=xa)g->rect(xa,y,xb-xa+1,1,cr,cg,cb);
    }
}
static void thick_line(gui_gfx_t*g,float x0,float y0,float x1,float y1,float w,
                       u8 cr,u8 cg,u8 cb,int x0c,int y0c,int x1c,int y1c){
    float dx=x1-x0, dy=y1-y0;
    float len=b_sqrt(dx*dx+dy*dy);
    if(len<0.001f)return;
    int steps=(int)(len/2)+1;
    int hw=(int)(w*0.5f);
    for(int i=0;i<=steps;++i){
        float t=(float)i/steps;
        int x=(int)(x0+dx*t), y=(int)(y0+dy*t);
        int xa=x-hw, ya=y-1, xb=x+hw, yb=y+1;
        if(xa<x0c)xa=x0c; if(ya<y0c)ya=y0c;
        if(xb>x1c)xb=x1c; if(yb>y1c)yb=y1c;
        if(xb>=xa&&yb>=ya)g->rect(xa,ya,xb-xa+1,yb-ya+1,cr,cg,cb);
    }
}

static void draw_ball_r(gui_gfx_t*g,int x,int y,int t,float ang,int rr,
                        int x0,int y0,int x1,int y1){
    if(rr<2)return;
    fill_circle(g,x,y,rr,30,30,34,x0,y0,x1,y1);
    float ca=f_cos(ang), sa=f_sin(ang);
    int R1=rr-1;
    for(int dy=-R1;dy<=R1;++dy){
        int hw=(int)b_sqrt((float)(R1*R1-dy*dy));
        int yy=y+dy; if(yy<y0||yy>y1)continue;
        int xa=x-hw, xb=x+hw;
        if(xa<x0)xa=x0; if(xb>x1)xb=x1;
        if(xb<xa)continue;
        if(ca>0.0001f || ca<-0.0001f){
            int split=x+(int)((-(float)dy*sa)/ca);
            if(split<xa)split=xa;
            if(split>xb+1)split=xb+1;
            const u8 *L = (ca>0.0001f)? CA[t] : CB[t];
            const u8 *R = (ca>0.0001f)? CB[t] : CA[t];
            if(split-1>=xa) g->rect(xa,yy,split-xa,1,L[0],L[1],L[2]);
            if(xb>=split)   g->rect(split,yy,xb-split+1,1,R[0],R[1],R[2]);
        } else {
            const u8 *C = (dy*sa>0)? CB[t] : CA[t];
            g->rect(xa,yy,xb-xa+1,1,C[0],C[1],C[2]);
        }
    }
    int hr=rr/4; if(hr<1)hr=1;
    fill_circle(g,x-rr/3,y-rr/3,hr,235,235,235,x0,y0,x1,y1);
}
static void draw_ball(gui_gfx_t*g,int x,int y,int t,float ang,
                      int x0,int y0,int x1,int y1){
    draw_ball_r(g,x,y,t,ang,BALL_R,x0,y0,x1,y1);
}

#define POP_MAX 24
typedef struct { float x,y; int type; float t; int on; } pop_t;
static pop_t pops[POP_MAX];
static void pop_add(float x,float y,int type){
    for(int i=0;i<POP_MAX;++i)
        if(!pops[i].on){ pops[i].x=x; pops[i].y=y; pops[i].type=type; pops[i].t=0; pops[i].on=1; return; }
}
static void pop_tick(float dt){
    for(int i=0;i<POP_MAX;++i)
        if(pops[i].on){ pops[i].t+=dt; if(pops[i].t>POP_TIME) pops[i].on=0; }
}
static void pop_draw(gui_gfx_t*g,int cx,int cy,int x0,int y0,int x1,int y1){
    for(int i=0;i<POP_MAX;++i){
        if(!pops[i].on)continue;
        float k=1.0f-pops[i].t/POP_TIME;
        int rr=(int)(BALL_R*k);
        int X=cx+(int)pops[i].x, Y=cy+(int)pops[i].y;
        draw_ball_r(g,X,Y,pops[i].type,pops[i].t*20.0f,rr,x0,y0,x1,y1);
        int wr=(int)(BALL_R*k*0.5f);
        if(wr>=1) fill_circle(g,X,Y,wr,255,255,255,x0,y0,x1,y1);
    }
}

static void track_pos(balls_t*B,float s,float*x,float*y,float*tx,float*ty){
    if(s<0)s=0; if(s>B->total_len)s=B->total_len;
    int i=1;
    while(i<B->npts-1&&B->cum[i]<s)i++;
    float seg=B->cum[i]-B->cum[i-1];
    float t=seg>0.001f?(s-B->cum[i-1])/seg:0;
    float ax=B->px[i-1],ay=B->py[i-1],bx=B->px[i],by=B->py[i];
    *x=ax+(bx-ax)*t; *y=ay+(by-ay)*t;
    float dx=bx-ax,dy=by-ay,L=b_sqrt(dx*dx+dy*dy);
    if(L<0.001f){*tx=1;*ty=0;}else{*tx=dx/L;*ty=dy/L;}
}
static void gen_level(balls_t*B){
    int w=B->vw,h=B->vh;
    B->can_x=w*0.5f; B->can_y=h*0.55f;
    B->aim_x=0; B->aim_y=-1;
    B->hole_x=60; B->hole_y=60;
    for(int t=0;t<60;++t){
        float hx=40+b_irnd(B,w-80), hy=40+b_irnd(B,h-80);
        float dx=hx-B->can_x, dy=hy-B->can_y;
        if(b_sqrt(dx*dx+dy*dy)>150){ B->hole_x=hx; B->hole_y=hy; break; }
    }
    B->npts=0;
    float sx=w-B->hole_x, sy=h-B->hole_y;
    if(sx<40)sx=40; if(sx>w-40)sx=w-40;
    if(sy<40)sy=40; if(sy>h-40)sy=h-40;
    B->px[0]=sx; B->py[0]=sy; B->npts=1;
    int wp=4+b_irnd(B,4);
    for(int i=0;i<wp&&B->npts<BALLS_MAX_PTS-1;++i){
        for(int t=0;t<40;++t){
            float x=36+b_irnd(B,w-72), y=36+b_irnd(B,h-72);
            float dcx=x-B->can_x, dcy=y-B->can_y;
            if(b_sqrt(dcx*dcx+dcy*dcy)<110)continue;
            float dpx=x-B->px[B->npts-1], dpy=y-B->py[B->npts-1];
            if(b_sqrt(dpx*dpx+dpy*dpy)<70)continue;
            B->px[B->npts]=x; B->py[B->npts]=y; B->npts++;
            break;
        }
    }
    B->px[B->npts]=B->hole_x; B->py[B->npts]=B->hole_y; B->npts++;
    B->cum[0]=0;
    for(int i=1;i<B->npts;++i){
        float dx=B->px[i]-B->px[i-1],dy=B->py[i]-B->py[i-1];
        B->cum[i]=B->cum[i-1]+b_sqrt(dx*dx+dy*dy);
    }
    B->total_len=B->cum[B->npts-1];
    int cnt=16+B->level*4; if(cnt>BALLS_MAX_CHAIN-4)cnt=BALLS_MAX_CHAIN-4;
    B->n=cnt;
    B->s[0]=60;
    for(int i=0;i<cnt;++i){
        int t=b_irnd(B,3);
        if(i>=2&&B->type[i-1]==t&&B->type[i-2]==t)t=(t+1)%3;
        B->type[i]=t;
        if(i>0)B->s[i]=B->s[i-1]-SP;
    }
    B->fly_on=0;
    B->loaded=b_irnd(B,3);
    B->over=0; B->paused=0;
    for(int i=0;i<POP_MAX;++i)pops[i].on=0;
}

static int gap_at(balls_t*B,int i){
    return (i>=0 && i<B->n-1) && ((B->s[i]-B->s[i+1]) > SP+2);
}
static void remove_run(balls_t*B,int a,int b){
    for(int i=a;i<=b;++i){
        float x,y,tx,ty;
        track_pos(B,B->s[i],&x,&y,&tx,&ty);
        pop_add(x,y,B->type[i]);
    }
    int c=b-a+1;
    for(int i=a;i+c<B->n;++i){ B->type[i]=B->type[i+c]; B->s[i]=B->s[i+c]; }
    B->n-=c;
    B->score+=c*10;
}
static void match_remove(balls_t*B,int k){
    for(;;){
        if(k<0||k>=B->n)return;
        int a=k,b=k;
        while(a>0 && !gap_at(B,a-1) && B->type[a-1]==B->type[k])a--;
        while(b<B->n-1 && !gap_at(B,b) && B->type[b+1]==B->type[k])b++;
        if(b-a+1<3)return;
        remove_run(B,a,b);
        if(B->n==0){ B->over=2; return; }
        int j=a-1;
        if(j>=0 && j+1<B->n && !gap_at(B,j) && B->type[j]==B->type[j+1]){ k=j; continue; }
        return;
    }
}
static void chain_insert(balls_t*B,int k,int type,float s){
    if(B->n>=BALLS_MAX_CHAIN)return;
    for(int i=B->n;i>k;--i){ B->type[i]=B->type[i-1]; B->s[i]=B->s[i-1]; }
    B->type[k]=type; B->s[k]=s; B->n++;
    match_remove(B,k);
}
static void try_insert(balls_t*B,int j,float fx,float fy){
    float x,y,tx,ty;
    track_pos(B,B->s[j],&x,&y,&tx,&ty);
    float d=(fx-x)*tx+(fy-y)*ty;
    int k; float s;
    if(d>0){ k=j;   s=B->s[j]+SP; if(s>B->total_len-2)s=B->total_len-2; }
    else   { k=j+1; s=B->s[j]-SP; }
    B->fly_on=0;
    chain_insert(B,k,B->fly_type,s);
}
static void fire(balls_t*B){
    if(B->fly_on||B->paused||B->over||!B->gen)return;
    B->fly_on=1;
    B->fx=B->can_x+B->aim_x*(BALL_R+16);
    B->fy=B->can_y+B->aim_y*(BALL_R+16);
    B->fvx=B->aim_x*FLY; B->fvy=B->aim_y*FLY;
    B->fly_type=B->loaded;
    B->loaded=b_irnd(B,3);
}
static void rot_aim(balls_t*B,int dir){
    float c=0.998f, s=0.06f*dir;
    float nx=B->aim_x*c-B->aim_y*s;
    float ny=B->aim_x*s+B->aim_y*c;
    float L=b_sqrt(nx*nx+ny*ny);
    if(L>0.001f){ B->aim_x=nx/L; B->aim_y=ny/L; }
}

void balls_init(balls_t*B){
    for(unsigned i=0;i<sizeof(*B);++i)((u8*)B)[i]=0;
    B->gen=0;
}
void balls_tick(balls_t*B){
    static int calib_done=0;
    if(!calib_done){ calib_done=1; tsc_calibrate(); }
    unsigned long long now=b_rdtsc();
    float dt=(float)((double)(now-B->last_tsc)/(double)tsc_per_ms)/1000.0f;
    B->last_tsc=now;
    if(dt<0)dt=0; if(dt>0.05f)dt=0.05f;
    pop_tick(dt);
    if(!B->gen||B->paused||B->over)return;
    float speed=12.0f+2.0f*(float)B->level;
    if(speed>28.0f)speed=28.0f;
    float cls=CLOSE_SPEED*dt;
    if(B->n>0){
        B->s[0]+=speed*dt;
        for(int i=1;i<B->n;++i){
            float tgt=B->s[i-1]-SP;
            float d=tgt-B->s[i];
            if(d>0){ B->s[i]+= (cls<d?cls:d); }
            else if(d<0){ B->s[i]+= (-cls>d?-cls:d); }
        }
        if(B->s[0]>=B->total_len-4)B->over=1;
    }
    if(B->fly_on){
        B->fx+=B->fvx*dt; B->fy+=B->fvy*dt;
        if(B->fx<-20||B->fy<-20||B->fx>B->vw+20||B->fy>B->vh+20)B->fly_on=0;
        else for(int i=0;i<B->n;++i){
            if(B->s[i]<0)continue;
            float x,y,tx,ty;
            track_pos(B,B->s[i],&x,&y,&tx,&ty);
            float dx=B->fx-x,dy=B->fy-y;
            if(dx*dx+dy*dy<(2*BALL_R-2)*(2*BALL_R-2)){ try_insert(B,i,B->fx,B->fy); break; }
        }
    }
}
int balls_key(balls_t*B,int key,int ctrl){
    if(key==KEY_ESC||(ctrl&&(key=='l'||key=='L')))return BALLS_ACT_QUIT;
    if(key=='p'||key=='P'){ B->paused=1; return 0; }
    if(key=='r'||key=='R'){ B->paused=0; B->last_tsc=b_rdtsc(); return 0; }
    if(B->over){ if(key==KEY_ENTER)gen_level(B); return 0; }
    if(B->paused)return 0;
    if(key==KEY_LEFT){ rot_aim(B,-1); return 0; }
    if(key==KEY_RIGHT){ rot_aim(B,1); return 0; }
    if(key==KEY_ENTER){ fire(B); return 0; }
    return 0;
}
void balls_click(balls_t*B,int lx,int ly){
    if(B->over){ gen_level(B); return; }
    if(B->paused)return;
    float dx=lx-B->can_x, dy=ly-B->can_y;
    float L=b_sqrt(dx*dx+dy*dy);
    if(L<1)return;
    B->aim_x=dx/L; B->aim_y=dy/L;
    fire(B);
}

void balls_draw(balls_t*B,gui_gfx_t*g,int cx,int cy,int cw,int ch){
    B->rx=cx; B->ry=cy;
    if(!B->gen){
        B->gen=1; B->vw=cw; B->vh=ch;
        B->rnd=(unsigned)(b_rdtsc()&0xFFFFFFFFu); if(!B->rnd)B->rnd=881726454u;
        B->level=1; B->score=0;
        gen_level(B);
        B->last_tsc=b_rdtsc();
    } else if(cw!=B->vw||ch!=B->vh){
        B->vw=cw; B->vh=ch;
        gen_level(B);
    }
    int x0=cx,y0=cy,x1=cx+cw-1,y1=cy+ch-1;
    g->rect(cx,cy,cw,ch,112,112,112);
    for(int i=1;i<B->npts;++i)
        thick_line(g,cx+B->px[i-1],cy+B->py[i-1],cx+B->px[i],cy+B->py[i],
                   2*BALL_R+8,226,150,148,x0,y0,x1,y1);
    for(int i=0;i<B->npts;++i)
        fill_circle(g,cx+(int)B->px[i],cy+(int)B->py[i],(2*BALL_R+8)/2,226,150,148,x0,y0,x1,y1);
    fill_circle(g,cx+(int)B->hole_x,cy+(int)B->hole_y,BALL_R+7,70,70,74,x0,y0,x1,y1);
    fill_circle(g,cx+(int)B->hole_x,cy+(int)B->hole_y,BALL_R+4,26,26,30,x0,y0,x1,y1);
    for(int i=0;i<B->n;++i){
        if(B->s[i]<0)continue;
        float x,y,tx,ty;
        track_pos(B,B->s[i],&x,&y,&tx,&ty);
        draw_ball(g,cx+(int)x,cy+(int)y,B->type[i],B->s[i]*0.09f,x0,y0,x1,y1);
    }
    if(B->fly_on)
        draw_ball(g,cx+(int)B->fx,cy+(int)B->fy,B->fly_type,(B->fx+B->fy)*0.09f,x0,y0,x1,y1);
    pop_draw(g,cx,cy,x0,y0,x1,y1);
    fill_circle(g,cx+(int)B->can_x,cy+(int)B->can_y,18,70,72,78,x0,y0,x1,y1);
    thick_line(g,cx+B->can_x,cy+B->can_y,
               cx+B->can_x+B->aim_x*30,cy+B->can_y+B->aim_y*30,12,50,52,58,x0,y0,x1,y1);
    draw_ball(g,cx+(int)B->can_x,cy+(int)B->can_y,B->loaded,0.0f,x0,y0,x1,y1);

    char hud[80]; int p=0;
    p=str_app_hud(hud,p,"BALLS  LVL "); p=b_num(hud,p,B->level);
    p=str_app_hud(hud,p,"  SCORE "); p=b_num(hud,p,B->score);
    p=str_app_hud(hud,p,"  CHAIN "); p=b_num(hud,p,B->n);
    hud[p]=0;
    g->str(hud,cx+6,cy+4,255,255,255);
    g->str("LMB AIM+FIRE  <-/-> AIM  ENTER FIRE  P PAUSE  R RESUME  ESC EXIT",
           cx+6,cy+ch-16,40,40,40);
    if(B->paused)
        g->str("PAUSED - PRESS R",cx+cw/2-64,cy+ch/2,255,255,255);
    if(B->over==1)
        g->str("BALLS FELL INTO THE HOLE - ENTER TO RETRY",cx+cw/2-160,cy+ch/2,255,120,120);
    if(B->over==2)
        g->str("LEVEL CLEAR! - ENTER FOR NEXT",cx+cw/2-120,cy+ch/2,120,255,140);
}

void balls_run_cli(gui_gfx_t*g){
    static balls_t B;
    balls_init(&B);
    g->enable_bb();
    for(;;){
        balls_tick(&B);
        balls_draw(&B,g,0,0,(int)g->virt_w,(int)g->virt_h);
        g->present();
        mouse_draw_plain();
        mouse_update();
        int pressed=mouse_pressed();
        int mx,my,mb; mouse_get(&mx,&my,&mb); (void)mb;
        int vx=mx*(int)g->virt_w/(int)g->hw_w;
        int vy=my*(int)g->virt_h/(int)g->hw_h;
        int key=key_poll();
        key_modifiers_t m; key_get_modifiers(&m);
        if(pressed&MOUSE_B1) balls_click(&B,vx,vy);
        if(key!=KEY_NONE){
            if(balls_key(&B,key,m.ctrl)==BALLS_ACT_QUIT)break;
        }
        __asm__ volatile("pause");
    }
}
