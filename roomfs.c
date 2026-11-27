/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
roomfs - APSys 00.01 */

#include "roomfs.h"
#include "diskmgr.h"
#include "fss.h"

static roomfs_node_t N[ROOMFS_MAX];
static int inited = 0;
static int boot_disk = -1;

static int  slen(const char *s){ int n=0; while(s&&s[n])n++; return n; }
static char low(char c){ return (c>='A'&&c<='Z')?(char)(c+32):c; }
static int  seq_ci(const char*a,const char*b){
    int i=0; while(a[i]&&b[i]){ if(low(a[i])!=low(b[i]))return 0; i++; } return a[i]==b[i];
}
static void scopy(char*d,const char*s,int max){
    int i=0; while(s[i]&&i<max-1){ d[i]=s[i]; i++; } d[i]=0;
}

static void init(void){
    if(inited) return;
    for(int i=0;i<ROOMFS_MAX;++i){
        N[i].used=0; N[i].name[0]=0; N[i].parent=-1; N[i].vol=-1;
        N[i].is_dir=0; N[i].size=0; N[i].ctime=0; N[i].cdate=0;
    }
    inited=1;
}

static unsigned char cmos_rd(unsigned char reg){
    unsigned short p0=0x70, p1=0x71; unsigned char v;
    __asm__ volatile("outb %0,%1"::"a"(reg),"Nd"(p0));
    __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p1));
    return v;
}
static unsigned char b2b(unsigned char v){ return (unsigned char)((v&0x0F)+((v>>4)*10)); }
static void stamp(unsigned short *t, unsigned short *d){
    unsigned char b=cmos_rd(0x0B);
    unsigned char mn=cmos_rd(2), hr=cmos_rd(4);
    unsigned char dd=cmos_rd(7), mo=cmos_rd(8), yy=cmos_rd(9);
    unsigned char pm=hr&0x80; hr&=0x7F;
    if(!(b&4)){ mn=b2b(mn); hr=b2b(hr); dd=b2b(dd); mo=b2b(mo); yy=b2b(yy); }
    if(!(b&2)){ if(pm&&hr!=12)hr+=12; if(!pm&&hr==12)hr=0; }
    if(hr>23)hr=23; if(mn>59)mn=59;
    if(mo<1)mo=1; if(mo>12)mo=12; if(dd<1)dd=1; if(dd>31)dd=31;
    *t=(unsigned short)(((hr&31)<<11)|((mn&63)<<5));
    *d=(unsigned short)((((yy+20)&127)<<9)|((mo&15)<<5)|(dd&31));
}

void roomfs_set_boot_disk(int d){ boot_disk=d; }
int  roomfs_vol_count(void){ diskmgr_ensure(); return diskmgr_volume_count(); }

void roomfs_addr_of_vol(int vol, char *out){
    diskmgr_ensure();
    if(vol<0||vol>=diskmgr_volume_count()){ out[0]='?';out[1]='?';out[2]='?';out[3]=0; return; }
    const volume_t *V=diskmgr_volume(vol);
    int d=V->disk, p=V->part;
    if(d<0)d=0; if(d>25)d=25;
    if(p<0)p=0; if(p>32)p=32;
    out[0]=(char)('A'+d);
    out[1]=(char)('0'+p/10);
    out[2]=(char)('0'+p%10);
    out[3]=0;
}

int roomfs_vol_of_addr(const char *a){
    if(!a||slen(a)!=3) return -1;
    char c=low(a[0]);
    if(c<'a'||c>'z') return -1;
    if(a[1]<'0'||a[1]>'9'||a[2]<'0'||a[2]>'9') return -1;
    int d=c-'a', p=(a[1]-'0')*10+(a[2]-'0');
    if(p>32) return -1;
    diskmgr_ensure();
    for(int i=0;i<diskmgr_volume_count();++i){
        const volume_t *V=diskmgr_volume(i);
        if(V->disk==d && V->part==p) return i;
    }
    return -1;
}

int roomfs_system_vol(void){
    diskmgr_ensure();
    int n=diskmgr_volume_count();
    if(n<=0) return -1;
    if(boot_disk>=0){
        for(int i=0;i<n;++i){ const volume_t*V=diskmgr_volume(i);
            if(V->disk==boot_disk&&(V->fs==FS_FAT32||V->fs==FS_EXT2)) return i; }
        for(int i=0;i<n;++i){ const volume_t*V=diskmgr_volume(i);
            if(V->disk==boot_disk) return i; }
    }
    for(int i=0;i<n;++i){ const volume_t*V=diskmgr_volume(i);
        if(V->fs==FS_FAT32||V->fs==FS_EXT2) return i; }
    return 0;
}

int roomfs_parse(const char *s, roomfs_ref_t *out){
    init();
    out->vol=-1; out->err=0; out->path[0]=0;
    int i=0;
    if(s){ while(s[i]==' ')i++; }
    if(!s||!s[i]){
        out->vol=roomfs_system_vol();
        if(out->vol<0) out->err=2;
        return out->vol;
    }
    if(slen(s+i)>=3){
        char h[4]; h[0]=s[i]; h[1]=s[i+1]; h[2]=s[i+2]; h[3]=0;
        char c=low(h[0]);
        if(c>='a'&&c<='z'&&h[1]>='0'&&h[1]<='9'&&h[2]>='0'&&h[2]<='9'){
            int v=roomfs_vol_of_addr(h);
            if(v<0){ out->err=2; return -1; }
            out->vol=v; i+=3;
            while(s[i]==' ')i++;
            if(s[i]=='/') i++;
            else if(s[i]!=0){ out->err=1; return -1; }
            while(s[i]=='/')i++;
            scopy(out->path,s+i,ROOMFS_PATH_MAX);
            return v;
        }
    }
    out->vol=roomfs_system_vol();
    if(out->vol<0){ out->err=2; return -1; }
    while(s[i]=='/')i++;
    scopy(out->path,s+i,ROOMFS_PATH_MAX);
    return out->vol;
}

