/*****************************************************************************
 * deinterlace.c: ARM NEON deinterlacing functions
 *****************************************************************************
 * Copyright (C) 2022 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_cpu.h>
#include <vlc_plugin.h>
#include "../../../video_filter/deinterlace/merge.h"

void merge8_arm_neon(void *, const void *, const void *, size_t);
void merge16_arm_neon(void *, const void *, const void *, size_t);

static void Probe(void *data)
{
    if (vlc_CPU_ARM_NEON()) {
        struct deinterlace_functions *const f = data;

        f->merges[0] = merge8_arm_neon;
        f->merges[1] = merge16_arm_neon;
    }
}

vlc_module_begin()
    set_description("ARM NEON optimisation for deinterlacing")
    set_cpu_funcs("deinterlace functions", Probe, 20)
vlc_module_end()
