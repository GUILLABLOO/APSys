/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
hda - APSys 00.01 */

#ifndef HDA_H
#define HDA_H

typedef unsigned int  hda_u32;
typedef unsigned char hda_u8;

void hda_diag(void (*pl)(const char*, hda_u8, hda_u8, hda_u8),
              void (*pn)(const char*, hda_u32, const char*, hda_u8, hda_u8, hda_u8));

int  hda_init(void);
void hda_play(const short* samples, unsigned int count);

#endif
