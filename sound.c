/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
sound - APSys 00.01 */

#include "sound.h"
#include "hda.h"

static int use_hda = 0;

void sound_init(void){ use_hda = hda_init(); }
void sound_beep(snd_uint32_t f, snd_uint32_t ms){
    (void)f; (void)ms;
    if (!use_hda) return;
}
void sound_boot_chime(void){}
void sound_error_beep(void){}
