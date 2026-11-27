/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
calc - APSys 00.01 */

#include "calc.h"
#include "key.h"
#include "mouse.h"

typedef unsigned char u8;

static int  c_slen(const char*s){ int n=0; while(s&&s[n])n++; return n; }
static void c_scopy(char*d,const char*s,int max){
    int i=0; while(s&&s[i]&&i<max-1){ d[i]=s[i]; i++; } d[i]=0;
}
static double d_abs(double v){ return v<0?-v:v; }
static double d_floor(double v){ long t=(long)v; double d=(double)t; if(d>v)d-=1.0; return d; }
static double d_sqrt(double v){
    if(v<=0.0) return 0.0;
    double x=v*0.5+0.5;
    for(int i=0;i<80;++i) x=(x+v/x)*0.5;
    return x;
}

static const char *BTN[20] = {
    "C","<-","/-","/",
    "7","8","9","!",
    "4","5","6","-",
    "1","2","3","+",
    "0",".","=","+/-"
};

static double parse_entry(const char*s){
    double v=0; int neg=0,i=0;
    if(s[i]=='-'){ neg=1; i++; }
    while(s[i]>='0'&&s[i]<='9'){ v=v*10.0+(s[i]-'0'); i++; }
    if(s[i]=='.'){ i++; double w=0.1;
        while(s[i]>='0'&&s[i]<='9'){ v+=(s[i]-'0')*w; w*=0.1; i++; } }
    return neg?-v:v;
}
static void fmt(double v,char*out,int max){
    int p=0;
    if(!(v==v) || d_abs(v)>=1e15){ c_scopy(out,"ERR",max); return; }
    if(v<0){ out[p++]='-'; v=-v; }
    double ip=d_floor(v);
    double fr=d_floor((v-ip)*1000000.0+0.5);
    if(fr>=1000000.0){ ip+=1.0; fr=0.0; }
    unsigned long i32=(unsigned long)ip;
    char tmp[16]; int n=0;
    if(i32==0) tmp[n++]='0';
    while(i32>0 && n<12){ tmp[n++]=(char)('0'+i32%10); i32/=10; }
    while(n>0 && p<max-1) out[p++]=tmp[--n];
    if(fr>0.5 && p<max-1){
        out[p++]='.';
        unsigned long f32=(unsigned long)fr;
        char fd[6];
        for(int i=5;i>=0;--i){ fd[i]=(char)('0'+f32%10); f32/=10; }
        int last=5; while(last>0 && fd[last]=='0') last--;
        for(int i=0;i<=last && p<max-1;++i) out[p++]=fd[i];
    }
    out[p]=0;
}
static double apply(double a,char op,double b,int*err){
    if(op=='+') return a+b;
    if(op=='-') return a-b;
    if(op=='!') return a*b;
    if(op=='/'){ if(b==0.0){ *err=1; return 0.0; } return a/b; }
    return b;
}

void calc_init(calc_t*K){
    c_scopy(K->disp,"0",24);
    K->has_entry=0; K->just_eq=0; K->err=0; K->pend=0; K->acc=0; K->sel=0;
}

