/*****************************************************************************
 * screensaver.c : disable screen savers when VLC is playing
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/aout.h>
#include <vlc/vout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Run          ( intf_thread_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("X Screensaver disabler") );
    set_capability( "interface", 0 );
    set_callbacks( Activate, NULL );
vlc_module_end();

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

    p_intf->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************
 * This part of the module is in a separate thread so that we do not have
 * too much system() overhead.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    int i_lastcall = 0;

    while( !p_intf->b_die )
    {
        msleep( 100000 );

        /* Check screensaver every 30 seconds */
        if( ++i_lastcall > 300 )
        {
            vlc_object_t *p_vout;
            p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
            /* If there is a video output, disable xscreensaver */
            if( p_vout )
            {
                vlc_object_release( p_vout );

                /* http://www.jwz.org/xscreensaver/faq.html#dvd */
                system( "xscreensaver-command -deactivate >&- 2>&- &" );

                /* FIXME: add support for other screensavers */
            }

            i_lastcall = 0;
        }
    }
}

