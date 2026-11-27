/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
fatwr - APSys 00.01 */

#include "fatwr.h"
#include "diskmgr.h"
#include "satasys.h"
#include "fss.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

static u8 sec[512];
static char errbuf[72];
static u32 hint[32];

#define CLIST_MAX 256
static u32 clist[CLIST_MAX];

static u32 le16(const u8*p){ return (u32)p[0]|((u32)p[1]<<8); }
static u32 le32(const u8*p){ return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24); }
static void put16(u8*p,u32 v){ p[0]=(u8)v; p[1]=(u8)(v>>8); }
static void put32(u8*p,u32 v){ p[0]=(u8)v; p[1]=(u8)(v>>8); p[2]=(u8)(v>>16); p[3]=(u8)(v>>24); }

static void err(const char*s){ int i=0; while(s[i]&&i<71){errbuf[i]=s[i];i++;} errbuf[i]=0; }
const char *fw_err(void){ return errbuf; }
void fw_arm(int on){ (void)on; }
int  fw_armed(void){ return 1; }

typedef struct {
    int  disk;
    u64  start, end;
    u32  spc, res, nfats, fatsz, root, data, nclus, fsinfo;
} fw_t;

static int rd(fw_t*M,u64 lba){
    if(lba<M->start || lba>=M->end){ err("SECTOR OUT OF VOLUME"); return 0; }
    if(!satasys_read(M->disk,lba,1,sec)){ err("DISK READ FAILED"); return 0; }
    return 1;
}
static int wr(fw_t*M,u64 lba){
    if(lba<M->start || lba>=M->end){ err("SECTOR OUT OF VOLUME"); return 0; }
    if(!satasys_write(M->disk,lba,1,sec)){ err("DISK WRITE FAILED"); return 0; }
    return 1;
}

static int mount(int vol, fw_t*M){
    err("");
    if(vol<0||vol>=32){ err("BAD VOLUME INDEX"); return 0; }
    const volume_t*V=diskmgr_volume(vol);
    if(!V){ err("NO SUCH VOLUME"); return 0; }
    if(V->fs!=FS_FAT32){ err("NOT FAT32 - READ ONLY"); return 0; }
    M->disk=V->disk; M->start=V->start; M->end=V->start+V->sectors;
    if(!rd(M,M->start)) return 0;
    u32 bps=le16(sec+11);
    if(bps!=512){ err("SECTOR SIZE NOT 512"); return 0; }
    M->spc  = sec[13];
    M->res  = le16(sec+14);
    M->nfats= sec[16];
    M->fatsz= le32(sec+36);
    M->root = le32(sec+44);
    M->fsinfo=le16(sec+48);
    if(!M->spc||(M->spc&(M->spc-1))){ err("BAD CLUSTER SIZE"); return 0; }
    if(!M->nfats||!M->fatsz||!M->res){ err("BAD FAT GEOMETRY"); return 0; }
    if(M->root<2){ err("BAD ROOT CLUSTER"); return 0; }
    u32 total=le32(sec+32);
    if(!total) total=(u32)V->sectors;
    M->data=M->res+M->nfats*M->fatsz;
    if(total<=M->data){ err("BAD TOTAL SECTORS"); return 0; }
    M->nclus=(total-M->data)/M->spc;
    if(M->nclus<65525){ err("NOT A REAL FAT32"); return 0; }
    if(hint[vol]<2) hint[vol]=2;
    return 1;
}
int fw_mountable(int vol){ fw_t M; return mount(vol,&M); }

static u64 clu_lba(fw_t*M,u32 c){ return M->start+M->data+(u64)(c-2)*M->spc; }

