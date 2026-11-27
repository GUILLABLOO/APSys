/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
wm - APSys 00.01 */

#include "wm.h"

typedef unsigned int u32;

#define COL_EDGE 60
#define COL_FILL 255
#define COL_TEXT 40

static wm_window_t wins[WM_MAX_WINDOWS];
static int  order[WM_MAX_WINDOWS];
static int  wcount = 0;
static int  next_id = 1;
static int  skip_id = -1;
static wm_content_cb content_cb = 0;

void wm_set_content_cb(wm_content_cb cb){ content_cb = cb; }
void wm_set_skip(int id){ skip_id = id; }

static int idx_of(int id){
    for(int i=0;i<WM_MAX_WINDOWS;++i) if(wins[i].alive && wins[i].id==id) return i;
    return -1;
}

static void geom(wm_window_t *W,int *ix,int *iy,int *iw,int *ih,int *ty,int *cx,int *cy){
    *ix=W->x+WM_BORDER; *iy=W->y+WM_BORDER;
    *iw=W->w-2*WM_BORDER;
    *ih=W->h-2*WM_BORDER-WM_TITLE_H;
    *ty=*iy+*ih+1;
    *cx=W->x+W->w-WM_BORDER-20;
    *cy=*ty+3;
}

int wm_create(const char *title,int x,int y,int w,int h,void *user){
    int s=-1;
    for(int i=0;i<WM_MAX_WINDOWS;++i) if(!wins[i].alive){ s=i; break; }
    if(s<0) return -1;
    wm_window_t *W=&wins[s];
    W->id=next_id++; W->x=x; W->y=y; W->w=w; W->h=h;
    W->visible=1; W->focused=0; W->alive=1; W->user=user;
    W->content=0;
    int p=0;
    while(title && title[p] && p<WM_TITLE_MAX-1){ W->title[p]=title[p]; p++; }
    W->title[p]=0;
    order[wcount++]=W->id;
    wm_focus(W->id);
    return W->id;
}

static void reorder_move_top(int id){
    int tmp[WM_MAX_WINDOWS]; int n=0;
    for(int k=0;k<wcount;++k) if(order[k]!=id) tmp[n++]=order[k];
    tmp[n++]=id;
    for(int k=0;k<wcount;++k) order[k]=tmp[k];
}

void wm_focus(int id){
    if(idx_of(id)<0) return;
    reorder_move_top(id);
    for(int i=0;i<WM_MAX_WINDOWS;++i) if(wins[i].alive) wins[i].focused=0;
    wins[idx_of(id)].focused=1;
}

void wm_destroy(int id){
    int i=idx_of(id); if(i<0) return;
    wins[i].alive=0;
    int tmp[WM_MAX_WINDOWS]; int n=0;
    for(int k=0;k<wcount;++k) if(order[k]!=id) tmp[n++]=order[k];
    for(int k=0;k<n;++k) order[k]=tmp[k];
    wcount=n;
}

void wm_toggle_min(int id){
    int i=idx_of(id); if(i<0) return;
    wins[i].visible=!wins[i].visible;
    if(wins[i].visible) wm_focus(id);
}

void wm_move_delta(int id,int dx,int dy){
    int i=idx_of(id); if(i<0) return;
    wm_window_t *W=&wins[i];
    W->x+=dx; W->y+=dy;
    if(W->y<0) W->y=0;
    if(W->x<-(W->w-40)) W->x=-(W->w-40);
}

void wm_resize_edges(int id,int edges,int dx,int dy){
    int i=idx_of(id); if(i<0) return;
    wm_window_t *W=&wins[i];
    if(edges & WM_EDGE_L){ W->x+=dx; W->w-=dx; }
    if(edges & WM_EDGE_R){ W->w+=dx; }
    if(edges & WM_EDGE_T){ W->y+=dy; W->h-=dy; }
    if(edges & WM_EDGE_B){ W->h+=dy; }
    const int minw=120, minh=90;
    if(W->w<minw){ if(edges & WM_EDGE_L) W->x-=(minw-W->w); W->w=minw; }
    if(W->h<minh){ if(edges & WM_EDGE_T) W->y-=(minh-W->h); W->h=minh; }
}

int wm_count(void){ return wcount; }
wm_window_t *wm_get(int k){
    if(k<0||k>=wcount) return 0;
    int i=idx_of(order[k]);
    return i<0?0:&wins[i];
}
wm_window_t *wm_find(int id){
    int i=idx_of(id);
    return i<0?0:&wins[i];
}
wm_window_t *wm_top(void){
    for(int k=wcount-1;k>=0;--k){
        wm_window_t *W=wm_get(k);
        if(W && W->visible) return W;
    }
    return 0;
}

