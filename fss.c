/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
fss - APSys 00.01 */

#include "fss.h"
#include "diskmgr.h"
#include "satasys.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

static u8 sec[512];
static u8 sb[1024];
static u8 blk[4096];
static u8 ib[4096];
static u8 ind[4096];
static u8 gd[4096];

static u32 le16(const u8*p){ return (u32)p[0]|((u32)p[1]<<8); }
static u32 le32(const u8*p){ return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24); }

const char *fss_fs_name(int fs){
    if(fs==FS_FAT32) return "FAT32";
    if(fs==FS_9P)    return "9P/FFS";
    if(fs==FS_EXT2)  return "EXT2";
    return "UNREADABLE";
}

int fss_detect_disk(int disk,u64 start){
    if(satasys_read(disk,start,1,sec)){
        if(sec[510]==0x55 && sec[511]==0xAA){
            u32 bps=le16(sec+11);
            u8  spc=sec[13];
            int sane=(bps==512||bps==1024||bps==2048||bps==4096)
                     && spc && !(spc&(spc-1))
                     && le16(sec+14)!=0 && sec[16]!=0;
            if(sane &&
               sec[0x52]=='F'&&sec[0x53]=='A'&&sec[0x54]=='T'&&sec[0x55]=='3')
                return FS_FAT32;
        }
    }
    if(satasys_read(disk,start+2,2,sb)){
        if(le16(sb+56)==0xEF53) return FS_EXT2;
    }
    return FS_UNKNOWN;
}

/* FAT32 */
typedef struct { u32 bps,spc,res,nfats,fatsz,root,data; } fat_t;

static int fat_geom(int disk,u64 start,fat_t *M){
    if(!satasys_read(disk,start,1,sec)) return 0;
    M->bps  =le16(sec+11);
    M->spc  =sec[13];
    M->res  =le16(sec+14);
    M->nfats=sec[16];
    M->fatsz=le32(sec+36);
    M->root =le32(sec+44);
    if(M->bps!=512) return 0;
    if(M->spc==0||(M->spc&(M->spc-1))) return 0;
    if(M->nfats==0||M->fatsz==0||M->res==0) return 0;
    if(M->root<2) return 0;
    M->data=M->res+M->nfats*M->fatsz;
    return 1;
}
static u64 fat_lba(fat_t*M,u64 start,u32 clu){
    return start+M->data+(u64)(clu-2)*M->spc;
}
static u32 fat_next(fat_t*M,int disk,u64 start,u32 clu){
    u32 off=clu*4;
    u64 lba=start+M->res+off/512;
    if(!satasys_read(disk,lba,1,sec)) return 0x0FFFFFFF;
    return le32(sec+(off%512))&0x0FFFFFFF;
}
static void fat_name83(const u8*e,char*out){
    int p=0;
    for(int i=0;i<8;++i){ char c=(char)e[i]; if(c!=' ') out[p++]=c; }
    int ep=p;
    out[p++]='.';
    for(int i=8;i<11;++i){ char c=(char)e[i]; if(c!=' ') out[p++]=c; }
    if(p==ep+1) --p;
    out[p]=0;
}

static int fat_list(int vol,u32 clu0,fss_dirent_t*out,int max){
    const volume_t*V=diskmgr_volume(vol);
    if(!V||V->fs!=FS_FAT32||max<=0) return 0;
    fat_t M;
    if(!fat_geom(V->disk,V->start,&M)) return 0;
    int n=0;
    u32 clu=clu0;
    for(int guard=0;guard<4096 && clu>=2 && clu<0x0FFFFFF8 && n<max;++guard){
        for(u32 s=0;s<M.spc && n<max;++s){
            if(!satasys_read(V->disk,fat_lba(&M,V->start,clu)+s,1,sec)) return n;
            for(int i=0;i<16 && n<max;++i){
                const u8*e=sec+i*32;
                if(e[0]==0x00) return n;
                if(e[0]==0xE5) continue;
                if(e[11]==0x0F) continue;
                if(e[11]&0x08) continue;
                fss_dirent_t*D=&out[n];
                fat_name83(e,D->name);
                D->is_dir=(e[11]&0x10)?1:0;
                D->size=le32(e+28);
                D->first_cluster=(le32(e+26)&0xFFFF)|((le32(e+20)&0xFFFF)<<16);
                D->inode=0;
                D->create_time=(u16)le16(e+14);
                D->create_date=(u16)le16(e+16);
                n++;
            }
        }
        clu=fat_next(&M,V->disk,V->start,clu);
    }
    return n;
}