void calc_press(calc_t*K,int idx){
    const char*L=BTN[idx];
    if(K->err){
        if(L[0]=='C'){ calc_init(K); return; }
        int isd=(L[0]>='0'&&L[0]<='9')||L[0]=='.';
        if(!isd) return;
        calc_init(K);
    }
    if(L[0]=='C'){ calc_init(K); return; }
    if(L[0]=='<'){
        int n=c_slen(K->disp);
        if(n>0){ K->disp[n-1]=0;
            if(n-1==0){ K->has_entry=0; c_scopy(K->disp,"0",24); } }
        return;
    }
    if(L[0]=='/'&&L[1]=='-'){
        double v=K->has_entry?parse_entry(K->disp):K->acc;
        if(v<0){ K->err=1; c_scopy(K->disp,"ERR",24); return; }
        fmt(d_sqrt(v),K->disp,24);
        K->has_entry=1; K->just_eq=1;
        return;
    }
    if(L[0]=='+'&&L[1]=='/'){
        if(K->has_entry){
            if(K->disp[0]=='-'){ for(int i=0;K->disp[i];++i)K->disp[i]=K->disp[i+1]; }
            else{ int n=c_slen(K->disp);
                if(n<20){ for(int i=n;i>=0;--i)K->disp[i+1]=K->disp[i]; K->disp[0]='-'; } }
        } else {
            K->acc=-K->acc; fmt(K->acc,K->disp,24);
            K->has_entry=1; K->just_eq=1;
        }
        return;
    }
    if(L[0]=='='){
        double v=K->has_entry?parse_entry(K->disp):K->acc;
        if(K->pend){
            double r=apply(K->acc,K->pend,v,&K->err);
            K->pend=0;
            if(K->err){ c_scopy(K->disp,"ERR",24); K->has_entry=0; return; }
            K->acc=r;
        } else K->acc=v;
        fmt(K->acc,K->disp,24);
        K->has_entry=0; K->just_eq=1;
        return;
    }
    if(L[0]=='+'||L[0]=='-'||L[0]=='!'||L[0]=='/'){
        double v=K->has_entry?parse_entry(K->disp):K->acc;
        if(K->pend&&K->has_entry){
            double r=apply(K->acc,K->pend,v,&K->err);
            if(K->err){ c_scopy(K->disp,"ERR",24); K->pend=0; K->has_entry=0; return; }
            K->acc=r;
        } else if(!K->pend) K->acc=v;
        K->pend=L[0]; K->has_entry=0; K->just_eq=0;
        fmt(K->acc,K->disp,24);
        return;
    }
    if(L[0]=='.'){
        if(!K->has_entry||K->just_eq){
            c_scopy(K->disp,"0.",24); K->has_entry=1; K->just_eq=0; return;
        }
        int dot=0; for(int i=0;K->disp[i];++i) if(K->disp[i]=='.') dot=1;
        if(!dot){ int n=c_slen(K->disp); if(n<20){ K->disp[n]='.'; K->disp[n+1]=0; } }
        return;
    }
    if(L[0]>='0'&&L[0]<='9'){
        if(K->just_eq){ K->acc=0; K->pend=0; K->just_eq=0; K->disp[0]=0; K->has_entry=1; }
        if(!K->has_entry){ K->disp[0]=0; K->has_entry=1; }
        int n=c_slen(K->disp);
        if(n==1&&K->disp[0]=='0'){ K->disp[0]=L[0]; K->disp[1]=0; return; }
        if(n<20){ K->disp[n]=L[0]; K->disp[n+1]=0; }
        return;
    }
}

int calc_key(calc_t*K,int key,int ctrl){
    if(ctrl){
        if(key=='l'||key=='L'||key=='q'||key=='Q') return CALC_ACT_QUIT;
        if(key=='c'||key=='C'){ calc_press(K,0);  return CALC_ACT_NONE; }
        if(key=='s'||key=='S'){ calc_press(K,2);  return CALC_ACT_NONE; }  /* корень */
        return CALC_ACT_NONE;
    }
    switch(key){
        case KEY_ESC:       return CALC_ACT_QUIT;
        case KEY_LEFT:      if(K->sel%4>0)K->sel--;  return CALC_ACT_NONE;
        case KEY_RIGHT:     if(K->sel%4<3)K->sel++;  return CALC_ACT_NONE;
        case KEY_UP:        if(K->sel>=4)K->sel-=4;  return CALC_ACT_NONE;
        case KEY_DOWN:      if(K->sel<16)K->sel+=4;  return CALC_ACT_NONE;
        case KEY_ENTER:     calc_press(K,K->sel);    return CALC_ACT_NONE;
        case KEY_BACKSPACE: calc_press(K,1);         return CALC_ACT_NONE;
        default: break;
    }
    if(key>=32&&key<=126){
        for(int i=0;i<20;++i)
            if(BTN[i][1]==0 && BTN[i][0]==(char)key){ K->sel=i; calc_press(K,i); return CALC_ACT_NONE; }
    }
    return CALC_ACT_NONE;
}

