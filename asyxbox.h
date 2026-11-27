/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
asyxbox - APSys 00.01 */

#ifndef ASYXBOX_H
#define ASYXBOX_H
#define ASYXBOX_SIGNAL_NONE            0
#define ASYXBOX_SIGNAL_SWITCH_TERMINAL 1
#define ASYXBOX_SIGNAL_SWITCH_ARTHE    2
#define ASYXBOX_SIGNAL_CLEAR           3
#define ASYXBOX_SIGNAL_OPEN_COLORS     4
#define ASYXBOX_SIGNAL_OPEN_CUBE       5
#define ASYXBOX_SIGNAL_OPEN_CUBE_4D    6
#define ASYXBOX_SIGNAL_OPEN_EDITOR     7
#define ASYXBOX_SIGNAL_OPEN_CALC       8
#define ASYXBOX_SIGNAL_OPEN_CHESS      9
#define ASYXBOX_SIGNAL_OPEN_BALLS     10
#define ASYXBOX_SIGNAL_OPEN_ASM       11
typedef unsigned int  uint32_t;
typedef unsigned char uint8_t;
typedef struct {
    uint32_t mem_lower_kb, mem_upper_kb;
    uint32_t virt_w, virt_h, hw_w, hw_h;
    uint8_t  bpp;
    void (*print_line)(const char *text, uint8_t r, uint8_t g, uint8_t b);
    void (*print_num)(const char *prefix, uint32_t val, const char *suffix, uint8_t r, uint8_t g, uint8_t b);
    void (*print_raw)(const char *text, uint8_t r, uint8_t g, uint8_t b);
    void (*pump)(void);
    int signal;
    int  edit_vol;
    char edit_path[96];
    char edit_name[32];
} asyxbox_ctx_t;
int asyxbox_nav_resolve(const char *arg, char *out, int max);
int asyxbox_exec(const char *cmd, int len, asyxbox_ctx_t *ctx);
#endif
