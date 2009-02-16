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

#include <xosd.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_interface.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

/*****************************************************************************
 * intf_sys_t: description and status of rc interface
 *****************************************************************************/
struct intf_sys_t
{
    xosd * p_osd;               /* libxosd handle */
    bool  b_need_update;   /* Update display ? */
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

vlc_module_begin ()
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("XOSD interface") )
    set_shortname( "XOSD" )
    add_bool( "xosd-position", 1, NULL, POSITION_TEXT, POSITION_LONGTEXT, true )
    add_integer( "xosd-text-offset", 30, NULL, TXT_OFS_TEXT, TXT_OFS_LONGTEXT, true )
    add_integer( "xosd-shadow-offset", 2, NULL,
                 SHD_OFS_TEXT, SHD_OFS_LONGTEXT, true )
    add_string( "xosd-font",
                "-adobe-helvetica-bold-r-normal-*-*-160-*-*-p-*-iso8859-1",
                NULL, FONT_TEXT, FONT_LONGTEXT, true )
    add_string( "xosd-colour", "LawnGreen",
                    NULL, COLOUR_TEXT, COLOUR_LONGTEXT, true )
    set_capability( "interface", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    xosd *p_osd;
    char *psz_font, *psz_colour;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
        return VLC_ENOMEM;

    if( getenv( "DISPLAY" ) == NULL )
    {
        msg_Err( p_intf, "no display, please set the DISPLAY variable" );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }

    /* Initialize library */
#if defined(HAVE_XOSD_VERSION_0) || defined(HAVE_XOSD_VERSION_1)
    psz_font = config_GetPsz( p_intf, "xosd-font" );
    psz_colour = config_GetPsz( p_intf,"xosd-colour" );
    p_osd  = p_intf->p_sys->p_osd = xosd_init( psz_font, psz_colour, 3,
                                               XOSD_top, 0, 1 );
    free( psz_font );
    free( psz_colour );

    if( p_intf->p_sys->p_osd == NULL )
    {
        msg_Err( p_intf, "couldn't initialize libxosd" );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }
#else
    p_osd = p_intf->p_sys->p_osd = xosd_create( 1 );
    if( p_osd == NULL )
    {
        msg_Err( p_intf, "couldn't initialize libxosd" );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }

    psz_colour = config_GetPsz( p_intf,"xosd-colour" );
    xosd_set_colour( p_osd, psz_colour );
    xosd_set_timeout( p_osd, 3 );
    free( psz_colour );
#endif


    playlist_t *p_playlist = pl_Hold( p_intf );
    var_AddCallback( p_playlist, "item-current", PlaylistNext, p_this );
    var_AddCallback( p_playlist, "item-change", PlaylistNext, p_this );
    pl_Release( p_intf );

    /* Set user preferences */
    psz_font = config_GetPsz( p_intf, "xosd-font" );
    xosd_set_font( p_intf->p_sys->p_osd, psz_font );
    free( psz_font );
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

    p_intf->p_sys->b_need_update = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist = pl_Hold( p_intf );
    var_DelCallback( p_playlist, "item-current", PlaylistNext, p_this );
    var_DelCallback( p_playlist, "item-change", PlaylistNext, p_this );
    pl_Release( p_intf );

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
    playlist_t *p_playlist;
    playlist_item_t *p_item = NULL;
    input_item_t *p_input;
    char psz_duration[MSTRTIME_MAX_SIZE+2];
    char *psz_display = NULL;

    for( ;; )
    {
        int canc = vlc_savecancel();
        if( p_intf->p_sys->b_need_update == true )
        {
            p_intf->p_sys->b_need_update = false;
            p_playlist = pl_Hold( p_intf );

            if( playlist_IsEmpty( p_playlist ) )
            {
                pl_Release( p_intf );
                continue;
            }
            free( psz_display );
            psz_display = NULL;
            int i_status = playlist_Status( p_playlist );
            if( i_status == PLAYLIST_STOPPED )
            {
                psz_display = strdup(_("Stop"));
                pl_Release( p_intf );
            }
            else if( i_status == PLAYLIST_PAUSED )
            {
                psz_display = strdup(_("Pause"));
                pl_Release( p_intf );
            }
            else
            {
                p_item = playlist_CurrentPlayingItem( p_playlist );
                p_input = p_item->p_input;

                pl_Release( p_intf );
                if( !p_item )
                    continue;

                mtime_t i_duration = input_item_GetDuration( p_input );
                if( i_duration != -1 )
                {
                    char psz_durationstr[MSTRTIME_MAX_SIZE];
                    secstotimestr( psz_durationstr, i_duration / 1000000 );
                    sprintf( psz_duration, "(%s)", psz_durationstr );
                }
                else
                {
                    sprintf( psz_duration," " );
                }

                psz_display = (char *)malloc(
                                          (strlen( p_input->psz_name ) +
                                          MSTRTIME_MAX_SIZE + 2+6 + 10 +10 ));
                sprintf( psz_display,"%s %s",
                         p_input->psz_name, psz_duration);
            }

            /* Display */
            xosd_display( p_intf->p_sys->p_osd,
                            0,                               /* first line */
                            XOSD_string,
                            psz_display );
        }

        vlc_restorecancel( canc );
        msleep( INTF_IDLE_SLEEP );
    }
}

static int PlaylistNext( vlc_object_t *p_this, const char *psz_variable,
                vlc_value_t oval, vlc_value_t nval, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;

    p_intf->p_sys->b_need_update = true;
    return VLC_SUCCESS;
}

