/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
diskmgr - APSys 00.01 */

#ifndef DISKMGR_H
#define DISKMGR_H

#define DISKMGR_MAX_VOLUMES 32

typedef struct {
    int disk;
    int part;
    unsigned long long start;
    unsigned long long sectors;
    unsigned char type;
    int fs;
} volume_t;

void diskmgr_ensure(void);
int  diskmgr_volume_count(void);
const volume_t *diskmgr_volume(int i);
int  diskmgr_vol_geom(int vol, int *port, unsigned long *lba, unsigned long *sectors);
void diskmgr_redetect(int vol);

#endif