/* FAT */
static u32 fat_get(fw_t*M,u32 c){
    u32 off=c*4;
    if(!rd(M,M->start+M->res+off/512)) return 0x0FFFFFFF;
    return le32(sec+(off%512))&0x0FFFFFFF;
}
static int fat_set(fw_t*M,u32 c,u32 val){
    u32 off=c*4, bo=off%512;
    u64 base=M->start+M->res+off/512;
    for(u32 k=0;k<M->nfats;++k){
        u64 l=base+(u64)k*M->fatsz;
        if(!rd(M,l)) return 0;
        u32 old=le32(sec+bo);
        put32(sec+bo,(old&0xF0000000u)|(val&0x0FFFFFFFu));
        if(!wr(M,l)) return 0;
    }
    return 1;
}
static u32 fat_alloc(fw_t*M,int vol){
    u32 maxc=M->nclus+1;
    u32 first=(hint[vol]>=2&&hint[vol]<=maxc)?hint[vol]:2;
    for(int pass=0;pass<2;++pass){
        u32 lo=pass?2:first, hi=pass?first:maxc+1;
        if(lo>=hi) continue;
        u32 s0=lo/128, s1=(hi-1)/128;
        for(u32 s=s0;s<=s1;++s){
            if(!rd(M,M->start+M->res+s)) return 0;
            for(u32 i=0;i<128;++i){
                u32 c=s*128+i;
                if(c<lo||c>=hi) continue;
                if((le32(sec+i*4)&0x0FFFFFFFu)==0){ hint[vol]=c+1; return c; }
            }
        }
    }
    err("DISK FULL"); return 0;
}
static int fat_free_chain(fw_t*M,u32 c){
    for(u32 g=0;g<65536 && c>=2 && c<0x0FFFFFF8;++g){
        u32 nx=fat_get(M,c);
        if(!fat_set(M,c,0)) return 0;
        c=nx;
    }
    return 1;
}
static void fsinfo_bump(fw_t*M,int delta,u32 next){
    if(!M->fsinfo) return;
    u64 l=M->start+M->fsinfo;
    if(!rd(M,l)) return;
    if(le32(sec)!=0x41615252) return;
    u32 fc=le32(sec+488);
    if(fc!=0xFFFFFFFFu){
        long v=(long)fc+delta;
        if(v<0)v=0;
        put32(sec+488,(u32)v);
    }
    put32(sec+492,next);
    (void)wr(M,l);
}

/* Тайм */
static u8 cmos(u8 r){
    u16 p0=0x70,p1=0x71; u8 v;
    __asm__ volatile("outb %0,%1"::"a"(r),"Nd"(p0));
    __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p1));
    return v;
}
static u8 b2b(u8 v){ return (u8)((v&0x0F)+((v>>4)*10)); }
static void stamp(u8*e){
    u8 b=cmos(0x0B);
    u8 mn=cmos(2),hr=cmos(4),dd=cmos(7),mo=cmos(8),yy=cmos(9);
    u8 pm=hr&0x80; hr&=0x7F;
    if(!(b&4)){ mn=b2b(mn);hr=b2b(hr);dd=b2b(dd);mo=b2b(mo);yy=b2b(yy); }
    if(!(b&2)){ if(pm&&hr!=12)hr+=12; if(!pm&&hr==12)hr=0; }
    if(hr>23)hr=23; if(mn>59)mn=59;
    if(mo<1)mo=1; if(mo>12)mo=12; if(dd<1)dd=1; if(dd>31)dd=31;
    put16(e+14,(u16)(((hr&31)<<11)|((mn&63)<<5)));
    put16(e+16,(u16)((((yy+20)&127)<<9)|((mo&15)<<5)|(dd&31)));
    put16(e+22,(u16)(((hr&31)<<11)|((mn&63)<<5)));
    put16(e+24,(u16)((((yy+20)&127)<<9)|((mo&15)<<5)|(dd&31)));
}

