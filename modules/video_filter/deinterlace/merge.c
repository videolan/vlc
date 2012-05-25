/*****************************************************************************
 * merge.c : Merge (line blending) routines for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2011 the VideoLAN team
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>                      (generic C routine)
 *         Sigmund Augdal Helberg <sigmunau@videolan.org> (MMXEXT, 3DNow, SSE2)
 *         Eric Petit <eric.petit@lapsus.org>             (Altivec)
 *         Rémi Denis-Courmont <remi@remlab.net>          (ARM NEON)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdlib.h>
#include <stdint.h>

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
                                                 "m" (*p_s2) );
        p_dest += 8;
        p_s1 += 8;
        p_s2 += 8;
    }

    for( ; i_bytes > 0; i_bytes-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;
}
#endif

#if defined(CAN_COMPILE_3DNOW)
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
                                                 "m" (*p_s2) );
        p_dest += 8;
        p_s1 += 8;
        p_s2 += 8;
    }

    for( ; i_bytes > 0; i_bytes-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ ) >> 1;
}
#endif

#if defined(CAN_COMPILE_SSE)
void MergeSSE2( void *_p_dest, const void *_p_s1, const void *_p_s2,
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
                                                 "m" (*p_s2) );
        p_dest += 16;
        p_s1 += 16;
        p_s2 += 16;
    }

    for( ; i_bytes > 0; i_bytes-- )
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

#ifdef __ARM_NEON__
void MergeNEON (void *restrict out, const void *in1,
                const void *in2, size_t n)
{
    uint8_t *outp = out;
    const uint8_t *in1p = in1;
    const uint8_t *in2p = in2;
    size_t mis = __MIN((16 - ((uintptr_t)outp & 15)) & 15, n);

    if (mis)
    {
        Merge8BitGeneric (outp, in1p, in2p, mis);
        outp += mis;
        in1p += mis;
        in2p += mis;
        n -= mis;
    }

    uint8_t *end = outp + (n & ~15);

    if ((((uintptr_t)in1p)|((uintptr_t)in2p)) & 15)
        while (outp < end)
            asm volatile (
                "vld1.u8  {q0-q1}, [%[in1]]!\n"
                "vld1.u8  {q2-q3}, [%[in2]]!\n"
                "vhadd.u8 q4, q0, q2\n"
                "vld1.u8  {q6-q7}, [%[in1]]!\n"
                "vhadd.u8 q5, q1, q3\n"
                "vld1.u8  {q8-q9}, [%[in2]]!\n"
                "vhadd.u8 q10, q6, q8\n"
                "vhadd.u8 q11, q7, q9\n"
                "vst1.u8  {q4-q5}, [%[out],:128]!\n"
                "vst1.u8  {q10-q11}, [%[out],:128]!\n"
                : [out] "+r" (outp), [in1] "+r" (in1p), [in2] "+r" (in2p)
                :
                : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7",
                  "q8", "q9", "q10", "q11", "memory");
    else
         while (outp < end)
            asm volatile (
                "vld1.u8  {q0-q1}, [%[in1],:128]!\n"
                "vld1.u8  {q2-q3}, [%[in2],:128]!\n"
                "vhadd.u8 q4, q0, q2\n"
                "vld1.u8  {q6-q7}, [%[in1],:128]!\n"
                "vhadd.u8 q5, q1, q3\n"
                "vld1.u8  {q8-q9}, [%[in2],:128]!\n"
                "vhadd.u8 q10, q6, q8\n"
                "vhadd.u8 q11, q7, q9\n"
                "vst1.u8  {q4-q5}, [%[out],:128]!\n"
                "vst1.u8  {q10-q11}, [%[out],:128]!\n"
                : [out] "+r" (outp), [in1] "+r" (in1p), [in2] "+r" (in2p)
                :
                : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7",
                  "q8", "q9", "q10", "q11", "memory");
    n &= 15;
    if (n)
        Merge8BitGeneric (outp, in1p, in2p, n);
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
