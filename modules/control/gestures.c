/*****************************************************************************
 * gestures.c: control vlc with mouse gestures
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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
    vlc_object_t *      p_vout;
    input_thread_t *    p_input;
    vlc_bool_t          b_got_gesture;
    vlc_bool_t          b_button_pressed;
    int                 i_mouse_x, i_mouse_y;
    int                 i_last_x, i_last_y;
    unsigned int        i_pattern;
    int                 i_num_gestures;
    int                 i_threshold;
    int                 i_button_mask;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
#define NONE 0
#define GESTURE( a, b, c, d ) (a | ( b << 4 ) | ( c << 8 ) | ( d << 12 ))

int  E_(Open)   ( vlc_object_t * );
void E_(Close)  ( vlc_object_t * );
static int  InitThread     ( intf_thread_t *p_intf );
static int  MouseEvent     ( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

/* Exported functions */
static void RunIntf        ( intf_thread_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define THRESHOLD_TEXT N_( "Motion threshold (10-100)" )
#define THRESHOLD_LONGTEXT N_( \
    "Amount of movement required for a mouse" \
    " gesture to be recorded." )

#define BUTTON_TEXT N_( "Trigger button" )
#define BUTTON_LONGTEXT N_( \
    "You can set the trigger button for mouse gestures here." )

static char *button_list[] = { "left", "middle", "right" };
static char *button_list_text[] = { N_("Left"), N_("Middle"), N_("Right") };

vlc_module_begin();
    set_shortname( _("Gestures"));
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    add_integer( "gestures-threshold", 30, NULL, THRESHOLD_TEXT, THRESHOLD_LONGTEXT, VLC_TRUE );
    add_string( "gestures-button", "right", NULL,
                BUTTON_TEXT, BUTTON_LONGTEXT, VLC_FALSE );
        change_string_list( button_list, button_list_text, 0 );
    set_description( _("Mouse gestures control interface") );

    set_capability( "interface", 0 );
    set_callbacks( E_(Open), E_(Close) );
vlc_module_end();

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int E_(Open) ( vlc_object_t *p_this )
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
 * gesture: return a subpattern within a pattern
 *****************************************************************************/
static int gesture( int i_pattern, int i_num )
{
    return ( i_pattern >> ( i_num * 4 ) ) & 0xF;
}
        
