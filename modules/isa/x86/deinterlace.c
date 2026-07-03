/*****************************************************************************
 * deinterlace.c: X86 deinterlacing functions
 *****************************************************************************
 * Copyright (C) 2011, 2026 VLC authors and VideoLAN
 * 
 * Author: Sam Hocevar <sam@zoy.org>                      (generic C routine)
 *         Sigmund Augdal Helberg <sigmunau@videolan.org> (MMXEXT, 3DNow, SSE2)
 *         Mazen Ewiss <mazenrory@gmail.com>         (moved functions to x86)
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
#include <stdint.h>
#include "../../video_filter/deinterlace/merge.h"
VLC_SSE
static void Merge8BitSSE2( void *_p_dest, const void *_p_s1, const void *_p_s2,
                    size_t i_bytes )
{
    uint8_t *p_dest = _p_dest;
    const uint8_t *p_s1 = _p_s1;
    const uint8_t *p_s2 = _p_s2;

    for( ; i_bytes > 0 && ((uintptr_t)p_s1 & 15); i_bytes-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ + 1) >> 1;

    for( ; i_bytes >= 16; i_bytes -= 16 )
    {
        __asm__  __volatile__( "movdqu %2,%%xmm1;"
                               "pavgb %1, %%xmm1;"
                               "movdqu %%xmm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) : "xmm1" );
        p_dest += 16;
        p_s1 += 16;
        p_s2 += 16;
    }

    for( ; i_bytes > 0; i_bytes-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ + 1) >> 1;
}

VLC_SSE
static void Merge16BitSSE2( void *_p_dest, const void *_p_s1, const void *_p_s2,
                     size_t i_bytes )
{
    uint16_t *p_dest = _p_dest;
    const uint16_t *p_s1 = _p_s1;
    const uint16_t *p_s2 = _p_s2;

    size_t i_words = i_bytes / 2;
    for( ; i_words > 0 && ((uintptr_t)p_s1 & 15); i_words-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ + 1) >> 1;

    for( ; i_words >= 8; i_words -= 8 )
    {
        __asm__  __volatile__( "movdqu %2,%%xmm1;"
                               "pavgw %1, %%xmm1;"
                               "movdqu %%xmm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) : "xmm1" );
        p_dest += 8;
        p_s1 += 8;
        p_s2 += 8;
    }

    for( ; i_words > 0; i_words-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ + 1) >> 1;
}


static void Probe(void *data)
{
    if (vlc_CPU_SSE2()) {
        struct deinterlace_functions *const f = data;

        f->merges[0] = Merge8BitSSE2;
        f->merges[1] = Merge16BitSSE2;
    }
}

vlc_module_begin()
    set_description("X86 optimisation for deinterlacing")
    set_cpu_funcs("deinterlace functions", Probe, 20)
vlc_module_end()
