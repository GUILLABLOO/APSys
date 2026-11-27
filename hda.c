/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
hda - APSys 00.01 */

#include "hda.h"

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

/* Port I/O */
static inline void outl(u16 p,u32 v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline u32  inl (u16 p){u32 v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline u8   inb (u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC
static u32 pci_rd(u8 b,u8 d,u8 f,u8 off){
    outl(PCI_ADDR,0x80000000u|((u32)b<<16)|((u32)d<<11)|((u32)f<<8)|(off&0xFC));
    return inl(PCI_DATA);
}
static void pci_wr(u8 b,u8 d,u8 f,u8 off,u32 v){
    outl(PCI_ADDR,0x80000000u|((u32)b<<16)|((u32)d<<11)|((u32)f<<8)|(off&0xFC));
    outl(PCI_DATA,v);
}

/* MMIO */
static volatile u8* mmio = 0;
static u32  rd32(u32 o){return *(volatile u32*)(mmio+o);}
static void wr32(u32 o,u32 v){*(volatile u32*)(mmio+o)=v;}
static u16  rd16(u32 o){return *(volatile u16*)(mmio+o);}
static void wr16(u32 o,u16 v){*(volatile u16*)(mmio+o)=v;}
static u8   rd8 (u32 o){return *(volatile u8*)(mmio+o);}
static void wr8 (u32 o,u8 v){*(volatile u8*)(mmio+o)=v;}

#define GCAP      0x00
#define GCTL      0x08
#define STATESTS  0x0E
#define INTCTL    0x20
#define CORBLBASE 0x40
#define CORBUBASE 0x44
#define CORBWP    0x48
#define CORBRP    0x4A
#define CORBCTL   0x4C
#define CORBSTS   0x4D
#define CORBSIZE  0x4E
#define RIRBLBASE 0x50
#define RIRBUBASE 0x54
#define RIRBWP    0x58
#define RINTCNT   0x5A
#define RIRBCTL   0x5C
#define RIRBSTS   0x5D
#define RIRBSIZE  0x5E
#define ICD       0x60
#define ICIS      0x64
#define IRD       0x68

static volatile u32 corb[256] __attribute__((aligned(128)));
static volatile u64 rirb[256] __attribute__((aligned(128)));

static u32 tsc_per_ms = 2000000;
static u8  codec_addr = 0;
static u16 corb_wp = 0, rirb_rp = 0;
static int cmd_mode = 0;
static int hda_ready = 0;

static u8  f_bus, f_dev, f_fn;
static u32 f_pciid, f_bar0, f_codec_id;
static u16 f_gcap, f_gctl, f_statests, f_dac, f_pin;
static u32 f_corbptr, f_rirbwp, f_sts;
static int f_stage = 0;

static u64 rdtsc_(void){u32 lo,hi;__asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));return ((u64)hi<<32)|lo;}
static void sleep_ms(u32 ms){u64 s=rdtsc_();while(rdtsc_()-s<(u64)ms*tsc_per_ms)__asm__ volatile("pause");}

static void calibrate(void){
    outb(0x43,0xB0); outb(0x42,0xA9); outb(0x42,0x04);
    u8 p=inb(0x61); outb(0x61,p&0xFE); outb(0x61,p|1);
    u64 t0=rdtsc_(); u32 guard=0;
    while(!(inb(0x61)&0x20)){ if(++guard>50000000u) return; __asm__ volatile("pause"); }
    u64 el=rdtsc_()-t0;
    if(el>100000&&el<100000000) tsc_per_ms=(u32)el;
}

static void pci_set_d0(u8 b,u8 d,u8 f){
    u32 cap = pci_rd(b,d,f,0x34) & 0xFC;
    for(int i=0;i<48 && cap; ++i){
        u32 dw = pci_rd(b,d,f,(u8)cap);
        if((dw & 0xFF) == 0x01){
            u32 pmcsr = pci_rd(b,d,f,(u8)(cap+4));
            if(pmcsr & 3){ pci_wr(b,d,f,(u8)(cap+4), pmcsr & ~3u); sleep_ms(10); }
            return;
        }
        cap = (dw >> 8) & 0xFC;
    }
}

static u32 imm_cmd(u32 cmd){
    for(int i=0;i<50 && (rd16(ICIS)&1);++i) sleep_ms(1);
    if(rd16(ICIS)&1) return 0xFFFFFFFFu;
    wr32(ICD, cmd);
    u64 end = rdtsc_() + (u64)50*tsc_per_ms;
    while(rdtsc_() < end){
        if(rd16(ICIS)&2){
            u32 r = rd32(IRD);
            wr16(ICIS, 2);
            return r;
        }
        __asm__ volatile("pause");
    }
    return 0xFFFFFFFFu;
}

static u32 corb_cmd(u32 cmd){
    wr8(RIRBSTS, rd8(RIRBSTS));
    corb_wp = (corb_wp + 1) & 0xFF;
    corb[corb_wp] = cmd;
    wr16(CORBWP, corb_wp);
    u64 end = rdtsc_() + (u64)50*tsc_per_ms;
    while(rdtsc_() < end){
        if((rd16(RIRBWP)&0xFF) != rirb_rp){
            rirb_rp = (rirb_rp + 1) & 0xFF;
            return (u32)(rirb[rirb_rp] & 0xFFFFFFFFu);
        }
        __asm__ volatile("pause");
    }
    return 0xFFFFFFFFu;
}

static u32 codec_cmd(u32 cmd){ return (cmd_mode==1) ? corb_cmd(cmd) : imm_cmd(cmd); }
static u32 v12(u16 nid,u32 verb,u32 p8){
    return ((u32)codec_addr<<28)|((u32)nid<<20)|((verb&0xFFF)<<8)|(p8&0xFF);
}
static u32 v4(u16 nid,u32 verb,u32 p16){
    return ((u32)codec_addr<<28)|((u32)nid<<20)|((verb&0xF)<<16)|(p16&0xFFFF);
}
static u32 get_param(u16 nid,u32 p){ return codec_cmd(v12(nid,0xF00,p)); }

static void corb_stop(void){
    wr8(CORBCTL,0); wr8(RIRBCTL,0);
    for(int i=0;i<20 && (rd8(CORBCTL)&2);++i) sleep_ms(1);
    for(int i=0;i<20 && (rd8(RIRBCTL)&1);++i) sleep_ms(1);
}

static void corb_setup(void){
    corb_stop();
    wr8(CORBSIZE,0x02);
    wr8(RIRBSIZE,0x02);
    for(int i=0;i<256;++i){ corb[i]=0; rirb[i]=0; }
    wr32(CORBUBASE,0); wr32(CORBLBASE,(u32)(unsigned long)corb);
    wr32(RIRBUBASE,0); wr32(RIRBLBASE,(u32)(unsigned long)rirb);
    wr16(CORBRP,0x8000);
    for(int i=0;i<50 && !(rd16(CORBRP)&0x8000);++i) sleep_ms(1);
    wr16(CORBRP,0);
    for(int i=0;i<50 && (rd16(CORBRP)&0x8000);++i) sleep_ms(1);

    wr16(RIRBWP,0x8000); sleep_ms(1);
    wr16(CORBWP,0);
    wr16(RINTCNT,1);
    wr8(RIRBSTS,0x07); wr8(CORBSTS,0x01);
    wr32(INTCTL,0);
    wr8(RIRBCTL,0x01);
    wr8(CORBCTL,0x02);
    corb_wp=0; rirb_rp=0;
    sleep_ms(1);
}

static void setup_path(void){
    codec_cmd(v4 (f_dac,0x2,0x0011));
    codec_cmd(v12(f_dac,0x706,0x10));
    codec_cmd(v12(f_pin,0x707,0x40));
    codec_cmd(v12(f_pin,0x70C,0x02));
    codec_cmd(v4 (f_pin,0x3,0xB000));
    codec_cmd(v4 (f_dac,0x3,0xB000));
}

static int probe(void){
    if(hda_ready) return 1;
    calibrate();

    int found=0;
    for(int b=0;b<8&&!found;++b)
    for(int d=0;d<32&&!found;++d)
    for(int f=0;f<8;++f){
        u32 id=pci_rd(b,d,f,0);
        if((id&0xFFFF)==0xFFFF) continue;
        if((pci_rd(b,d,f,8)>>16)==0x0403){
            f_bus=b;f_dev=d;f_fn=f;f_pciid=id;found=1;break;
        }
    }
    if(!found) return 0;
    f_stage=1;

    pci_set_d0(f_bus,f_dev,f_fn);
    pci_wr(f_bus,f_dev,f_fn,4,pci_rd(f_bus,f_dev,f_fn,4)|0x6);
    f_bar0=pci_rd(f_bus,f_dev,f_fn,0x10)&0xFFFFFFF0u;
    if(!f_bar0) return 0;
    if(pci_rd(f_bus,f_dev,f_fn,0x14)!=0) return 0;
    mmio=(volatile u8*)(unsigned long)f_bar0;

    f_gcap=rd16(GCAP);
    if(f_gcap==0xFFFF || f_gcap==0) return 0;

    wr32(GCTL,rd32(GCTL)&~1u);
    for(int i=0;i<100 && (rd32(GCTL)&1);++i) sleep_ms(1);
    if(rd32(GCTL)&1) return 0;
    wr32(GCTL,rd32(GCTL)|1u);
    for(int i=0;i<100 && !(rd32(GCTL)&1);++i) sleep_ms(1);
    if(!(rd32(GCTL)&1)) return 0;
    f_gctl=rd16(GCTL);
    f_stage=2;

    for(int i=0;i<300 && !(rd16(STATESTS)&0x7FFF);++i) sleep_ms(1);
    f_statests=rd16(STATESTS);
    if(!(f_statests&0x7FFF)) return 0;
    for(codec_addr=0;codec_addr<15;++codec_addr) if(f_statests&(1<<codec_addr)) break;
    wr16(STATESTS,f_statests);
    f_stage=3;

    corb_setup();
    cmd_mode=1;
    f_codec_id=get_param(0,0);
    if(f_codec_id==0xFFFFFFFFu || f_codec_id==0){
        corb_stop();
        cmd_mode=2;
        f_codec_id=get_param(0,0);
    }
    f_corbptr=((u32)rd16(CORBWP)<<16)|rd16(CORBRP);
    f_rirbwp=rd16(RIRBWP);
    f_sts=((u32)rd8(CORBSTS)<<8)|rd8(RIRBSTS);
    if(f_codec_id==0xFFFFFFFFu || f_codec_id==0){ cmd_mode=0; return 0; }
    f_stage=4;

    u32 root=get_param(0,4);
    u16 fs=(root>>16)&0xFF, fc=root&0xFF;
    for(u16 fg=fs;fg<fs+fc;++fg){
        u32 nc=get_param(fg,4);
        u16 ws=(nc>>16)&0xFF, wn=nc&0xFF;
        for(u16 n=ws;n<ws+wn;++n){
            u32 caps=get_param(n,9);
            u8 type=(caps>>20)&0xF;
            if(type==0 && !f_dac) f_dac=n;
            if(type==4 && !f_pin) f_pin=n;
        }
    }
    f_stage=5;
    if(f_dac && f_pin) setup_path();
    hda_ready=1;
    return 1;
}

void hda_diag(void (*pl)(const char*,u8,u8,u8),
              void (*pn)(const char*,u32,const char*,u8,u8,u8)){
    probe();
    pl("--- APSYS HDA DIAGNOSTICS ---",120,210,255);
    if(f_stage==0){ pl("HDA CONTROLLER: NOT FOUND ON PCI",240,110,110); return; }
    pn("PCI ID  : ",f_pciid,"",220,220,220);
    pn("BAR0    : ",f_bar0,"",220,220,220);
    pn("GCAP    : ",f_gcap,"",180,180,200);
    if(f_stage<3){
        pn("GCTL    : ",f_gctl,"",240,110,110);
        pn("STATESTS: ",f_statests,"",240,110,110);
        pl("RESET OR CODEC DETECT: FAIL",240,110,110);
        return;
    }
    pn("STATESTS: ",f_statests,"",180,180,200);
    pn("CODEC   : ",codec_addr,"",180,180,200);
    if(cmd_mode==1)      pl("CMD MODE: CORB",120,240,140);
    else if(cmd_mode==2) pl("CMD MODE: IMMEDIATE (CORB FAILED)",255,220,90);
    else                 pl("CMD MODE: NONE",240,110,110);
    pn("CORB W/R: ",f_corbptr,"",150,150,170);
    pn("RIRB WP : ",f_rirbwp,"",150,150,170);
    pn("STS C/R : ",f_sts,"",150,150,170);
    if(f_stage<4){ pl("STATUS: NO CODEC RESPONSE",240,110,110); return; }
    pn("CODEC ID: ",f_codec_id,"",120,240,140);
    pn("DAC NODE: ",f_dac,"",120,240,140);
    pn("PIN NODE: ",f_pin,"",120,240,140);
    if(f_dac && f_pin) pl("STATUS: READY",120,255,140);
    else               pl("STATUS: NEED ROUTING",255,220,90);
}

int hda_init(void){ return probe(); }

void hda_play(const short* s, unsigned int count){
    (void)s;(void)count;
}
