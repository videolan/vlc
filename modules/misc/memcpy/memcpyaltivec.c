/*****************************************************************************
 * memcpyaltivec.c : AltiVec memcpy module
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
 *
 * Author: Christophe Massiot <massiot@via.ecp.fr>
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

#ifndef __BUILD_ALTIVEC_ASM__

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#ifdef HAVE_ALTIVEC_H
#   include <altivec.h>
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void * fast_memcpy ( void * to, const void * from, size_t len );

/*****************************************************************************
 * Module initializer.
 *****************************************************************************/
static int Activate ( vlc_object_t *p_this )
{
    vlc_fastmem_register( fast_memcpy, NULL );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("AltiVec memcpy") );
    set_category( CAT_ADVANCED );
    set_subcategory( SUBCAT_ADVANCED_MISC );
    add_requirement( ALTIVEC );
    set_capability( "memcpy", 100 );
    set_callbacks( Activate, NULL );
    add_shortcut( "altivec" );
vlc_module_end();

#else
typedef unsigned long size_t;
#endif /* __BUILD_ALTIVEC_ASM__ */

#if defined(CAN_COMPILE_C_ALTIVEC) || defined( __BUILD_ALTIVEC_ASM__ )

#define vector_s16_t vector signed short
#define vector_u16_t vector unsigned short
#define vector_s8_t vector signed char
#define vector_u8_t vector unsigned char
#define vector_s32_t vector signed int
#define vector_u32_t vector unsigned int
#define MMREG_SIZE 16

#define SMALL_MEMCPY(to, from, len)                                         \
{                                                                           \
    unsigned char * end = to + len;                                         \
    while( to < end )                                                       \
    {                                                                       \
        *to++ = *from++;                                                    \
    }                                                                       \
}

static void * fast_memcpy( void * _to, const void * _from, size_t len )
{
    void * retval = _to;
    unsigned char * to = (unsigned char *)_to;
    unsigned char * from = (unsigned char *)_from;

    if( len > 16 )
    {
        /* Align destination to MMREG_SIZE -boundary */
        register unsigned long int delta;

        delta = ((unsigned long)to)&(MMREG_SIZE-1);
        if( delta )
        {
            delta = MMREG_SIZE - delta;
            len -= delta;
            SMALL_MEMCPY(to, from, delta);
        }

        if( len & ~(MMREG_SIZE-1) )
        {
            vector_u8_t perm, ref0, ref1, tmp;

            perm = vec_lvsl( 0, from );
            ref0 = vec_ld( 0, from );
            ref1 = vec_ld( 15, from );
            from += 16;
            len -= 16;
            tmp = vec_perm( ref0, ref1, perm );
            while( len & ~(MMREG_SIZE-1) )
            {
                ref0 = vec_ld( 0, from );
                ref1 = vec_ld( 15, from );
                from += 16;
                len -= 16;
                vec_st( tmp, 0, to );
                tmp = vec_perm( ref0, ref1, perm );
                to += 16;
            }
            vec_st( tmp, 0, to );
            to += 16;
        }
    }

    if( len )
    {
        SMALL_MEMCPY( to, from, len );
    }

    return retval;
}

#endif

#if !defined(CAN_COMPILE_C_ALTIVEC) && !defined(__BUILD_ALTIVEC_ASM__)

/*
 * The asm code is generated with:
 *
 * gcc-2.95 -fvec -D__BUILD_ALTIVEC_ASM__ -O9 -fomit-frame-pointer -mregnames -S *      memcpyaltivec.c
 *
 * sed 's/.L/._L/g' memcpyaltivec.s |
 * awk '{args=""; len=split ($2, arg, ",");
 *      for (i=1; i<=len; i++) { a=arg[i]; if (i<len) a=a",";
 *                               args = args sprintf ("%-6s", a) }
 *      printf ("\t\"\t%-16s%-24s\\n\"\n", $1, args) }' |
 * unexpand -a
 */

static void * fast_memcpy( void * _to, const void * _from, size_t len )
{
    asm ("                                              \n"
    "    cmplwi        %cr0, %r5,  16        \n"
    "    mr        %r9,  %r3        \n"
    "    bc        4,    1,    ._L3    \n"
    "    andi.        %r0,  %r3,  15        \n"
    "    bc        12,   2,    ._L4    \n"
    "    subfic        %r0,  %r0,  16        \n"
    "    add        %r11, %r3,  %r0        \n"
    "    cmplw        %cr0, %r3,  %r11    \n"
    "    subf        %r5,  %r0,  %r5        \n"
    "    bc        4,    0,    ._L4    \n"
    "    ._L7:                    \n"
    "    lbz        %r0,  0(%r4)        \n"
    "    stb        %r0,  0(%r9)        \n"
    "    addi        %r9,  %r9,  1        \n"
    "    cmplw        %cr0, %r9,  %r11    \n"
    "    addi        %r4,  %r4,  1        \n"
    "    bc        12,   0,    ._L7    \n"
    "    ._L4:                    \n"
    "    rlwinm.        %r0,  %r5,  0,      0,    27    \n"
    "    bc        12,   2,    ._L3    \n"
    "    addi        %r5,  %r5,  -16        \n"
    "    li        %r11, 15        \n"
    "    lvsl        %v12, 0,    %r4        \n"
    "    lvx        %v1,  0,    %r4        \n"
    "    lvx        %v0,  %r11, %r4        \n"
    "    rlwinm.        %r0,  %r5,  0,      0,    27    \n"
    "    vperm        %v13, %v1,  %v0,  %v12    \n"
    "    addi        %r4,  %r4,  16        \n"
    "    bc        12,   2,    ._L11    \n"
    "    ._L12:                    \n"
    "    addi        %r5,  %r5,  -16        \n"
    "    li        %r11, 15        \n"
    "    lvx        %v1,  0,    %r4        \n"
    "    lvx        %v0,  %r11, %r4        \n"
    "    rlwinm.        %r0,  %r5,  0,      0,    27    \n"
    "    stvx        %v13, 0,    %r9        \n"
    "    vperm        %v13, %v1,  %v0,  %v12    \n"
    "    addi        %r4,  %r4,  16        \n"
    "    addi        %r9,  %r9,  16        \n"
    "    bc        4,    2,    ._L12    \n"
    "    ._L11:                    \n"
    "    stvx        %v13, 0,    %r9        \n"
    "    addi        %r9,  %r9,  16        \n"
    "    ._L3:                    \n"
    "    cmpwi        %cr0, %r5,  0        \n"
    "    bclr        12,   2            \n"
    "    add        %r5,  %r9,  %r5        \n"
    "    cmplw        %cr0, %r9,  %r5        \n"
    "    bclr        4,    0            \n"
    "    ._L17:                    \n"
    "    lbz        %r0,  0(%r4)        \n"
    "    stb        %r0,  0(%r9)        \n"
    "    addi        %r9,  %r9,  1        \n"
    "    cmplw        %cr0, %r9,  %r5        \n"
    "    addi        %r4,  %r4,  1        \n"
    "    bc        12,   0,    ._L17    \n"
        );
}

#endif
