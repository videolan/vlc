/*****************************************************************************
 * motion.c: control VLC with laptop built-in motion sensors
 *****************************************************************************
 * Copyright (C) 2006 - 2007 the VideoLAN team
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Jérôme Decoodt <djc@videolan.org> (unimotion integration)
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <unistd.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>

#include "motionlib.h"

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    motion_sensors_t *p_motion;
    vlc_thread_t thread;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

static void *RunIntf( void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( N_("motion"))
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("motion control interface") )
    set_help( N_("Use HDAPS, AMS, APPLESMC or UNIMOTION motion sensors " \
                 "to rotate the video") )

    add_obsolete_bool( "motion-use-rotate" ) /* since 2.1.0 */

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * OpenIntf: initialise interface
 *****************************************************************************/
int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_sys->p_motion = motion_create( VLC_OBJECT( p_intf ) );
    if( p_sys->p_motion == NULL )
    {
error:
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_intf->p_sys = p_sys;

    if( vlc_clone( &p_sys->thread, RunIntf, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        motion_destroy( p_sys->p_motion );
        goto error;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );
    motion_destroy( p_sys->p_motion );
    free( p_sys );
}

/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
#define LOW_THRESHOLD 800
#define HIGH_THRESHOLD 1000
static void *RunIntf( void *data )
{
    intf_thread_t *p_intf = data;
    int i_oldx = 0;

    for( ;; )
    {
        const char *psz_type;
        bool b_change = false;

        /* Wait a bit, get orientation, change filter if necessary */
#warning FIXME: check once (or less) per picture, not once per interval
        msleep( INTF_IDLE_SLEEP );

        int canc = vlc_savecancel();
        int i_x = motion_get_angle( p_intf->p_sys->p_motion );

        if( i_x < -HIGH_THRESHOLD && i_oldx > -LOW_THRESHOLD )
        {
            b_change = true;
            psz_type = "90";
        }
        else if( ( i_x > -LOW_THRESHOLD && i_oldx < -HIGH_THRESHOLD )
                 || ( i_x < LOW_THRESHOLD && i_oldx > HIGH_THRESHOLD ) )
        {
            b_change = true;
            psz_type = NULL;
        }
        else if( i_x > HIGH_THRESHOLD && i_oldx < LOW_THRESHOLD )
        {
            b_change = true;
            psz_type = "270";
        }

        if( b_change )
        {
#warning FIXME: refactor this plugin as a video filter!
            input_thread_t *p_input;

            p_input = playlist_CurrentInput( pl_Get( p_intf ) );
            if( p_input )
            {
                vout_thread_t *p_vout;

                p_vout = input_GetVout( p_input );
                if( p_vout )
                {
                    if( psz_type != NULL )
                    {
                        var_Create( p_vout, "transform-type", VLC_VAR_STRING );
                        var_SetString( p_vout, "transform-type", psz_type );
                    }
                    else
                        var_Destroy( p_vout, "transform-type" );

                    var_SetString( p_vout, "video-filter",
                                   psz_type != NULL ? "transform" : "" );
                    vlc_object_release( p_vout );
                }
                vlc_object_release( p_input );
                i_oldx = i_x;
            }
        }

        vlc_restorecancel( canc );
    }
    assert(0);
}
#undef LOW_THRESHOLD
#undef HIGH_THRESHOLD