static int okchar(char c){
    if(c>='a'&&c<='z')c-=32;
    if(c>='A'&&c<='Z')return 1;
    if(c>='0'&&c<='9')return 1;
    return c=='_'||c=='-'||c=='$'||c=='~'||c=='!'||c=='#';
}
static int name83(const char*in,u8*out){
    for(int i=0;i<11;++i)out[i]=' ';
    int len=0; while(in[len])len++;
    if(len<1){ err("EMPTY NAME"); return 0; }
    int dot=-1;
    for(int i=len-1;i>=0;--i) if(in[i]=='.'){ dot=i; break; }
    int bn=(dot<0)?len:dot;
    int en=(dot<0)?0:len-dot-1;
    if(bn<1||bn>8){ err("NAME: 1..8 CHARS BEFORE DOT"); return 0; }
    if(en>3){ err("EXTENSION: MAX 3 CHARS (NO LFN YET)"); return 0; }
    for(int i=0;i<bn;++i){
        if(!okchar(in[i])){ err("BAD CHAR IN NAME"); return 0; }
        char c=in[i]; if(c>='a'&&c<='z')c-=32;
        out[i]=(u8)c;
    }
    for(int i=0;i<en;++i){
        if(!okchar(in[dot+1+i])){ err("BAD CHAR IN EXTENSION"); return 0; }
        char c=in[dot+1+i]; if(c>='a'&&c<='z')c-=32;
        out[8+i]=(u8)c;
    }
    return 1;
}

typedef struct { u32 clu,s,idx; } slot_t;

static int dir_find(fw_t*M,u32 dc,const u8*n11,slot_t*out,u8*eout){
    u32 c=dc;
    for(u32 g=0;g<4096 && c>=2 && c<0x0FFFFFF8;++g){
        for(u32 s=0;s<M->spc;++s){
            if(!rd(M,clu_lba(M,c)+s)) return 0;
            for(u32 i=0;i<16;++i){
                const u8*e=sec+i*32;
                if(e[0]==0x00) return 0;
                if(e[0]==0xE5) continue;
                if(e[11]==0x0F) continue;
                int eq=1;
                for(int k=0;k<11;++k) if(e[k]!=n11[k]){ eq=0; break; }
                if(eq){
                    if(out){ out->clu=c; out->s=s; out->idx=i; }
                    if(eout) for(int k=0;k<32;++k) eout[k]=e[k];
                    return 1;
                }
            }
        }
        c=fat_get(M,c);
    }
    return 0;
}
static int zero_clu(fw_t*M,u32 c){
    for(int k=0;k<512;++k) sec[k]=0;
    for(u32 s=0;s<M->spc;++s) if(!wr(M,clu_lba(M,c)+s)) return 0;
    return 1;
}
static int dir_slot(fw_t*M,u32 dc,int vol,slot_t*out){
    u32 c=dc,last=dc;
    for(u32 g=0;g<4096 && c>=2 && c<0x0FFFFFF8;++g){
        for(u32 s=0;s<M->spc;++s){
            if(!rd(M,clu_lba(M,c)+s)) return 0;
            for(u32 i=0;i<16;++i){
                u8 b=sec[i*32];
                if(b==0x00||b==0xE5){ out->clu=c; out->s=s; out->idx=i; return 1; }
            }
        }
        last=c;
        u32 nx=fat_get(M,c);
        if(nx>=0x0FFFFFF8){
            u32 nc=fat_alloc(M,vol);
            if(!nc) return 0;
            if(!fat_set(M,last,nc)) return 0;
            if(!fat_set(M,nc,0x0FFFFFFF)) return 0;
            if(!zero_clu(M,nc)) return 0;
            fsinfo_bump(M,-1,hint[vol]);
            out->clu=nc; out->s=0; out->idx=0;
            return 1;
        }
        c=nx;
    }
    err("DIRECTORY TOO LARGE"); return 0;
}
static int dir_put(fw_t*M,slot_t*S,const u8*e){
    u64 l=clu_lba(M,S->clu)+S->s;
    if(!rd(M,l)) return 0;
    for(int k=0;k<32;++k) sec[S->idx*32+k]=e[k];
    return wr(M,l);
}
static u32 alloc_chain(fw_t*M,int vol,u32 need){
    if(need==0) return 0;
    if(need>CLIST_MAX){ err("FILE TOO BIG (MAX 128 KB)"); return 0; }
    for(u32 i=0;i<need;++i){
        u32 c=fat_alloc(M,vol);
        if(!c){ for(u32 k=0;k<i;++k) fat_set(M,clist[k],0); return 0; }
        clist[i]=c;
    }
    for(u32 i=0;i+1<need;++i) if(!fat_set(M,clist[i],clist[i+1])) return 0;
    if(!fat_set(M,clist[need-1],0x0FFFFFFF)) return 0;
    fsinfo_bump(M,-(long)need,hint[vol]);
    return clist[0];
}
static void mkentry(u8*e,const u8*n11,u8 attr,u32 clu,u32 size){
    for(int i=0;i<32;++i)e[i]=0;
    for(int i=0;i<11;++i)e[i]=n11[i];
    e[11]=attr;
    stamp(e);
    put16(e+20,(clu>>16)&0xFFFF);
    put16(e+26,clu&0xFFFF);
    put32(e+28,size);
}

