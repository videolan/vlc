/*****************************************************************************
 * memcpy.c : classic memcpy module
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: memcpyaltivec.c,v 1.1 2002/04/03 22:36:50 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef __BUILD_ALTIVEC_ASM__

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <videolan/vlc.h>

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void memcpy_getfunctions( function_list_t * p_function_list );
void *      _M( fast_memcpy )  ( void * to, const void * from, size_t len );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "Altivec memcpy module" )
    ADD_CAPABILITY( MEMCPY, 100 )
    ADD_REQUIREMENT( ALTIVEC )
    ADD_SHORTCUT( "altivec" )
    ADD_SHORTCUT( "memcpyaltivec" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    memcpy_getfunctions( &p_module->p_functions->memcpy );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/* Following functions are local */

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void memcpy_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.memcpy.pf_memcpy = _M( fast_memcpy );
}

#else
#   include <sys/types.h>
#   define _M( toto ) toto
#endif /* __BUILD_ALTIVEC_ASM__ */

#if defined(CAN_COMPILE_C_ALTIVEC) || defined( __BUILD_ALTIVEC_ASM__ )

#define vector_s16_t vector signed short
#define vector_u16_t vector unsigned short
#define vector_s8_t vector signed char
#define vector_u8_t vector unsigned char
#define vector_s32_t vector signed int
#define vector_u32_t vector unsigned int
#define MMREG_SIZE 16

void * _M( fast_memcpy )(void * _to, const void * _from, size_t len)
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
            memcpy(to, from, delta);
            to += delta;
            from += delta;
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
            do
            {
                ref0 = vec_ld( 0, from );
                ref1 = vec_ld( 15, from );
                from += 16;
                len -= 16;
                vec_st( tmp, 0, to );
                tmp = vec_perm( ref0, ref1, perm );
                to += 16;
            } while( len & ~(MMREG_SIZE-1) );
            vec_st( tmp, 0, to );
        }
    }

    if( len )
    {
        memcpy( to, from, len );
    }

    return retval;
}

#endif