void wm_client_rect(wm_window_t *W,int *x,int *y,int *w,int *h){
    int ix,iy,iw,ih,ty,cx,cy;
    geom(W,&ix,&iy,&iw,&ih,&ty,&cx,&cy);
    *x=ix; *y=iy; *w=iw; *h=ih;
}

int wm_hit(int vx,int vy,int *part){
    *part=0;
    for(int k=wcount-1;k>=0;--k){
        wm_window_t *W=wm_get(k);
        if(!W || !W->visible) continue;
        if(vx<W->x||vx>=W->x+W->w||vy<W->y||vy>=W->y+W->h) continue;
        *part=3; return W->id;
    }
    return -1;
}

int wm_hit_border(int vx,int vy,int *edges){
    *edges=0;
    for(int k=wcount-1;k>=0;--k){
        wm_window_t *W=wm_get(k);
        if(!W || !W->visible) continue;
        if(vx<W->x||vx>=W->x+W->w||vy<W->y||vy>=W->y+W->h) continue;
        const int gt=WM_BORDER+3;
        int l=(vx - W->x) < gt;
        int r=(W->x+W->w - vx) <= gt;
        int t=(vy - W->y) < gt;
        int b=(W->y+W->h - vy) <= gt;
        if(!l&&!r&&!t&&!b) return -1;
        int e=0;
        if(l) e|=WM_EDGE_L;
        if(r) e|=WM_EDGE_R;
        if(t) e|=WM_EDGE_T;
        if(b) e|=WM_EDGE_B;
        *edges=e;
        return W->id;
    }
    return -1;
}

void wm_draw(gui_gfx_t *g){
    for(int k=0;k<wcount;++k){
        wm_window_t *W=wm_get(k);
        if(!W || !W->visible) continue;
        if(W->id==skip_id) continue;

        int ix,iy,iw,ih,ty,cx,cy;
        geom(W,&ix,&iy,&iw,&ih,&ty,&cx,&cy);

        g->rect(W->x+5, W->y+5, W->w, W->h, 25, 25, 28);
        g->rect(W->x, W->y, W->w, W->h, COL_EDGE, COL_EDGE, COL_EDGE);
        g->rect(ix, iy, iw, ih + WM_TITLE_H, COL_FILL, COL_FILL, COL_FILL);

        g->rect(ix, iy, iw, ih, COL_FILL, COL_FILL, COL_FILL);
        if(W->content) W->content(g,W);
        else if(content_cb) content_cb(g,W);

        g->rect(ix, ty-1, iw, 1, COL_EDGE, COL_EDGE, COL_EDGE);
        if(W->focused){
            g->rect(ix, ty, iw, WM_TITLE_H-1, COL_EDGE, COL_EDGE, COL_EDGE);
            g->str(W->title, ix+6, ty+4, 255, 255, 255);
        } else {
            g->rect(ix, ty, iw, WM_TITLE_H-1, COL_FILL, COL_FILL, COL_FILL);
            g->str(W->title, ix+6, ty+4, COL_TEXT, COL_TEXT, COL_TEXT);
        }
    }
}

static void inv_px(gui_gfx_t *g,int x,int y){
    if(x<0||y<0) return;
    if(x>=(int)g->hw_w||y>=(int)g->hw_h) return;
    u32 off=(u32)y*g->pitch + (u32)x*(g->bpp/8);
    g->vram[off+0]=(gu8)(255-g->vram[off+0]);
    g->vram[off+1]=(gu8)(255-g->vram[off+1]);
    g->vram[off+2]=(gu8)(255-g->vram[off+2]);
}
static void inv_outline(gui_gfx_t *g,int x,int y,int w,int h,int t){
    for(int i=0;i<t;++i){
        for(int cx=x;cx<x+w;++cx){ inv_px(g,cx,y+i); inv_px(g,cx,y+h-1-i); }
        for(int cy=y;cy<y+h;++cy){ inv_px(g,x+i,cy); inv_px(g,x+w-1-i,cy); }
    }
}
void wm_rubber(gui_gfx_t *g, wm_window_t *W){
    inv_outline(g, W->x,   W->y,   W->w, W->h, WM_BORDER);
    inv_outline(g, W->x+5, W->y+5, W->w, W->h, WM_BORDER);
}