static int is_sep(char c){ return c=='/'||c=='\\'; }
static void strip_room(char*comp){
    int n=0; while(comp[n])n++;
    if(n>5){
        const char*suf=".room"; int m=1;
        for(int k=0;k<5;++k){ char a=comp[n-5+k]; if(a>='A'&&a<='Z')a+=32; if(a!=suf[k]){m=0;break;} }
        if(m) comp[n-5]=0;
    }
}
unsigned long fw_mkpath(int vol,const char*path){
    fw_t M;
    if(!mount(vol,&M)) return 0;
    u32 c=M.root;
    if(!path) return c;
    int i=0;
    char comp[16];
    while(path[i]){
        while(is_sep(path[i])||path[i]==' ')i++;
        if(!path[i])break;
        int n=0;
        while(path[i]&&!is_sep(path[i])&&n<15) comp[n++]=path[i++];
        comp[n]=0;
        while(path[i]&&!is_sep(path[i]))i++;
        if(!comp[0])continue;
        if(comp[0]=='.'&&comp[1]==0)continue;
        strip_room(comp);
        u8 n11[11];
        if(!name83(comp,n11)) return 0;
        slot_t S; u8 e[32];
        if(dir_find(&M,c,n11,&S,e)){
            if(!(e[11]&0x10)){ err("PATH HIT A FILE"); return 0; }
            c=(le32(e+26)&0xFFFF)|((le32(e+20)&0xFFFF)<<16);
            if(c<2){ err("BROKEN ROOM ENTRY"); return 0; }
            continue;
        }
        u32 nc=alloc_chain(&M,vol,1);
        if(!nc) return 0;
        if(!zero_clu(&M,nc)) return 0;
        u8 dot[32];
        for(int k=0;k<11;++k) dot[k]=' ';
        dot[0]='.';
        mkentry(dot,dot,0x10,nc,0);
        if(!rd(&M,clu_lba(&M,nc))) return 0;
        for(int k=0;k<32;++k) sec[k]=dot[k];
        for(int k=0;k<11;++k) dot[k]=' ';
        dot[0]='.'; dot[1]='.';
        mkentry(dot,dot,0x10,(c==M.root)?0:c,0);
        for(int k=0;k<32;++k) sec[32+k]=dot[k];
        if(!wr(&M,clu_lba(&M,nc))) return 0;
        if(!dir_slot(&M,c,vol,&S)) return 0;
        u8 ne[32];
        mkentry(ne,n11,0x10,nc,0);
        if(!dir_put(&M,&S,ne)) return 0;
        c=nc;
    }
    return c;
}
static int split(const char*path,char*dir,int dmax,char*nm,int nmax){
    int len=0; while(path&&path[len])len++;
    int cut=-1;
    for(int i=len-1;i>=0;--i) if(is_sep(path[i])){ cut=i; break; }
    int dn=(cut<0)?0:cut;
    int nn=len-(cut+1);
    if(nn<1||nn>=nmax){ err("BAD NAME IN PATH"); return 0; }
    if(dn>=dmax) dn=dmax-1;
    for(int i=0;i<dn;++i) dir[i]=path[i];
    dir[dn]=0;
    for(int i=0;i<nn;++i) nm[i]=path[cut+1+i];
    nm[nn]=0;
    return 1;
}

