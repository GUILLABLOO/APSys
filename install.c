/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
install - APSys 00.01 */

#include "install.h"
#include "key.h"
#include "satasys.h"
#include "diskmgr.h"
#include "roomfs.h"
#include "fsc.h"
#include "bootblobs.h"
typedef unsigned char u8;
typedef unsigned int  u32;
typedef unsigned long long u64;

extern char _flat_data_end[];
extern char _flat_end[];
extern char _kernel_entry[];
#define KERNEL_LOAD 0x100000u
#define KERNEL_LBA  64u

static asyxbox_ctx_t *IC;
static u8 mbr[512];
static u8 s1[8192];

static void out(const char *s, u8 r, u8 g, u8 b){ IC->print_line(s, r, g, b); }
static int wait_key(void){
    for (;;) {
        int k = key_poll();
        if (k == KEY_NONE) { if (IC->pump) IC->pump(); __asm__ volatile("pause"); continue; }
        return k;
    }
}
static void put32(u8 *p, int o, u32 v){ p[o]=v&255; p[o+1]=(v>>8)&255; p[o+2]=(v>>16)&255; p[o+3]=(v>>24)&255; }

void install_cmd(asyxbox_ctx_t *ctx) {
    IC = ctx;
    out("APSYS INSTALLER", 120, 210, 255);
    out("BLOBS EMBEDDED, KERNEL TAKEN FROM OWN RAM", 180, 180, 200);
    int n = satasys_count();
    for (int i = 0; i < n; i++) {
        const sata_port_t *P = satasys_get(i);
        char l[80]; int p = 0;
        l[p++] = (char)('A' + i); l[p++] = ')'; l[p++] = ' ';
        for (int c = 0; P->model[c] && p < 50; c++) l[p++] = P->model[c];
        l[p] = 0;
        IC->print_num(l, (u32)(P->sectors / 2048), " MB", 220, 220, 220);
    }
    out("PRESS DISK LETTER (A/B/C...) OR ESC TO ABORT", 255, 220, 90);
    int k = wait_key();
    if (k == KEY_ESC) { out("INSTALL ABORTED", 120, 240, 140); return; }
    char kc = (char)k;
    if (kc >= 'a' && kc <= 'z') kc -= 32;
    int port = kc - 'A';
    if (port < 0 || port >= n) { out("INSTALL: NO SUCH DISK", 240, 110, 110); return; }
    const sata_port_t *TP = satasys_get(port);
    u64 dsec = TP->sectors;

    int sv = roomfs_system_vol();
    const volume_t *SV = diskmgr_volume(sv);
    if (SV && SV->disk == port) { out("INSTALL: CANNOT INSTALL ONTO SYSTEM DISK", 240, 110, 110); return; }

    out("FORMAT TYPE: 1 = FAT32, 2 = EXT2", 255, 220, 90);
    k = wait_key();
    int want_fat = (k == '1');
    if (k != '1' && k != '2') { out("INSTALL ABORTED", 120, 240, 140); return; }

    for (int i = 0; i < 512; i++) mbr[i] = mbr_blob[i];
    for (int i = 0; i < 8192; i++) s1[i] = stage1_blob[i];
    u32 data_bytes = (u32)((unsigned long)_flat_data_end - KERNEL_LOAD);
    u32 bss_len    = (u32)((unsigned long)_flat_end - (unsigned long)_flat_data_end);
    u32 ksec = (data_bytes + 511) / 512;
    if (KERNEL_LBA + ksec > 2048) { out("INSTALL: KERNEL TOO BIG FOR BOOT GAP", 240, 110, 110); return; }
    u64 pstart = 2048;
    u64 psec = dsec - pstart;
    if (psec < 66600) { out("INSTALL: DISK TOO SMALL", 240, 110, 110); return; }

    char q[96]; int qp = 0;
    const char *pre = "CONFIRM: FORMAT DISK ";
    while (*pre) q[qp++] = *pre++;
    q[qp++] = (char)('A' + port);
    const char *mid = " TO ";
    while (*mid) q[qp++] = *mid++;
    const char *fs = want_fat ? "FAT32" : "EXT2";
    while (*fs) q[qp++] = *fs++;
    const char *tail = " AND INSTALL APSYS? Y/N";
    while (*tail) q[qp++] = *tail++;
    q[qp] = 0;
    out(q, 255, 90, 90);
    k = wait_key();
    if (k != 'y' && k != 'Y') { out("INSTALL ABORTED", 120, 240, 140); return; }

    out("FORMATTING...", 120, 210, 255);
    if (fsc_format_geom(port, pstart, psec, want_fat) < 0) {
        out("INSTALL: FORMAT FAILED", 240, 110, 110); return;
    }
    out("FORMAT OK. SELECT SAME DISK AGAIN FOR SYSTEM COPY", 255, 220, 90);
    for (;;) {
        k = wait_key();
        if (k == KEY_ESC) { out("INSTALL ABORTED", 120, 240, 140); return; }
        char c2 = (char)k;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        if (c2 - 'A' == port) break;
        out("THAT IS ANOTHER DISK. PRESS SAME LETTER OR ESC", 255, 220, 90);
    }

    u8 *e = mbr + 446;
    e[0] = 0x80;
    e[4] = want_fat ? 0x0B : 0x83;
    put32(e, 8, (u32)pstart);
    put32(e, 12, (u32)psec);
    if (!satasys_write(port, 0, 1, mbr)) { out("INSTALL: MBR WRITE FAIL", 240, 110, 110); return; }

    u8 *pp = s1 + 0x1E00;
    put32(pp, 0, 0x31534150);
    pp[4] = (u8)(TP->io_base & 255); pp[5] = (u8)(TP->io_base >> 8);
    pp[6] = (u8)TP->slave;
    pp[7] = 0;
    put32(pp, 8, KERNEL_LBA);
    put32(pp, 12, ksec);
    put32(pp, 16, bss_len);
    put32(pp, 20, (u32)(unsigned long)_kernel_entry);
    if (!satasys_write(port, 1, 16, s1)) { out("INSTALL: STAGE1 WRITE FAIL", 240, 110, 110); return; }

    out("COPYING KERNEL FROM RAM...", 120, 210, 255);
    u32 done = 0;
    while (done < ksec) {
        u32 chunk = ksec - done; if (chunk > 128) chunk = 128;
        if (!satasys_write(port, KERNEL_LBA + done, (int)chunk,
                           (const u8*)KERNEL_LOAD + (u64)done * 512)) {
            out("INSTALL: KERNEL WRITE FAIL", 240, 110, 110); return;
        }
        done += chunk;
    }
    out("! INSTALL COMPLETE !", 120, 240, 140);
    IC->print_num("KERNEL  : ", ksec, " SECTORS AT LBA 64", 220, 220, 220);
    IC->print_num("BSS     : ", bss_len, " BYTES ZEROED BY STAGE1", 220, 220, 220);
    IC->print_num("DATA    : ", (u32)(psec / 2048), " MB PARTITION", 220, 220, 220);
    out("REBOOT AND BOOT FROM THIS DISK", 255, 220, 90);
}