int roomfs_peek(int vol){
    init();
    if(vol<0) return -1;
    for(int i=0;i<ROOMFS_MAX;++i)
        if(N[i].used && N[i].parent<0 && N[i].vol==vol) return i;
    return -1;
}
int roomfs_root(int vol){
    init();
    if(vol<0) return -1;
    int e=roomfs_peek(vol);
    if(e>=0) return e;
    for(int i=0;i<ROOMFS_MAX;++i) if(!N[i].used){
        N[i].used=1; N[i].parent=-1; N[i].vol=vol; N[i].is_dir=1;
        N[i].name[0]=0; N[i].size=0; N[i].ctime=0; N[i].cdate=0;
        return i;
    }
    return -3;
}
roomfs_node_t *roomfs_get(int i){
    if(i<0||i>=ROOMFS_MAX) return 0;
    return &N[i];
}
int roomfs_find(int dir,const char*name){
    init();
    if(dir<0||dir>=ROOMFS_MAX||!N[dir].used) return -1;
    if(!name||!name[0]) return -1;
    for(int i=0;i<ROOMFS_MAX;++i)
        if(N[i].used&&N[i].parent==dir&&seq_ci(N[i].name,name)) return i;
    return -1;
}
static int alloc_node(int dir,const char*name,int is_dir){
    init();
    if(dir<0||dir>=ROOMFS_MAX||!N[dir].used||!N[dir].is_dir) return -1;
    if(!name||!name[0]||slen(name)>=ROOMFS_NAME_MAX) return -1;
    int ex=roomfs_find(dir,name);
    if(ex>=0) return (N[ex].is_dir==is_dir)?ex:-2;
    for(int i=0;i<ROOMFS_MAX;++i) if(!N[i].used){
        N[i].used=1; N[i].parent=dir; N[i].vol=N[dir].vol; N[i].is_dir=is_dir;
        N[i].size=0; scopy(N[i].name,name,ROOMFS_NAME_MAX);
        stamp(&N[i].ctime,&N[i].cdate);
        return i;
    }
    return -3;
}
int roomfs_mkdir(int d,const char*n){ return alloc_node(d,n,1); }
int roomfs_mkfile(int d,const char*n){ return alloc_node(d,n,0); }

static int path_step(int vol,const char*path,int create){
    int dir = create ? roomfs_root(vol) : roomfs_peek(vol);
    if(dir<0) return create?dir:-1;
    char comp[ROOMFS_NAME_MAX];
    int i=0;
    if(!path) return dir;
    while(path[i]){
        while(path[i]=='/'||path[i]==' ')i++;
        if(!path[i])break;
        int n=0;
        while(path[i]&&path[i]!='/'&&n<ROOMFS_NAME_MAX-1) comp[n++]=path[i++];
        comp[n]=0;
        while(path[i]&&path[i]!='/')i++;
        if(!comp[0])continue;
        if(seq_ci(comp,"."))continue;
        if(seq_ci(comp,"..")){ if(N[dir].parent>=0)dir=N[dir].parent; continue; }
        int f=roomfs_find(dir,comp);
        if(f>=0){
            if(!N[f].is_dir) return -2;
            dir=f; continue;
        }
        if(!create) return -1;
        int r=roomfs_mkdir(dir,comp);
        if(r<0) return r;
        dir=r;
    }
    return dir;
}
int roomfs_mkpath(int vol,const char*path){ return path_step(vol,path,1); }
int roomfs_walk(int vol,const char*path){
    if(!path||!path[0]) return roomfs_peek(vol);
    return path_step(vol,path,0);
}

int roomfs_path(int idx,char*out,int max){
    init();
    if(max<1) return 0;
    out[0]=0;
    if(idx<0||idx>=ROOMFS_MAX||!N[idx].used) return 0;
    int chain[32],n=0,cur=idx;
    while(cur>=0 && N[cur].parent>=0 && n<32){ chain[n++]=cur; cur=N[cur].parent; }
    char addr[4]; roomfs_addr_of_vol(N[idx].vol,addr);
    int p=0;
    for(int k=0;k<3&&p<max-1;++k) out[p++]=addr[k];
    for(int k=n-1;k>=0;--k){
        if(p<max-1) out[p++]='/';
        const char*nm=N[chain[k]].name;
        for(int j=0;nm[j]&&p<max-1;++j) out[p++]=nm[j];
    }
    out[p]=0;
    return p;
}
int roomfs_child(int dir,int n){
    init();
    if(dir<0||dir>=ROOMFS_MAX||!N[dir].used) return -1;
    int c=0;
    for(int i=0;i<ROOMFS_MAX;++i)
        if(N[i].used&&N[i].parent==dir){ if(c==n)return i; c++; }
    return -1;
}
