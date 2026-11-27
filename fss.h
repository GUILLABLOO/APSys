/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
fss - APSys 00.01 */

#ifndef FSS_H
#define FSS_H

#define FS_UNKNOWN   0
#define FS_FAT32     1
#define FS_9P        2
#define FS_EXT2      3
#define APSYS_FSS_DIRWALK

#define FSS_NAME_LEN 256

typedef struct {
    char name[FSS_NAME_LEN];
    int  is_dir;
    unsigned long size;
    unsigned long first_cluster;
    unsigned long inode;
    unsigned short create_time;
    unsigned short create_date;
} fss_dirent_t;

int  fss_detect_disk(int disk, unsigned long long start);
const char *fss_fs_name(int fs);
int  fss_list(int vol, fss_dirent_t *out, int max);
int  fss_list_dir(int vol, unsigned long handle, fss_dirent_t *out, int max);
long fss_read_file(int vol, unsigned long first_cluster, unsigned long size,
                   unsigned char *buf, unsigned long max);

#endif
