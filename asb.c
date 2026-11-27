/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
asb - APSys 00.01 */

#include "asb.h"
#include "asmxbox.h"
#include "fatwr.h"
typedef unsigned char u8;
typedef unsigned int  u32;

static u8 imgbuf[ASB_MAX];

typedef void (*asb_entry_t)(asyxbox_ctx_t *ctx);

static void asb_num(asyxbox_ctx_t *ctx, const char *pre, u32 v, const char *suf) {
    ctx->print_num(pre, v, suf, 180, 180, 200);
}

static int asb_native(int len, asyxbox_ctx_t *ctx) {
    /* хватает ли памяти выше 1 МБ */
    u32 need_kb = (ASB_BASE - 0x100000u) / 1024u + ASB_MAX / 1024u + 2048u;
    if (ctx->mem_upper_kb < need_kb) {
        ctx->print_line("ASB: NOT ENOUGH RAM (NEED 32 MB+)", 240, 110, 110);
        return -1;
    }
    u8 *dst = (u8*)ASB_BASE;
    for (int i = 0; i < len; i++) dst[i] = imgbuf[i];
    /* .bss программы не занимает места в файле — обнуляем хвост */
    for (int i = len; i < ASB_MAX; i++) dst[i] = 0;
    __asm__ volatile("" ::: "memory");

    ctx->print_line("ASB: NATIVE CODE, NO MEMORY PROTECTION", 255, 220, 90);
    ctx->print_line("--- ASB RUN ---", 120, 210, 255);
    asb_entry_t fn = (asb_entry_t)ASB_BASE;
    fn(ctx);
    ctx->print_line("--- ASB END ---", 120, 210, 255);
    return 0;
}

void asb_cmd(asyxbox_ctx_t *ctx, const char *cmd, int len) {
    char from[96], path[96], name[40];
    int fn = -1, nn = -1;
    const char *keys[] = {"from", "name"};
    int *results[] = {&fn, &nn};
    char *buffers[] = {from, name};
    int maxs[] = {96, 40};

    for (int k = 0; k < 2; k++) {
        int kl = 0;
        while (keys[k][kl]) kl++;
        for (int i = 0; i + kl <= len; i++) {
            if (i > 0 && cmd[i-1] != ' ') continue;
            int ok = 1;
            for (int j = 0; j < kl; j++) {
                char a = cmd[i+j], b = keys[k][j];
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
            if (j >= len) continue;
            int n = j - st;
            if (n >= maxs[k]) n = maxs[k] - 1;
            for (int t = 0; t < n; t++) buffers[k][t] = cmd[st+t];
            buffers[k][n] = 0;
            *results[k] = n;
            break;
        }
    }

    if (nn <= 0) {
        ctx->print_line("USAGE: ASB FROM <B00> NAME <PROG.BIN>", 240, 110, 110);
        ctx->print_line("NATIVE : FLAT 32-BIT, ENTRY AT START", 180, 180, 200);
        asb_num(ctx, "LOAD AT: 0X", ASB_BASE, " (LINK WITH -TTEXT SAME)");
        ctx->print_line("OR ASM1 BYTECODE MADE BY: ASM ... TO <X.BIN>", 180, 180, 200);
        return;
    }

    int v = asyxbox_nav_resolve(fn > 0 ? from : "", path, 96);
    if (v < 0) { ctx->print_line("ASB: NO SUCH VOLUME - SEE DK", 240, 110, 110); return; }

    char full[160]; int p = 0;
    for (int i = 0; path[i] && p < 120; i++) full[p++] = path[i];
    if (p && path[0]) full[p++] = '/';
    for (int i = 0; name[i] && p < 150; i++) full[p++] = name[i];
    full[p] = 0;

    long got = fw_read(v, full, imgbuf, ASB_MAX);
    if (got < 0) { ctx->print_line("ASB: CANNOT READ FILE", 240, 110, 110); return; }
    if (got < 4) { ctx->print_line("ASB: FILE TOO SMALL", 240, 110, 110); return; }
    asb_num(ctx, "LOADED : ", (u32)got, " BYTES");

    /* наш байткод? */
    if (imgbuf[0]=='A' && imgbuf[1]=='S' && imgbuf[2]=='M' && imgbuf[3]=='1') {
        ctx->print_line("ASB: ASM1 BYTECODE DETECTED", 120, 240, 140);
        ctx->print_line("--- ASB RUN ---", 120, 210, 255);
        asmxbox_run_image(imgbuf, (int)got, ctx);
        ctx->print_line("--- ASB END ---", 120, 210, 255);
        return;
    }

    asb_native((int)got, ctx);
}