/* ext2 */
typedef struct { int disk; u64 start; u32 bs,isz,ipg,groups; } ext_t;

static int ext_rb(ext_t*C,u32 b,u8*buf){
    u32 s=C->bs/512; if(!s) s=1;
    return satasys_read(C->disk,C->start+(u64)b*s,(int)s,buf);
}
static int ext_setup(int vol,ext_t*C){
    const volume_t*V=diskmgr_volume(vol);
    if(!V||V->fs!=FS_EXT2) return 0;
    C->disk=V->disk; C->start=V->start;
    if(!satasys_read(V->disk,V->start+2,2,sb)) return 0;
    if(le16(sb+56)!=0xEF53) return 0;
    u32 log=le32(sb+24); if(log>2) log=2;
    C->bs=1024u<<log;
    C->isz=le16(sb+88);
    if(C->isz<32||C->isz>C->bs) C->isz=128;
    C->ipg=le32(sb+40); if(C->ipg==0) return 0;
    u32 nblk=le32(sb+4), bpg=le32(sb+32);
    if(bpg==0||nblk==0) return 0;
    C->groups=(nblk+bpg-1)/bpg;
    u32 desc=(C->bs==1024)?2:1;
    if(!ext_rb(C,desc,gd)) return 0;
    return 1;
}

static int ext_inode(ext_t*C,u32 ino,u8*dst){
    if(ino<1) return 0;
    u32 g=(ino-1)/C->ipg, idx=(ino-1)%C->ipg;
    if(g>=C->groups) return 0;
    if(g*32+32>C->bs) return 0;
    u32 itab=le32(gd+g*32+8);
    u32 byte=idx*C->isz;
    u32 b=itab+byte/C->bs, off=byte%C->bs;
    u32 n=C->isz<128?C->isz:128;
    if(off+n>C->bs) return 0;
    if(!ext_rb(C,b,ib)) return 0;
    for(u32 i=0;i<n;++i) dst[i]=ib[off+i];
    return 1;
}

static int ext_list(int vol,u32 ino,fss_dirent_t*out,int max){
    ext_t C;
    if(!ext_setup(vol,&C)||max<=0) return 0;
    u8 ino_buf[128];
    if(!ext_inode(&C,ino,ino_buf)) return 0;
    if((le16(ino_buf)&0xF000)!=0x4000) return 0;
    u32 size=le32(ino_buf+4);
    if(size==0) return 0;

    u32 direct[12];
    for(int i=0;i<12;++i) direct[i]=le32(ino_buf+40+i*4);
    u32 iblk=le32(ino_buf+40+48);
    u32 per=C.bs/4;
    int have_ind=0;
    if(iblk && size>12*C.bs){ if(ext_rb(&C,iblk,ind)) have_ind=1; }

    u32 total=size/C.bs + ((size%C.bs)?1:0);
    int n=0;
    for(u32 bi=0;bi<total && n<max;++bi){
        u32 b;
        if(bi<12) b=direct[bi];
        else{
            if(!have_ind) break;
            u32 ii=bi-12;
            if(ii>=per) break;
            b=le32(ind+ii*4);
        }
        if(b==0) continue;
        if(!ext_rb(&C,b,blk)) continue;
        u32 off=0;
        while(off+8<=C.bs && n<max){
            u32 di=le32(blk+off);
            u32 rl=le16(blk+off+4);
            u8  nl=blk[off+6];
            u8  ft=blk[off+7];
            if(rl<8 || off+rl>C.bs) break;
            if(di && nl && off+8+nl<=C.bs){
                int skip=(nl==1&&blk[off+8]=='.')||
                         (nl==2&&blk[off+8]=='.'&&blk[off+9]=='.');
                if(!skip){
                    fss_dirent_t*D=&out[n];
                    u32 cp=nl<(FSS_NAME_LEN-1)?nl:(FSS_NAME_LEN-1);
                    for(u32 c=0;c<cp;++c) D->name[c]=(char)blk[off+8+c];
                    D->name[cp]=0;
                    D->inode=di;
                    D->first_cluster=di;
                    D->is_dir=(ft==2)?1:0;
                    D->size=0;
                    D->create_time=0; D->create_date=0;
                    u8 sub[128];
                    if(ext_inode(&C,di,sub)){
                        u32 mode=le16(sub);
                        if(ft==0) D->is_dir=((mode&0xF000)==0x4000)?1:0;
                        D->size=le32(sub+4);
                        D->create_time=0; D->create_date=0;
                    }
                    n++;
                }
            }
            off+=rl;
        }
    }
    return n;
}

