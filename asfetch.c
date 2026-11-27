/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO & Tinwo
asfetch - APSys 00.01 */
#include "asyxbox.h"

static const char *logo[15] = {
"**               -*",
"*.=+            -= *",
"*:  *          :+   +-",
"*.   +=        *     +-      .**",
"*.     ********=-=--=-+:     ++ *",
"*.  +**-              .*     *:  -",
"*.  +.  .:    .+++=    -     +  +.",
"*    +   .:   +     :.  *    *",
"*      +**=     =****-   -  .*",
"*           .**.         +  -*",
"*           =   =:        *    *.",
".*                         -.     .*.",
"*                          *        :*",
"*                          =-   :=*+:",
"...........*******************:."
};

static const char *info_key[8] = {
"OS", "KERNEL", "SHELL", "MEMORY",
"VIEWPORT", "HARDWARE", "COLOR", "FETCH"
};
static const char *info_val[8] = {
"APSYS (ASYA OPERATING SYSTEM)",
"APSYS KERNEL (x86)",
"ASYXBOX SHELL",
"", "", "", "",
};

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }

static void raw_str(asyxbox_ctx_t *c, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    if (c->print_raw) c->print_raw(s, r, g, b);
}
static void raw_num(asyxbox_ctx_t *c, unsigned v, uint8_t r, uint8_t g, uint8_t b) {
    char t[12]; int n = 0;
    if (v == 0) t[n++] = '0';
    while (v && n < 11) { t[n++] = (char)('0' + v % 10); v /= 10; }
    char o[12]; int p = 0;
    while (n > 0) o[p++] = t[--n];
    o[p] = 0;
    raw_str(c, o, r, g, b);
}
static void raw_pad(asyxbox_ctx_t *c, int n) {
    static const char sp[65] =
    "                                                                ";
    if (n > 64) n = 64;
    if (n > 0) raw_str(c, sp + (64 - n), 0, 0, 0);
}

__attribute__((section(".text.entry"), used))
void app(asyxbox_ctx_t *ctx) {
    if (!ctx || !ctx->print_line) return;

    const int LW   = 38;
    const int ROWS = 15;
    const int INFO = 8;
    int off = (ROWS - INFO) / 2;

    for (int r = 0; r < ROWS; ++r) {
        ctx->print_line(logo[r], 100, 220, 240);
        raw_pad(ctx, LW - slen(logo[r]));

        int i = r - off;
        if (i >= 0 && i < INFO) {
            raw_str(ctx, "  ", 0, 0, 0);
            raw_str(ctx, info_key[i], 120, 240, 140);
            raw_str(ctx, " : ", 150, 150, 170);
            if (i == 3) {
                raw_num(ctx, (ctx->mem_lower_kb + ctx->mem_upper_kb) / 1024u + 1, 220, 220, 220);
                raw_str(ctx, " MB", 220, 220, 220);
            } else if (i == 4) {
                raw_num(ctx, ctx->virt_w, 220, 220, 220);
                raw_str(ctx, " x ", 150, 150, 170);
                raw_num(ctx, ctx->virt_h, 220, 220, 220);
            } else if (i == 5) {
                raw_num(ctx, ctx->hw_w, 220, 220, 220);
                raw_str(ctx, " x ", 150, 150, 170);
                raw_num(ctx, ctx->hw_h, 220, 220, 220);
                raw_str(ctx, " NATIVE", 180, 180, 200);
            } else if (i == 6) {
                raw_num(ctx, (unsigned)ctx->bpp, 220, 220, 220);
                raw_str(ctx, " BPP LINEAR FB", 180, 180, 200);
            } else {
                raw_str(ctx, info_val[i], 220, 220, 220);
            }
        }
        if (ctx->pump) ctx->pump();
    }
    ctx->print_line("", 0, 0, 0);
}
