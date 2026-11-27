/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
asmbox - APSys 00.01 */

#include "asmxbox.h"
#include "fatwr.h"
#include "key.h"
typedef unsigned char u8;
typedef unsigned int  u32;

static unsigned long long rdtsc(void) {
    unsigned lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}
static volatile int g_asm_interrupted = 0;
static unsigned long long g_pump_last = 0;
static void asm_pump(asyxbox_ctx_t *ctx) {
    unsigned long long now = rdtsc();
    if (now - g_pump_last < 4000000ULL) return;
    g_pump_last = now;
    if (ctx->pump) ctx->pump();
}
static int asm_break(asyxbox_ctx_t *ctx) {
    int k = key_poll();
    if (k == KEY_NONE) return 0;
    if (k == KEY_ESC || k == 12) {
        ctx->print_line("ASM: INTERRUPTED", 255, 220, 90);
        return 1;
    }
    if (k == 'l' || k == 'L') {
        key_modifiers_t m; key_get_modifiers(&m);
        if (m.ctrl) { ctx->print_line("ASM: INTERRUPTED", 255, 220, 90); return 1; }
    }
    return 0;
}
static void asm_print_int(asyxbox_ctx_t *ctx, int v) {
    char buf[16]; int n = 0;
    if (v < 0) { buf[n++] = '-'; v = -v; }
    if (v == 0) buf[n++] = '0';
    int tmp[10], tn = 0;
    while (v && tn < 10) { tmp[tn++] = v % 10; v /= 10; }
    while (tn > 0) buf[n++] = (char)('0' + tmp[--tn]);
    buf[n] = 0;
    ctx->print_line(buf, 220, 220, 220);
}

/* говорун чертов */
typedef struct { const char *src; int len, pos, line; } asm_lex_t;

static void asm_skip(asm_lex_t *L) {
    while (L->pos < L->len) {
        char c = L->src[L->pos];
        if (c == ' ' || c == '\t' || c == '\r') { L->pos++; continue; }
        if (c == '\n') { L->pos++; L->line++; continue; }
        if (c == ';') {
            while (L->pos < L->len && L->src[L->pos] != '\n') L->pos++;
            continue;
        }
        break;
    }
}
static int asm_at_end(asm_lex_t *L) { asm_skip(L); return L->pos >= L->len; }

static int asm_read_word(asm_lex_t *L, char *buf, int max) {
    asm_skip(L);
    int p = 0;
    while (L->pos < L->len && p < max - 1) {
        char c = L->src[L->pos];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') { buf[p++] = c; L->pos++; }
        else break;
    }
    buf[p] = 0;
    return p;
}
static int asm_read_number(asm_lex_t *L) {
    asm_skip(L);
    int v = 0, neg = 0;
    if (L->pos < L->len && L->src[L->pos] == '-') { neg = 1; L->pos++; }
    while (L->pos < L->len && L->src[L->pos] >= '0' && L->src[L->pos] <= '9') {
        v = v * 10 + (L->src[L->pos] - '0'); L->pos++;
    }
    return neg ? -v : v;
}
static int asm_read_string(asm_lex_t *L, char *buf, int max) {
    asm_skip(L);
    if (L->pos >= L->len || L->src[L->pos] != '"') return 0;
    L->pos++;
    int p = 0;
    while (L->pos < L->len && L->src[L->pos] != '"' && p < max - 1) {
        char c = L->src[L->pos++];
        if (c == '\\' && L->pos < L->len) {
            char e = L->src[L->pos++];
            if (e == 'n') c = '\n';
            else if (e == 't') c = '\t';
            else if (e == 'r') c = '\r';
            else c = e;
        }
        buf[p++] = c;
    }
    if (L->pos < L->len && L->src[L->pos] == '"') L->pos++;
    buf[p] = 0;
    return p;
}

