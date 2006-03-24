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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <xosd.h>

#include <vlc/intf.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

/*****************************************************************************
 * intf_sys_t: description and status of rc interface
 *****************************************************************************/
struct intf_sys_t
{
    xosd * p_osd;               /* libxosd handle */
    vlc_bool_t  b_need_update;   /* Update display ? */
};

#define MAX_LINE_LENGTH 256

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static void Run          ( intf_thread_t * );

static int PlaylistNext( vlc_object_t *p_this, const char *psz_variable,
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

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_description( _("XOSD interface") );
    set_shortname( "XOSD" );
    add_bool( "xosd-position", 1, NULL, POSITION_TEXT, POSITION_LONGTEXT, VLC_TRUE );
    add_integer( "xosd-text-offset", 30, NULL, TXT_OFS_TEXT, TXT_OFS_LONGTEXT, VLC_TRUE );
    add_integer( "xosd-shadow-offset", 2, NULL,
                 SHD_OFS_TEXT, SHD_OFS_LONGTEXT, VLC_TRUE );
    add_string( "xosd-font",
                "-adobe-helvetica-bold-r-normal-*-*-160-*-*-p-*-iso8859-1",
                NULL, FONT_TEXT, FONT_LONGTEXT, VLC_TRUE );
    add_string( "xosd-colour", "LawnGreen",
                    NULL, COLOUR_TEXT, COLOUR_LONGTEXT, VLC_TRUE );
    set_capability( "interface", 10 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    xosd *p_osd;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return VLC_ENOMEM;
    }

    if( getenv( "DISPLAY" ) == NULL )
    {
        msg_Err( p_intf, "no display, please set the DISPLAY variable" );
        return VLC_EGENERIC;
    }

    /* Initialize library */
#if defined(HAVE_XOSD_VERSION_0) || defined(HAVE_XOSD_VERSION_1)
    p_osd  = p_intf->p_sys->p_osd =
        xosd_init( config_GetPsz( p_intf, "xosd-font" ),
                   config_GetPsz( p_intf,"xosd-colour" ), 3,
                   XOSD_top, 0, 1 );
    if( p_intf->p_sys->p_osd == NULL )
    {
        msg_Err( p_intf, "couldn't initialize libxosd" );
        return VLC_EGENERIC;
    }
#else
    p_osd = p_intf->p_sys->p_osd = xosd_create( 1 );
    if( p_osd == NULL )
    {
        msg_Err( p_intf, "couldn't initialize libxosd" );
        return VLC_EGENERIC;
    }
    xosd_set_colour( p_osd, config_GetPsz( p_intf,"xosd-colour" ) );
    xosd_set_timeout( p_osd, 3 );
#endif


    playlist_t *p_playlist =
            (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return VLC_EGENERIC;
    }

    var_AddCallback( p_playlist, "playlist-current", PlaylistNext, p_this );
    var_AddCallback( p_playlist, "item-change", PlaylistNext, p_this );

    vlc_object_release( p_playlist );

    /* Set user preferences */
    xosd_set_font( p_intf->p_sys->p_osd,
                    config_GetPsz( p_intf, "xosd-font" ) );
    xosd_set_outline_colour( p_intf->p_sys->p_osd,"black" );
#ifdef HAVE_XOSD_VERSION_2
    xosd_set_horizontal_offset( p_intf->p_sys->p_osd,
                    config_GetInt( p_intf, "xosd-text-offset" ) );
    xosd_set_vertical_offset( p_intf->p_sys->p_osd,
                    config_GetInt( p_intf, "xosd-text-offset" ) );
#else
    xosd_set_offset( p_intf->p_sys->p_osd,
                    config_GetInt( p_intf, "xosd-text-offset" ) );
#endif
    xosd_set_shadow_offset( p_intf->p_sys->p_osd,
                    config_GetInt( p_intf, "xosd-shadow-offset" ));
    xosd_set_pos( p_intf->p_sys->p_osd,
                    config_GetInt( p_intf, "xosd-position" ) ?
                                         XOSD_bottom: XOSD_top );

    /* Initialize to NULL */
    xosd_display( p_osd, 0, XOSD_string, "XOSD interface initialized" );

    p_intf->pf_run = Run;

    p_intf->p_sys->b_need_update = VLC_TRUE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Uninitialize library */
    xosd_destroy( p_intf->p_sys->p_osd );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: xosd thread
 *****************************************************************************
 * This part of the interface runs in a separate thread
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    int i_size,i_index;
    playlist_t *p_playlist;
    playlist_item_t *p_item = NULL;
    input_item_t item;
    char psz_duration[MSTRTIME_MAX_SIZE+2];
    char *psz_display = NULL;

    while( !p_intf->b_die )
    {
        if( p_intf->p_sys->b_need_update == VLC_TRUE )
        {
            p_intf->p_sys->b_need_update = VLC_FALSE;
            p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                      VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
            if( !p_playlist )
            {
                continue;
            }

            if( p_playlist->i_size < 0 || p_playlist->i_index < 0 )
            {
                vlc_object_release( p_playlist );
                continue;
            }
            if( psz_display )
            {
                free( psz_display );
                psz_display = NULL;
            }
            if( p_playlist->status.i_status == PLAYLIST_STOPPED )
            {
                psz_display = strdup(_("Stop"));
                vlc_object_release( p_playlist );
            }
            else if( p_playlist->status.i_status == PLAYLIST_PAUSED )
            {
                psz_display = strdup(_("Pause"));
                vlc_object_release( p_playlist );
            }
            else
            {
    //           vlc_mutex_lock(&p_playlist->object_lock );
                 p_item = p_playlist->status.p_item;
                item = p_item->input;
                if( !p_item )
                {
                    vlc_object_release( p_playlist );
     //            vlc_mutex_unlock(&p_playlist->object_lock );
                    continue;
                }
                i_size = p_playlist->i_size;
                i_index = p_playlist->i_index+1;
    //            vlc_mutex_unlock(&p_playlist->object_lock );

                vlc_object_release( p_playlist );

                if( item.i_duration != -1 )
                {
                    char psz_durationstr[MSTRTIME_MAX_SIZE];
                    secstotimestr( psz_durationstr, item.i_duration/1000000 );
                    sprintf( psz_duration, "(%s)", psz_durationstr );
                }
                else
                {
                    sprintf( psz_duration," " );
                }

                psz_display = (char *)malloc( sizeof(char )*
                                          (strlen( item.psz_name ) +
                                          MSTRTIME_MAX_SIZE + 2+6 + 10 +10 ));
                sprintf( psz_display," %i/%i - %s %s",
                         i_index,i_size, item.psz_name, psz_duration);
            }

            /* Display */
            xosd_display( p_intf->p_sys->p_osd,
                            0,                               /* first line */
                            XOSD_string,
                            psz_display );
        }

        msleep( INTF_IDLE_SLEEP );
    }
}

static int PlaylistNext( vlc_object_t *p_this, const char *psz_variable,
                vlc_value_t oval, vlc_value_t nval, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;

    p_intf->p_sys->b_need_update = VLC_TRUE;
    return VLC_SUCCESS;
}

