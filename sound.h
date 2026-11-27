/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
sound - APSys 00.01 */

#ifndef SOUND_H
#define SOUND_H

typedef unsigned int  snd_uint32_t;
typedef unsigned char snd_uint8_t;

void sound_init(void);
void sound_beep(snd_uint32_t freq_hz, snd_uint32_t ms);
void sound_boot_chime(void);
void sound_error_beep(void);

#endif
