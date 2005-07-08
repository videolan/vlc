/*****************************************************************************
 * memcpy.c : classic memcpy module
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
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

#include <vlc/vlc.h>

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_SSE
#undef HAVE_SSE2
#undef HAVE_3DNOW
#undef HAVE_ALTIVEC

#if defined( MODULE_NAME_IS_memcpy3dn )
#   define PRIORITY 100
#   define HAVE_3DNOW
#elif defined( MODULE_NAME_IS_memcpymmx )
#   define PRIORITY 100
#   define HAVE_MMX
#elif defined( MODULE_NAME_IS_memcpymmxext )
#   define PRIORITY 200
#   define HAVE_MMX2
#else
#   define PRIORITY 50
#endif

/*****************************************************************************
 * Extern prototype
 *****************************************************************************/
#ifndef MODULE_NAME_IS_memcpy
#   define fast_memcpy E_(fast_memcpy)
#   include "fastmemcpy.h"
#endif

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Activate ( vlc_object_t *p_this )
{
#ifdef MODULE_NAME_IS_memcpy
    p_this->p_vlc->pf_memcpy = memcpy;
    p_this->p_vlc->pf_memset = memset;
#else
    p_this->p_vlc->pf_memcpy = fast_memcpy;
    p_this->p_vlc->pf_memset = NULL;
#endif

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_ADVANCED );
    set_subcategory( SUBCAT_ADVANCED_MISC );
#ifdef MODULE_NAME_IS_memcpy
    set_description( _("libc memcpy") );
    add_shortcut( "c" );
    add_shortcut( "libc" );
#elif defined( MODULE_NAME_IS_memcpy3dn )
    set_description( _("3D Now! memcpy") );
    add_requirement( 3DNOW );
    add_shortcut( "3dn" );
    add_shortcut( "3dnow" );
    add_shortcut( "memcpy3dn" );
    add_shortcut( "memcpy3dnow" );
#elif defined( MODULE_NAME_IS_memcpymmx )
    set_description( _("MMX memcpy") );
    add_requirement( MMX );
    add_shortcut( "mmx" );
    add_shortcut( "memcpymmx" );
#elif defined( MODULE_NAME_IS_memcpymmxext )
    set_description( _("MMX EXT memcpy") );
    add_requirement( MMXEXT );
    add_shortcut( "mmxext" );
    add_shortcut( "memcpymmxext" );
#endif
    set_capability( "memcpy", PRIORITY );
    set_callbacks( Activate, NULL );
vlc_module_end();

