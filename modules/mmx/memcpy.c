/*****************************************************************************
 * memcpy.c : classic memcpy module
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_cpu.h>

#define HAVE_MMX
#include "../mmx/fastmemcpy.h"

static int Activate( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this);
    vlc_fastmem_register( fast_memcpy, NULL );

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_MISC )
    set_description( N_("MMX memcpy") )
    add_shortcut( "mmx" )
    add_shortcut( "memcpymmx" )
    set_capability( "memcpy", 100 )
    set_callbacks( Activate, NULL )
vlc_module_end ()