int fw_create(int vol,const char*path,const char*name,int isdir){
    fw_t M;
    if(!mount(vol,&M)) return 0;
    u32 dc=fw_mkpath(vol,path);
    if(!dc) return 0;
    if(!mount(vol,&M)) return 0;
    char nm[32];
    int nl=0; while(name[nl]&&nl<31){ nm[nl]=name[nl]; nl++; }
    nm[nl]=0;
    if(isdir) strip_room(nm);
    if(nl<1){ err("BAD NAME LENGTH"); return 0; }
    u8 n11[11];
    if(!name83(nm,n11)) return 0;
    slot_t S; u8 old[32];
    if(dir_find(&M,dc,n11,&S,old)){ err("ALREADY EXISTS ON DISK"); return 0; }
    u32 nc=alloc_chain(&M,vol,1);
    if(!nc) return 0;
    if(!zero_clu(&M,nc)) return 0;
    if(isdir){
        u8 dot[32];
        for(int k=0;k<11;++k) dot[k]=' ';
        dot[0]='.';
        mkentry(dot,dot,0x10,nc,0);
        if(!rd(&M,clu_lba(&M,nc))) return 0;
        for(int k=0;k<32;++k) sec[k]=dot[k];
        for(int k=0;k<11;++k) dot[k]=' ';
        dot[0]='.'; dot[1]='.';
        mkentry(dot,dot,0x10,(dc==M.root)?0:dc,0);
        for(int k=0;k<32;++k) sec[32+k]=dot[k];
        if(!wr(&M,clu_lba(&M,nc))) return 0;
    }
    if(!dir_slot(&M,dc,vol,&S)) return 0;
    u8 ne[32];
    mkentry(ne,n11,isdir?0x10:0x20,nc,0);
    if(!dir_put(&M,&S,ne)) return 0;
    err(""); return 1;
}

int fw_write(int vol,const char*path,const char*name,
             const unsigned char*data,unsigned long len){
    fw_t M;
    if(!mount(vol,&M)) return 0;
    if(len>(unsigned long)FW_MAX_FILE_KB*1024){ err("FILE TOO BIG (MAX 128 KB)"); return 0; }
    u32 dc=fw_mkpath(vol,path);
    if(!dc) return 0;
    if(!mount(vol,&M)) return 0;
    u8 n11[11];
    if(!name83(name,n11)) return 0;
    u32 bclu=M.spc*512;
    u32 need=(u32)((len+bclu-1)/bclu);
    slot_t S; u8 old[32];
    int exists=dir_find(&M,dc,n11,&S,old);
    if(exists&&(old[11]&0x10)){ err("THAT IS A ROOM"); return 0; }
    u32 first=0;
    if(need){
        first=alloc_chain(&M,vol,need);
        if(!first) return 0;
        unsigned long done=0;
        u32 c=first;
        while(done<len && c>=2 && c<0x0FFFFFF8){
            for(u32 s=0;s<M.spc && done<len;++s){
                for(int k=0;k<512;++k) sec[k]=0;
                u32 chunk=len-done; if(chunk>512) chunk=512;
                for(u32 k=0;k<chunk;++k) sec[k]=data[done+k];
                if(!wr(&M,clu_lba(&M,c)+s)){ fat_free_chain(&M,first); return 0; }
                done+=chunk;
            }
            c=fat_get(&M,c);
        }
    }
    if(exists){
        u32 oc=(le32(old+26)&0xFFFF)|((le32(old+20)&0xFFFF)<<16);
        if(oc>=2) fat_free_chain(&M,oc);
    } else {
        if(!dir_slot(&M,dc,vol,&S)) return 0;
    }
    u8 ne[32];
    mkentry(ne,n11,0x20,first,(u32)len);
    if(!dir_put(&M,&S,ne)) return 0;
    err(""); return 1;
}

