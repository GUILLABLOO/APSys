/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
asmc - APSys 00.01 */

#include "asmc.h"
#include "fatwr.h"
#include "key.h"
typedef unsigned char u8;
typedef unsigned int u32;

#define MAX_INSTR 512
#define MAX_VARS  128
#define MAX_BLOCKS 64
#define MAX_MEM   512
#define MAX_LINE  128
#define MAX_TOKENS 8
#define MAX_NAME  16

#define OP_MOV   1
#define OP_ADD   3
#define OP_SUB   4
#define OP_MUL   5
#define OP_DIV   6
#define OP_JMP   7
#define OP_IF    8
#define OP_READ  11
#define OP_PRINT 12
#define OP_HALT  13

#define CMP_EQ  1
#define CMP_LT  2
#define CMP_GT  3
#define CMP_LE  4
#define CMP_GE  5
#define CMP_NE  6

typedef struct { int opcode; int p[5]; } instr_t;
typedef struct { char name[MAX_NAME]; int addr; int size; int is_const; int const_val; } var_t;
typedef struct { char name[MAX_NAME]; int instr_no; } block_t;

static volatile int g_interrupted = 0;
void asmc_interrupt(void) { g_interrupted = 1; }
static instr_t  g_instr[MAX_INSTR];
static int      g_ninstr;
static var_t    g_vars[MAX_VARS];
static int      g_nvars;
static block_t  g_blocks[MAX_BLOCKS];
static int      g_nblocks;
static int      g_mem[MAX_MEM];
static int      g_mem_top;
static asyxbox_ctx_t *g_ctx;

static unsigned long long asm_rdtsc(void){
    unsigned lo,hi;
    __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    return ((unsigned long long)hi<<32)|lo;
}
static unsigned long long g_last_pump=0;
static void asm_pump(asyxbox_ctx_t *ctx){
    unsigned long long now=asm_rdtsc();
    if(now-g_last_pump<5000000ULL) return;
    g_last_pump=now;
    if(ctx&&ctx->pump) ctx->pump();
}
static int asm_break(asyxbox_ctx_t *ctx){
    int k=key_poll();
    if(k==KEY_NONE) return 0;
    if(k==KEY_ESC||k==12){
        ctx->print_line("ASMC: INTERRUPTED (ESC / CTRL+L)",255,220,90);
        return 1;
    }
    if(k=='l'||k=='L'){
        key_modifiers_t m; key_get_modifiers(&m);
        if(m.ctrl){
            ctx->print_line("ASMC: INTERRUPTED (CTRL+L)",255,220,90);
            return 1;
        }
    }
    return 0;
}
static void err(const char *m) {
    char buf[120]; int p = 0;
    const char *pre = "ASMC ERROR: ";
    while (*pre && p < 100) buf[p++] = *pre++;
    while (*m && p < 115) buf[p++] = *m++;
    buf[p] = 0;
    g_ctx->print_line(buf, 240, 110, 110);
}

static int kw_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] || b[i]) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
        i++;
    }
    return 1;
}

static int parse_int(const char *s) {
    int v = 0, neg = 0, i = 0;
    while (s[i] == ' ') i++;
    if (s[i] == '-') { neg = 1; i++; }
    while (s[i] >= '0' && s[i] <= '9') { v = v * 10 + (s[i] - '0'); i++; }
    return neg ? -v : v;
}

static int tokenize(const char *line, char tokens[MAX_TOKENS][MAX_NAME]) {
    for (int i = 0; i < MAX_TOKENS; i++) tokens[i][0] = 0;
    int ti = 0, i = 0;
    while (line[i] && ti < MAX_TOKENS) {
        while (line[i] == ' ' || line[i] == '\t' || line[i] == ',') i++;
        if (!line[i] || line[i] == ';') break;
        int ci = 0;
        while (line[i] && line[i] != ' ' && line[i] != '\t' &&
               line[i] != ',' && line[i] != ';' && ci < MAX_NAME - 1) {
            tokens[ti][ci++] = line[i++];
        }
        tokens[ti][ci] = 0;
        ti++;
    }
    return ti;
}

