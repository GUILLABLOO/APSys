/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
chess - APSys 00.01 */

#include "chess.h"
#include "key.h"
#include "mouse.h"

typedef unsigned char u8;

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
static float f_sqrt(float v){
    if(v<=0.0f) return 0.0f;
    float x=v*0.5f+0.5f;
    for(int i=0;i<30;++i) x=(x+v/x)*0.5f;
    return x;
}
static void vnorm(float*a){
    float l=f_sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
    if(l>1e-6f){ a[0]/=l; a[1]/=l; a[2]/=l; }
}
static float vdot(const float*a,const float*b){ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static void vcross(const float*a,const float*b,float*out){
    out[0]=a[1]*b[2]-a[2]*b[1];
    out[1]=a[2]*b[0]-a[0]*b[2];
    out[2]=a[0]*b[1]-a[1]*b[0];
}
static void cpy_msg(chess_t*C,const char*s){
    int i=0; while(s[i]&&i<47){ C->msg[i]=s[i]; i++; } C->msg[i]=0;
}

#define PT(p) ((p)&7)
#define PC(p) (((p)>>3)&1)
static u8 mk(int t,int c){ return (u8)(t|(c<<3)); }

void chess_init(chess_t *C){
    for(int i=0;i<64;++i) C->board[i]=0;
    static const u8 back[8]={4,2,3,5,6,3,2,4};
    for(int c=0;c<8;++c){
        C->board[c]=mk(back[c],0);
        C->board[8+c]=mk(1,0);
        C->board[48+c]=mk(1,1);
        C->board[56+c]=mk(back[c],1);
    }
    C->turn=0; C->sel=-1; C->cursor=12; C->n_mv=0; C->over=0;
    C->yaw=0.55f; C->dist=11.0f; C->height=6.5f;
    cpy_msg(C,"WHITE TO MOVE");
}

static int sq_attacked(const u8*b,int sq,int by){
    int r=sq/8, c=sq%8;
    int dr=(by==0)?-1:1;
    for(int k=0;k<2;++k){
        int rr=r+dr, cc=c+(k?1:-1);
        if(rr>=0&&rr<8&&cc>=0&&cc<8&&b[rr*8+cc]==mk(1,by)) return 1;
    }
    static const int kn[8][2]={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
    for(int i=0;i<8;++i){
        int rr=r+kn[i][0], cc=c+kn[i][1];
        if(rr>=0&&rr<8&&cc>=0&&cc<8&&b[rr*8+cc]==mk(2,by)) return 1;
    }
    for(int a=-1;a<=1;++a)for(int d=-1;d<=1;++d){
        if(!a&&!d) continue;
        int rr=r+a, cc=c+d;
        if(rr>=0&&rr<8&&cc>=0&&cc<8&&b[rr*8+cc]==mk(6,by)) return 1;
    }
    static const int rd[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    for(int i=0;i<4;++i){
        int rr=r, cc=c;
        for(;;){
            rr+=rd[i][0]; cc+=rd[i][1];
            if(rr<0||rr>7||cc<0||cc>7) break;
            u8 p=b[rr*8+cc];
            if(p){ if(PC(p)==by&&(PT(p)==4||PT(p)==5)) return 1; break; }
        }
    }
    static const int bd[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
    for(int i=0;i<4;++i){
        int rr=r, cc=c;
        for(;;){
            rr+=bd[i][0]; cc+=bd[i][1];
            if(rr<0||rr>7||cc<0||cc>7) break;
            u8 p=b[rr*8+cc];
            if(p){ if(PC(p)==by&&(PT(p)==3||PT(p)==5)) return 1; break; }
        }
    }
    return 0;
}
static int king_sq(const u8*b,int col){
    for(int i=0;i<64;++i) if(b[i]==mk(6,col)) return i;
    return -1;
}
static int in_check(const u8*b,int col){
    int k=king_sq(b,col);
    if(k<0) return 0;
    return sq_attacked(b,k,col^1);
}
static void pseudo(const u8*b,int from,int*tos,int*n){
    u8 p=b[from]; int t=PT(p), col=PC(p);
    int r=from/8, c=from%8;
    if(t==1){
        int dir=col?-1:1, sr=col?6:1;
        int r1=r+dir;
        if(r1>=0&&r1<8&&!b[r1*8+c]) tos[(*n)++]=r1*8+c;
        int r2=r+2*dir;
        if(r==sr&&r2>=0&&r2<8&&!b[r1*8+c]&&!b[r2*8+c]) tos[(*n)++]=r2*8+c;
        for(int k=0;k<2;++k){
            int cc=c+(k?1:-1);
            if(r1>=0&&r1<8&&cc>=0&&cc<8){
                u8 q=b[r1*8+cc];
                if(q&&PC(q)!=col) tos[(*n)++]=r1*8+cc;
            }
        }
        return;
    }
    if(t==2){
        static const int kn[8][2]={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
        for(int i=0;i<8;++i){
            int rr=r+kn[i][0], cc=c+kn[i][1];
            if(rr<0||rr>7||cc<0||cc>7) continue;
            u8 q=b[rr*8+cc];
            if(!q||PC(q)!=col) tos[(*n)++]=rr*8+cc;
        }
        return;
    }
    if(t==6){
        for(int a=-1;a<=1;++a)for(int d=-1;d<=1;++d){
            if(!a&&!d) continue;
            int rr=r+a, cc=c+d;
            if(rr<0||rr>7||cc<0||cc>7) continue;
            u8 q=b[rr*8+cc];
            if(!q||PC(q)!=col) tos[(*n)++]=rr*8+cc;
        }
        return;
    }
    for(int d=0;d<8;++d){
        int dr=(d<4)?((d==0)?1:((d==1)?-1:0)):((d==4)?1:((d==5)?1:((d==6)?-1:-1)));
        int dc=(d<4)?((d==0)?0:((d==1)?0:((d==2)?1:-1))):((d==4)?1:((d==5)?-1:((d==6)?1:-1)));
        if(d<4){ if(t==3) continue; } else { if(t==4) continue; }
        int rr=r, cc=c;
        for(;;){
            rr+=dr; cc+=dc;
            if(rr<0||rr>7||cc<0||cc>7) break;
            u8 q=b[rr*8+cc];
            if(!q){ tos[(*n)++]=rr*8+cc; continue; }
            if(PC(q)!=col) tos[(*n)++]=rr*8+cc;
            break;
        }
    }
}
static int legal_from(chess_t*C,int from,int*tos){
    u8 p=C->board[from];
    int col=PC(p), raw[32], nr=0;
    pseudo(C->board,from,raw,&nr);
    int n=0;
    for(int i=0;i<nr;++i){
        u8 target=C->board[raw[i]];
        if(target&&PT(target)==6) continue;
        u8 tmp[64];
        for(int k=0;k<64;++k) tmp[k]=C->board[k];
        tmp[raw[i]]=tmp[from]; tmp[from]=0;
        if(!in_check(tmp,col)) tos[n++]=raw[i];
    }
    return n;
}
static int any_legal(chess_t*C,int col){
    for(int sq=0;sq<64;++sq){
        u8 p=C->board[sq];
        if(!p||PC(p)!=col) continue;
        int tos[32];
        if(legal_from(C,sq,tos)>0) return 1;
    }
    return 0;
}
static int only_kings(const u8*b){
    for(int i=0;i<64;++i){ u8 p=b[i]; if(p&&PT(p)!=6) return 0; }
    return 1;
}
static void build_moves(chess_t*C,int from){
    C->n_mv=legal_from(C,from,C->mv_to);
}
static int in_mv(chess_t*C,int sq){
    for(int i=0;i<C->n_mv;++i) if(C->mv_to[i]==sq) return 1;
    return 0;
}
static void do_move(chess_t*C,int from,int to){
    u8 p=C->board[from];
    int col=PC(p);
    C->board[to]=p; C->board[from]=0;
    int tr=to/8;
    int promo=0;
    if(PT(p)==1&&tr==(col?0:7)){ C->board[to]=mk(5,col); promo=1; }
    C->turn^=1;
    C->sel=-1; C->n_mv=0;
    int chk=in_check(C->board,C->turn);
    C->over=0;
    if(only_kings(C->board)) C->over=3;
    if(!any_legal(C,C->turn)) C->over = chk ? (C->turn?1:2) : 3;
    if(C->over==1) cpy_msg(C,"CHECKMATE - WHITE WINS - ENTER RESTART");
    else if(C->over==2) cpy_msg(C,"CHECKMATE - BLACK WINS - ENTER RESTART");
    else if(C->over==3) cpy_msg(C,"DRAW - NO WINNERS - ENTER RESTART");
    else if(chk) cpy_msg(C,"CHECK! KING IS UNDER ATTACK");
    else if(promo) cpy_msg(C,"PROMOTED TO QUEEN");
    else cpy_msg(C, C->turn?"BLACK TO MOVE":"WHITE TO MOVE");
}
static void activate(chess_t*C,int sq){
    if(C->over){ chess_init(C); return; }
    if(C->sel<0){
        u8 p=C->board[sq];
        if(p&&PC(p)==C->turn){ C->sel=sq; build_moves(C,sq); cpy_msg(C,"PIECE SELECTED - ENTER TO MOVE"); }
        else cpy_msg(C,"NOT YOUR PIECE");
    } else if(sq==C->sel){
        C->sel=-1; C->n_mv=0; cpy_msg(C,"CANCELLED");
    } else if(in_mv(C,sq)){
        do_move(C,C->sel,sq);
    } else {
        u8 p=C->board[sq];
        if(p&&PC(p)==C->turn){ C->sel=sq; build_moves(C,sq); cpy_msg(C,"PIECE SELECTED"); }
        else cpy_msg(C,"ILLEGAL MOVE");
    }
}

static float camP[3],camR[3],camU[3],camF[3];
static int scx,scy; static float sfocal;
typedef struct { int x[4],y[4]; u8 r,g,b; float depth; } poly_t;
static poly_t polys[900];
static int npoly;

static int proj(const float*p,int*ox,int*oy,float*od){
    float d[3]={p[0]-camP[0],p[1]-camP[1],p[2]-camP[2]};
    float zc=vdot(d,camF);
    if(zc<0.6f) return 0;
    float xc=vdot(d,camR), yc=vdot(d,camU);
    *ox=scx+(int)(xc*sfocal/zc);
    *oy=scy-(int)(yc*sfocal/zc);
    *od=zc;
    return 1;
}
static void add_quad(float*p0,float*p1,float*p2,float*p3,u8 r,u8 g,u8 b){
    if(npoly>=900) return;
    float e1[3]={p1[0]-p0[0],p1[1]-p0[1],p1[2]-p0[2]};
    float e2[3]={p3[0]-p0[0],p3[1]-p0[1],p3[2]-p0[2]};
    float n[3]; vcross(e1,e2,n); vnorm(n);
    float L[3]={0.35f,0.80f,0.45f};
    float d=vdot(n,L); if(d<0)d=-d;
    float sh=0.45f+0.55f*d;
    poly_t*P=&polys[npoly];
    float*pts[4]={p0,p1,p2,p3};
    float dep=0;
    for(int i=0;i<4;++i){
        float dd;
        if(!proj(pts[i],&P->x[i],&P->y[i],&dd)) return;
        dep+=dd;
    }
    P->r=(u8)(r*sh); P->g=(u8)(g*sh); P->b=(u8)(b*sh);
    P->depth=dep*0.25f;
    npoly++;
}
static void add_box_y(float cxw,float czw,float y0,float hw,float h,u8 r,u8 g,u8 b){
    float x0=cxw-hw,x1=cxw+hw,z0=czw-hw,z1=czw+hw,y1=y0+h;
    float t0[3]={x0,y1,z0},t1[3]={x1,y1,z0},t2[3]={x1,y1,z1},t3[3]={x0,y1,z1};
    add_quad(t0,t1,t2,t3,r,g,b);
    float a0[3]={x0,y0,z0},a1[3]={x1,y0,z0},a2[3]={x1,y0,z1},a3[3]={x0,y0,z1};
    add_quad(a0,a1,t1,t0,r,g,b);
    add_quad(a2,a3,t3,t2,r,g,b);
    add_quad(a3,a0,t0,t3,r,g,b);
    add_quad(a1,a2,t2,t1,r,g,b);
}
static void fill_poly(gui_gfx_t*g,const poly_t*P,int cx,int cy,int cw,int chh){
    int miny=P->y[0],maxy=P->y[0];
    for(int i=1;i<4;++i){ if(P->y[i]<miny)miny=P->y[i]; if(P->y[i]>maxy)maxy=P->y[i]; }
    if(miny<cy)miny=cy;
    if(maxy>cy+chh-1)maxy=cy+chh-1;
    for(int sy=miny;sy<=maxy;++sy){
        int xs[8],cn=0;
        for(int i=0;i<4;++i){
            int j=(i+1)&3;
            if((P->y[i]<=sy&&P->y[j]>sy)||(P->y[j]<=sy&&P->y[i]>sy)){
                float t=(float)(sy-P->y[i])/(float)(P->y[j]-P->y[i]);
                if(cn<8) xs[cn++]=(int)((float)P->x[i]+t*(float)(P->x[j]-P->x[i]));
            }
        }
        for(int i=0;i<cn-1;++i)
            for(int j=i+1;j<cn;++j)
                if(xs[i]>xs[j]){ int t=xs[i]; xs[i]=xs[j]; xs[j]=t; }
        for(int i=0;i+1<cn;i+=2){
            int a=xs[i], b2=xs[i+1];
            if(a>b2){ int t=a; a=b2; b2=t; }
            if(a<cx)a=cx;
            if(b2>cx+cw-1)b2=cx+cw-1;
            if(b2>=a) g->rect(a,sy,b2-a+1,1,P->r,P->g,P->b);
        }
    }
}

/*Фигурочки */
static void piece_model(int t,float cxw,float czw,u8 pr,u8 pg,u8 pb){
    u8 dr=(u8)(pr*80/100), dg=(u8)(pg*80/100), db=(u8)(pb*80/100);
    switch(t){
        case 1: /* Пешка */
            add_box_y(cxw,czw,0.00f,0.30f,0.10f,dr,dg,db);
            add_box_y(cxw,czw,0.10f,0.18f,0.34f,pr,pg,pb);
            add_box_y(cxw,czw,0.44f,0.12f,0.14f,pr,pg,pb);
            break;
        case 2: /* Конь */
            add_box_y(cxw,czw,0.00f,0.32f,0.10f,dr,dg,db);
            add_box_y(cxw,czw,0.10f,0.20f,0.40f,pr,pg,pb);
            add_box_y(cxw+0.10f,czw,0.50f,0.14f,0.20f,pr,pg,pb);
            break;
        case 3: /* Слоняра */
            add_box_y(cxw,czw,0.00f,0.30f,0.10f,dr,dg,db);
            add_box_y(cxw,czw,0.10f,0.18f,0.48f,pr,pg,pb);
            add_box_y(cxw,czw,0.58f,0.13f,0.16f,pr,pg,pb);
            add_box_y(cxw,czw,0.74f,0.05f,0.10f,pr,pg,pb);
            break;
        case 4: /* Ладья */
            add_box_y(cxw,czw,0.00f,0.32f,0.10f,dr,dg,db);
            add_box_y(cxw,czw,0.10f,0.22f,0.42f,pr,pg,pb);
            add_box_y(cxw,czw,0.52f,0.28f,0.14f,pr,pg,pb);
            break;
        case 5: /* Ферзь */
            add_box_y(cxw,czw,0.00f,0.34f,0.10f,dr,dg,db);
            add_box_y(cxw,czw,0.10f,0.22f,0.56f,pr,pg,pb);
            add_box_y(cxw,czw,0.66f,0.28f,0.10f,pr,pg,pb);
            add_box_y(cxw,czw,0.76f,0.07f,0.12f,pr,pg,pb);
            break;
        default: /* Король */
            add_box_y(cxw,czw,0.00f,0.34f,0.10f,dr,dg,db);
            add_box_y(cxw,czw,0.10f,0.22f,0.64f,pr,pg,pb);
            add_box_y(cxw,czw,0.74f,0.26f,0.10f,pr,pg,pb);
            add_box_y(cxw,czw,0.84f,0.05f,0.22f,pr,pg,pb);
            add_box_y(cxw,czw,0.90f,0.14f,0.06f,pr,pg,pb);
            break;
    }
}

void chess_draw(chess_t*C,gui_gfx_t*g,int cx,int cy,int cw,int chh){
    g->rect(cx,cy,cw,chh,18,20,28);
    if(cw<140||chh<120) return;
    camP[0]=f_sin(C->yaw)*C->dist;
    camP[1]=C->height;
    camP[2]=f_cos(C->yaw)*C->dist;
    float tgt[3]={0,0.4f,0};
    camF[0]=tgt[0]-camP[0]; camF[1]=tgt[1]-camP[1]; camF[2]=tgt[2]-camP[2];
    vnorm(camF);
    float wup[3]={0,1,0};
    vcross(camF,wup,camR); vnorm(camR);
    vcross(camR,camF,camU); vnorm(camU);
    scx=cx+cw/2; scy=cy+chh/2;
    sfocal=(cw<chh?cw:chh)*1.15f;
    npoly=0;

    {
        float y=-0.02f;
        float f0[3]={-4.7f,y,4.0f},f1[3]={4.7f,y,4.0f},f2[3]={4.7f,y,4.7f},f3[3]={-4.7f,y,4.7f};
        add_quad(f0,f1,f2,f3,70,52,38);
        float b0[3]={-4.7f,y,-4.7f},b1[3]={4.7f,y,-4.7f},b2[3]={4.7f,y,-4.0f},b3[3]={-4.7f,y,-4.0f};
        add_quad(b0,b1,b2,b3,70,52,38);
        float l0[3]={-4.7f,y,-4.0f},l1[3]={-4.0f,y,-4.0f},l2[3]={-4.0f,y,4.0f},l3[3]={-4.7f,y,4.0f};
        add_quad(l0,l1,l2,l3,70,52,38);
        float r0[3]={4.0f,y,-4.0f},r1[3]={4.7f,y,-4.0f},r2[3]={4.7f,y,4.0f},r3[3]={4.0f,y,4.0f};
        add_quad(r0,r1,r2,r3,70,52,38);
    }

    for(int r=0;r<8;++r)for(int c=0;c<8;++c){
        int sq=r*8+c;
        float x0=c-4.0f,x1=c-3.0f,z0=r-4.0f,z1=r-3.0f;
        float q[4][3]={{x0,0,z0},{x1,0,z0},{x1,0,z1},{x0,0,z1}};
        for(int i=0;i<4;++i){
            float dd; int px,py;
            if(proj(q[i],&px,&py,&dd)){ C->sqx[sq][i]=px; C->sqy[sq][i]=py; }
            else { C->sqx[sq][i]=-9999; C->sqy[sq][i]=-9999; }
        }
        u8 br,bg,bb; float y=0.0f;
        if(sq==C->sel){ br=90; bg=200; bb=90; y=0.03f; }
        else if(in_mv(C,sq)){ br=225; bg=200; bb=80; y=0.03f; }
        else if((r+c)&1){ br=125; bg=92; bb=60; }
        else { br=235; bg=228; bb=210; }
        float p0[3]={x0,y,z0},p1[3]={x1,y,z0},p2[3]={x1,y,z1},p3[3]={x0,y,z1};
        add_quad(p0,p1,p2,p3,br,bg,bb);
        if(sq==C->cursor){
            float m=0.14f, y2=0.06f;
            float c0[3]={x0+m,y2,z0+m},c1[3]={x1-m,y2,z0+m},c2[3]={x1-m,y2,z1-m},c3[3]={x0+m,y2,z1-m};
            add_quad(c0,c1,c2,c3,230,80,80);
        }
    }

    /* фигуры */
    for(int sq=0;sq<64;++sq){
        u8 p=C->board[sq];
        if(!p) continue;
        int r=sq/8, c=sq%8;
        float cxw=c-3.5f, czw=r-3.5f;
        u8 pr,pg,pb;
        if(PC(p)==0){ pr=235; pg=232; pb=220; } else { pr=48; pg=48; pb=56; }
        piece_model(PT(p),cxw,czw,pr,pg,pb);
    }

    for(int i=1;i<npoly;++i){
        poly_t t=polys[i];
        int j=i-1;
        while(j>=0&&polys[j].depth<t.depth){ polys[j+1]=polys[j]; j--; }
        polys[j+1]=t;
    }
    for(int i=0;i<npoly;++i) fill_poly(g,&polys[i],cx,cy,cw,chh);

    for(int c=0;c<8;++c){
        float pf[3]={c-3.5f,0.02f,4.35f}, pb2[3]={c-3.5f,0.02f,-4.35f};
        int sx,sy; float dd;
        if(proj(pf,&sx,&sy,&dd)) g->ch((u8)('A'+c),sx-4,sy-8,230,220,200);
        if(proj(pb2,&sx,&sy,&dd)) g->ch((u8)('A'+c),sx-4,sy-8,230,220,200);
    }
    for(int r=0;r<8;++r){
        float pl[3]={-4.35f,0.02f,r-3.5f}, pr2[3]={4.35f,0.02f,r-3.5f};
        int sx,sy; float dd;
        if(proj(pl,&sx,&sy,&dd)) g->ch((u8)('1'+r),sx-4,sy-8,230,220,200);
        if(proj(pr2,&sx,&sy,&dd)) g->ch((u8)('1'+r),sx-4,sy-8,230,220,200);
    }

    char hud[64]; int p=0;
    const char*h1="CHESS  TURN: ";
    while(*h1&&p<60) hud[p++]=*h1++;
    const char*h2=C->turn?"BLACK":"WHITE";
    while(*h2&&p<60) hud[p++]=*h2++;
    hud[p]=0;
    g->str(hud,cx+6,cy+4,255,255,255);
    g->str(C->msg,cx+6,cy+20,255,220,90);
    g->str("WASD CAMERA  E/Q UP-DOWN  ARROWS PICK  ENTER OK  1 CANCEL  LMB CLICK  ESC EXIT",
           cx+6,cy+chh-16,150,150,170);
}

int chess_key(chess_t*C,int key,int ctrl){
    if(key==KEY_ESC||(ctrl&&(key=='l'||key=='L'))) return 1;
    if(ctrl) return 0;
    switch(key){
        case 'w': case 'W': C->dist-=0.7f; if(C->dist<5.0f)C->dist=5.0f; return 0;
        case 's': case 'S': C->dist+=0.7f; if(C->dist>22.0f)C->dist=22.0f; return 0;
        case 'a': case 'A': C->yaw-=0.12f; return 0;
        case 'd': case 'D': C->yaw+=0.12f; return 0;
        case 'e': case 'E': C->height+=0.6f;
            if(C->height>14.0f)C->height=14.0f;
            if(C->height>C->dist-1.0f)C->height=C->dist-1.0f;
            return 0;
        case 'q': case 'Q': C->height-=0.6f;
            if(C->height<1.5f)C->height=1.5f;
            return 0;
        case '1': C->sel=-1; C->n_mv=0; cpy_msg(C,"CANCELLED"); return 0;
        case KEY_LEFT:  if(C->cursor%8>0)C->cursor--; return 0;
        case KEY_RIGHT: if(C->cursor%8<7)C->cursor++; return 0;
        case KEY_UP:    if(C->cursor>=8)C->cursor-=8; return 0;
        case KEY_DOWN:  if(C->cursor<56)C->cursor+=8; return 0;
        case KEY_ENTER: activate(C,C->cursor); return 0;
    }
    return 0;
}
static int point_in_sq(chess_t*C,int sq,int vx,int vy){
    const int*x=C->sqx[sq]; const int*y=C->sqy[sq];
    for(int i=0;i<4;++i) if(x[i]==-9999) return 0;
    int sign=0;
    for(int i=0;i<4;++i){
        int j=(i+1)&3;
        int cr=(x[j]-x[i])*(vy-y[i])-(y[j]-y[i])*(vx-x[i]);
        int s=cr>0?1:(cr<0?-1:0);
        if(s){ if(!sign) sign=s; else if(s!=sign) return 0; }
    }
    return 1;
}
void chess_click(chess_t*C,int vx,int vy){
    for(int sq=0;sq<64;++sq){
        if(point_in_sq(C,sq,vx,vy)){ C->cursor=sq; activate(C,sq); return; }
    }
}

void chess_run_cli(gui_gfx_t*g){
    static chess_t C;
    chess_init(&C);
    int need=1;
    for(;;){
        if(need){
            chess_draw(&C,g,0,0,(int)g->virt_w,(int)g->virt_h);
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
        if(!pressed&&key==KEY_NONE){ __asm__ volatile("pause"); continue; }
        if(pressed&MOUSE_B1){ chess_click(&C,vx,vy); need=1; }
        if(key!=KEY_NONE){
            if(chess_key(&C,key,m.ctrl)) break;
            need=1;
        }
    }
}
