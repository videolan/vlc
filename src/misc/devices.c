/*****************************************************************************
 * devices.c : Handling of devices probing
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id: cpu.c 14103 2006-02-01 12:44:16Z sam $
 *
 * Authors: Cl√ment Stenac <zorglub@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc_devices.h>

void devices_ProbeCreate( vlc_object_t *p_this )
{
    intf_thread_t * p_probe;
    p_this->p_libvlc->p_probe = NULL;

    /* Allocate structure */
    p_probe = vlc_object_create( p_this, VLC_OBJECT_INTF );
    if( !p_probe )
    {
        msg_Err( p_this, "out of memory" );
        return;
    }
    p_probe->p_module = module_Need( p_probe, "devices probe", "", VLC_FALSE );
    if( p_probe->p_module == NULL )
    {
        msg_Err( p_this, "no devices probing module could be loaded" );
        vlc_object_destroy( p_probe );
        return;
    }

    p_this->p_libvlc->p_probe = p_probe;
}