static int parse_operand(const char *s) {
    if (!s || !s[0]) return -1;
    if (s[1] == 'X' && s[2] == 0 && s[0] >= 'A' && s[0] <= 'H') {
        return s[0] - 'A';
    }
    char name[MAX_NAME]; int ni = 0;
    int idx = 0, has_idx = 0;
    int i = 0;
    while (s[i] && s[i] != '[' && ni < MAX_NAME - 1) { name[ni++] = s[i++]; }
    name[ni] = 0;
    if (s[i] == '[') {
        i++;
        while (s[i] && s[i] != ']') {
            idx = idx * 10 + (s[i] - '0');
            i++;
        }
        has_idx = 1;
    }
    for (int v = 0; v < g_nvars; v++) {
        if (kw_eq(g_vars[v].name, name)) {
            return g_vars[v].addr + (has_idx ? idx : 0);
        }
    }
    return -1;
}

static int add_var(const char *name, int size, int is_const, int const_val) {
    if (g_nvars >= MAX_VARS) { err("too many vars"); return -1; }
    int idx = g_nvars++;
    for (int i = 0; i < MAX_NAME - 1 && name[i]; i++) g_vars[idx].name[i] = name[i];
    g_vars[idx].name[MAX_NAME - 1] = 0;
    g_vars[idx].size = size > 0 ? size : 1;
    g_vars[idx].is_const = is_const;
    g_vars[idx].const_val = const_val;
    g_vars[idx].addr = g_mem_top;
    g_mem_top += g_vars[idx].size;
    return idx;
}

static int add_block(const char *name, int instr_no) {
    if (g_nblocks >= MAX_BLOCKS) { err("too many blocks"); return -1; }
    int idx = g_nblocks++;
    for (int i = 0; i < MAX_NAME - 1 && name[i]; i++) g_blocks[idx].name[i] = name[i];
    g_blocks[idx].name[MAX_NAME - 1] = 0;
    g_blocks[idx].instr_no = instr_no;
    return idx;
}

static int find_block(const char *name) {
    for (int i = 0; i < g_nblocks; i++) {
        if (kw_eq(g_blocks[i].name, name)) return g_blocks[i].instr_no;
    }
    return -1;
}

static int parse_cmp_op(const char *s) {
    if (kw_eq(s, "eq")) return CMP_EQ;
    if (kw_eq(s, "lt")) return CMP_LT;
    if (kw_eq(s, "gt")) return CMP_GT;
    if (kw_eq(s, "lteq")) return CMP_LE;
    if (kw_eq(s, "gteq")) return CMP_GE;
    if (kw_eq(s, "neq")) return CMP_NE;
    return -1;
}

