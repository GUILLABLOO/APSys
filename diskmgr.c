/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
diskmgr - APSys 00.01 */

#include "diskmgr.h"
#include "satasys.h"
#include "fss.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

static volume_t vols[DISKMGR_MAX_VOLUMES];
static int vol_count=0;
static int inited=0;

static u8 sec[512];
static u8 sb[1024];

static u32 le16(const u8*p){ return (u32)p[0]|((u32)p[1]<<8); }
static u32 le32(const u8*p){ return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24); }
static u64 le64(const u8*p){ return (u64)le32(p) | ((u64)le32(p+4)<<32); }

static void add_volume(int disk,int part,u64 start,u64 sectors,u8 type){
    if(vol_count>=DISKMGR_MAX_VOLUMES) return;
    if(sectors==0) return;
    for(int i=0;i<vol_count;++i)
        if(vols[i].disk==disk && vols[i].start==start && vols[i].sectors==sectors) return;
    volume_t *V=&vols[vol_count++];
    V->disk=disk; V->part=part; V->start=start; V->sectors=sectors; V->type=type;
    V->fs=fss_detect_disk(disk,start);
}

static int looks_fat(const u8 *b){
    if(b[510]!=0x55 || b[511]!=0xAA) return 0;
    if(b[0]!=0xEB && b[0]!=0xE9) return 0;
    u32 bps=le16(b+11);
    if(bps!=512&&bps!=1024&&bps!=2048&&bps!=4096) return 0;
    u8 spc=b[13];
    if(spc==0 || (spc&(spc-1))) return 0;
    if(le16(b+14)==0) return 0;
    u8 nf=b[16]; if(nf==0||nf>4) return 0;
    if(le16(b+22)==0 && le32(b+36)==0) return 0;
    return 1;
}
static int looks_ext2(int disk,u64 start){
    if(!satasys_read(disk,start+2,2,sb)) return 0;
    return le16(sb+56)==0xEF53;
}

static void scan_gpt(int disk){
    if(!satasys_read(disk,1,2,sb)) return;
    static const char sig[8]={'E','F','I',' ','P','A','R','T'};
    for(int i=0;i<8;++i) if(sb[i]!=sig[i]) return;
    u64 ent_lba=le64(sb+72);
    u32 ent_num=le32(sb+80);
    u32 ent_sz =le32(sb+84);
    if(ent_sz<128 || ent_sz>512 || ent_num==0) return;
    if(ent_num>128) ent_num=128;

    u64 last_lba=0xFFFFFFFFFFFFFFFFULL;
    for(u32 i=0;i<ent_num && vol_count<DISKMGR_MAX_VOLUMES;++i){
        u64 byte=(u64)i*ent_sz;
        u64 lba=ent_lba + byte/512;
        u32 off=(u32)(byte%512);
        if(lba!=last_lba){
            if(!satasys_read(disk,lba,1,sec)) return;
            last_lba=lba;
        }
        const u8 *e=sec+off;
        int empty=1;
        for(int k=0;k<16;++k) if(e[k]){ empty=0; break; }
        if(empty) continue;
        u64 first=le64(e+32), last=le64(e+40);
        if(last<first) continue;
        add_volume(disk,(int)(i+1),first,last-first+1,0xEE);
    }
}

static void scan_ebr(int disk,u64 ext_base,int *partno){
    u64 cur=ext_base;
    for(int guard=0;guard<64;++guard){
        if(!satasys_read(disk,cur,1,sec)) return;
        if(sec[510]!=0x55||sec[511]!=0xAA) return;

        const u8 *e=sec+446;
        u8  t=e[4];
        u64 s=le32(e+8), n=le32(e+12);
        if(t!=0 && n>0 && *partno<=32)
            add_volume(disk,(*partno)++,cur+s,n,t);

        const u8 *nx=sec+462;
        u8  t2=nx[4];
        u64 s2=le32(nx+8), n2=le32(nx+12);
        if(t2==0||n2==0) return;
        u64 next=ext_base+s2;
        if(next==cur) return;
        cur=next;
    }
}

void diskmgr_ensure(void){
    if(inited) return;
    inited=1;
    vol_count=0;
    satasys_init();

    for(int d=0;d<satasys_count();++d){
        const sata_port_t *P=satasys_get(d);
        if(!P||!P->sectors) continue;

        if(!satasys_read(d,0,1,sec)){
            add_volume(d,0,0,P->sectors,0x00);
            continue;
        }

        if(looks_fat(sec)){ add_volume(d,0,0,P->sectors,0x00); continue; }
        if(looks_ext2(d,0)){ add_volume(d,0,0,P->sectors,0x00); continue; }
        if(sec[510]!=0x55 || sec[511]!=0xAA){
            add_volume(d,0,0,P->sectors,0x00);
            continue;
        }

        int gpt=0, nonempty=0;
        for(int i=0;i<4;++i){
            u8 t=sec[446+i*16+4];
            if(t==0xEE) gpt=1;
            if(t!=0x00) nonempty=1;
        }
        if(gpt){ scan_gpt(d); continue; }
        if(!nonempty){ add_volume(d,0,0,P->sectors,0x00); continue; }

        int partno=5;
        u64 ext_base=0;
        for(int i=0;i<4;++i){
            const u8 *e=sec+446+i*16;
            u8  t=e[4];
            u64 s=le32(e+8), n=le32(e+12);
            if(t==0x00||n==0) continue;
            if(t==0x05||t==0x0F||t==0x85){
                if(!ext_base) ext_base=s;
                continue;
            }
            add_volume(d,i+1,s,n,t);
        }
        if(ext_base) scan_ebr(d,ext_base,&partno);
    }
}

int diskmgr_volume_count(void){ return vol_count; }
const volume_t *diskmgr_volume(int i){ return (i>=0&&i<vol_count)?&vols[i]:0; }

int diskmgr_vol_geom(int vol, int *port, unsigned long *lba, unsigned long *sectors) {
    const volume_t *V = diskmgr_volume(vol);
    if (!V) return -1;
    *port    = V->disk;
    *lba     = (unsigned long)V->start;
    *sectors = (unsigned long)V->sectors;
    return 0;
}

void diskmgr_redetect(int vol) {
    if (vol < 0 || vol >= vol_count) return;
    vols[vol].fs = fss_detect_disk(vols[vol].disk, vols[vol].start);
}
