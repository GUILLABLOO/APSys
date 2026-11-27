/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
asb - APSys 00.01 */
#ifndef ASB_H
#define ASB_H
#include "asyxbox.h"
#define ASB_BASE 0x01000000u
#define ASB_MAX  (256*1024)
void asb_cmd(asyxbox_ctx_t *ctx, const char *cmd, int len);
#endif
