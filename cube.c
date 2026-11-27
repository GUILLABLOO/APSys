/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
cube - APSys 00.01 */

#include "cube.h"
#include "key.h"

typedef unsigned char u8;
typedef unsigned int  u32;

static float my_fmodf(float x, float y) {
    while (x >= y) x -= y;
    while (x <= -y) x += y;
    return x;
}
static float my_sinf(float x) {
    const float PI = 3.14159265f, TWO_PI = 6.28318530f;
    x = my_fmodf(x, TWO_PI);
    if (x >  PI) x -= TWO_PI;
    if (x < -PI) x += TWO_PI;
    float x2 = x * x, t = x, sum = t;
    t *= -x2 / (2.0f * 3.0f); sum += t;
    t *= -x2 / (4.0f * 5.0f); sum += t;
    t *= -x2 / (6.0f * 7.0f); sum += t;
    t *= -x2 / (8.0f * 9.0f); sum += t;
    return sum;
}
static float my_cosf(float x) { return my_sinf(x + 1.57079633f); }

static void hue_rgb(int h, u8 *r, u8 *g, u8 *b) {
    h &= 255;
    int seg = (h * 6) >> 8;
    int f   = (h * 6) & 255;
    int q = 255 - f, t = f;
    switch (seg) {
        case 0: *r=255; *g=t;   *b=0;   break;
        case 1: *r=q;   *g=255; *b=0;   break;
        case 2: *r=0;   *g=255; *b=t;   break;
        case 3: *r=0;   *g=q;   *b=255; break;
        case 4: *r=t;   *g=0;   *b=255; break;
        default:*r=255; *g=0;   *b=q;   break;
    }
}