static int compile(const char *src, int len) {
    g_ninstr = 0; g_nvars = 0; g_nblocks = 0;
    g_mem_top = 8;
    for (int i = 0; i < MAX_MEM; i++) g_mem[i] = 0;

    const char *p = src;
    const char *end = src + len;

    while (p < end) {
        char line[MAX_LINE];
        int li = 0;
        while (p < end && *p != '\n' && li < MAX_LINE - 1) {
            if (*p != '\r') line[li++] = *p;
            p++;
        }
        line[li] = 0;
        if (p < end) p++;

        int si = 0;
        while (line[si] == ' ' || line[si] == '\t') si++;
        if (line[si] == 0 || line[si] == ';') continue;

        if (li > 0 && line[li - 1] == ':') {
            line[li - 1] = 0;
            int e = li - 2;
            while (e >= si && (line[e] == ' ' || line[e] == '\t')) line[e--] = 0;
            add_block(line + si, g_ninstr);
            continue;
        }

        char tokens[MAX_TOKENS][MAX_NAME];
        int nt = tokenize(line + si, tokens);
        if (nt == 0) continue;

        if (kw_eq(tokens[0], "data")) {
            char name[MAX_NAME]; int ni2 = 0, size = 1, i = 0;
            while (tokens[1][i] && tokens[1][i] != '[') { name[ni2++] = tokens[1][i++]; }
            name[ni2] = 0;
            if (tokens[1][i] == '[') {
                i++; size = 0;
                while (tokens[1][i] && tokens[1][i] != ']') {
                    size = size * 10 + (tokens[1][i] - '0'); i++;
                }
            }
            add_var(name, size, 0, 0);
            continue;
        }

        if (kw_eq(tokens[0], "const")) {
            int v = parse_int(tokens[3]);
            add_var(tokens[1], 1, 1, v);
            continue;
        }

        if (kw_eq(tokens[0], "read")) {
            int addr = parse_operand(tokens[1]);
            if (addr < 0) { err("unknown var in READ"); return -1; }
            g_instr[g_ninstr].opcode = OP_READ;
            g_instr[g_ninstr].p[0] = addr;
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "print")) {
            int addr = parse_operand(tokens[1]);
            if (addr < 0) { err("unknown var in PRINT"); return -1; }
            g_instr[g_ninstr].opcode = OP_PRINT;
            g_instr[g_ninstr].p[0] = addr;
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "mov")) {
            int d = parse_operand(tokens[1]);
            int s = parse_operand(tokens[2]);
            if (d < 0 || s < 0) { err("unknown var in MOV"); return -1; }
            g_instr[g_ninstr].opcode = OP_MOV;
            g_instr[g_ninstr].p[0] = d;
            g_instr[g_ninstr].p[1] = s;
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "add")) {
            int d = parse_operand(tokens[1]);
            int a = parse_operand(tokens[2]);
            int b = parse_operand(tokens[3]);
            if (d < 0) { err("unknown dest in ADD"); return -1; }
            if (a < 0) { err("unknown operand A in ADD"); return -1; }
            if (b < 0) { err("unknown operand B in ADD"); return -1; }
            g_instr[g_ninstr].opcode = OP_ADD;
            g_instr[g_ninstr].p[0] = d;
            g_instr[g_ninstr].p[1] = a;
            g_instr[g_ninstr].p[2] = b;
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "sub")) {
            int d = parse_operand(tokens[1]);
            int a = parse_operand(tokens[2]);
            int b = parse_operand(tokens[3]);
            if (d < 0) { err("unknown dest in SUB"); return -1; }
            if (a < 0) { err("unknown operand A in SUB"); return -1; }
            if (b < 0) { err("unknown operand B in SUB"); return -1; }
            g_instr[g_ninstr].opcode = OP_SUB;
            g_instr[g_ninstr].p[0] = d;
            g_instr[g_ninstr].p[1] = a;
            g_instr[g_ninstr].p[2] = b;
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "mul")) {
            int d = parse_operand(tokens[1]);
            int a = parse_operand(tokens[2]);
            int b = parse_operand(tokens[3]);
            if (d < 0 || a < 0 || b < 0) { err("unknown var in MUL"); return -1; }
            g_instr[g_ninstr].opcode = OP_MUL;
            g_instr[g_ninstr].p[0] = d;
            g_instr[g_ninstr].p[1] = a;
            g_instr[g_ninstr].p[2] = b;
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "div")) {
            int d = parse_operand(tokens[1]);
            int a = parse_operand(tokens[2]);
            int b = parse_operand(tokens[3]);
            if (d < 0 || a < 0 || b < 0) { err("unknown var in DIV"); return -1; }
            g_instr[g_ninstr].opcode = OP_DIV;
            g_instr[g_ninstr].p[0] = d;
            g_instr[g_ninstr].p[1] = a;
            g_instr[g_ninstr].p[2] = b;
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "jump")) {
            g_instr[g_ninstr].opcode = OP_JMP;
            g_instr[g_ninstr].p[0] = -2;
            g_instr[g_ninstr].p[1] = g_ninstr;
            for (int i = 0; i < MAX_NAME - 1 && tokens[1][i]; i++) {
                int slot = 2 + i / 4;
                g_instr[g_ninstr].p[slot] = (g_instr[g_ninstr].p[slot] << 8) | tokens[1][i];
            }
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "if")) {
            int a = parse_operand(tokens[1]);
            int b = parse_operand(tokens[3]);
            int op = parse_cmp_op(tokens[2]);
            if (a < 0) { err("unknown var A in IF"); return -1; }
            if (b < 0) { err("unknown var B in IF"); return -1; }
            if (op < 0) { err("unknown cmp op in IF"); return -1; }
            g_instr[g_ninstr].opcode = OP_IF;
            g_instr[g_ninstr].p[0] = a;
            g_instr[g_ninstr].p[1] = b;
            g_instr[g_ninstr].p[2] = op;
            g_instr[g_ninstr].p[3] = -2;
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "else")) {
            for (int i = g_ninstr - 1; i >= 0; i--) {
                if (g_instr[i].opcode == OP_IF && g_instr[i].p[3] == -2) {
                    g_instr[i].p[3] = g_ninstr + 1;
                    break;
                }
            }
            g_instr[g_ninstr].opcode = OP_JMP;
            g_instr[g_ninstr].p[0] = -2;
            g_instr[g_ninstr].p[1] = g_ninstr;
            g_ninstr++;
            continue;
        }

        if (kw_eq(tokens[0], "endif")) {
            int done = 0;
            for (int i = g_ninstr - 1; i >= 0; i--) {
                if (g_instr[i].opcode == OP_JMP && g_instr[i].p[0] == -2 &&
                    g_instr[i].p[1] == i) {
                    g_instr[i].p[0] = g_ninstr;
                    done = 1;
                    break;
                }
            }
            if (!done) {
                for (int i = g_ninstr - 1; i >= 0; i--) {
                    if (g_instr[i].opcode == OP_IF && g_instr[i].p[3] == -2) {
                        g_instr[i].p[3] = g_ninstr;
                        break;
                    }
                }
            }
            continue;
        }

        if (kw_eq(tokens[0], "end")) {
            g_instr[g_ninstr].opcode = OP_HALT;
            g_ninstr++;
            continue;
        }

        err("unknown instruction");
        return -1;
    }

    for (int i = 0; i < g_ninstr; i++) {
        if (g_instr[i].opcode == OP_JMP && g_instr[i].p[1] == i &&
            g_instr[i].p[0] == -2) {
            char name[MAX_NAME]; int ni2 = 0;
            for (int j = 2; j < 5 && ni2 < MAX_NAME - 1; j++) {
                int v = g_instr[i].p[j];
                for (int k = 3; k >= 0 && ni2 < MAX_NAME - 1; k--) {
                    char c = (v >> (k * 8)) & 0xFF;
                    if (c) name[ni2++] = c;
                }
            }
            name[ni2] = 0;
            int addr = find_block(name);
            if (addr < 0) { err("unknown label in JUMP"); return -1; }
            g_instr[i].p[0] = addr;
        }
    }

    return 0;
}

