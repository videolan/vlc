/*****************************************************************************
 * showintf.c: control the display of the interface in fullscreen mode
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    vlc_object_t * p_vout;
    vlc_bool_t     b_button_pressed;
    vlc_bool_t     b_triggered;
    int            i_threshold;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
int  E_(Open) ( vlc_object_t * );
void E_(Close)( vlc_object_t * );
static void RunIntf( intf_thread_t *p_intf );
static int  InitThread( intf_thread_t *p_intf );
static int  MouseEvent( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define THRESHOLD_TEXT N_( "Threshold" )
#define THRESHOLD_LONGTEXT N_( "Height of the zone triggering the interface" )

vlc_module_begin();
    set_shortname( "Showintf" );
    add_integer( "showintf-threshold", 10, NULL, THRESHOLD_TEXT, THRESHOLD_LONGTEXT, VLC_TRUE );
    set_description( _("Show interface with mouse") );

    set_capability( "interface", 0 );
    set_callbacks( E_(Open), E_(Close) );
vlc_module_end();

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
int E_(Open)( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    p_intf->pf_run = RunIntf;

    return( 0 );
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
void E_(Close)( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void RunIntf( intf_thread_t *p_intf )
{
    p_intf->p_sys->p_vout = NULL;

    if( InitThread( p_intf ) < 0 )
    {
        msg_Err( p_intf, "cannot initialize interface" );
        return;
    }

    /* Main loop */
    while( !p_intf->b_die )
    {
        vlc_mutex_lock( &p_intf->change_lock );

        /* Notify the interfaces */
        if( p_intf->p_sys->b_triggered )
        {
            playlist_t *p_playlist =
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );

            if( p_playlist != NULL )
            {
                vlc_value_t val;
                val.b_bool = VLC_TRUE;
                var_Set( p_playlist, "intf-show", val );
                vlc_object_release( p_playlist );
            }
            p_intf->p_sys->b_triggered = VLC_FALSE;
        }

        vlc_mutex_unlock( &p_intf->change_lock );


        /* Take care of the video output */
        if( p_intf->p_sys->p_vout && p_intf->p_sys->p_vout->b_die )
        {
            var_DelCallback( p_intf->p_sys->p_vout, "mouse-moved",
                             MouseEvent, p_intf );
            var_DelCallback( p_intf->p_sys->p_vout, "mouse-button-down",
                             MouseEvent, p_intf );
            vlc_object_release( p_intf->p_sys->p_vout );
            p_intf->p_sys->p_vout = NULL;
        }

        if( p_intf->p_sys->p_vout == NULL )
        {
            p_intf->p_sys->p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                     FIND_ANYWHERE );
            if( p_intf->p_sys->p_vout )
            {
                var_AddCallback( p_intf->p_sys->p_vout, "mouse-moved",
                                 MouseEvent, p_intf );
                var_AddCallback( p_intf->p_sys->p_vout, "mouse-button-down",
                                 MouseEvent, p_intf );
            }
        }

        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }

    if( p_intf->p_sys->p_vout )
    {
        var_DelCallback( p_intf->p_sys->p_vout, "mouse-moved",
                         MouseEvent, p_intf );
        var_DelCallback( p_intf->p_sys->p_vout, "mouse-button-down",
                         MouseEvent, p_intf );
        vlc_object_release( p_intf->p_sys->p_vout );
    }
}

/*****************************************************************************
 * InitThread:
 *****************************************************************************/
static int InitThread( intf_thread_t * p_intf )
{
    if( !p_intf->b_die )
    {
        vlc_mutex_lock( &p_intf->change_lock );

        p_intf->p_sys->b_triggered = VLC_FALSE;
        p_intf->p_sys->b_button_pressed = VLC_FALSE;
        p_intf->p_sys->i_threshold =
            config_GetInt( p_intf, "showintf-threshold" );

        vlc_mutex_unlock( &p_intf->change_lock );

        return 0;
    }
    else
    {
        return -1;
    }
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vlc_value_t val;

    int i_mouse_x, i_mouse_y;
    intf_thread_t *p_intf = (intf_thread_t *)p_data;

    /* Do nothing when the interface is already requested */
    if( p_intf->p_sys->b_triggered )
        return VLC_SUCCESS;

    /* Nothing to do when not in fullscreen mode */
    var_Get( p_intf->p_sys->p_vout, "fullscreen", &val );
    if( !val.i_int )
        return VLC_SUCCESS;

    vlc_mutex_lock( &p_intf->change_lock );
    if( !strcmp( psz_var, "mouse-moved" ) && !p_intf->p_sys->b_button_pressed )
    {
        var_Get( p_intf->p_sys->p_vout, "mouse-x", &val );
        i_mouse_x = val.i_int;
        var_Get( p_intf->p_sys->p_vout, "mouse-y", &val );
        i_mouse_y = val.i_int;

        /* Very basic test, we even ignore the x value :) */
        if ( i_mouse_y < p_intf->p_sys->i_threshold )
        {
            msg_Dbg( p_intf, "interface showing requested" );
            p_intf->p_sys->b_triggered = VLC_TRUE;
        }
    }

    /* We keep track of the button state to avoid interferences with the
     * gestures plugin */
    if( !p_intf->p_sys->b_button_pressed &&
        !strcmp( psz_var, "mouse-button-down" ) )
    {
        p_intf->p_sys->b_button_pressed = VLC_TRUE;
    }
    if( p_intf->p_sys->b_button_pressed &&
        !strcmp( psz_var, "mouse-button-down" ) )
    {
        p_intf->p_sys->b_button_pressed = VLC_FALSE;
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}