static void line_c(gui_gfx_t *g, int x0, int y0, int x1, int y1,
                   u8 r, u8 gr, u8 b,
                   int cx, int cy, int cw, int ch, int th)
{
    if (th < 1) th = 1;
    int half = th / 2;
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int guard = 8192;
    while (guard--) {
        if (x0 >= cx && x0 < cx + cw) {
            int yt = y0 - half;
            int yb = yt + th;
            if (yt < cy) yt = cy;
            if (yb > cy + ch) yb = cy + ch;
            if (yb > yt) g->rect(x0, yt, 1, yb - yt, r, gr, b);
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Кубик */
#define CUBE_SIZE 1.0f
#define FOV       256.0f
#define DISTANCE  4.0f

static float vertices[8][3] = {
    {-CUBE_SIZE,-CUBE_SIZE,-CUBE_SIZE}, { CUBE_SIZE,-CUBE_SIZE,-CUBE_SIZE},
    { CUBE_SIZE, CUBE_SIZE,-CUBE_SIZE}, {-CUBE_SIZE, CUBE_SIZE,-CUBE_SIZE},
    {-CUBE_SIZE,-CUBE_SIZE, CUBE_SIZE}, { CUBE_SIZE,-CUBE_SIZE, CUBE_SIZE},
    { CUBE_SIZE, CUBE_SIZE, CUBE_SIZE}, {-CUBE_SIZE, CUBE_SIZE, CUBE_SIZE}
};
static int faces[6][4] = {
    {0,1,2,3}, {5,4,7,6}, {4,0,3,7}, {1,5,6,2}, {3,2,6,7}, {4,5,1,0}
};
static u8 face_colors[6][3] = {
    {255,100,100}, {100,255,100}, {100,100,255},
    {255,255,100}, {255,100,255}, {100,255,255}
};

static void rotate_x(float *v, float a) {
    float c=my_cosf(a), s=my_sinf(a), y=v[1], z=v[2];
    v[1]=y*c-z*s; v[2]=y*s+z*c;
}
static void rotate_y(float *v, float a) {
    float c=my_cosf(a), s=my_sinf(a), x=v[0], z=v[2];
    v[0]=x*c+z*s; v[2]=-x*s+z*c;
}
static void rotate_z(float *v, float a) {
    float c=my_cosf(a), s=my_sinf(a), x=v[0], y=v[1];
    v[0]=x*c-y*s; v[1]=x*s+y*c;
}

static void fill_poly(gui_gfx_t *g, int *x, int *y, int n,
                      u8 r, u8 gr, u8 b, int cx, int cy, int cw, int ch)
{
    int min_y=y[0], max_y=y[0];
    for (int i=1;i<n;++i){ if(y[i]<min_y)min_y=y[i]; if(y[i]>max_y)max_y=y[i]; }
    if (min_y < cy) min_y = cy;
    if (max_y > cy+ch-1) max_y = cy+ch-1;
    for (int scan=min_y; scan<=max_y; ++scan) {
        int xs[8], cnt=0;
        for (int i=0;i<n;++i) {
            int j=(i+1)%n;
            if ((y[i]<=scan && y[j]>scan) || (y[j]<=scan && y[i]>scan)) {
                float t=(float)(scan-y[i])/(float)(y[j]-y[i]);
                if (cnt<8) xs[cnt++] = (int)(x[i]+t*(x[j]-x[i]));
            }
        }
        for (int i=0;i<cnt-1;++i)
            for (int j=i+1;j<cnt;++j)
                if (xs[i]>xs[j]) { int tmp=xs[i]; xs[i]=xs[j]; xs[j]=tmp; }
        for (int i=0;i<cnt-1;i+=2) {
            int a=xs[i], b2=xs[i+1];
            if (a>b2){ int tmp=a; a=b2; b2=tmp; }
            if (a<cx) a=cx;
            if (b2>cx+cw-1) b2=cx+cw-1;
            if (b2>=a) g->rect(a, scan, b2-a+1, 1, r, gr, b);
        }
    }
}

void cube_draw_window(gui_gfx_t *g, int cx, int cy, int cw, int ch, int *frame) {
    if (cw < 8 || ch < 8) return;
    g->rect(cx, cy, cw, ch, 8, 10, 14);

    float ax=(*frame)*0.05f, ay=(*frame)*0.07f, az=(*frame)*0.03f;
    float rot[8][3];
    for (int i=0;i<8;++i) {
        rot[i][0]=vertices[i][0]; rot[i][1]=vertices[i][1]; rot[i][2]=vertices[i][2];
        rotate_x(rot[i],ax); rotate_y(rot[i],ay); rotate_z(rot[i],az);
    }
    int ccx=cx+cw/2, ccy=cy+ch/2;
    int scale=(cw<ch?cw:ch)/3;

    float depth[6];
    int order[6]={0,1,2,3,4,5};
    for (int f=0;f<6;++f){
        float s=0; for(int v=0;v<4;++v) s+=rot[faces[f][v]][2];
        depth[f]=s*0.25f;
    }
    for (int i=0;i<5;++i)
        for (int j=i+1;j<6;++j)
            if (depth[order[i]]>depth[order[j]]) { int t=order[i]; order[i]=order[j]; order[j]=t; }

    for (int fi=0;fi<6;++fi) {
        int f=order[fi];
        int px[4], py[4];
        float az2=0;
        for (int v=0;v<4;++v) {
            float x=rot[faces[f][v]][0], y=rot[faces[f][v]][1];
            float z=rot[faces[f][v]][2]+DISTANCE;
            float sf=FOV/z;
            px[v]=ccx+(int)(x*scale*sf/FOV);
            py[v]=ccy+(int)(y*scale*sf/FOV);
            az2+=z;
        }
        az2*=0.25f;
        float light=1.0f-(az2-DISTANCE)*0.075f;
        if (light<0.3f) light=0.3f;
        if (light>1.0f) light=1.0f;
        u8 r=(u8)(face_colors[f][0]*light);
        u8 gr=(u8)(face_colors[f][1]*light);
        u8 b=(u8)(face_colors[f][2]*light);
        fill_poly(g,px,py,4,r,gr,b,cx,cy,cw,ch);
        for (int v=0;v<4;++v) {
            int nx=(v+1)%4;
            line_c(g,px[v],py[v],px[nx],py[nx],255,255,255,cx,cy,cw,ch,2);
        }
    }
    (*frame)++;
}

void cube_run_cli(gui_gfx_t *g) {
    int frame=0;
    for (;;) {
        cube_draw_window(g,0,0,(int)g->virt_w,(int)g->virt_h,&frame);
        g->present();
        int k=key_poll();
        key_modifiers_t m; key_get_modifiers(&m);
        if (k==KEY_ESC || (m.ctrl && (k=='l'||k=='L'))) return;
        for (int i=0;i<50000;++i) __asm__ volatile("pause");
    }
}

/* Тессеракт|CUBEF*/
static float vertices_4d[16][4] = {
    {-1,-1,-1,-1}, { 1,-1,-1,-1}, {-1, 1,-1,-1}, { 1, 1,-1,-1},
    {-1,-1, 1,-1}, { 1,-1, 1,-1}, {-1, 1, 1,-1}, { 1, 1, 1,-1},
    {-1,-1,-1, 1}, { 1,-1,-1, 1}, {-1, 1,-1, 1}, { 1, 1,-1, 1},
    {-1,-1, 1, 1}, { 1,-1, 1, 1}, {-1, 1, 1, 1}, { 1, 1, 1, 1}
};

static int edges_4d[32][2];
static int edges_ready = 0;
static void build_edges(void) {
    if (edges_ready) return;
    int n=0;
    for (int i=0;i<16;++i)
        for (int bit=0;bit<4;++bit) {
            int j = i ^ (1<<bit);
            if (j > i && n < 32) { edges_4d[n][0]=i; edges_4d[n][1]=j; n++; }
        }
    edges_ready = 1;
}

static void rot_plane(float *p, int a, int b, float ang) {
    float c=my_cosf(ang), s=my_sinf(ang);
    float pa=p[a], pb=p[b];
    p[a]=pa*c-pb*s;
    p[b]=pa*s+pb*c;
}

void cube_draw_4d_window(gui_gfx_t *g, int cx, int cy, int cw, int ch, int *frame) {
    if (cw < 8 || ch < 8) return;
    build_edges();
    g->rect(cx, cy, cw, ch, 8, 10, 14);

    int f = *frame;
    float a_xw = f * 0.020f;
    float a_yz = f * 0.013f;
    float a_xy = f * 0.006f;

    float v[16][4];
    for (int i=0;i<16;++i) {
        v[i][0]=vertices_4d[i][0]; v[i][1]=vertices_4d[i][1];
        v[i][2]=vertices_4d[i][2]; v[i][3]=vertices_4d[i][3];
        rot_plane(v[i],0,3,a_xw);
        rot_plane(v[i],1,2,a_yz);
        rot_plane(v[i],0,1,a_xy);
    }

    int m = (cw < ch ? cw : ch);
    float scale = (float)m / 4.6f;
    float ccx = cx + cw * 0.5f, ccy = cy + ch * 0.5f;

    int px[16], py[16];
    float near[16];
    for (int i=0;i<16;++i) {
        float k4 = 2.0f / (3.5f - v[i][3]);
        float x3 = v[i][0]*k4, y3 = v[i][1]*k4, z3 = v[i][2]*k4;
        float k3 = 3.5f / (3.5f + z3);
        px[i] = (int)(ccx + x3*k3*scale);
        py[i] = (int)(ccy + y3*k3*scale);
        near[i] = k4*k3;
    }

    for (int e=0;e<32;++e) {
        int a=edges_4d[e][0], b=edges_4d[e][1];
        u8 r,gr,bb;
        hue_rgb(e*8 + f*2, &r, &gr, &bb);
        float d = (near[a]+near[b])*0.5f;
        int th = (d > 1.15f) ? 3 : ((d > 0.75f) ? 2 : 1);
        line_c(g,px[a],py[a],px[b],py[b],r,gr,bb,cx,cy,cw,ch,th);
    }
    (*frame)++;
}

void cube_run_cli_4d(gui_gfx_t *g) {
    int frame=0;
    for (;;) {
        cube_draw_4d_window(g,0,0,(int)g->virt_w,(int)g->virt_h,&frame);
        g->present();
        int k=key_poll();
        key_modifiers_t m; key_get_modifiers(&m);
        if (k==KEY_ESC || (m.ctrl && (k=='l'||k=='L'))) return;
        for (int i=0;i<50000;++i) __asm__ volatile("pause");
    }
}
