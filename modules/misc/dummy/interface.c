/*****************************************************************************
 * intf_dummy.c: dummy interface plugin
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: interface.c,v 1.2 2002/10/03 17:01:58 gbazin Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run   ( intf_thread_t * );

/*****************************************************************************
 * Open: initialize dummy interface
 *****************************************************************************/
int  E_(OpenIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    CONSOLE_INTRO_MSG;
    msg_Info( p_intf, _("Using the dummy interface plugin...") );

    p_intf->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    while( !p_intf->b_die )
    {
        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }
}

