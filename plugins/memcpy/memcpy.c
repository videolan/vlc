/*****************************************************************************
 * memcpy.c : classic memcpy module
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: memcpy.c,v 1.5 2002/02/15 13:32:53 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <videolan/vlc.h>

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_SSE
#undef HAVE_SSE2
#undef HAVE_3DNOW

#if defined( MODULE_NAME_IS_memcpy3dn )
#   define HAVE_3DNOW
#   include "fastmemcpy.h"
#elif defined( MODULE_NAME_IS_memcpymmx )
#   define HAVE_MMX
#   include "fastmemcpy.h"
#elif defined( MODULE_NAME_IS_memcpymmxext )
#   define HAVE_MMX2
#   include "fastmemcpy.h"
#endif

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void memcpy_getfunctions( function_list_t * p_function_list );
#ifndef MODULE_NAME_IS_memcpy
void *      _M( fast_memcpy )  ( void * to, const void * from, size_t len );
#endif

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
#ifdef MODULE_NAME_IS_memcpy
    SET_DESCRIPTION( "libc memcpy module" )
    ADD_CAPABILITY( MEMCPY, 50 )
    ADD_SHORTCUT( "c" )
    ADD_SHORTCUT( "libc" )
    ADD_SHORTCUT( "memcpy" )
#elif defined( MODULE_NAME_IS_memcpy3dn )
    SET_DESCRIPTION( "3D Now! memcpy module" )
    ADD_CAPABILITY( MEMCPY, 100 )
    ADD_REQUIREMENT( 3DNOW )
    ADD_SHORTCUT( "3dn" )
    ADD_SHORTCUT( "3dnow" )
    ADD_SHORTCUT( "memcpy3dn" )
    ADD_SHORTCUT( "memcpy3dnow" )
#elif defined( MODULE_NAME_IS_memcpymmx )
    SET_DESCRIPTION( "MMX memcpy module" )
    ADD_CAPABILITY( MEMCPY, 100 )
    ADD_REQUIREMENT( MMX )
    ADD_SHORTCUT( "mmx" )
    ADD_SHORTCUT( "memcpymmx" )
#elif defined( MODULE_NAME_IS_memcpymmxext )
    SET_DESCRIPTION( "MMX EXT memcpy module" )
    ADD_CAPABILITY( MEMCPY, 200 )
    ADD_REQUIREMENT( MMXEXT )
    ADD_SHORTCUT( "mmxext" )
    ADD_SHORTCUT( "memcpymmxext" )
#endif
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
#ifdef MODULE_NAME_IS_memcpy
    p_function_list->functions.memcpy.pf_memcpy = memcpy;
#else
    p_function_list->functions.memcpy.pf_memcpy = _M( fast_memcpy );
#endif
}