/* публичный API */
int fss_list(int vol,fss_dirent_t*out,int max){
    const volume_t*V=diskmgr_volume(vol);
    if(!V||!out||max<=0) return 0;
    if(V->fs==FS_FAT32){
        fat_t M;
        if(!fat_geom(V->disk,V->start,&M)) return 0;
        return fat_list(vol,M.root,out,max);
    }
    if(V->fs==FS_EXT2) return ext_list(vol,2,out,max);
    return 0;
}

int fss_list_dir(int vol,unsigned long handle,fss_dirent_t*out,int max){
    const volume_t*V=diskmgr_volume(vol);
    if(!V||!out||max<=0||handle==0) return 0;
    if(V->fs==FS_FAT32) return fat_list(vol,(u32)handle,out,max);
    if(V->fs==FS_EXT2)  return ext_list(vol,(u32)handle,out,max);
    return 0;
}

long fss_read_file(int vol,unsigned long handle,unsigned long size,
                   unsigned char*buf,unsigned long max){
    const volume_t*V=diskmgr_volume(vol);
    if(!V||!buf||max==0||handle==0) return -1;

    if(V->fs==FS_FAT32){
        fat_t M;
        if(!fat_geom(V->disk,V->start,&M)) return -1;
        unsigned long done=0;
        u32 clu=(u32)handle;
        for(int guard=0;guard<65536 && clu>=2 && clu<0x0FFFFFF8 && done<size && done<max;++guard){
            for(u32 s=0;s<M.spc && done<size && done<max;++s){
                unsigned long want=512;
                if(done+want>size) want=size-done;
                if(done+want>max)  want=max-done;
                if(!satasys_read(V->disk,fat_lba(&M,V->start,clu)+s,1,sec)) return (long)done;
                for(unsigned long i=0;i<want;++i) buf[done+i]=sec[i];
                done+=want;
            }
            clu=fat_next(&M,V->disk,V->start,clu);
        }
        return (long)done;
    }

    if(V->fs==FS_EXT2){
        ext_t C;
        if(!ext_setup(vol,&C)) return -1;
        u8 ino_buf[128];
        if(!ext_inode(&C,(u32)handle,ino_buf)) return -1;
        if((le16(ino_buf)&0xF000)!=0x8000) return -1;
        u32 fsz=le32(ino_buf+4);
        if(size>fsz) size=fsz;
        u32 iblk=le32(ino_buf+40+48);
        u32 per=C.bs/4;
        int have_ind=0;
        if(iblk && size>12*C.bs){ if(ext_rb(&C,iblk,ind)) have_ind=1; }
        unsigned long done=0;
        u32 total=(u32)(size/C.bs) + ((size%C.bs)?1:0);
        for(u32 bi=0;bi<total && done<size && done<max;++bi){
            u32 b;
            if(bi<12) b=le32(ino_buf+40+bi*4);
            else{
                if(!have_ind) break;
                u32 ii=bi-12;
                if(ii>=per) break;
                b=le32(ind+ii*4);
            }
            if(b==0){ done+=C.bs; continue; }
            if(!ext_rb(&C,b,blk)) return (long)done;
            unsigned long want=C.bs;
            if(done+want>size) want=size-done;
            if(done+want>max)  want=max-done;
            for(unsigned long i=0;i<want;++i) buf[done+i]=blk[i];
            done+=want;
        }
        return (long)done;
    }
    return -1;
}