/* компилятор */
#define MAX_LABELS 64
#define MAX_STRINGS 32
#define MAX_CODE 4096
typedef struct { char name[16]; int offset; } label_t;
typedef struct { char text[64]; int len; } string_t;
typedef struct {
    asm_lex_t lex;
    u8 code[MAX_CODE]; int code_len;
    label_t labels[MAX_LABELS]; int nlabels;
    string_t strings[MAX_STRINGS]; int nstrings;
    int icons[256]; int nicons;
    int err;
    asyxbox_ctx_t *ctx;
} asm_comp_t;

static void asm_error(asm_comp_t *C, const char *msg) {
    if (C->err) return;
    C->err = 1;
    char buf[120]; int p = 0;
    const char *pre = "ASM ERROR line ";
    while (*pre && p < 100) buf[p++] = *pre++;
    int v = C->lex.line;
    char t[8]; int n = 0;
    if (v == 0) t[n++] = '0';
    while (v && n < 8) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0 && p < 100) buf[p++] = t[--n];
    buf[p++] = ':'; buf[p++] = ' ';
    while (*msg && p < 110) buf[p++] = *msg++;
    buf[p] = 0;
    C->ctx->print_line(buf, 240, 110, 110);
}
static void asm_emit(asm_comp_t *C, u8 op) {
    if (C->code_len < MAX_CODE) C->code[C->code_len++] = op;
    else asm_error(C, "code overflow");
}
static void asm_emit_i32(asm_comp_t *C, int v) {
    asm_emit(C, v & 255); asm_emit(C, (v >> 8) & 255);
    asm_emit(C, (v >> 16) & 255); asm_emit(C, (v >> 24) & 255);
}
static int asm_find_label(asm_comp_t *C, const char *name) {
    for (int i = 0; i < C->nlabels; i++) {
        int match = 1;
        for (int j = 0; C->labels[i].name[j] || name[j]; j++)
            if (C->labels[i].name[j] != name[j]) { match = 0; break; }
        if (match) return i;
    }
    return -1;
}
static int asm_add_label(asm_comp_t *C, const char *name, int offset) {
    if (C->nlabels >= MAX_LABELS) { asm_error(C, "too many labels"); return -1; }
    int idx = C->nlabels++;
    for (int i = 0; i < 15 && name[i]; i++) C->labels[idx].name[i] = name[i];
    C->labels[idx].name[15] = 0;
    C->labels[idx].offset = offset;
    return idx;
}
static int asm_add_string(asm_comp_t *C, const char *text, int len) {
    if (C->nstrings >= MAX_STRINGS) { asm_error(C, "too many strings"); return -1; }
    int idx = C->nstrings++;
    for (int i = 0; i < len && i < 63; i++) C->strings[idx].text[i] = text[i];
    C->strings[idx].text[len < 63 ? len : 63] = 0;
    C->strings[idx].len = len < 63 ? len : 63;
    return idx;
}
static int asm_add_icon(asm_comp_t *C, int v) {
    for (int i = 0; i < C->nicons; i++) if (C->icons[i] == v) return i;
    if (C->nicons < 256) { C->icons[C->nicons++] = v; return C->nicons - 1; }
    asm_error(C, "const pool full");
    return 0;
}

