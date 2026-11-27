/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
asmxbox - APSys 00.01 */

#ifndef ASMXBOX_H
#define ASMXBOX_H
#include "asyxbox.h"

#define OP_HALT   0
#define OP_PUSHC  1
#define OP_LOAD   2
#define OP_STORE  3
#define OP_POP    4
#define OP_ADD    5
#define OP_SUB    6
#define OP_MUL    7
#define OP_DIV    8
#define OP_MOD    9
#define OP_NEG   10
#define OP_NOT   11
#define OP_EQ    12
#define OP_NE    13
#define OP_LT    14
#define OP_LE    15
#define OP_GT    16
#define OP_GE    17
#define OP_AND   18
#define OP_OR    19
#define OP_JMP   20
#define OP_JZ    21
#define OP_CALL  22
#define OP_PUSHS 25

#define BI_PRINT   0
#define BI_PRINTLN 1
#define BI_PUTCH   2
#define BI_WRITE   3
#define BI_WRITELN 4
#define BI_RND     5
#define BI_KEY     6
#define BI_GETNUM  7
#define BI_DELAY   8

int  asmxbox_compile(const char *src, int len, unsigned char *out, int outmax, asyxbox_ctx_t *ctx);
int asmxbox_run_image(const unsigned char *img, int len, asyxbox_ctx_t *ctx);
void asmxbox_cmd(asyxbox_ctx_t *ctx, const char *cmd, int len);
#endif
