/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
roomfs - APSys 00.01 */

#ifndef ROOMFS_H
#define ROOMFS_H

#define ROOMFS_MAX      192
#define ROOMFS_NAME_MAX 32
#define ROOMFS_PATH_MAX 96

typedef struct {
    char name[ROOMFS_NAME_MAX];
    int  parent;
    int  vol;
    int  is_dir;
    int  used;
    unsigned int   size;
    unsigned short ctime, cdate;
} roomfs_node_t;

typedef struct {
    int  vol;
    int  err;
    char path[ROOMFS_PATH_MAX];
} roomfs_ref_t;

void roomfs_set_boot_disk(int disk);
int  roomfs_vol_count(void);
int  roomfs_vol_of_addr(const char *addr);
void roomfs_addr_of_vol(int vol, char *out);
int  roomfs_system_vol(void);
int  roomfs_parse(const char *s, roomfs_ref_t *out);

int  roomfs_root(int vol);
int  roomfs_peek(int vol);
int  roomfs_find(int dir, const char *name);
int  roomfs_mkdir(int dir, const char *name);
int  roomfs_mkfile(int dir, const char *name);
int  roomfs_mkpath(int vol, const char *path);
int  roomfs_walk(int vol, const char *path);
int  roomfs_path(int idx, char *out, int max);
int  roomfs_child(int dir, int n);
roomfs_node_t *roomfs_get(int i);
#endif