static void asm_parse(asm_comp_t *C) {
    char word[16];
    while (!asm_at_end(&C->lex)) {
        int n = asm_read_word(&C->lex, word, 16);
        if (n == 0) { asm_error(C, "expected instruction"); return; }
        asm_skip(&C->lex);
        if (C->lex.pos < C->lex.len && C->lex.src[C->lex.pos] == ':') {
            C->lex.pos++;
            asm_add_label(C, word, C->code_len);
            continue;
        }
        if (word[0]=='p'&&word[1]=='u'&&word[2]=='s'&&word[3]=='h') {
            asm_skip(&C->lex);
            if (C->lex.pos < C->lex.len && C->lex.src[C->lex.pos] == '"') {
                char s[64];
                int slen = asm_read_string(&C->lex, s, 64);
                asm_emit(C, OP_PUSHS); asm_emit_i32(C, asm_add_string(C, s, slen));
            } else {
                int v = asm_read_number(&C->lex);
                asm_emit(C, OP_PUSHC); asm_emit_i32(C, asm_add_icon(C, v));
            }
        }
        else if (word[0]=='p'&&word[1]=='o'&&word[2]=='p') asm_emit(C, OP_POP);
        else if (word[0]=='a'&&word[1]=='d'&&word[2]=='d') asm_emit(C, OP_ADD);
        else if (word[0]=='s'&&word[1]=='u'&&word[2]=='b') asm_emit(C, OP_SUB);
        else if (word[0]=='m'&&word[1]=='u'&&word[2]=='l') asm_emit(C, OP_MUL);
        else if (word[0]=='d'&&word[1]=='i'&&word[2]=='v') asm_emit(C, OP_DIV);
        else if (word[0]=='m'&&word[1]=='o'&&word[2]=='d') asm_emit(C, OP_MOD);
        else if (word[0]=='n'&&word[1]=='e'&&word[2]=='g') asm_emit(C, OP_NEG);
        else if (word[0]=='n'&&word[1]=='o'&&word[2]=='t') asm_emit(C, OP_NOT);
        else if (word[0]=='e'&&word[1]=='q') asm_emit(C, OP_EQ);
        else if (word[0]=='n'&&word[1]=='e') asm_emit(C, OP_NE);
        else if (word[0]=='l'&&word[1]=='t') asm_emit(C, OP_LT);
        else if (word[0]=='l'&&word[1]=='e') asm_emit(C, OP_LE);
        else if (word[0]=='g'&&word[1]=='t') asm_emit(C, OP_GT);
        else if (word[0]=='g'&&word[1]=='e') asm_emit(C, OP_GE);
        else if (word[0]=='a'&&word[1]=='n'&&word[2]=='d') asm_emit(C, OP_AND);
        else if (word[0]=='o'&&word[1]=='r') asm_emit(C, OP_OR);
        else if (word[0]=='s'&&word[1]=='t'&&word[2]=='o'&&word[3]=='r'&&word[4]=='e') {
            int addr = asm_read_number(&C->lex);
            asm_emit(C, OP_STORE); asm_emit_i32(C, addr);
        }
        else if (word[0]=='l'&&word[1]=='o'&&word[2]=='a'&&word[3]=='d') {
            int addr = asm_read_number(&C->lex);
            asm_emit(C, OP_LOAD); asm_emit_i32(C, addr);
        }
        else if (word[0]=='j'&&word[1]=='m'&&word[2]=='p') {
            char label[16]; asm_read_word(&C->lex, label, 16);
            int lidx = asm_find_label(C, label);
            if (lidx < 0) { asm_error(C, "undefined label"); return; }
            asm_emit(C, OP_JMP); asm_emit_i32(C, C->labels[lidx].offset);
        }
        else if (word[0]=='j'&&word[1]=='z') {
            char label[16]; asm_read_word(&C->lex, label, 16);
            int lidx = asm_find_label(C, label);
            if (lidx < 0) { asm_error(C, "undefined label"); return; }
            asm_emit(C, OP_JZ); asm_emit_i32(C, C->labels[lidx].offset);
        }
        else if (word[0]=='c'&&word[1]=='a'&&word[2]=='l'&&word[3]=='l') {
            int bid = asm_read_number(&C->lex);
            asm_emit(C, OP_CALL); asm_emit(C, bid); asm_emit(C, 1);
        }
        else if (word[0]=='h'&&word[1]=='a'&&word[2]=='l'&&word[3]=='t') asm_emit(C, OP_HALT);
        else { asm_error(C, "unknown instruction"); return; }
        while (C->lex.pos < C->lex.len && C->lex.src[C->lex.pos] != '\n') C->lex.pos++;
    }
    asm_emit(C, OP_HALT);
}

