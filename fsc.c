/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
fsc - APSys 00.01 */

#include "fsc.h"
#include "key.h"
#include "diskmgr.h"
#include "roomfs.h"
#include "satasys.h"
typedef unsigned char u8;
typedef unsigned int  u32;
typedef unsigned long long u64;

static asyxbox_ctx_t *FC;
static u8 blk[4096];
static int FPort = -1;

static void out(const char *s, u8 r, u8 g, u8 b){ FC->print_line(s, r, g, b); }
static void put16(u8 *p, int o, u32 v){ p[o]=v&255; p[o+1]=(v>>8)&255; }
static void put32(u8 *p, int o, u32 v){ p[o]=v&255; p[o+1]=(v>>8)&255; p[o+2]=(v>>16)&255; p[o+3]=(v>>24)&255; }
static int wr(u64 lba, int n){ return satasys_write(FPort, lba, n, blk); }
static void clr(void){ for (int i = 0; i < 4096; i++) blk[i] = 0; }

static int confirm(const char *what) {
    out(what, 255, 90, 90);
    out("PRESS Y TO CONFIRM, ANY OTHER KEY TO CANCEL", 255, 220, 90);
    for (;;) {
        int k = key_poll();
        if (k == KEY_NONE) { if (FC->pump) FC->pump(); __asm__ volatile("pause"); continue; }
        return (k == 'y' || k == 'Y');
    }
}

/* FAT32 */
static int fmt_fat32(u64 lba, u32 sectors) {
    if (sectors < 66600) { out("FSC: FAT32 NEEDS >= 32 MB", 240, 110, 110); return -1; }
    u32 reserved = 32, nfats = 2, spc;
    if      (sectors > 16777216) spc = 64;
    else if (sectors >  8388608) spc = 32;
    else if (sectors >  4194304) spc = 16;
    else if (sectors >   532480) spc = 8;
    else                         spc = 4;
    u32 clusters = (sectors - reserved) / spc;
    u32 fat_sec  = (clusters * 4 + 511) / 512;
    clusters = (sectors - reserved - nfats * fat_sec) / spc;

    clr();
    blk[0]=0xEB; blk[1]=0x3C; blk[2]=0x90;
    for (int i=0;i<8;i++) blk[3+i] = "MSWIN4.1"[i];
    put16(blk, 11, 512);
    blk[13] = spc;
    put16(blk, 14, reserved);
    blk[16] = nfats;
    blk[21] = 0xF8;
    put16(blk, 24, 63);
    put16(blk, 26, 255);
    put32(blk, 28, (u32)lba);
    put32(blk, 32, sectors);
    put32(blk, 36, fat_sec);
    put32(blk, 44, 2);
    put16(blk, 48, 1);
    put16(blk, 50, 6);
    blk[64] = 0x80;
    blk[66] = 0x29;
    put32(blk, 67, 0x41505359);
    for (int i=0;i<11;i++) blk[71+i] = "APSYS FSC    "[i];
    for (int i=0;i<8;i++)  blk[82+i] = "FAT32   "[i];
    blk[510]=0x55; blk[511]=0xAA;
    if (!wr(lba+0, 1)) return -1;
    if (!wr(lba+6, 1)) return -1;

    clr();
    put32(blk, 0, 0x41615252);
    put32(blk, 484, 0x61417272);
    put32(blk, 488, clusters - 1);
    put32(blk, 492, 3);
    put32(blk, 508, 0xAA550000);
    if (!wr(lba+1, 1)) return -1;
    if (!wr(lba+7, 1)) return -1;

    for (u32 f = 0; f < nfats; f++) {
        u32 base = reserved + f * fat_sec;
        for (u32 s = 0; s < fat_sec; s += 8) {
            clr();
            if (s == 0) {
                put32(blk, 0, 0xFFFFFFF8);
                put32(blk, 4, 0xFFFFFFFF);
                put32(blk, 8, 0x0FFFFFFF);
            }
            u32 n = fat_sec - s; if (n > 8) n = 8;
            if (!wr(lba + base + s, (int)n)) return -1;
        }
    }
    u32 root = reserved + nfats * fat_sec;
    for (u32 s = 0; s < spc; s += 8) {
        clr();
        u32 n = spc - s; if (n > 8) n = 8;
        if (!wr(lba + root + s, (int)n)) return -1;
    }
    return 0;
}

