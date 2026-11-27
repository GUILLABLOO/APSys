/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
fsc - APSys 00.01 */

#ifndef FSC_H
#define FSC_H
#include "asyxbox.h"
void fsc_cmd(asyxbox_ctx_t *ctx, const char *cmd, int len);
int fsc_format_geom(int port, unsigned long long lba, unsigned long long sectors, int want_fat);
#endif
