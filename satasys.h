/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
satasys - APSys 00.01 */

#ifndef SATASYS_H
#define SATASYS_H

#define SATASYS_MAX_PORTS  16
#define SATASYS_MODEL_LEN  24

typedef struct {
    int present;
    int kind;
    int ahci_port;
    int io_base, io_ctrl;
    int slave;
    int slot;
    unsigned long long sectors;
    char model[SATASYS_MODEL_LEN];
    volatile unsigned char *abar;
} sata_port_t;

int satasys_init(void);
int satasys_count(void);
const sata_port_t *satasys_get(int i);
int satasys_read (int port, unsigned long long lba, int count, unsigned char *buf);
int satasys_write(int port, unsigned long long lba, int count, const unsigned char *buf);

#endif