int asmxbox_compile(const char *src, int len, u8 *out, int outmax, asyxbox_ctx_t *ctx) {
    static asm_comp_t C;
    for (unsigned i = 0; i < sizeof(C); i++) ((u8*)&C)[i] = 0;
    C.ctx = ctx;
    C.lex.src = src; C.lex.len = len; C.lex.pos = 0; C.lex.line = 1;
    asm_parse(&C);
    if (C.err) return -1;
    int nicon = C.nicons, nstr = C.nstrings, codelen = C.code_len;
    int splen = 0;
    for (int i = 0; i < nstr; i++) splen += 4 + C.strings[i].len;
    int need = 24 + nicon * 4 + splen + codelen;
    if (need > outmax) { ctx->print_line("ASM: IMAGE TOO BIG", 240, 110, 110); return -1; }
    out[0]='A'; out[1]='S'; out[2]='M'; out[3]='1';
    out[4]=0; out[5]=0; out[6]=0; out[7]=0;
    out[8]=nicon&255; out[9]=(nicon>>8)&255; out[10]=(nicon>>16)&255; out[11]=(nicon>>24)&255;
    out[12]=nstr&255; out[13]=(nstr>>8)&255; out[14]=(nstr>>16)&255; out[15]=(nstr>>24)&255;
    out[16]=codelen&255; out[17]=(codelen>>8)&255; out[18]=(codelen>>16)&255; out[19]=(codelen>>24)&255;
    out[20]=splen&255; out[21]=(splen>>8)&255; out[22]=(splen>>16)&255; out[23]=(splen>>24)&255;
    int p = 24;
    for (int i = 0; i < nicon; i++) {
        int v = C.icons[i];
        out[p++]=v&255; out[p++]=(v>>8)&255; out[p++]=(v>>16)&255; out[p++]=(v>>24)&255;
    }
    for (int i = 0; i < nstr; i++) {
        int L = C.strings[i].len;
        out[p++]=L&255; out[p++]=(L>>8)&255; out[p++]=(L>>16)&255; out[p++]=(L>>24)&255;
        for (int j = 0; j < L; j++) out[p++] = C.strings[i].text[j];
    }
    for (int i = 0; i < codelen; i++) out[p++] = C.code[i];
    return need;
}

