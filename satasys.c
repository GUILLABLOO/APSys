/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
satasys - APSys 00.01 */

#include "satasys.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline u8   inb (u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outw(u16 p,u16 v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline u16  inw (u16 p){u16 v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outl(u16 p,u32 v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline u32  inl (u16 p){u32 v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}

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
static void copy_out(void *dst,const void *src,unsigned long n){
    u8 *d=(u8*)dst; const u8 *s=(const u8*)src;
    for(unsigned long i=0;i<n;++i) d[i]=s[i];
}
static void copy_in(void *dst,const void *src,unsigned long n){
    u8 *d=(u8*)dst; const u8 *s=(const u8*)src;
    for(unsigned long i=0;i<n;++i) d[i]=s[i];
}

static sata_port_t ports[SATASYS_MAX_PORTS];
static int port_count = 0;

static u8 ahci_clb [SATASYS_MAX_PORTS*1024] __attribute__((aligned(1024)));
static u8 ahci_ctb [SATASYS_MAX_PORTS*256]  __attribute__((aligned(256)));
static u8 ahci_rfis[SATASYS_MAX_PORTS*256]  __attribute__((aligned(256)));
static u8 ahci_bounce[32768]                __attribute__((aligned(16)));

/* ATA PIO */
static void pio_delay(int base){ for(int i=0;i<4;++i) (void)inb(base+7); }

