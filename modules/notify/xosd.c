/*****************************************************************************
 * xosd.c : X On Screen Display interface
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Loïc Minier <lool@videolan.org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_interface.h>
#include <vlc_xlib.h>

#include <xosd.h>

/*****************************************************************************
 * intf_sys_t: description and status of rc interface
 *****************************************************************************/
struct intf_sys_t
{
    xosd *      p_osd;          /* libxosd handle */
    bool        b_need_update;  /* Update display ? */
    vlc_mutex_t lock;           /* lock for the condition variable */
    vlc_cond_t  cond;           /* condition variable to know when to update */
};

#define MAX_LINE_LENGTH 256

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );

static void Run         ( intf_thread_t * );

static int PlaylistNext ( vlc_object_t *p_this, const char *psz_variable,
                          vlc_value_t oval, vlc_value_t nval, void *param );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define POSITION_TEXT N_("Flip vertical position")
#define POSITION_LONGTEXT N_("Display XOSD output at the bottom of the " \
                             "screen instead of the top.")

#define TXT_OFS_TEXT N_("Vertical offset")
#define TXT_OFS_LONGTEXT N_("Vertical offset between the border of the screen "\
                            "and the displayed text (in pixels, defaults to "\
                            "30 pixels)." )

#define SHD_OFS_TEXT N_("Shadow offset")
#define SHD_OFS_LONGTEXT N_("Offset between the text and the shadow (in " \
                            "pixels, defaults to 2 pixels)." )

#define FONT_TEXT N_("Font")
#define FONT_LONGTEXT N_("Font used to display text in the XOSD output.")
#define COLOUR_TEXT N_("Color")
#define COLOUR_LONGTEXT N_("Color used to display text in the XOSD output.")