/*рантайм*/
static int asm_run(const u8 *img, int len, asyxbox_ctx_t *ctx) {
    if (len < 24 || img[0]!='A' || img[1]!='S' || img[2]!='M') {
        ctx->print_line("ASM: BAD IMAGE", 240, 110, 110);
        return -1;
    }
    int nicon = img[8] | (img[9]<<8) | (img[10]<<16) | (img[11]<<24);
    int nstr  = img[12] | (img[13]<<8) | (img[14]<<16) | (img[15]<<24);
    int clen  = img[16] | (img[17]<<8) | (img[18]<<16) | (img[19]<<24);
    int splen = img[20] | (img[21]<<8) | (img[22]<<16) | (img[23]<<24);
    int p = 24;
    int icons[256];
    for (int i = 0; i < nicon && i < 256; i++) {
        icons[i] = img[p] | (img[p+1]<<8) | (img[p+2]<<16) | (img[p+3]<<24);
        p += 4;
    }
    const u8 *spool = img + p;
    int soff[32], slen[32];
    int q = 0;
    for (int i = 0; i < nstr && i < 32; i++) {
        int L = spool[q] | (spool[q+1]<<8) | (spool[q+2]<<16) | (spool[q+3]<<24);
        soff[i] = q + 4; slen[i] = L;
        q += 4 + L;
    }
    const u8 *code = img + p + splen;
    int st[256]; int mem[64];
    int sp = 0, pc = 0, guard = 0, key_check = 0;
    for (int i = 0; i < 64; i++) mem[i] = 0;

    while (pc < clen && guard < 1000000) {
        guard++;
        if (g_asm_interrupted) {
            ctx->print_line("ASM: INTERRUPTED", 255, 220, 90);
            g_asm_interrupted = 0;
            return 0;
        }
        key_check++;
        if (key_check >= 256) {
            key_check = 0;
            if (asm_break(ctx)) return 0;
            asm_pump(ctx);
        }
        u8 op = code[pc++];
        switch (op) {
        case OP_HALT: pc = clen; break;
        case OP_PUSHC: {
            int ix = code[pc] | (code[pc+1]<<8) | (code[pc+2]<<16) | (code[pc+3]<<24);
            pc += 4;
            if (ix >= 0 && ix < nicon && sp < 256) st[sp++] = icons[ix];
            break;
        }
        case OP_PUSHS: {
            int ix = code[pc] | (code[pc+1]<<8) | (code[pc+2]<<16) | (code[pc+3]<<24);
            pc += 4;
            if (sp < 256) st[sp++] = ix;
            break;
        }
        case OP_POP: if (sp > 0) sp--; break;
        case OP_STORE: {
            int a = code[pc] | (code[pc+1]<<8) | (code[pc+2]<<16) | (code[pc+3]<<24);
            pc += 4;
            if (sp > 0 && a >= 0 && a < 64) mem[a] = st[--sp];
            break;
        }
        case OP_LOAD: {
            int a = code[pc] | (code[pc+1]<<8) | (code[pc+2]<<16) | (code[pc+3]<<24);
            pc += 4;
            if (a >= 0 && a < 64 && sp < 256) st[sp++] = mem[a];
            break;
        }
        case OP_ADD: if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a+b;} break;
        case OP_SUB: if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a-b;} break;
        case OP_MUL: if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a*b;} break;
        case OP_DIV: if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=b!=0?a/b:0;} break;
        case OP_MOD: if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=b!=0?a%b:0;} break;
        case OP_NEG: if (sp>0) st[sp-1] = -st[sp-1]; break;
        case OP_NOT: if (sp>0) st[sp-1] = st[sp-1] ? 0 : 1; break;
        case OP_EQ:  if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a==b;} break;
        case OP_NE:  if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a!=b;} break;
        case OP_LT:  if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a<b;} break;
        case OP_LE:  if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a<=b;} break;
        case OP_GT:  if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a>b;} break;
        case OP_GE:  if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a>=b;} break;
        case OP_AND: if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a&&b;} break;
        case OP_OR:  if (sp>=2){int b=st[--sp],a=st[--sp]; st[sp++]=a||b;} break;
        case OP_JMP: {
            int addr = code[pc] | (code[pc+1]<<8) | (code[pc+2]<<16) | (code[pc+3]<<24);
            pc = addr;
            break;
        }
        case OP_JZ: {
            int addr = code[pc] | (code[pc+1]<<8) | (code[pc+2]<<16) | (code[pc+3]<<24);
            pc += 4;
            if (sp > 0 && !st[--sp]) pc = addr;
            break;
        }
        case OP_CALL: {
            u8 bid = code[pc++];
            u8 argc = code[pc++];
            int v = (argc > 0 && sp > 0) ? st[sp-1] : 0;
            sp -= argc;
            if (sp < 0) sp = 0;
            if (bid == BI_PRINT || bid == BI_PRINTLN) {
                asm_print_int(ctx, v);
            } else if (bid == BI_PUTCH) {
                char s[2] = {(char)v, 0};
                if (ctx->print_raw) ctx->print_raw(s, 220, 220, 220);
                else ctx->print_line(s, 220, 220, 220);
            } else if (bid == BI_WRITE || bid == BI_WRITELN) {
                if (v >= 0 && v < nstr) {
                    const u8 *s = spool + soff[v];
                    char buf[65];
                    int n = slen[v] < 64 ? slen[v] : 64;
                    for (int i = 0; i < n; i++) buf[i] = s[i];
                    buf[n] = 0;
                    ctx->print_line(buf, 220, 220, 220);
                }
} else if (bid == BI_RND) {
    unsigned lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    int m = v > 0 ? v : 1;
    if (sp < 256) st[sp++] = (int)((lo ^ hi) % (unsigned)m);
} else if (bid == BI_KEY) {
    if (sp < 256) st[sp++] = key_poll();
} else if (bid == BI_GETNUM) {
    int num = 0;
    for (;;) {
        int k = key_poll();
        if (k == KEY_NONE) { asm_pump(ctx); __asm__ volatile("pause"); continue; }
        if (k == KEY_ESC || k == 12) {
            ctx->print_line("ASM: INTERRUPTED", 255, 220, 90);
            return 0;
        }
        if (k == 'l' || k == 'L') {
            key_modifiers_t m; key_get_modifiers(&m);
            if (m.ctrl) { ctx->print_line("ASM: INTERRUPTED", 255, 220, 90); return 0; }
        }
        if (k == KEY_ENTER) break;
        if (k >= '0' && k <= '9' && num < 100000000) {
            num = num * 10 + (k - '0');
            char s[2] = {(char)k, 0};
            if (ctx->print_raw) ctx->print_raw(s, 220, 220, 220);
            asm_pump(ctx);
        }
    }
    ctx->print_line("", 220, 220, 220);
    if (sp < 256) st[sp++] = num;
} else if (bid == BI_DELAY) {
    unsigned long long start = rdtsc();
    unsigned long long ticks = (unsigned long long)(v > 0 ? v : 0) * 2000000ULL;
    while (rdtsc() - start < ticks) {
        asm_pump(ctx);
        __asm__ volatile("pause");
    }
}
            break;
        }
        default:
            ctx->print_line("ASM RUNTIME: BAD OPCODE", 240, 110, 110);
            return -1;
        }
    }
    if (guard >= 1000000) ctx->print_line("ASM: EXECUTION LIMIT", 240, 110, 110);
    return 0;
}