static int pio_idle(int base){
    for(int i=0;i<400000;++i){
        u8 st=inb(base+7);
        if(st==0xFF) return -1;
        if(!(st&0x80)) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}
static int pio_drq(int base){
    for(int i=0;i<400000;++i){
        u8 st=inb(base+7);
        if(st==0xFF) return -1;
        if(st&0x01) return -1;
        if(st&0x20) return -1;
        if(!(st&0x80) && (st&0x08)) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

static int pio_identify(int base,int ctrl,int slave,u8 *buf){
    outb(base+6,(u8)(0xA0|(slave?0x10:0)));
    pio_delay(base);
    outb(ctrl,0);
    outb(base+2,0); outb(base+3,0); outb(base+4,0); outb(base+5,0);
    outb(base+7,0xEC);
    pio_delay(base);
    u8 st=inb(base+7);
    if(st==0 || st==0xFF) return 0;
    if(pio_drq(base)<0) return 0;
    for(int i=0;i<256;++i){
        u16 w=inw(base);
        buf[i*2]=(u8)(w&0xFF); buf[i*2+1]=(u8)(w>>8);
    }
    return 1;
}

static int pio_rw(int base,int ctrl,int slave,u32 lba,int count,u8 *buf,int write){
    if(count<=0 || count>256) return 0;
    if(lba>=0x0FFFFFFFu) return 0;
    if(pio_idle(base)<0) return 0;
    outb(base+6,(u8)(0xE0|(slave?0x10:0)|((lba>>24)&0x0F)));
    pio_delay(base);
    if(pio_idle(base)<0) return 0;
    outb(base+2,(u8)(count==256?0:count));
    outb(base+3,(u8)(lba&0xFF));
    outb(base+4,(u8)((lba>>8)&0xFF));
    outb(base+5,(u8)((lba>>16)&0xFF));
    outb(base+7,(u8)(write?0x30:0x20));
    for(int s=0;s<count;++s){
        if(pio_drq(base)<0) return 0;
        u8 *p=buf+(unsigned long)s*512;
        if(write){
            for(int i=0;i<256;++i) outw(base,(u16)(p[i*2]|((u16)p[i*2+1]<<8)));
        } else {
            for(int i=0;i<256;++i){
                u16 w=inw(base);
                p[i*2]=(u8)(w&0xFF); p[i*2+1]=(u8)(w>>8);
            }
        }
    }
    if(write){ outb(base+7,0xE7); if(pio_idle(base)<0) return 0; }
    return 1;
}

/* AHCI */
#define HBA_GHC 0x04
#define HBA_PI  0x0C

static volatile u8 *preg(const sata_port_t *P){
    return P->abar + 0x100 + (u32)P->ahci_port*0x80;
}
static u32 pr_rd(volatile u8 *r,u32 o){ return *(volatile u32*)(r+o); }
static void pr_wr(volatile u8 *r,u32 o,u32 v){ *(volatile u32*)(r+o)=v; }

static void ahci_stop(volatile u8 *r){
    u32 c=pr_rd(r,0x18);
    c &= ~(1u<<0);
    c &= ~(1u<<4);
    pr_wr(r,0x18,c);
    for(int i=0;i<200000;++i){
        u32 v=pr_rd(r,0x18);
        if(!(v&(1u<<15)) && !(v&(1u<<14))) return;
        __asm__ volatile("pause");
    }
}
static void ahci_start(volatile u8 *r){
    u32 c=pr_rd(r,0x18);
    c |= (1u<<4);
    pr_wr(r,0x18,c);
    c |= (1u<<0);
    pr_wr(r,0x18,c);
    for(int i=0;i<200000;++i){
        if(pr_rd(r,0x18)&(1u<<15)) return;
        __asm__ volatile("pause");
    }
}

static int ahci_cmd(sata_port_t *P,u8 cmd,u64 lba,u16 count,u8 *dbuf,u32 dbytes,int write){
    volatile u8 *r=preg(P);
    int slot=P->slot;

    pr_wr(r,0x10,0xFFFFFFFF);
    pr_wr(r,0x30,0xFFFFFFFF);

    for(int i=0;i<100000;++i){
        u32 tf=pr_rd(r,0x20);
        if(!(tf&0x88)) break;
        __asm__ volatile("pause");
    }

    u8 *clb  = ahci_clb  + slot*1024;
    u8 *ctb  = ahci_ctb  + slot*256;
    u8 *cfis = ctb;
    u8 *prdt = ctb + 0x80;

    for(int i=0;i<32;++i) clb[i]=0;

    u32 w0 = 5u | (write?(1u<<6):0u) | (1u<<16);
    clb[0]=(u8)(w0); clb[1]=(u8)(w0>>8); clb[2]=(u8)(w0>>16); clb[3]=(u8)(w0>>24);
    u32 ctba=(u32)(unsigned long)ctb;
    clb[4]=(u8)ctba; clb[5]=(u8)(ctba>>8); clb[6]=(u8)(ctba>>16); clb[7]=(u8)(ctba>>24);

    for(int i=0;i<64;++i) cfis[i]=0;
    cfis[0]=0x27;
    cfis[1]=0x80;
    cfis[2]=cmd;
    cfis[3]=0x40;
    cfis[4]=(u8)(lba);
    cfis[5]=(u8)(lba>>8);
    cfis[6]=(u8)(lba>>16);
    cfis[7]=0x40;
    cfis[8]=(u8)(lba>>24);
    cfis[9]=(u8)(lba>>32);
    cfis[10]=(u8)(lba>>40);
    cfis[12]=(u8)(count);
    cfis[13]=(u8)(count>>8);

    for(int i=0;i<16;++i) prdt[i]=0;
    u32 ba=(u32)(unsigned long)dbuf;
    prdt[0]=(u8)ba; prdt[1]=(u8)(ba>>8); prdt[2]=(u8)(ba>>16); prdt[3]=(u8)(ba>>24);
    u32 dbc=dbytes-1;
    prdt[8]=(u8)dbc; prdt[9]=(u8)(dbc>>8); prdt[10]=(u8)(dbc>>16); prdt[11]=(u8)(dbc>>24);

    pr_wr(r,0x38,1);

    for(int i=0;i<3000000;++i){
        u32 is=pr_rd(r,0x10);
        if(is & (1u<<30)) return 0;
        if(!(pr_rd(r,0x38)&1)){
            if(pr_rd(r,0x20)&0x01) return 0;
            return 1;
        }
        __asm__ volatile("pause");
    }
    return 0;
}

static int ahci_read(int slot_p,u64 lba,int count,u8 *buf){
    sata_port_t *P=&ports[slot_p];
    int done=0;
    while(done<count){
        int chunk=count-done; if(chunk>64) chunk=64;
        if(!ahci_cmd(P,0x25,lba+(u64)done,(u16)chunk,ahci_bounce,(u32)chunk*512,0)) return 0;
        copy_out(buf+(unsigned long)done*512,ahci_bounce,(unsigned long)chunk*512);
        done+=chunk;
    }
    return 1;
}
static int ahci_write(int slot_p,u64 lba,int count,const u8 *buf){
    sata_port_t *P=&ports[slot_p];
    int done=0;
    while(done<count){
        int chunk=count-done; if(chunk>64) chunk=64;
        copy_in(ahci_bounce,buf+(unsigned long)done*512,(unsigned long)chunk*512);
        if(!ahci_cmd(P,0x35,lba+(u64)done,(u16)chunk,ahci_bounce,(u32)chunk*512,1)) return 0;
        done+=chunk;
    }
    return 1;
}
static int ahci_identify(sata_port_t *P,u8 *buf){
    if(!ahci_cmd(P,0xEC,0,1,ahci_bounce,512,0)) return 0;
    copy_out(buf,ahci_bounce,512);
    return 1;
}

static void parse_ident(const u8 *id,sata_port_t *P){
    u64 lba48=0;
    for(int i=0;i<8;++i) lba48 |= ((u64)id[200+i])<<(i*8);
    u32 lba28=(u32)id[120]|((u32)id[121]<<8)|((u32)id[122]<<16)|((u32)id[123]<<24);
    u32 w83=(u32)id[166]|((u32)id[167]<<8);
    int ok48=(w83>>10)&1u;

    P->sectors=0;
    if(ok48 && lba48>0 && lba48<0x10000000000ULL) P->sectors=lba48;
    else if(lba28>0 && lba28<0x10000000u)         P->sectors=lba28;
    else{
        u32 cyl=(u32)id[108]|((u32)id[109]<<8);
        u32 hd =(u32)id[110]|((u32)id[111]<<8);
        u32 spt=(u32)id[112]|((u32)id[113]<<8);
        if(cyl&&cyl<16384&&hd&&hd<256&&spt&&spt<256) P->sectors=(u64)cyl*hd*spt;
    }
    if(P->sectors==0){ P->model[0]='?'; P->model[1]=0; return; }

    int p=0;
    for(int i=54;i<94 && p<SATASYS_MODEL_LEN-2;i+=2){
        char hi=(char)id[i+1], lo=(char)id[i];
        if(hi>=' '&&hi<='~') P->model[p++]=hi;
        if(lo>=' '&&lo<='~') P->model[p++]=lo;
    }
    while(p>0 && P->model[p-1]==' ') --p;
    P->model[p]=0;
}

static int io_seen[8]; static int io_seen_n=0;
static int io_claim(int base){
    for(int i=0;i<io_seen_n;++i) if(io_seen[i]==base) return 0;
    if(io_seen_n<8) io_seen[io_seen_n++]=base;
    return 1;
}

int satasys_init(void){
    port_count=0; io_seen_n=0;
    for(int i=0;i<SATASYS_MAX_PORTS;++i){
        ports[i].present=0; ports[i].sectors=0; ports[i].model[0]=0;
        ports[i].abar=0; ports[i].slot=i;
    }

    for(int b=0;b<256 && port_count<SATASYS_MAX_PORTS;++b)
    for(int d=0;d<32 && port_count<SATASYS_MAX_PORTS;++d){
        u32 id0=pci_rd((u8)b,(u8)d,0,0);
        if((id0&0xFFFF)==0xFFFF) continue;
        int multi=(pci_rd((u8)b,(u8)d,0,0x0C)>>23)&1;
        int nf=multi?8:1;

        for(int f=0;f<nf && port_count<SATASYS_MAX_PORTS;++f){
            u32 id=pci_rd((u8)b,(u8)d,(u8)f,0);
            if((id&0xFFFF)==0xFFFF) continue;
            u32 cls=pci_rd((u8)b,(u8)d,(u8)f,8);
            u8 base=(cls>>24)&0xFF;
            u8 sub =(cls>>16)&0xFF;
            u8 prog=(cls>>8)&0xFF;
            if(base!=0x01) continue;
            pci_wr((u8)b,(u8)d,(u8)f,4,pci_rd((u8)b,(u8)d,(u8)f,4)|0x05);

            if(sub==0x06 && prog==0x01){
                u32 bar5=pci_rd((u8)b,(u8)d,(u8)f,0x24)&0xFFFFFFF0u;
                if(!bar5) continue;
                volatile u8 *abar=(volatile u8*)(unsigned long)bar5;
                pr_wr(abar,HBA_GHC,pr_rd(abar,HBA_GHC)|(1u<<31));
                u32 pi=pr_rd(abar,HBA_PI);

                for(int p=0;p<32 && port_count<SATASYS_MAX_PORTS;++p){
                    if(!(pi&(1u<<p))) continue;
                    volatile u8 *r=abar+0x100+(u32)p*0x80;
                    if((pr_rd(r,0x28)&0x0F)!=3) continue;

                    int slot=port_count;
                    sata_port_t *P=&ports[slot];
                    P->present=1; P->kind=1; P->ahci_port=p;
                    P->abar=abar; P->slot=slot;
                    P->io_base=0; P->io_ctrl=0; P->slave=0;

                    ahci_stop(r);
                    u32 clb=(u32)(unsigned long)(ahci_clb+slot*1024);
                    u32 fb =(u32)(unsigned long)(ahci_rfis+slot*256);
                    pr_wr(r,0x00,clb); pr_wr(r,0x04,0);
                    pr_wr(r,0x08,fb);  pr_wr(r,0x0C,0);
                    pr_wr(r,0x10,0xFFFFFFFF);
                    pr_wr(r,0x30,0xFFFFFFFF);
                    ahci_start(r);

                    u8 idbuf[512];
                    if(ahci_identify(P,idbuf)) parse_ident(idbuf,P);
                    if(P->sectors==0){ P->present=0; continue; }
                    port_count++;
                }
            }
            else if(sub==0x01 || sub==0x05 || sub==0x04 ||
                    (sub==0x06 && prog!=0x01)){
                static const int leg[2][2]={{0x1F0,0x3F6},{0x170,0x376}};
                for(int ch=0;ch<2 && port_count<SATASYS_MAX_PORTS;++ch){
                    u32 b0=pci_rd((u8)b,(u8)d,(u8)f,0x10+ch*8)&0xFFFCu;
                    u32 b1=pci_rd((u8)b,(u8)d,(u8)f,0x14+ch*8)&0xFFFCu;
                    int io=b0?(int)b0:leg[ch][0];
                    int ct=b1?(int)b1:leg[ch][1];
                    if(io<0x100) continue;
                    if(!io_claim(io)) continue;

                    for(int sl=0;sl<2 && port_count<SATASYS_MAX_PORTS;++sl){
                        u8 idbuf[512];
                        if(!pio_identify(io,ct,sl,idbuf)) continue;
                        int slot=port_count;
                        sata_port_t *P=&ports[slot];
                        P->present=1; P->kind=0; P->ahci_port=-1;
                        P->abar=0; P->slot=slot;
                        P->io_base=io; P->io_ctrl=ct; P->slave=sl;
                        parse_ident(idbuf,P);
                        if(P->sectors==0){ P->present=0; continue; }
                        port_count++;
                    }
                }
            }
        }
    }
    return port_count;
}

int satasys_count(void){ return port_count; }
const sata_port_t *satasys_get(int i){ return (i>=0&&i<port_count)?&ports[i]:0; }

int satasys_read(int port,u64 lba,int count,u8 *buf){
    if(port<0||port>=port_count||count<=0||!buf) return 0;
    const sata_port_t *P=&ports[port];
    if(!P->present||!P->sectors) return 0;
    if(lba+((u64)count)>P->sectors) return 0;
    if(P->kind==1) return ahci_read(port,lba,count,buf);
    return pio_rw(P->io_base,P->io_ctrl,P->slave,(u32)lba,count,buf,0);
}

int satasys_write(int port,u64 lba,int count,const u8 *buf){
    if(port<0||port>=port_count||count<=0||!buf) return 0;
    const sata_port_t *P=&ports[port];
    if(!P->present||!P->sectors) return 0;
    if(lba+((u64)count)>P->sectors) return 0;
    if(P->kind==1) return ahci_write(port,lba,count,buf);
    return pio_rw(P->io_base,P->io_ctrl,P->slave,(u32)lba,count,(u8*)buf,1);
}