typedef struct { int gx,gy,bw,bh,m,dh; } cgeom_t;
static void calc_layout(int cw,int ch,cgeom_t*G){
    G->m=16; G->dh=44;
    G->bw=(cw-G->m*2)/4;
    G->bh=(ch-G->m*2-G->dh-8)/5;
    G->gx=G->m; G->gy=G->m+G->dh+8;
}
static void btn_rect(cgeom_t*G,int i,int*x,int*y,int*w,int*h){
    *x=G->gx+(i%4)*G->bw+2; *y=G->gy+(i/4)*G->bh+2;
    *w=G->bw-4; *h=G->bh-4;
}
static void outline(gui_gfx_t*g,int x,int y,int w,int h,u8 r,u8 gr,u8 b){
    g->rect(x,y,w,2,r,gr,b); g->rect(x,y+h-2,w,2,r,gr,b);
    g->rect(x,y,2,h,r,gr,b); g->rect(x+w-2,y,2,h,r,gr,b);
}
void calc_draw(calc_t*K,gui_gfx_t*g,int cx,int cy,int cw,int ch){
    g->rect(cx,cy,cw,ch,255,255,255);
    if(cw<220||ch<240) return;
    cgeom_t G; calc_layout(cw,ch,&G);
    int dx=cx+G.m, dy=cy+G.m, dw=cw-G.m*2, dh=G.dh;
    outline(g,dx,dy,dw,dh,60,60,60);
    int n=c_slen(K->disp);
    int tx=dx+dw-10-n*8;
    if(tx<dx+8) tx=dx+8;
    g->str(K->disp,tx,dy+(dh-16)/2,20,20,20);
    for(int i=0;i<20;++i){
        int x,y,w,h; btn_rect(&G,i,&x,&y,&w,&h); x+=cx; y+=cy;
        int lx=x+(w-c_slen(BTN[i])*8)/2, ly=y+(h-16)/2;
        if(i==K->sel){
            g->rect(x,y,w,h,60,60,60);
            g->str(BTN[i],lx,ly,255,255,255);
        } else {
            g->rect(x,y,w,h,255,255,255);
            outline(g,x,y,w,h,60,60,60);
            g->str(BTN[i],lx,ly,40,40,40);
        }
    }
}
int calc_hit(calc_t*K,int cx,int cy,int cw,int ch,int vx,int vy){
    (void)K;
    cgeom_t G; calc_layout(cw,ch,&G);
    for(int i=0;i<20;++i){
        int x,y,w,h; btn_rect(&G,i,&x,&y,&w,&h);
        x+=cx; y+=cy;
        if(vx>=x&&vx<x+w&&vy>=y&&vy<y+h) return i;
    }
    return -1;
}

void calc_run_cli(gui_gfx_t*g){
    calc_t K; calc_init(&K);
    int need=1;
    for(;;){
        if(need){
            calc_draw(&K,g,0,0,(int)g->virt_w,(int)g->virt_h);
            g->present();
            mouse_draw_plain();
            need=0;
        }
        mouse_update();
        int pressed=mouse_pressed();
        int mx,my,mb; mouse_get(&mx,&my,&mb); (void)mb;
        int vx=mx*(int)g->virt_w/(int)g->hw_w;
        int vy=my*(int)g->virt_h/(int)g->hw_h;
        int key=key_poll();
        key_modifiers_t m; key_get_modifiers(&m);
        if(!pressed && key==KEY_NONE){ __asm__ volatile("pause"); continue; }
        if(pressed){
            int idx=calc_hit(&K,0,0,(int)g->virt_w,(int)g->virt_h,vx,vy);
            if(idx>=0){ K.sel=idx; calc_press(&K,idx); need=1; }
        }
        if(key!=KEY_NONE){
            if(calc_key(&K,key,m.ctrl)==CALC_ACT_QUIT) break;
            need=1;
        }
    }
}
