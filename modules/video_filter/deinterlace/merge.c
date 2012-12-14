/*****************************************************************************
 * merge.c : Merge (line blending) routines for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>                      (generic C routine)
 *         Sigmund Augdal Helberg <sigmunau@videolan.org> (MMXEXT, 3DNow, SSE2)
 *         Eric Petit <eric.petit@lapsus.org>             (Altivec)
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
#   include "config.h"
#endif

#include <stdlib.h>
#include <stdint.h>

#include <vlc_common.h>
#include <vlc_cpu.h>
#include "merge.h"

#ifdef CAN_COMPILE_MMXEXT
#   include "mmx.h"
#endif

#ifdef HAVE_ALTIVEC_H
#   include <altivec.h>
#endif

/*****************************************************************************
 * Merge (line blending) routines
 *****************************************************************************/

void Merge8BitGeneric( void *_p_dest, const void *_p_s1,
                       const void *_p_s2, size_t i_bytes )
{
    uint8_t *p_dest = _p_dest;
    const uint8_t *p_s1 = _p_s1;
    const uint8_t *p_s2 = _p_s2;

    for( ; i_bytes > 0; i_bytes-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;
}

void Merge16BitGeneric( void *_p_dest, const void *_p_s1,
                        const void *_p_s2, size_t i_bytes )
{
    uint16_t *p_dest = _p_dest;
    const uint16_t *p_s1 = _p_s1;
    const uint16_t *p_s2 = _p_s2;

    for( size_t i_words = i_bytes / 2; i_words > 0; i_words-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;
}

#if defined(CAN_COMPILE_MMXEXT)
VLC_MMX
void MergeMMXEXT( void *_p_dest, const void *_p_s1, const void *_p_s2,
                  size_t i_bytes )
{
    uint8_t *p_dest = _p_dest;
    const uint8_t *p_s1 = _p_s1;
    const uint8_t *p_s2 = _p_s2;

    for( ; i_bytes >= 8; i_bytes -= 8 )
    {
        __asm__  __volatile__( "movq %2,%%mm1;"
                               "pavgb %1, %%mm1;"
                               "movq %%mm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) : "mm1" );
        p_dest += 8;
        p_s1 += 8;
        p_s2 += 8;
    }

    for( ; i_bytes > 0; i_bytes-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;
}
#endif

#if defined(CAN_COMPILE_3DNOW)
VLC_MMX
void Merge3DNow( void *_p_dest, const void *_p_s1, const void *_p_s2,
                 size_t i_bytes )
{
    uint8_t *p_dest = _p_dest;
    const uint8_t *p_s1 = _p_s1;
    const uint8_t *p_s2 = _p_s2;

    for( ; i_bytes >= 8; i_bytes -= 8 )
    {
        __asm__  __volatile__( "movq %2,%%mm1;"
                               "pavgusb %1, %%mm1;"
                               "movq %%mm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) : "mm1" );
        p_dest += 8;
        p_s1 += 8;
        p_s2 += 8;
    }

    for( ; i_bytes > 0; i_bytes-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;
}
#endif

#if defined(CAN_COMPILE_SSE)
VLC_SSE
void Merge8BitSSE2( void *_p_dest, const void *_p_s1, const void *_p_s2,
                    size_t i_bytes )
{
    uint8_t *p_dest = _p_dest;
    const uint8_t *p_s1 = _p_s1;
    const uint8_t *p_s2 = _p_s2;

    for( ; i_bytes > 0 && ((uintptr_t)p_s1 & 15); i_bytes-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;

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
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;
}

VLC_SSE
void Merge16BitSSE2( void *_p_dest, const void *_p_s1, const void *_p_s2,
                     size_t i_bytes )
{
    uint16_t *p_dest = _p_dest;
    const uint16_t *p_s1 = _p_s1;
    const uint16_t *p_s2 = _p_s2;

    size_t i_words = i_bytes / 2;
    for( ; i_words > 0 && ((uintptr_t)p_s1 & 15); i_words-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;

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
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;
}

#endif

#ifdef CAN_COMPILE_C_ALTIVEC
void MergeAltivec( void *_p_dest, const void *_p_s1,
                   const void *_p_s2, size_t i_bytes )
{
    uint8_t *p_dest = _p_dest;
    const uint8_t *p_s1 = _p_s1;
    const uint8_t *p_s2 = _p_s2;
    uint8_t *p_end  = p_dest + i_bytes - 15;

    /* Use C until the first 16-bytes aligned destination pixel */
    while( (uintptr_t)p_dest & 0xF )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }

    if( ( (int)p_s1 & 0xF ) | ( (int)p_s2 & 0xF ) )
    {
        /* Unaligned source */
        vector unsigned char s1v, s2v, destv;
        vector unsigned char s1oldv, s2oldv, s1newv, s2newv;
        vector unsigned char perm1v, perm2v;

        perm1v = vec_lvsl( 0, p_s1 );
        perm2v = vec_lvsl( 0, p_s2 );
        s1oldv = vec_ld( 0, p_s1 );
        s2oldv = vec_ld( 0, p_s2 );

        while( p_dest < p_end )
        {
            s1newv = vec_ld( 16, p_s1 );
            s2newv = vec_ld( 16, p_s2 );
            s1v    = vec_perm( s1oldv, s1newv, perm1v );
            s2v    = vec_perm( s2oldv, s2newv, perm2v );
            s1oldv = s1newv;
            s2oldv = s2newv;
            destv  = vec_avg( s1v, s2v );
            vec_st( destv, 0, p_dest );

            p_s1   += 16;
            p_s2   += 16;
            p_dest += 16;
        }
    }
    else
    {
        /* Aligned source */
        vector unsigned char s1v, s2v, destv;

        while( p_dest < p_end )
        {
            s1v   = vec_ld( 0, p_s1 );
            s2v   = vec_ld( 0, p_s2 );
            destv = vec_avg( s1v, s2v );
            vec_st( destv, 0, p_dest );

            p_s1   += 16;
            p_s2   += 16;
            p_dest += 16;
        }
    }

    p_end += 15;

    while( p_dest < p_end )
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;
}
#endif

/*****************************************************************************
 * EndMerge routines
 *****************************************************************************/

#if defined(CAN_COMPILE_MMXEXT) || defined(CAN_COMPILE_SSE)
void EndMMX( void )
{
    __asm__ __volatile__( "emms" :: );
}
#endif

#if defined(CAN_COMPILE_3DNOW)
void End3DNow( void )
{
    __asm__ __volatile__( "femms" :: );
}
#endif