/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
void E_(Close) ( vlc_object_t *p_this )
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
    playlist_t * p_playlist = NULL;
    p_intf->p_sys->p_vout = NULL;

    if( InitThread( p_intf ) < 0 )
    {
        msg_Err( p_intf, "can't initialize intf" );
        return;
    }
    msg_Dbg( p_intf, "intf initialized" );

    /* Main loop */
    while( !p_intf->b_die )
    {
        vlc_mutex_lock( &p_intf->change_lock );

        /* 
         * mouse cursor
         */
        if( p_intf->p_sys->b_got_gesture )
        {
            /* Do something */
            switch( p_intf->p_sys->i_pattern )
            {
            case LEFT:
                p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                              FIND_ANYWHERE );
                if( p_playlist == NULL )
                {
                    break;
                }
                
                playlist_Prev( p_playlist );
                vlc_object_release( p_playlist );
                break;
            case RIGHT:
                p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                              FIND_ANYWHERE );
                if( p_playlist == NULL )
                {
                    break;
                }
                
                playlist_Next( p_playlist );
                vlc_object_release( p_playlist );
                break;
            case GESTURE(UP,RIGHT,NONE,NONE):
                if (p_intf->p_sys->p_vout )
                {
                    ((vout_thread_t *)p_intf->p_sys->p_vout)->i_changes |=
                        VOUT_FULLSCREEN_CHANGE;
                }
                break;
            case GESTURE(DOWN,RIGHT,NONE,NONE):
                /* FIXME: Should close the vout!"*/
                p_intf->p_vlc->b_die = VLC_TRUE;
                break;
            case GESTURE(DOWN,LEFT,UP,RIGHT):
                msg_Dbg(p_intf, "a square!" );
                break;
            default:
                break;
            }
            p_intf->p_sys->i_num_gestures = 0;
            p_intf->p_sys->i_pattern = 0;
            p_intf->p_sys->b_got_gesture = VLC_FALSE;
        }
                

        vlc_mutex_unlock( &p_intf->change_lock );

        /* 
         * video output
         */
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
            p_intf->p_sys->p_vout = vlc_object_find( p_intf,
                                      VLC_OBJECT_VOUT, FIND_ANYWHERE );
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
    char *psz_button;
    /* we might need some locking here */
    if( !p_intf->b_die )
    {
        input_thread_t * p_input;

        p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_PARENT );

        vlc_mutex_lock( &p_intf->change_lock );

        p_intf->p_sys->p_input = p_input;
        p_intf->p_sys->b_got_gesture = VLC_FALSE;
        p_intf->p_sys->b_button_pressed = VLC_FALSE;
        p_intf->p_sys->i_threshold = config_GetInt( p_intf, "gestures-threshold" );
        psz_button = config_GetPsz( p_intf, "gestures-button" );
        if ( !strcmp( psz_button, "left" ) )
        {
            p_intf->p_sys->i_button_mask = 1;
        }
        else if ( !strcmp( psz_button, "middle" ) )
        {
            p_intf->p_sys->i_button_mask = 2;
        }
        else if ( !strcmp( psz_button, "right" ) )
        {
            p_intf->p_sys->i_button_mask = 4;
        }

        p_intf->p_sys->i_pattern = 0;
        p_intf->p_sys->i_num_gestures = 0;
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
    int pattern = 0;
    
    signed int i_horizontal, i_vertical;
    intf_thread_t *p_intf = (intf_thread_t *)p_data;

    /* don't process new gestures before the last events are processed */
    if( p_intf->p_sys->b_got_gesture ) {
        return VLC_SUCCESS;
    }

    vlc_mutex_lock( &p_intf->change_lock );
    if( !strcmp(psz_var, "mouse-moved" ) && p_intf->p_sys->b_button_pressed ) {
        var_Get( p_intf->p_sys->p_vout, "mouse-x", &val );
        p_intf->p_sys->i_mouse_x = val.i_int;
        var_Get( p_intf->p_sys->p_vout, "mouse-y", &val );
        p_intf->p_sys->i_mouse_y = val.i_int;
        i_horizontal = p_intf->p_sys->i_mouse_x -
            p_intf->p_sys->i_last_x;
        i_horizontal = i_horizontal / p_intf->p_sys->i_threshold;
        i_vertical = p_intf->p_sys->i_mouse_y
            - p_intf->p_sys->i_last_y;
        i_vertical = i_vertical / p_intf->p_sys->i_threshold;
        
        if ( i_horizontal < 0 )
        {
            msg_Dbg( p_intf, "left gesture(%d)", i_horizontal );
            pattern = LEFT;
        }
        else if ( i_horizontal > 0 )
        {
            msg_Dbg( p_intf, "right gesture(%d)", i_horizontal );
            pattern = RIGHT;
        }
        if ( i_vertical < 0 )
        {
            msg_Dbg( p_intf, "up gesture(%d)", i_vertical );
            pattern = UP;
        }
        else if ( i_vertical > 0 )
        {
            msg_Dbg( p_intf, "down gesture(%d)", i_vertical );
            pattern = DOWN;
        }
        if (pattern )
        {
            p_intf->p_sys->i_last_y = p_intf->p_sys->i_mouse_y;
            p_intf->p_sys->i_last_x = p_intf->p_sys->i_mouse_x;
            if( gesture(p_intf->p_sys->i_pattern, p_intf->p_sys->i_num_gestures - 1 ) != pattern )
            {
                p_intf->p_sys->i_pattern |= pattern << ( p_intf->p_sys->i_num_gestures * 4 );
                p_intf->p_sys->i_num_gestures++;
            }
        }

    }
    if( !strcmp( psz_var, "mouse-button-down" ) && newval.i_int & p_intf->p_sys->i_button_mask
        && !p_intf->p_sys->b_button_pressed )
    {
        p_intf->p_sys->b_button_pressed = VLC_TRUE;
        var_Get( p_intf->p_sys->p_vout, "mouse-x", &val );
        p_intf->p_sys->i_last_x = val.i_int;
        var_Get( p_intf->p_sys->p_vout, "mouse-y", &val );
        p_intf->p_sys->i_last_y = val.i_int;
    }
    if( !strcmp( psz_var, "mouse-button-down" ) && !( newval.i_int & p_intf->p_sys->i_button_mask )
        && p_intf->p_sys->b_button_pressed )
    {
        p_intf->p_sys->b_button_pressed = VLC_FALSE;
        p_intf->p_sys->b_got_gesture = VLC_TRUE;
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}