/* EXT2 (mini) */
static int fmt_ext2(u64 lba, u32 sectors) {
    u32 blocks = sectors / 8;
    if (blocks < 64)   { out("FSC: EXT2 NEEDS >= 512 KB", 240, 110, 110); return -1; }
    if (blocks > 8192) { out("FSC: EXT2 MINI SUPPORTS <= 32 MB, USE FAT32", 240, 110, 110); return -1; }
    u32 inodes = 32;
    u32 bbmap = 2, ibmap = 3, itab = 4, rootb = 5;
    u32 free_blocks = blocks - 6;
    u32 free_inodes = inodes - 11;

    clr();
    u8 *sb = blk + 1024;
    put32(sb, 0, inodes);
    put32(sb, 4, blocks);
    put32(sb, 12, free_blocks);
    put32(sb, 16, free_inodes);
    put32(sb, 20, 1);
    put32(sb, 24, 2);
    put32(sb, 28, 2);
    put32(sb, 32, 8192);
    put32(sb, 36, 8192);
    put32(sb, 40, inodes);
    put16(sb, 54, 0xFFFF);
    put16(sb, 56, 0xEF53);
    put16(sb, 58, 1);
    put16(sb, 60, 1);
    put32(sb, 84, 11);
    put16(sb, 88, 128);
    if (!wr(lba+0, 8)) return -1;

    clr();
    put32(blk, 0, bbmap);
    put32(blk, 4, ibmap);
    put32(blk, 8, itab);
    put16(blk, 12, (u32)(free_blocks & 0xFFFF));
    put16(blk, 16, (u32)(free_inodes & 0xFFFF));
    put16(blk, 20, 1);
    if (!wr(lba+8, 8)) return -1;

    clr(); blk[0] = 0x3F;
    if (!wr(lba+16, 8)) return -1;

    clr(); blk[0] = 0xFF; blk[1] = 0x07;
    if (!wr(lba+24, 8)) return -1;

    clr();
    u8 *in = blk + 128;
    put16(in, 0, 0x41ED);
    put32(in, 4, 4096);
    put16(in, 26, 2);
    put32(in, 28, 8);
    put32(in, 40, rootb);
    if (!wr(lba+32, 8)) return -1;

    clr();
    put32(blk, 0, 2); put16(blk, 4, 12); blk[6]=1; blk[7]=2; blk[8]='.';
    put32(blk, 12, 2); put16(blk, 16, 4096-12); blk[18]=2; blk[19]=2; blk[20]='.'; blk[21]='.';
    if (!wr(lba+40, 8)) return -1;
    return 0;
}

static int arg_named(const char *cmd, int len, const char *key, char *out, int max) {
    int kl = 0; while (key[kl]) kl++;
    for (int i = 0; i + kl <= len; i++) {
        if (i > 0 && cmd[i-1] != ' ') continue;
        int ok = 1;
        for (int j = 0; j < kl; j++) {
            char a = cmd[i+j], b = key[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (a != b) { ok = 0; break; }
        }
        if (!ok) continue;
        int j = i + kl;
        while (j < len && cmd[j] == ' ') j++;
        if (j >= len || cmd[j] != '<') continue;
        j++;
        int st = j;
        while (j < len && cmd[j] != '>') j++;
        if (j >= len) return -1;
        int n = j - st; if (n >= max) n = max - 1;
        for (int t = 0; t < n; t++) out[t] = cmd[st+t];
        out[n] = 0; return n;
    }
    return -1;
}

void fsc_cmd(asyxbox_ctx_t *ctx, const char *cmd, int len) {
    FC = ctx;
    char addr[40], type[16];
    int an = arg_named(cmd, len, "in", addr, 40);
    int tn = arg_named(cmd, len, "type", type, 16);
    if (an <= 0 || tn <= 0) {
        out("USAGE: FSC IN <C02> TYPE <FAT32|EXT2>", 240, 110, 110);
        out("FORMATS THE WHOLE PARTITION. DATA WILL BE LOST", 255, 220, 90);
        return;
    }
    int want_fat = 0, want_ext = 0;
    if (type[0]=='f'||type[0]=='F') want_fat = 1;
    else if (type[0]=='e'||type[0]=='E') want_ext = 1;
    else { out("FSC: TYPE MUST BE FAT32 OR EXT2", 240, 110, 110); return; }

    char path[96];
    int vol = asyxbox_nav_resolve(addr, path, 96);
    if (vol < 0 || path[0]) { out("FSC: TARGET MUST BE A WHOLE VOLUME (EX: C02)", 240, 110, 110); return; }
    if (vol == roomfs_system_vol()) { out("FSC: CANNOT FORMAT SYSTEM VOLUME", 240, 110, 110); return; }

    int port = -1; unsigned long vlba = 0, vsec = 0;
    if (diskmgr_vol_geom(vol, &port, &vlba, &vsec) < 0 || port < 0 || vsec == 0) {
        out("FSC: CANNOT READ PARTITION GEOMETRY", 240, 110, 110); return;
    }
    {
        char show[96]; int p = 0;
        const char *pre = "FSC: FORMAT VOLUME ";
        while (*pre) show[p++] = *pre++;
        for (int i = 0; addr[i] && p < 60; i++) show[p++] = addr[i];
        const char *mid = " ("; while (*mid) show[p++] = *mid++;
        unsigned long mb = vsec / 2048; char t[12]; int n = 0;
        if (mb == 0) t[n++] = '0';
        while (mb && n < 11) { t[n++] = (char)('0' + mb % 10); mb /= 10; }
        while (n > 0) show[p++] = t[--n];
        const char *ms = " MB) TO "; while (*ms) show[p++] = *ms++;
        const char *tt = want_fat ? "FAT32" : "EXT2"; while (*tt) show[p++] = *tt++;
        show[p] = 0;
        out(show, 255, 220, 90);
    }
    if (!confirm("FSC: ALL DATA ON THIS PARTITION WILL BE DESTROYED")) {
        out("FSC: CANCELLED", 120, 240, 140);
        return;
    }
    FPort = port;
    out("FSC: WRITING...", 120, 210, 255);
    int rc = want_fat ? fmt_fat32((u64)vlba, vsec) : fmt_ext2((u64)vlba, vsec);
    if (rc < 0) { out("FSC: WRITE ERROR", 240, 110, 110); return; }
    diskmgr_redetect(vol);
    out("FSC: FORMAT COMPLETE, VOLUME REMOUNTED LIVE", 120, 240, 140);
    out("CHECK DK AND SA. IF WT MISBEHAVES - RB ONCE", 180, 180, 200);
}

int fsc_format_geom(int port, unsigned long long lba, unsigned long long sectors, int want_fat) {
    FPort = port;
    return want_fat ? fmt_fat32(lba, (u32)sectors) : fmt_ext2(lba, (u32)sectors);
}
