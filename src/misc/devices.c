/*****************************************************************************
 * devices.c : Handling of devices probing
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#if 0

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc/intf.h>
#include <vlc_devices.h>

static intf_thread_t *p_probe_thread = NULL;

void devices_ProbeCreate( vlc_object_t *p_this )
{
    intf_thread_t * p_probe;

    /* Allocate structure */
    p_probe = vlc_object_create( p_this, VLC_OBJECT_INTF );
    if( !p_probe )
        return;
    p_probe->p_module = module_Need( p_probe, "devices probe", "", false );
    if( p_probe->p_module == NULL )
    {
        msg_Err( p_this, "no devices probing module could be loaded" );
        vlc_object_release( p_probe );
        return;
    }

    p_probe_thread = p_probe;
}

#endif
