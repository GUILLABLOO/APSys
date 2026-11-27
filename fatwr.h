/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
fatwr - APSys 00.01 */

#ifndef FATWR_H
#define FATWR_H

void fw_arm(int on);
int  fw_armed(void);
int  fw_mountable(int vol);
unsigned long fw_mkpath(int vol, const char *path);
int  fw_create(int vol, const char *path, const char *name, int isdir);
int  fw_write(int vol, const char *path, const char *name,
              const unsigned char *data, unsigned long len);
long fw_read(int vol, const char *path, unsigned char *buf, unsigned long max);
int  fw_remove(int vol, const char *path);
unsigned long fw_free_kb(int vol);
const char *fw_err(void);

#define FW_MAX_FILE_KB 128
#endif