static void print_int(int v) {
    char buf[16]; int n = 0;
    if (v < 0) { buf[n++] = '-'; v = -v; }
    if (v == 0) buf[n++] = '0';
    int tmp[10], tn = 0;
    while (v && tn < 10) { tmp[tn++] = v % 10; v /= 10; }
    while (tn > 0) buf[n++] = '0' + tmp[--tn];
    buf[n] = 0;
    g_ctx->print_line(buf, 220, 220, 220);
}

static void execute(void) {
if (g_interrupted) {
    g_ctx->print_line("ASMC: INTERRUPTED", 255, 220, 90);
    g_interrupted = 0;
    return;
}
    int pc = 0, guard = 0, key_check = 0;
    while (pc < g_ninstr && guard < 1000000) {
        guard++;
        key_check++;
        if (key_check >= 256) {
            key_check = 0;
            if (asm_break(g_ctx)) return;
            asm_pump(g_ctx);
        }
        instr_t *I = &g_instr[pc];
        switch (I->opcode) {
        case OP_MOV:
            g_mem[I->p[0]] = g_mem[I->p[1]];
            break;
        case OP_ADD:
            g_mem[I->p[0]] = g_mem[I->p[1]] + g_mem[I->p[2]];
            break;
        case OP_SUB:
            g_mem[I->p[0]] = g_mem[I->p[1]] - g_mem[I->p[2]];
            break;
        case OP_MUL:
            g_mem[I->p[0]] = g_mem[I->p[1]] * g_mem[I->p[2]];
            break;
        case OP_DIV:
            g_mem[I->p[0]] = g_mem[I->p[2]] != 0 ? g_mem[I->p[1]] / g_mem[I->p[2]] : 0;
            break;
        case OP_JMP:
            pc = I->p[0];
            continue;
        case OP_IF: {
            int a = g_mem[I->p[0]];
            int b = g_mem[I->p[1]];
            int op = I->p[2];
            int cond = 0;
            if (op == CMP_EQ) cond = (a == b);
            else if (op == CMP_LT) cond = (a < b);
            else if (op == CMP_GT) cond = (a > b);
            else if (op == CMP_LE) cond = (a <= b);
            else if (op == CMP_GE) cond = (a >= b);
            else if (op == CMP_NE) cond = (a != b);
            if (!cond) { pc = I->p[3]; continue; }
            break;
        }
        case OP_PRINT:
            print_int(g_mem[I->p[0]]);
            break;
        case OP_READ: {
            int v = 0;
            for (;;) {
                int k = key_poll();
                if (k == KEY_NONE) { asm_pump(g_ctx); __asm__ volatile("pause"); continue; }
                if (asm_break(g_ctx)) return;
                if (k == KEY_ENTER) break;
                if (k >= '0' && k <= '9' && v < 100000000) {
                    v = v * 10 + (k - '0');
                    char s[2] = {(char)k, 0};
                    g_ctx->print_line(s, 220, 220, 220);
                }
            }
            g_ctx->print_line("", 220, 220, 220);
            g_mem[I->p[0]] = v;
            break;
        }
        case OP_HALT:
            return;
        }
        pc++;
    }
    if (guard >= 1000000) g_ctx->print_line("ASMC: EXECUTION LIMIT", 240, 110, 110);
}

