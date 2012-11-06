/*****************************************************************************
 * cpu.c: CPU capabilities for libavcodec
 *****************************************************************************
 * Copyright (C) 1999-2012 VLC authors and VideoLAN
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

#define HAVE_MMX 1
#include <libavcodec/avcodec.h>
#include "avcommon.h"

/**
 * Maps CPU capabilities computed by VLC to libav DSP mask.
 */
unsigned GetVlcDspMask( void )
{
    unsigned mask = 0;

#if defined (__i386__) || defined (__x86_64__)
    if( !vlc_CPU_MMX() )
        mask |= AV_CPU_FLAG_MMX;
    if( !vlc_CPU_MMXEXT() )
        mask |= AV_CPU_FLAG_MMXEXT;
    if( !vlc_CPU_3dNOW() )
        mask |= AV_CPU_FLAG_3DNOW;
    if( !vlc_CPU_SSE() )
        mask |= AV_CPU_FLAG_SSE;
    if( !vlc_CPU_SSE2() )
        mask |= AV_CPU_FLAG_SSE2;
# ifdef AV_CPU_FLAG_SSE3
    if( !vlc_CPU_SSE3() )
        mask |= AV_CPU_FLAG_SSE3;
# endif
# ifdef AV_CPU_FLAG_SSSE3
    if( !vlc_CPU_SSSE3() )
        mask |= AV_CPU_FLAG_SSSE3;
# endif
# ifdef AV_CPU_FLAG_SSE4
    if( !vlc_CPU_SSE4_1() )
        mask |= AV_CPU_FLAG_SSE4;
# endif
# ifdef AV_CPU_FLAG_SSE42
    if( !vlc_CPU_SSE4_2() )
        mask |= AV_CPU_FLAG_SSE42;
# endif
# ifdef AV_CPU_FLAG_AVX
    if( !vlc_CPU_AVX() )
        mask |= AV_CPU_FLAG_AVX;
# endif
# ifdef AV_CPU_FLAG_XOP
    if( !vlc_CPU_XOP() )
        mask |= AV_CPU_FLAG_XOP;
# endif
# ifdef AV_CPU_FLAG_FMA4
    if( !vlc_CPU_FMA4() )
        mask |= AV_CPU_FLAG_FMA4;
# endif
#endif

#if defined (__ppc__) || defined (__ppc64__) || defined (__powerpc__)
    if( !vlc_CPU_ALTIVEC() )
        mask |= AV_CPU_FLAG_ALTIVEC;
#endif

#if defined ( __arm__)
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51, 29, 0)
    if( !vlc_CPU_ARM_NEON() )
        mask |= AV_CPU_FLAG_NEON;
#endif
#endif

    return mask;
}