vlc_module_begin ()
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("XOSD interface") )
    set_shortname( "XOSD" )
    add_bool( "xosd-position", true, POSITION_TEXT, POSITION_LONGTEXT, true )
    add_integer( "xosd-text-offset", 30, TXT_OFS_TEXT, TXT_OFS_LONGTEXT, true )
    add_integer( "xosd-shadow-offset", 2,
                 SHD_OFS_TEXT, SHD_OFS_LONGTEXT, true )
    add_string( "xosd-font", "-adobe-helvetica-bold-r-normal-*-*-160-*-*-p-*-iso8859-1",
                FONT_TEXT, FONT_LONGTEXT, true )
    add_string( "xosd-colour", "LawnGreen",
                COLOUR_TEXT, COLOUR_LONGTEXT, true )
    set_capability( "interface", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys;
    xosd *p_osd;
    char *psz_font, *psz_colour;

    if (!vlc_xlib_init(p_this))
        return VLC_EGENERIC;

    if( getenv( "DISPLAY" ) == NULL )
    {
        msg_Err( p_intf, "no display, please set the DISPLAY variable" );
        return VLC_EGENERIC;
    }

    /* Allocate instance and initialize some members */
    p_sys = p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* Initialize library */
    psz_font = var_InheritString( p_intf, "xosd-font" );
    psz_colour = var_InheritString( p_intf, "xosd-colour" );

    p_osd = xosd_create( 1 );
    if( p_osd == NULL )
    {
        msg_Err( p_intf, "couldn't initialize libxosd" );
        free( psz_colour );
        free( psz_font );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->p_osd = p_osd;

    /* Set user preferences */
    xosd_set_outline_colour( p_osd, "black" );
    xosd_set_font( p_osd, psz_font );
    xosd_set_colour( p_osd, psz_colour );
    xosd_set_timeout( p_osd, 3 );
    xosd_set_pos( p_osd, var_InheritBool( p_intf, "xosd-position" ) ?
                                        XOSD_bottom: XOSD_top );
    xosd_set_horizontal_offset( p_osd,
                    var_InheritInteger( p_intf, "xosd-text-offset" ) );
    xosd_set_vertical_offset( p_osd,
                    var_InheritInteger( p_intf, "xosd-text-offset" ) );
    xosd_set_shadow_offset( p_osd,
                    var_InheritInteger( p_intf, "xosd-shadow-offset" ));

    /* Initialize to NULL */
    xosd_display( p_osd, 0, XOSD_string, "XOSD interface initialized" );

    free( psz_colour );
    free( psz_font );

    // Initialize mutex and condition variable before adding the callbacks
    vlc_mutex_init( &p_sys->lock );
    vlc_cond_init( &p_sys->cond );
    // Add the callbacks
    playlist_t *p_playlist = pl_Get( p_intf );
    var_AddCallback( p_playlist, "item-current", PlaylistNext, p_this );
    var_AddCallback( p_playlist, "item-change", PlaylistNext, p_this );

    p_sys->b_need_update = true;
    p_intf->pf_run = Run;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    playlist_t *p_playlist = pl_Get( p_intf );
    var_DelCallback( p_playlist, "item-current", PlaylistNext, p_this );
    var_DelCallback( p_playlist, "item-change", PlaylistNext, p_this );

    /* Uninitialize library */
    xosd_destroy( p_intf->p_sys->p_osd );

    /* Destroy structure */
    vlc_cond_destroy( &p_intf->p_sys->cond );
    vlc_mutex_destroy( &p_intf->p_sys->lock );
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: xosd thread
 *****************************************************************************
 * This part of the interface runs in a separate thread
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    playlist_t *p_playlist;
    playlist_item_t *p_item = NULL;
    char *psz_display = NULL;
    int cancel = vlc_savecancel();

    while( true )
    {
        // Wait for a signal
        vlc_restorecancel( cancel );
        vlc_mutex_lock( &p_intf->p_sys->lock );
        mutex_cleanup_push( &p_intf->p_sys->lock );
        while( !p_intf->p_sys->b_need_update )
            vlc_cond_wait( &p_intf->p_sys->cond, &p_intf->p_sys->lock );
        p_intf->p_sys->b_need_update = false;
        vlc_cleanup_run();

        // Compute the signal
        cancel = vlc_savecancel();
        p_playlist = pl_Get( p_intf );
        PL_LOCK;

        // If the playlist is empty don't do anything
        if( playlist_IsEmpty( p_playlist ) )
        {
            PL_UNLOCK;
            continue;
        }

        free( psz_display );
        int i_status = playlist_Status( p_playlist );
        if( i_status == PLAYLIST_STOPPED )
        {
            psz_display = strdup(_("Stop"));
        }
        else if( i_status == PLAYLIST_PAUSED )
        {
            psz_display = strdup(_("Pause"));
        }
        else
        {
            p_item = playlist_CurrentPlayingItem( p_playlist );
            if( !p_item )
            {
                psz_display = NULL;
                PL_UNLOCK;
                continue;
            }
            input_item_t *p_input = p_item->p_input;

            mtime_t i_duration = input_item_GetDuration( p_input );
            if( i_duration != -1 )
            {
                char psz_durationstr[MSTRTIME_MAX_SIZE];
                secstotimestr( psz_durationstr, i_duration / 1000000 );
                if( asprintf( &psz_display, "%s (%s)", p_input->psz_name, psz_durationstr ) == -1 )
                    psz_display = NULL;
            }
            else
                psz_display = strdup( p_input->psz_name );
        }
        PL_UNLOCK;

        /* Display */
        xosd_display( p_intf->p_sys->p_osd, 0, /* first line */
                      XOSD_string, psz_display );
    }
}

static int PlaylistNext( vlc_object_t *p_this, const char *psz_variable,
                vlc_value_t oval, vlc_value_t nval, void *param )
{
    (void)p_this;    (void)psz_variable;    (void)oval;    (void)nval;
    intf_thread_t *p_intf = (intf_thread_t *)param;

    // Send the signal using the condition variable
    vlc_mutex_lock( &p_intf->p_sys->lock );
    p_intf->p_sys->b_need_update = true;
    vlc_cond_signal( &p_intf->p_sys->cond );
    vlc_mutex_unlock( &p_intf->p_sys->lock );

    return VLC_SUCCESS;
}