/* команда для терминала */
static u8 srcbuf[8192];
static u8 imgbuf[16384];

void asmxbox_cmd(asyxbox_ctx_t *ctx, const char *cmd, int len) {
    char from[96], path[96], name[40], to[40];
    int fn = -1, nn = -1, tn = -1;
    const char *keys[] = {"from", "name", "to"};
    int *results[] = {&fn, &nn, &tn};
    char *buffers[] = {from, name, to};
    int maxs[] = {96, 40, 40};
    for (int k = 0; k < 3; k++) {
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
        ctx->print_line("USAGE: ASM FROM <B00> NAME <PROG.ASM> [TO <PROG.BIN>]", 240, 110, 110);
        return;
    }
    int v = asyxbox_nav_resolve(fn > 0 ? from : "", path, 96);
    if (v < 0) { ctx->print_line("ASM: NO SUCH VOLUME - SEE DK", 240, 110, 110); return; }
    char full[160]; int p = 0;
    for (int i = 0; path[i] && p < 120; i++) full[p++] = path[i];
    if (p && path[0]) full[p++] = '/';
    for (int i = 0; name[i] && p < 150; i++) full[p++] = name[i];
    full[p] = 0;
    long got = fw_read(v, full, srcbuf, sizeof(srcbuf) - 1);
    if (got < 0) { ctx->print_line("ASM: CANNOT READ SOURCE", 240, 110, 110); return; }
    srcbuf[got] = 0;
    int ilen = asmxbox_compile((const char*)srcbuf, (int)got, imgbuf, sizeof(imgbuf), ctx);
    if (ilen < 0) return;
    char st[80]; int q = 0;
    const char *pre = "COMPILED ASM: ";
    while (*pre && q < 70) st[q++] = *pre++;
    int vv = ilen; char t[8]; int nq = 0;
    if (vv == 0) t[nq++] = '0';
    while (vv && nq < 8) { t[nq++] = (char)('0' + vv % 10); vv /= 10; }
    while (nq > 0 && q < 70) st[q++] = t[--nq];
    const char *suf = " BYTES";
    while (*suf && q < 78) st[q++] = *suf++;
    st[q] = 0;
    ctx->print_line(st, 120, 240, 140);
    if (tn > 0) {
        if (!fw_write(v, path, to, imgbuf, (unsigned long)ilen))
            ctx->print_line("ASM: CANNOT WRITE", 240, 110, 110);
        else
            ctx->print_line("SAVED BYTECODE TO DISK", 120, 240, 140);
        return;
    }
    ctx->print_line("--- ASM RUNTIME ---", 120, 210, 255);
    g_asm_interrupted = 0;
    asm_run(imgbuf, ilen, ctx);
    ctx->print_line("--- END ---", 120, 210, 255);
}

int asmxbox_run_image(const u8 *img, int len, asyxbox_ctx_t *ctx) {
    g_asm_interrupted = 0;
    return asm_run(img, len, ctx);
}