void asmc_cmd(asyxbox_ctx_t *ctx, const char *cmd, int len) {
    g_ctx = ctx;
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
        ctx->print_line("USAGE: ASMC FROM <B00> NAME <PROG.ASM>", 240, 110, 110);
        return;
    }

    int v = asyxbox_nav_resolve(fn > 0 ? from : "", path, 96);
    if (v < 0) {
        ctx->print_line("ASMC: NO SUCH VOLUME", 240, 110, 110);
        return;
    }

    char full[160];
    int p = 0;
    for (int i = 0; path[i] && p < 120; i++) full[p++] = path[i];
    if (p && path[0]) full[p++] = '/';
    for (int i = 0; name[i] && p < 150; i++) full[p++] = name[i];
    full[p] = 0;

    static u8 srcbuf[8192];
    long got = fw_read(v, full, srcbuf, sizeof(srcbuf) - 1);
    if (got < 0) {
        ctx->print_line("ASMC: CANNOT READ SOURCE", 240, 110, 110);
        return;
    }
    srcbuf[got] = 0;

    ctx->print_line("--- ASMC COMPILING ---", 120, 210, 255);
    if (compile((const char*)srcbuf, (int)got) < 0) return;

    char st[40]; int q = 0;
    const char *pre = "COMPILED: ";
    while (*pre && q < 30) st[q++] = *pre++;
    int vv = g_ninstr;
    char t[8]; int nq = 0;
    if (vv == 0) t[nq++] = '0';
    while (vv && nq < 8) { t[nq++] = (char)('0' + vv % 10); vv /= 10; }
    while (nq > 0 && q < 30) st[q++] = t[--nq];
    const char *suf = " INSTR";
    while (*suf && q < 38) st[q++] = *suf++;
    st[q] = 0;
    ctx->print_line(st, 120, 240, 140);

    ctx->print_line("--- EXECUTING ---", 120, 210, 255);
g_interrupted = 0;
    execute();
    ctx->print_line("--- END ---", 120, 210, 255);
}
