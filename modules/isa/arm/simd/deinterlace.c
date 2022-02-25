/*****************************************************************************
 * deinterlace.c: ARMv6 SIMD deinterlacing functions
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

void merge8_armv6(void *, const void *, const void *, size_t);
void merge16_armv6(void *, const void *, const void *, size_t);

static void Probe(void *data)
{
    if (vlc_CPU_ARMv6()) {
        struct deinterlace_functions *const f = data;

        f->merges[0] = merge8_armv6;
        f->merges[1] = merge16_armv6;
    }
}

vlc_module_begin()
    set_description("ARM SIMD optimisation for deinterlacing")
    set_cpu_funcs("deinterlace functions", Probe, 10)
vlc_module_end()
