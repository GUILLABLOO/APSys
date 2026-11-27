/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
asmc - APSys 00.01 */

#ifndef ASMC_H
#define ASMC_H
#include "asyxbox.h"
void asmc_cmd(asyxbox_ctx_t *ctx, const char *cmd, int len);
void asmc_interrupt(void);
#endif
