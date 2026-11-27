/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
asyxbox - APSys 00.01 */

#include "asyxbox.h"
#include "hda.h"
#include "satasys.h"
#include "diskmgr.h"
#include "fss.h"
#include "roomfs.h"
#include "jaro.h"
#include "asmxbox.h"
#include "asmc.h"
#include "asb.h"
#include "fsc.h"
#include "settings.h"
#include "draw.h"

#define APSYS_FATWR       1
#define APSYS_FSS_DIRWALK 1

#if APSYS_FATWR
#include "fatwr.h"
#endif

#ifndef ASYXBOX_SIGNAL_OPEN_CALC
#define ASYXBOX_SIGNAL_OPEN_CALC  8
#endif
#ifndef ASYXBOX_SIGNAL_OPEN_CHESS
#define ASYXBOX_SIGNAL_OPEN_CHESS 9
#endif
#ifndef ASYXBOX_SIGNAL_OPEN_BALLS
#define ASYXBOX_SIGNAL_OPEN_BALLS 10
#endif

typedef unsigned short uint16_t;

static inline void ab_outb(uint16_t p,uint8_t v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline void ab_outw(uint16_t p,uint16_t v){ __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t ab_inb(uint16_t p){ uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline void ab_outl(uint16_t p,uint32_t v){ __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline uint32_t ab_inl(uint16_t p){ uint32_t r; __asm__ volatile("inl %1,%0":"=a"(r):"Nd"(p)); return r; }

#define NAVMAX 96

static char to_lower(char c){ if(c>='A'&&c<='Z')return c+32; return c; }
static int  slen(const char*s){ int n=0; while(s&&s[n])n++; return n; }
static void scopy(char*d,const char*s,int max){
    int i=0; while(s&&s[i]&&i<max-1){ d[i]=s[i]; i++; } d[i]=0;
}

static int cmd_match(const char*s,int len,char c1,char c2){
    if(len!=2)return 0;
    return (to_lower(s[0])==c1)&&(to_lower(s[1])==c2);
}
static int word_cmd(const char*s,int len,const char*w){
    int n=0; while(w[n])n++;
    if(len<n)return 0;
    for(int i=0;i<n;++i) if(to_lower(s[i])!=w[i])return 0;
    return (len==n || s[n]==' ');
}
static int str_app(char*d,int p,const char*s){ while(*s&&p<150)d[p++]=*s++; return p; }
static int num_dec(char*d,int p,uint32_t v){
    char t[12]; int n=0;
    if(v==0)t[n++]='0';
    while(v&&n<12){ t[n++]=(char)('0'+v%10); v/=10; }
    while(n>0&&p<150)d[p++]=t[--n];
    return p;
}
static int num_hex(char*d,int p,uint32_t v){
    const char*h="0123456789ABCDEF";
    p=str_app(d,p,"0X");
    for(int i=7;i>=0;--i) if(p<150)d[p++]=h[(v>>(i*4))&0xF];
    return p;
}
static int put2(char*b,int p,int v){
    if(v<0)v=0; if(v>99)v=99;
    b[p++]=(char)('0'+v/10); b[p++]=(char)('0'+v%10); return p;
}
static int hex2(char*b,int p,uint32_t v){
    const char*h="0123456789ABCDEF";
    if(p<150)b[p++]=h[(v>>4)&0xF];
    if(p<150)b[p++]=h[v&0xF];
    return p;
}
static void join_path(const char*dir,const char*nm,char*out,int max){
    int p=0;
    if(dir) for(int i=0;dir[i]&&p<max-1;++i) out[p++]=dir[i];
    if(p&&p<max-1) out[p++]='/';
    for(int i=0;nm&&nm[i]&&p<max-1;++i) out[p++]=nm[i];
    out[p]=0;
}

/* Aргументы =D */
static int parse_quoted_arg(const char*cmd,int len,char*out,int max){
    int st=-1;
    for(int i=0;i<len;++i) if(cmd[i]=='<'){ st=i+1; break; }
    if(st<0)return 0;
    int en=-1;
    for(int i=st;i<len;++i) if(cmd[i]=='>'){ en=i; break; }
    if(en<0)return 0;
    int n=en-st; if(n>=max)n=max-1;
    for(int i=0;i<n;++i)out[i]=cmd[st+i];
    out[n]=0; return n;
}
static int parse_named_arg(const char*cmd,int len,const char*key,char*out,int max){
    int kl=0; while(key[kl])kl++;
    for(int i=0;i+kl<=len;++i){
        if(i>0&&cmd[i-1]!=' ')continue;
        int ok=1;
        for(int k=0;k<kl;++k) if(to_lower(cmd[i+k])!=key[k]){ ok=0; break; }
        if(!ok)continue;
        int j=i+kl;
        while(j<len&&cmd[j]==' ')j++;
        if(j>=len||cmd[j]!='<')continue;
        j++;
        int st=j;
        while(j<len&&cmd[j]!='>')j++;
        if(j>=len)return -1;
        int n=j-st; if(n>=max)n=max-1;
        for(int t=0;t<n;++t)out[t]=cmd[st+t];
        out[n]=0; return n;
    }
    return -1;
}
static int tail_arg(const char*cmd,int len,char*out,int max){
    int i=2;
    while(i<len&&cmd[i]==' ')i++;
    int n=0;
    while(i<len&&n<max-1){
        char c=cmd[i++];
        if(c=='<'||c=='>')continue;
        out[n++]=c;
    }
    out[n]=0;
    while(n>0&&out[n-1]==' ') out[--n]=0;
    return n;
}
static int str_eq_ignorecase(const char*a,const char*b,int n){
    for(int i=0;i<n;++i){
        char x=a[i],y=b[i];
        if(x>='A'&&x<='Z')x+=32;
        if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;
    }
    return 1;
}

static int  cur_vol=-1, home_vol=-1;
static char cur_path[NAVMAX], home_path[NAVMAX];

static void nav_init(void){
    if(cur_vol>=0)return;
    diskmgr_ensure();
    cur_vol=roomfs_system_vol();
    cur_path[0]=0;
    home_vol=cur_vol;
    home_path[0]=0;
}
static int has_addr(const char*s){
    if(!s)return 0;
    int i=0; while(s[i]==' ')i++;
    if(slen(s+i)<3)return 0;
    char c=to_lower(s[i]);
    return (c>='a'&&c<='z')&&(s[i+1]>='0'&&s[i+1]<='9')&&(s[i+2]>='0'&&s[i+2]<='9');
}
static void path_norm(char*p){
    char tmp[NAVMAX]; int n=0,i=0;
    while(p[i]){
        while(p[i]=='/'||p[i]==' ')i++;
        if(!p[i])break;
        int st=i;
        while(p[i]&&p[i]!='/')i++;
        int L=i-st;
        if(L==1&&p[st]=='.')continue;
        if(L==2&&p[st]=='.'&&p[st+1]=='.'){
            while(n>0&&tmp[n-1]!='/')n--;
            if(n>0)n--;
            continue;
        }
        if(n&&n<NAVMAX-1)tmp[n++]='/';
        for(int k=0;k<L&&n<NAVMAX-1;++k)tmp[n++]=p[st+k];
    }
    tmp[n]=0;
    scopy(p,tmp,NAVMAX);
}
static int nav_resolve(const char*arg,char*out,int max,int*err){
    nav_init();
    *err=0;
    if(has_addr(arg)){
        roomfs_ref_t R;
        roomfs_parse(arg,&R);
        if(R.vol<0){ *err=R.err?R.err:2; return -1; }
        scopy(out,R.path,max);
        return R.vol;
    }
    if(!arg||!arg[0]){ scopy(out,cur_path,max); return cur_vol; }
    if(arg[0]=='/'){ scopy(out,arg+1,max); return cur_vol; }
    join_path(cur_path,arg,out,max);
    return cur_vol;
}
static void nav_where(asyxbox_ctx_t*ctx){
    char a[4]; roomfs_addr_of_vol(cur_vol,a);
    char l[128]; int p=0;
    p=str_app(l,p,"AT      : "); p=str_app(l,p,a);
    if(cur_path[0]){ l[p++]='/'; p=str_app(l,p,cur_path); }
    l[p]=0;
    ctx->print_line(l,120,240,140);
    if(cur_vol==home_vol && slen(cur_path)==slen(home_path) &&
       str_eq_ignorecase(cur_path,home_path,slen(cur_path)))
        ctx->print_line("HOME    : YOU ARE HERE",150,150,170);
    else {
        char h[128]; int q=0;
        char ha[4]; roomfs_addr_of_vol(home_vol,ha);
        q=str_app(h,q,"HOME    : "); q=str_app(h,q,ha);
        if(home_path[0]){ h[q++]='/'; q=str_app(h,q,home_path); }
        h[q]=0;
        ctx->print_line(h,150,150,170);
    }
}

#if APSYS_FSS_DIRWALK
static int name_eq(const char*disk_name,const char*want){
    char b[64]; int n=0;
    while(want[n]&&n<63){ b[n]=want[n]; n++; }
    b[n]=0;
    if(n>5){
        const char*suf=".room"; int m=1;
        for(int k=0;k<5;++k) if(to_lower(b[n-5+k])!=suf[k]){ m=0; break; }
        if(m)b[n-5]=0;
    }
    int i=0;
    while(disk_name[i]&&b[i]){
        if(to_lower(disk_name[i])!=to_lower(b[i]))return 0;
        i++;
    }
    return disk_name[i]==b[i];
}
#endif

static int disk_list(int vol,const char*path,fss_dirent_t*out,int max){
    const volume_t*V=diskmgr_volume(vol);
    if(!V||(V->fs!=FS_FAT32&&V->fs!=FS_EXT2))return -1;
    if(!path||!path[0])return fss_list(vol,out,max);
#if APSYS_FSS_DIRWALK
    unsigned long h=0;
    int i=0;
    fss_dirent_t tmp[24];
    while(path[i]){
        while(path[i]=='/')i++;
        if(!path[i])break;
        char comp[64]; int n=0;
        while(path[i]&&path[i]!='/'&&n<63)comp[n++]=path[i++];
        comp[n]=0;
        int cnt = h ? fss_list_dir(vol,h,tmp,24) : fss_list(vol,tmp,24);
        if(cnt<=0)return -1;
        int found=0;
        for(int k=0;k<cnt;++k){
            if(name_eq(tmp[k].name,comp)){
                if(!tmp[k].is_dir)return -1;
                h=tmp[k].first_cluster;
                found=1; break;
            }
        }
        if(!found||!h)return -1;
    }
    return h ? fss_list_dir(vol,h,out,max) : fss_list(vol,out,max);
#else
    (void)out;(void)max;
    return -1;
#endif
}

/* ComeTo|CT */
static void cmd_ct(asyxbox_ctx_t*ctx,const char*cmd,int len){
    nav_init();
    char a[NAVMAX];
    int n=tail_arg(cmd,len,a,NAVMAX);
    if(n==0){ nav_where(ctx); return; }

    if(a[0]=='-'){
        if(n==2&&to_lower(a[1])=='t'){
            if(!cur_path[0]){
                ctx->print_line("CT: ALREADY AT VOLUME ROOT",255,220,90);
                nav_where(ctx); return;
            }
            int L=slen(cur_path), cut=-1;
            for(int i=L-1;i>=0;--i) if(cur_path[i]=='/'){ cut=i; break; }
            if(cut<0) cur_path[0]=0; else cur_path[cut]=0;
            ctx->print_line("CT: UP ONE ROOM",120,240,140);
            nav_where(ctx); return;
        }
        if(n==3&&to_lower(a[1])=='f'&&to_lower(a[2])=='r'){
            cur_vol=home_vol;
            scopy(cur_path,home_path,NAVMAX);
            ctx->print_line("CT: FULL RETURN",120,240,140);
            nav_where(ctx); return;
        }
        ctx->print_line("CT FLAGS: -T (UP)  -FR (FULL RETURN)",240,110,110);
        return;
    }

    char p[NAVMAX]; int err;
    int v=nav_resolve(a,p,NAVMAX,&err);
    if(v<0){
        if(err==2)ctx->print_line("CT: NO SUCH VOLUME - SEE DK",240,110,110);
        else ctx->print_line("CT: BAD ADDRESS. EX: CT <B00/DOCS>",240,110,110);
        return;
    }
    path_norm(p);
    if(p[0]){
        int ok=0;
        int rn=roomfs_walk(v,p);
        if(rn>=0){
            roomfs_node_t*N=roomfs_get(rn);
            if(N&&N->used&&N->is_dir)ok=1;
            else if(N&&N->used&&!N->is_dir){
                ctx->print_line("CT: THAT IS A FILE, NOT A ROOM",240,110,110);
                return;
            }
        }
        if(!ok){
            fss_dirent_t probe[2];
            if(disk_list(v,p,probe,2)>=0)ok=1;
        }
        if(!ok){
            ctx->print_line("CT: NO SUCH ROOM",240,110,110);
            ctx->print_line("SA SHOWS WHAT IS HERE",150,150,170);
            return;
        }
    }
    cur_vol=v;
    scopy(cur_path,p,NAVMAX);
    nav_where(ctx);
}

/* RemovE|RE */
static int ram_remove(int node){
    roomfs_node_t*N=roomfs_get(node);
    if(!N||!N->used)return 0;
    if(N->parent<0)return -3;
    if(N->is_dir&&roomfs_child(node,0)>=0)return -2;
    N->used=0; N->name[0]=0; N->parent=-1; N->vol=-1;
    return 1;
}
static void cmd_re(asyxbox_ctx_t*ctx,const char*cmd,int len){
    nav_init();
    char from[NAVMAX],name[64];
    int fn=parse_named_arg(cmd,len,"from",from,NAVMAX);
    int nn=parse_named_arg(cmd,len,"name",name,64);
    if(nn<=0){
        ctx->print_line("USAGE: RE FROM <B00/PATH> NAME <FILE.EXT>",240,110,110);
        ctx->print_line("FROM CAN BE OMITTED - THEN CURRENT ROOM",180,180,200);
        ctx->print_line("EX: RE FROM <B00> NAME <TEST.TXT>",150,150,170);
        ctx->print_line("EX: RE NAME <GALLERY.ROOM>",150,150,170);
        return;
    }
    if(slen(name)==3&&has_addr(name)){
        ctx->print_line("RE: CANNOT REMOVE A VOLUME",240,110,110);
        return;
    }
    if(name[0]=='/'||name[0]=='\\'){
        ctx->print_line("RE: NAME MUST NOT START WITH SLASH",240,110,110);
        return;
    }
    for(int i=0;name[i];++i) if(name[i]=='/'||name[i]=='\\'){
        ctx->print_line("RE: NAME MUST NOT CONTAIN SLASH",240,110,110);
        return;
    }
    if(name[0]=='.'){
        ctx->print_line("RE: CANNOT REMOVE . OR ..",240,110,110);
        return;
    }

    char p[NAVMAX]; int err;
    int v=nav_resolve(fn>0?from:"",p,NAVMAX,&err);
    if(v<0){
        if(err==2)ctx->print_line("RE: NO SUCH VOLUME - SEE DK",240,110,110);
        else ctx->print_line("RE: BAD ADDRESS",240,110,110);
        return;
    }
    path_norm(p);
    char full[160];
    join_path(p,name,full,160);

    int dir = p[0] ? roomfs_walk(v,p) : roomfs_peek(v);
    if(dir>=0){
        int node=roomfs_find(dir,name);
        if(node>=0){
            int r=ram_remove(node);
            if(r==1){
                ctx->print_line("REMOVED FROM RAM:",120,240,140);
                char a[4]; roomfs_addr_of_vol(v,a);
                char l[160]; int q=str_app(l,0,a);
                if(full[0]){ l[q++]='/'; q=str_app(l,q,full); }
                l[q]=0;
                ctx->print_line(l,220,220,220);
                return;
            }
            if(r==-2){ ctx->print_line("RE: ROOM NOT EMPTY",240,110,110); return; }
            ctx->print_line("RE: CANNOT REMOVE VOLUME ROOT",240,110,110);
            return;
        }
    }
#if APSYS_FATWR
    if(fw_mountable(v)){
        if(fw_remove(v,full)){
            ctx->print_line("REMOVED FROM DISK:",120,240,140);
            char a[4]; roomfs_addr_of_vol(v,a);
            char l[160]; int q=str_app(l,0,a);
            if(full[0]){ l[q++]='/'; q=str_app(l,q,full); }
            l[q]=0;
            ctx->print_line(l,220,220,220);
            return;
        }
        ctx->print_line(fw_err(),240,110,110);
        return;
    }
#endif
    ctx->print_line("RE: NOT FOUND HERE",240,110,110);
    ctx->print_line("DISK WRITE LAYER OFF OR FS READ-ONLY",180,180,200);
}

/* SeeAll|SA */
static void sa_row(asyxbox_ctx_t*ctx,const char*name,int is_dir,const char*where,
                   unsigned short tm,unsigned short dt)
{
    char l[80]; int p=0;
    for(int c=0;c<22&&name[c];++c)l[p++]=name[c];
    while(p<23)l[p++]=' ';
    if(is_dir)p=str_app(l,p,"ROOM");
    else{
        const char*dot=0;
        for(int c=0;name[c];++c)if(name[c]=='.')dot=&name[c];
        if(dot&&dot[1]){ dot++; for(int c=0;c<4&&dot[c];++c)l[p++]=dot[c]; }
        else p=str_app(l,p,"FILE");
    }
    while(p<29)l[p++]=' ';
    p=str_app(l,p,where);
    while(p<34)l[p++]=' ';
    if(tm||dt){
        int hh=(tm>>11)&31, mm=(tm>>5)&63;
        int dd=dt&31, mo=(dt>>5)&15, yy=(((dt>>9)&127)+1980)%100;
        p=put2(l,p,hh); l[p++]=':'; p=put2(l,p,mm); l[p++]=' ';
        p=put2(l,p,dd); l[p++]='/'; p=put2(l,p,mo); l[p++]='/'; p=put2(l,p,yy);
    } else p=str_app(l,p,"--:-- --/--/--");
    l[p]=0;
    ctx->print_line(l,is_dir?255:220,is_dir?220:220,is_dir?90:220);
}
static void sa_head(asyxbox_ctx_t*ctx){
    ctx->print_line("NAME                   TYPE WHERE CREATED",180,180,200);
    ctx->print_line("--------------------------------------------",90,90,100);
}
static void cmd_sa(asyxbox_ctx_t*ctx,const char*cmd,int len){
    nav_init();
    char a[NAVMAX];
    int an=tail_arg(cmd,len,a,NAVMAX);
    char p[NAVMAX]; int err;
    int v=nav_resolve(an>0?a:"",p,NAVMAX,&err);
    if(v<0){
        if(err==2)ctx->print_line("SA: NO SUCH VOLUME - SEE DK",240,110,110);
        else ctx->print_line("SA: BAD ADDRESS. EX: SA <B00/DOCS>",240,110,110);
        return;
    }
    path_norm(p);
    const volume_t*V=diskmgr_volume(v);
    char addr[4]; roomfs_addr_of_vol(v,addr);
    char hdr[128]; int q=0;
    q=str_app(hdr,q,"AT "); q=str_app(hdr,q,addr);
    if(p[0]){ hdr[q++]='/'; q=str_app(hdr,q,p); }
    q=str_app(hdr,q,"  "); q=str_app(hdr,q,fss_fs_name(V->fs));
    if(v==roomfs_system_vol())q=str_app(hdr,q," [SYSTEM]");
    hdr[q]=0;
    ctx->print_line(hdr,120,210,255);
    sa_head(ctx);

    int nd=0;
    fss_dirent_t d[24];
    int got=disk_list(v,p,d,24);
    if(got>0){
        for(int i=0;i<got;++i)
            sa_row(ctx,d[i].name,d[i].is_dir,"DISK",d[i].create_time,d[i].create_date);
        nd=got;
    } else if(got<0 && p[0]){
        ctx->print_line("(DISK ROOM NOT WALKABLE)",180,180,200);
    } else if(!p[0] && V->fs!=FS_FAT32 && V->fs!=FS_EXT2){
        ctx->print_line("(VOLUME HAS NO READABLE FS)",240,110,110);
    }

    int rn = p[0] ? roomfs_walk(v,p) : roomfs_peek(v);
    int nr=0;
    if(rn>=0){
        roomfs_node_t*N=roomfs_get(rn);
        if(N&&N->used&&N->is_dir){
            for(int k=0;;++k){
                int c=roomfs_child(rn,k); if(c<0)break;
                roomfs_node_t*C=roomfs_get(c);
                sa_row(ctx,C->name,C->is_dir,"RAM",C->ctime,C->cdate);
                nr++;
            }
        }
    }
    if(nd==0&&nr==0)ctx->print_line("(EMPTY)",150,150,170);
}

/* CreatE|CE */
static int ends_room(const char*s){
    int n=slen(s);
    if(n<5)return 0;
    const char*suf=".room";
    for(int i=0;i<5;++i) if(to_lower(s[n-5+i])!=suf[i])return 0;
    return 1;
}
static void cmd_ce(asyxbox_ctx_t*ctx,const char*cmd,int len){
    nav_init();
    char to[NAVMAX],name[64],full[160];
    int tn=parse_named_arg(cmd,len,"to",to,NAVMAX);
    int nn=parse_named_arg(cmd,len,"name",name,64);
    if(nn<=0){
        ctx->print_line("USAGE: CE TO <B00/PATH> NAME <FILE.EXT>",240,110,110);
        ctx->print_line("TO CAN BE OMITTED - THEN CURRENT ROOM",180,180,200);
        ctx->print_line("NAME ENDS .ROOM -> ROOM, ELSE FILE",180,180,200);
        ctx->print_line("EX: CE NAME <NOTES.TXT>",150,150,170);
        return;
    }
    for(int i=0;name[i];++i) if(name[i]=='/'||name[i]=='\\'){
        ctx->print_line("CE: NAME MUST NOT CONTAIN SLASH",240,110,110); return;
    }
    char p[NAVMAX]; int err;
    int v=nav_resolve(tn>0?to:"",p,NAVMAX,&err);
    if(v<0){
        if(err==2)ctx->print_line("CE: NO SUCH VOLUME - SEE DK",240,110,110);
        else ctx->print_line("CE: BAD ADDRESS",240,110,110);
        return;
    }
    path_norm(p);
    int isd=ends_room(name);
    join_path(p,name,full,160);
    char addr[4]; roomfs_addr_of_vol(v,addr);
    char show[176]; int q=str_app(show,0,addr);
    if(full[0]){ show[q++]='/'; q=str_app(show,q,full); }
    show[q]=0;
#if APSYS_FATWR
    if(fw_mountable(v)){
        if(fw_create(v,p,name,isd)){
            ctx->print_line(isd?"CREATED ROOM ON DISK:":"CREATED FILE ON DISK:",120,240,140);
            ctx->print_line(show,220,220,220);
            return;
        }
        ctx->print_line(fw_err(),240,110,110);
        return;
    }
#endif
    int dir=roomfs_mkpath(v,p);
    if(dir<0){
        if(dir==-3)ctx->print_line("CE: RAM TREE FULL",240,110,110);
        else if(dir==-2)ctx->print_line("CE: PATH HIT A FILE, NOT A ROOM",240,110,110);
        else ctx->print_line("CE: BAD PATH (31 CHARS PER ROOM)",240,110,110);
        return;
    }
    int ex=roomfs_find(dir,name);
    if(ex>=0){
        ctx->print_line("ALREADY EXISTS:",255,220,90);
        ctx->print_line(show,220,220,220);
        return;
    }
    int r=isd?roomfs_mkdir(dir,name):roomfs_mkfile(dir,name);
    if(r>=0){
        ctx->print_line(isd?"CREATED ROOM:":"CREATED FILE:",120,240,140);
        ctx->print_line(show,220,220,220);
        ctx->print_line("RAM OVERLAY - DISK IS READ-ONLY",150,150,170);
    } else if(r==-3)ctx->print_line("CE: RAM TREE FULL",240,110,110);
    else ctx->print_line("CE: BAD NAME (31 CHARS MAX)",240,110,110);
}

/* TRee|TR (скрыл, не благодарите!) */
#if 0   /* <- Не, если хотите её потестить, то можете, достаточно поставить сюда 1 :P*/
static asyxbox_ctx_t*tr_ctx;
static void tr_walk(int dir,int depth){
    if(depth>8)return;
    for(int k=0;;++k){
        int c=roomfs_child(dir,k); if(c<0)break;
        roomfs_node_t*N=roomfs_get(c); if(!N)break;
        char l[96]; int p=0;
        for(int i=0;i<depth&&p<60;++i){ l[p++]=' '; l[p++]=' '; }
        const char*nm=N->name;
        while(*nm&&p<88)l[p++]=*nm++;
        if(N->is_dir&&p<92)l[p++]='/';
        l[p]=0;
        tr_ctx->print_line(l,N->is_dir?255:220,N->is_dir?220:220,N->is_dir?90:220);
        if(N->is_dir)tr_walk(c,depth+1);
    }
}
static void cmd_tr(asyxbox_ctx_t*ctx){
    tr_ctx=ctx;
    ctx->print_line("--- RAM OVERLAY TREE ---",120,210,255);
    int total=0,n=roomfs_vol_count();
    for(int v=0;v<n;++v){
        int root=roomfs_peek(v);
        if(root<0||roomfs_child(root,0)<0)continue;
        char a[4]; roomfs_addr_of_vol(v,a);
        char l[16]; int p=str_app(l,0,a); l[p++]='/'; l[p]=0;
        ctx->print_line(l,255,220,90);
        tr_walk(root,1);
        total++;
    }
    if(!total)ctx->print_line("EMPTY ;)",150,150,170);
}
#endif

/* DisK|DK + PC */
static void cmd_disks(asyxbox_ctx_t*ctx){
    diskmgr_ensure();
    ctx->print_num("PORTS   : ",(uint32_t)satasys_count()," STORAGE DEVICES",120,210,255);
    for(int i=0;i<satasys_count();++i){
        const sata_port_t*P=satasys_get(i);
        char l[64]; int p=0;
        l[p++]=(char)('A'+(i>25?25:i)); l[p++]=' '; l[p++]=':'; l[p++]=' ';
        p=str_app(l,p,P->kind?"AHCI ":"ATA  "); l[p]=0;
        ctx->print_num(l,(uint32_t)(P->sectors/2048)," MB",220,220,220);
        ctx->print_line(P->model,180,180,200);
    }
    int n=diskmgr_volume_count(), sv=roomfs_system_vol();
    ctx->print_num("VOLUMES : ",(uint32_t)n," MOUNTED",120,240,140);
    ctx->print_line("ADDR  FS      SIZE MB   NOTE",180,180,200);
    ctx->print_line("--------------------------------",90,90,100);
    for(int i=0;i<n;++i){
        const volume_t*V=diskmgr_volume(i);
        char a[4]; roomfs_addr_of_vol(i,a);
        char l[80]; int p=0;
        p=str_app(l,p,a); p=str_app(l,p,"  ");
        p=str_app(l,p,fss_fs_name(V->fs));
        while(p<12)l[p++]=' ';
        p=num_dec(l,p,(uint32_t)(V->sectors/2048));
        while(p<22)l[p++]=' ';
        if(i==sv)p=str_app(l,p,"SYSTEM");
        else if(V->fs!=FS_FAT32&&V->fs!=FS_EXT2)p=str_app(l,p,"NO FS");
        l[p]=0;
        ctx->print_line(l,(i==sv)?120:220,(i==sv)?240:220,(i==sv)?140:220);
    }
    if(n==0)ctx->print_line("(NO VOLUMES)",150,150,170);
}
static uint32_t pc_rd(int b,int d,int f,int off){
    ab_outl(0xCF8,0x80000000u|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(off&0xFC));
    return ab_inl(0xCFC);
}
static void cmd_pc(asyxbox_ctx_t*ctx){
    ctx->print_line("--- PCI MASS STORAGE (B:D:F  BASE SUB PROG) ---",120,210,255);
    int found=0;
    for(int b=0;b<64;++b)
    for(int d=0;d<32;++d){
        if((pc_rd(b,d,0,0)&0xFFFF)==0xFFFF)continue;
        int multi=(pc_rd(b,d,0,0x0C)>>23)&1;
        int nf=multi?8:1;
        for(int f=0;f<nf;++f){
            uint32_t id=pc_rd(b,d,f,0);
            if((id&0xFFFF)==0xFFFF)continue;
            uint32_t cls=pc_rd(b,d,f,8);
            if(((cls>>24)&0xFF)!=0x01)continue;
            found=1;
            char l[64]; int p=0;
            p=hex2(l,p,(uint32_t)b); l[p++]=':';
            p=hex2(l,p,(uint32_t)d); l[p++]=':';
            p=hex2(l,p,(uint32_t)f);
            p=str_app(l,p,"  ");
            p=hex2(l,p,(cls>>24)&0xFF); l[p++]=' ';
            p=hex2(l,p,(cls>>16)&0xFF); l[p++]=' ';
            p=hex2(l,p,(cls>>8)&0xFF);
            p=str_app(l,p,"  ");
            if(((cls>>16)&0xFF)==0x06&&((cls>>8)&0xFF)==0x01)p=str_app(l,p,"AHCI");
            else p=str_app(l,p,"IDE/ATA");
            l[p]=0;
            ctx->print_line(l,220,220,220);
        }
    }
    if(!found)ctx->print_line("(NO MASS STORAGE CONTROLLER ON PCI)",240,110,110);
}

/* WriteText|WT */
#if APSYS_FATWR
static void cmd_wt(asyxbox_ctx_t*ctx,const char*cmd,int len){
    nav_init();
    char to[NAVMAX],name[64],txt[256];
    int tn=parse_named_arg(cmd,len,"to",to,NAVMAX);
    int nn=parse_named_arg(cmd,len,"name",name,64);
    int xn=parse_named_arg(cmd,len,"text",txt,256);
    if(nn<=0||xn<0){
        ctx->print_line("USAGE: WT TO <B00/P> NAME <A.TXT> TEXT <...>",240,110,110);
        ctx->print_line("TO CAN BE OMITTED - THEN CURRENT ROOM",180,180,200);
        return;
    }
    char p[NAVMAX]; int err;
    int v=nav_resolve(tn>0?to:"",p,NAVMAX,&err);
    if(v<0){ ctx->print_line("WT: NO SUCH VOLUME",240,110,110); return; }
    path_norm(p);
    if(!fw_mountable(v)){ ctx->print_line(fw_err(),240,110,110); return; }
    if(!fw_write(v,p,name,(const unsigned char*)txt,(unsigned long)xn)){
        ctx->print_line(fw_err(),240,110,110); return;
    }
    char full[160]; join_path(p,name,full,160);
    char a[4]; roomfs_addr_of_vol(v,a);
    char show[176]; int q=str_app(show,0,a);
    if(full[0]){ show[q++]='/'; q=str_app(show,q,full); }
    show[q]=0;
    ctx->print_line("WRITTEN TO DISK:",120,240,140);
    ctx->print_line(show,220,220,220);
    ctx->print_num("BYTES   : ",(uint32_t)xn,"",180,180,200);
}
#endif

/* JARO */
static int jaro_ext_local(const char*s){
    int n=slen(s);
    if(n<5)return 0;
    if(s[n-4]!='.')return 0;
    char a=to_lower(s[n-3]),b=to_lower(s[n-2]),c=to_lower(s[n-1]);
    if(a=='t'&&b=='x'&&c=='t')return 1;
    if(a=='a'&&b=='s'&&c=='m')return 1;
    return 0;
}
static void cmd_jaro(asyxbox_ctx_t*ctx,const char*cmd,int len){
    nav_init();
    char from[NAVMAX],name[40];
    int fn=parse_named_arg(cmd,len,"from",from,NAVMAX);
    int nn=parse_named_arg(cmd,len,"name",name,40);
    if(nn<=0){
        ctx->print_line("USAGE: JARO FROM <B00/PATH> NAME <FILE.ASM>",240,110,110);
        ctx->print_line("FROM CAN BE OMITTED - THEN CURRENT ROOM",180,180,200);
        ctx->print_line("ONLY .ASM AND .TXT",180,180,200);
        return;
    }
    for(int i=0;name[i];++i) if(name[i]=='/'||name[i]=='\\'){
        ctx->print_line("JARO: NAME MUST NOT CONTAIN SLASH",240,110,110); return;
    }
    if(!jaro_ext_local(name)){
        ctx->print_line("JARO: ONLY .ASM AND .TXT FILES",240,110,110); return;
    }
    char p[NAVMAX]; int err;
    int v=nav_resolve(fn>0?from:"",p,NAVMAX,&err);
    if(v<0){ ctx->print_line("JARO: NO SUCH VOLUME - SEE DK",240,110,110); return; }
    path_norm(p);
    ctx->edit_vol=v;
    scopy(ctx->edit_path,p,96);
    scopy(ctx->edit_name,name,32);
    ctx->signal=ASYXBOX_SIGNAL_OPEN_EDITOR;
    ctx->print_line("OPENING JARO...",255,220,90);
}

/* Приложухи! */
static void cmd_calc(asyxbox_ctx_t*ctx){
    ctx->signal=ASYXBOX_SIGNAL_OPEN_CALC;
    ctx->print_line("OPENING CALC",255,220,90);
}
static void cmd_chess(asyxbox_ctx_t*ctx){
    ctx->signal=ASYXBOX_SIGNAL_OPEN_CHESS;
    ctx->print_line("OPENING CHESS",255,220,90);
}
static void cmd_balls(asyxbox_ctx_t*ctx){
    ctx->signal=ASYXBOX_SIGNAL_OPEN_BALLS;
    ctx->print_line("OPENING BALLS",255,220,90);
}
static void cmd_draw(asyxbox_ctx_t*ctx){
ctx->signal=ASYXBOX_SIGNAL_OPEN_DRAW;
ctx->print_line("OPENING DRAW",255,220,90);
}

/* фигня всякая */
static void cmd_cots(asyxbox_ctx_t*ctx){
    ctx->print_line("BY GUILLABLOO - 2026",255,220,90);
    ctx->print_line("---Special thanks---",255,220,90);
    ctx->print_line("Tinwo, aero_wx.2, raz3r_hype",255,220,90);
    ctx->print_line("and monitor_shinco_y198_hp8",255,220,90);
}
static void cmd_switch(asyxbox_ctx_t*ctx,const char*cmd,int len){
    char arg[32];
    int n=parse_quoted_arg(cmd,len,arg,32);
    if(n==0)n=tail_arg(cmd,len,arg,32);
    if(n==8&&str_eq_ignorecase(arg,"terminal",n)){
        ctx->signal=ASYXBOX_SIGNAL_SWITCH_TERMINAL;
        ctx->print_line("SWITCHING TO TERMINAL...",255,220,90);
    } else if(n==5&&str_eq_ignorecase(arg,"arthe",n)){
        ctx->signal=ASYXBOX_SIGNAL_SWITCH_ARTHE;
        ctx->print_line("SWITCHING TO ARTHE...",255,220,90);
    } else ctx->print_line("USAGE: SH <terminal> OR SH <arthe>",240,110,110);
}
static void cmd_print(asyxbox_ctx_t*ctx,const char*cmd,int len){
    char arg[128];
    int n=parse_quoted_arg(cmd,len,arg,128);
    if(n==0){ ctx->print_line("USAGE: PT <text>",240,110,110); return; }
    ctx->print_line(arg,220,220,220);
}
static void cmd_clear(asyxbox_ctx_t*ctx){ ctx->signal=ASYXBOX_SIGNAL_CLEAR; }
static void cmd_colors(asyxbox_ctx_t*ctx){ ctx->signal=ASYXBOX_SIGNAL_OPEN_COLORS; }
static void cmd_cube(asyxbox_ctx_t*ctx){ ctx->signal=ASYXBOX_SIGNAL_OPEN_CUBE; }
static void cmd_cube_4d(asyxbox_ctx_t*ctx){ ctx->signal=ASYXBOX_SIGNAL_OPEN_CUBE_4D; }
static void cmd_reboot(asyxbox_ctx_t*ctx){
    ctx->print_line("Wait!...",255,220,90);
    uint8_t t;
    __asm__ volatile("cli");
    do{ t=ab_inb(0x64); if(t&0x01)ab_inb(0x60); }while(t&0x02);
    ab_outb(0x64,0xFE);
    while(1)__asm__ volatile("hlt");
}
static void cmd_shutdown(asyxbox_ctx_t*ctx){
    ctx->print_line("Goodbye!...",255,120,120);
    __asm__ volatile("cli");
    ab_outw(0x604,0x2000); ab_outw(0xB004,0x2000); ab_outw(0x4004,0x3400);
    ctx->print_line("SYSTEM HALTED. SAFE TO POWER OFF.",180,180,180);
    while(1)__asm__ volatile("hlt");
}
static void cmd_sysinfo(asyxbox_ctx_t*ctx){
    nav_init();
    uint32_t mb=(ctx->mem_lower_kb+ctx->mem_upper_kb)/1024+1;
    ctx->print_line("--- APSYS/ASYA OPERATING SYSTEM SI ---",120,210,255);
    ctx->print_line("OSv     : APSYS 00.01",220,220,220);
    ctx->print_line("KERNEL  : APSYS KERNEL (x86)",220,220,220);
    ctx->print_line("SHELL   : ASYXBOX Shell",220,220,220);
    ctx->print_num ("MEMORY  : ",mb," MB DETECTED",120,240,140);
    ctx->print_num ("VIEWPORT: ",ctx->virt_w," PX WIDTH",255,220,90);
    ctx->print_num (" ",ctx->virt_h," PX HEIGHT",255,220,90);
    ctx->print_num ("HARDWARE: ",ctx->hw_w," PX NATIVE VBE",180,180,200);
    ctx->print_num ("COLOR   : ",(uint32_t)ctx->bpp," BPP LINEAR FB",180,180,200);
    char a[4]; roomfs_addr_of_vol(cur_vol,a);
    char l[48]; int p=str_app(l,0,"SYSDK  : "); p=str_app(l,p,a); l[p]=0;
    ctx->print_line(l,120,240,140);
}

/* HD (Пока ничего особенного нету в HD :( ) */
static asyxbox_ctx_t*diag_ctx;
static void hd_line(const char*t,uint8_t r,uint8_t g,uint8_t b){ diag_ctx->print_line(t,r,g,b); }
static void hd_num(const char*pfx,uint32_t v,const char*s,uint8_t r,uint8_t g,uint8_t b){
    char buf[160]; int p=0;
    p=str_app(buf,p,pfx); p=num_dec(buf,p,v);
    p=str_app(buf,p," ("); p=num_hex(buf,p,v); p=str_app(buf,p,")");
    p=str_app(buf,p,s); buf[p]=0;
    diag_ctx->print_line(buf,r,g,b);
}
static void cmd_hda_diag(asyxbox_ctx_t*ctx){ diag_ctx=ctx; hda_diag(hd_line,hd_num); }

/* DateTime|DT */
static inline uint8_t dt_cmos(uint8_t reg){ ab_outb(0x70,reg); return ab_inb(0x71); }
static inline uint8_t dt_bcd(uint8_t v){ return (uint8_t)((v&0x0F)+((v>>4)*10)); }
static const char*dt_month(uint8_t m){
    static const char*n[12]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
    if(m<1||m>12)m=1; return n[m-1];
}
static int dt_num(char*b,int p,uint32_t v,int dg){
    uint32_t d=1; for(int i=1;i<dg;++i)d*=10;
    for(int i=0;i<dg;++i){ b[p++]=(char)('0'+(v/d)%10); d/=10; }
    return p;
}
static void cmd_datetime(asyxbox_ctx_t*ctx){
    for(int i=0;i<1000000;++i) if(!(dt_cmos(0x0A)&0x80))break;
    uint8_t sec=dt_cmos(0),min=dt_cmos(2),hr=dt_cmos(4);
    uint8_t dd=dt_cmos(7),mo=dt_cmos(8),yy=dt_cmos(9),ct=dt_cmos(0x32),rb=dt_cmos(0x0B);
    uint8_t pm=hr&0x80; hr&=0x7F;
    if(!(rb&4)){ sec=dt_bcd(sec);min=dt_bcd(min);hr=dt_bcd(hr);dd=dt_bcd(dd);mo=dt_bcd(mo);yy=dt_bcd(yy);ct=dt_bcd(ct); }
    if(!(rb&2)){ if(pm&&hr!=12)hr+=12; if(!pm&&hr==12)hr=0; }
    if(dd<1)dd=1; if(dd>31)dd=31;
    if(mo<1)mo=1; if(mo>12)mo=12;
    if(hr>23)hr=23; if(min>59)min=59; if(sec>59)sec=59;
    uint32_t fy=(ct>=19&&ct<=30)?((uint32_t)ct*100+yy):((yy>=70?1900u:2000u)+yy);
    if(fy>9999)fy=9999;
    char l1[32]; int a=0;
    a=str_app(l1,a,"DATE: "); a=dt_num(l1,a,dd,2); l1[a++]=' ';
    a=str_app(l1,a,dt_month(mo)); l1[a++]=' '; a=dt_num(l1,a,fy,4); l1[a]=0;
    char l2[32]; int b=0;
    b=str_app(l2,b,"TIME: "); b=dt_num(l2,b,hr,2); l2[b++]=':';
    b=dt_num(l2,b,min,2); l2[b++]=':'; b=dt_num(l2,b,sec,2); l2[b]=0;
    ctx->print_line("--- DATE & TIME ---",120,210,255);
    ctx->print_line(l1,220,220,220);
    ctx->print_line(l2,120,240,140);
}

/* HelP|HP */
static void cmd_help(asyxbox_ctx_t*ctx){
    ctx->print_line("--- COMMANDS ---",120,210,255);
    ctx->print_line("HP        : HelP (this list)",220,220,220);
    ctx->print_line("COTS      : Creator Of This System",220,220,220);
    ctx->print_line("CT        : show Current Target",120,240,140);
    ctx->print_line("CT <PATH> : Change Target (go there)",120,240,140);
    ctx->print_line("CT -T     : Top - up one room",120,240,140);
    ctx->print_line("CT -FR    : Full Return - back home",120,240,140);
    ctx->print_line("SA        : See All here",120,240,140);
    ctx->print_line("SA <PATH> : See All there",120,240,140);
    ctx->print_line("CE TO <P> NAME <N> : CrEate (.ROOM = room)",120,240,140);
    ctx->print_line("RE FROM <P> NAME <N> : REmove file / room",255,150,150);
    ctx->print_line("JARO FROM <P> NAME <N.ASM> : code editor",120,240,140);
#if APSYS_FATWR
    ctx->print_line("WT TO <P> NAME <N> TEXT <T> : write text",120,240,140);
#endif
    ctx->print_line("DK        : DisK + volume addresses",220,220,220);
    ctx->print_line("PC        : Pci storage controllers",220,220,220);
    ctx->print_line("CALC      : calculator",120,240,140);
    ctx->print_line("CHESS     : 3D chess",120,240,140);
    ctx->print_line("BALLS     : balls game",120,240,140);
    ctx->print_line("asmc from <P> Name <N.asm> : ASM compiler (indev)",120,240,140);
    ctx->print_line("ASB FROM <P> NAME <N.BIN> : AsyaStartBin",120,240,140);
    ctx->print_line("          native flat 32-bit @ 0X01000000",180,180,200);
    ctx->print_line("SI        : SysInfo",220,220,220);
    ctx->print_line("DT        : DaTe & Time",220,220,220);
    ctx->print_line("HD        : HDA diagnostics",220,220,220);
    ctx->print_line("SH <mode> : <terminal> / <arthe>",220,220,220);
    ctx->print_line("PT <text> : Print text",220,220,220);
    ctx->print_line("CR        : Clear screen",220,220,220);
    ctx->print_line("CO/colors : COlors demo",220,220,220);
    ctx->print_line("DRAW      : paint tool (Arthe only)",120,240,140);
    ctx->print_line("CUBE      : cube",220,220,220);
    ctx->print_line("CUBEF     : tesseract",220,220,220);
    ctx->print_line("SETTINGS  : Arthe desktop settings",120,240,140);
    ctx->print_line("FSC IN <C02> TYPE <FAT32|EXT2> : format",255,150,150);
    ctx->print_line("RB / SN   : ReBoot / ShutdowN",220,220,220);
}

/*  */
int asyxbox_exec(const char*cmd,int len,asyxbox_ctx_t*ctx){
    int st=0;
    while(st<len&&cmd[st]==' ')st++;
    while(len>st&&cmd[len-1]==' ')len--;
    int cl=len-st;
    const char*c=cmd+st;
    if(cl==0)return 0;

    /* словечки с 3 и более символами (с аргументами и без) */
    if(word_cmd(c,cl,"cots")) { cmd_cots(ctx);             return 0; }
    if(word_cmd(c,cl,"cubef")){ cmd_cube_4d(ctx);          return 0; }
    if(word_cmd(c,cl,"cube")) { cmd_cube(ctx);             return 0; }
    if(word_cmd(c,cl,"jaro")) { cmd_jaro(ctx,c,cl);        return 0; }
    if(word_cmd(c,cl,"calc")) { cmd_calc(ctx);             return 0; }
    if(word_cmd(c,cl,"chess")){ cmd_chess(ctx);            return 0; }
    if(word_cmd(c,cl,"balls")){ cmd_balls(ctx);            return 0; }
    if(word_cmd(c,cl,"draw")) { cmd_draw(ctx);             return 0; }
if (word_cmd(c, cl, "asmc")) { asmc_cmd(ctx, c, cl);    return 0; }
if (word_cmd(c, cl, "asm"))  { asmxbox_cmd(ctx, c, cl); return 0; }
if (word_cmd(c, cl, "asb")) { asb_cmd(ctx, c, cl); return 0; }
if (word_cmd(c, cl, "fsc")) { fsc_cmd(ctx, c, cl); return 0; }
if(word_cmd(c,cl,"settings")) { ctx->signal=ASYXBOX_SIGNAL_OPEN_SETTINGS; ctx->print_line("OPENING SETTINGS...",255,220,90); return 0; }

/* всякая прочая фигня тут VVV */
    int k=cl<2?cl:2;
    if      (cmd_match(c,k,'h','p')) cmd_help(ctx);
    else if (cmd_match(c,k,'c','t')) cmd_ct(ctx,c,cl);
    else if (cmd_match(c,k,'r','e')) cmd_re(ctx,c,cl);
    else if (cmd_match(c,k,'s','a')) cmd_sa(ctx,c,cl);
    else if (cmd_match(c,k,'c','e')) cmd_ce(ctx,c,cl);
    else if (cmd_match(c,k,'d','k')) cmd_disks(ctx);
    else if (cmd_match(c,k,'p','c')) cmd_pc(ctx);
    else if (cmd_match(c,k,'s','i')) cmd_sysinfo(ctx);
    else if (cmd_match(c,k,'d','t')) cmd_datetime(ctx);
    else if (cmd_match(c,k,'h','d')) cmd_hda_diag(ctx);
    else if (cmd_match(c,k,'s','h')) cmd_switch(ctx,c,cl);
    else if (cmd_match(c,k,'p','t')) cmd_print(ctx,c,cl);
    else if (cmd_match(c,k,'c','r')) cmd_clear(ctx);
    else if (cmd_match(c,k,'c','o')) cmd_colors(ctx);
#if APSYS_FATWR
    else if (cmd_match(c,k,'w','t')) cmd_wt(ctx,c,cl);
#endif
    else if (cmd_match(c,k,'r','b')) cmd_reboot(ctx);
    else if (cmd_match(c,k,'s','n')) cmd_shutdown(ctx);
    else { ctx->print_line("What?.",240,110,110); return 1; }
    return 0;
}

int asyxbox_nav_resolve(const char*arg,char*out,int max){
    char tmp[NAVMAX];
    scopy(tmp,arg?arg:"",NAVMAX);
    int err=0;
    return nav_resolve(tmp,out,max,&err);
}