long fw_read(int vol,const char*path,unsigned char*buf,unsigned long max){
    fw_t M;
    if(!mount(vol,&M)) return -1;
    char dir[96],nm[32];
    if(!split(path,dir,96,nm,32)) return -1;
    u32 dc=dir[0]?fw_mkpath(vol,dir):M.root;
    if(!dc) return -1;
    if(!mount(vol,&M)) return -1;
    u8 n11[11];
    if(!name83(nm,n11)) return -1;
    slot_t S; u8 e[32];
    if(!dir_find(&M,dc,n11,&S,e)){ err("FILE NOT FOUND"); return -1; }
    if(e[11]&0x10){ err("THAT IS A ROOM"); return -1; }
    u32 size=le32(e+28);
    u32 c=(le32(e+26)&0xFFFF)|((le32(e+20)&0xFFFF)<<16);
    if(size>max) size=max;
    unsigned long done=0;
    while(done<size && c>=2 && c<0x0FFFFFF8){
        for(u32 s=0;s<M.spc && done<size;++s){
            if(!rd(&M,clu_lba(&M,c)+s)) return (long)done;
            u32 chunk=size-done; if(chunk>512) chunk=512;
            for(u32 k=0;k<chunk;++k) buf[done+k]=sec[k];
            done+=chunk;
        }
        c=fat_get(&M,c);
    }
    err(""); return (long)done;
}

static int dir_empty(fw_t*M,u32 c){
    for(u32 g=0;g<4096 && c>=2 && c<0x0FFFFFF8;++g){
        for(u32 s=0;s<M->spc;++s){
            if(!rd(M,clu_lba(M,c)+s)) return 0;
            for(u32 i=0;i<16;++i){
                const u8*e=sec+i*32;
                if(e[0]==0x00) return 1;
                if(e[0]==0xE5) continue;
                if(e[11]==0x0F) continue;
                if(e[0]=='.') continue;
                return 0;
            }
        }
        c=fat_get(M,c);
    }
    return 1;
}

int fw_remove(int vol,const char*path){
    fw_t M;
    if(!mount(vol,&M)) return 0;
    char dir[96],nm[32];
    if(!split(path,dir,96,nm,32)) return 0;
    u32 dc=dir[0]?fw_mkpath(vol,dir):M.root;
    if(!dc) return 0;
    if(!mount(vol,&M)) return 0;
    strip_room(nm);
    u8 n11[11];
    if(!name83(nm,n11)) return 0;
    slot_t S; u8 e[32];
    if(!dir_find(&M,dc,n11,&S,e)){ err("NOT FOUND"); return 0; }
    u32 c=(le32(e+26)&0xFFFF)|((le32(e+20)&0xFFFF)<<16);
    if(e[11]&0x10){
        if(c<2){ err("BROKEN ROOM"); return 0; }
        if(!dir_empty(&M,c)){ err("ROOM NOT EMPTY"); return 0; }
    }
    u64 l=clu_lba(&M,S.clu)+S.s;
    if(!rd(&M,l)) return 0;
    for(int i=(int)S.idx-1;i>=0;--i){
        if(sec[i*32+11]!=0x0F) break;
        sec[i*32]=0xE5;
    }
    sec[S.idx*32]=0xE5;
    if(!wr(&M,l)) return 0;
    if(c>=2){
        u32 n=0,cc=c;
        while(cc>=2&&cc<0x0FFFFFF8&&n<65536){ cc=fat_get(&M,cc); n++; }
        if(!fat_free_chain(&M,c)) return 0;
        fsinfo_bump(&M,(long)n,hint[vol]);
    }
    err(""); return 1;
}

unsigned long fw_free_kb(int vol){
    fw_t M;
    if(!mount(vol,&M)) return 0;
    if(M.fsinfo){
        if(rd(&M,M.start+M.fsinfo)&&le32(sec)==0x41615252){
            u32 fc=le32(sec+488);
            if(fc!=0xFFFFFFFFu) return (unsigned long)fc*M.spc/2;
        }
    }
    return 0;
}
