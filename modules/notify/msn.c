/*****************************************************************************
 * msn.c : msn title plugin
 *****************************************************************************
 * Copyright (C) 2005-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <vlc_interface.h>
#include <vlc_playlist.h>                                      /* playlist_t */
#include <vlc_charset.h>                                           /* ToWide */

/*****************************************************************************
 * intf_sys_t: description and status of log interface
 *****************************************************************************/
struct intf_sys_t
{
    char *psz_format;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );
static int SendToMSN( const char * psz_msg );

#define MSN_MAX_LENGTH 256

/*****************************************************************************
 * Module descriptor
 *****************************************************************************
 * This module should be used on windows with MSN (i think that you need to
 * have MSN 7 or newer) to "advertise" what you are playing in VLC.
 * You need to enable the "What I'm Listening To" option in MSN.
 *****************************************************************************/
#define FORMAT_DEFAULT "{0} - {1}"
#define FORMAT_TEXT N_("Title format string")
#define FORMAT_LONGTEXT N_("Format of the string to send to MSN " \
"{0} Artist, {1} Title, {2} Album. Defaults to \"Artist - Title\" ({0} - {1}).")

vlc_module_begin ()
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_shortname( "MSN" )
    set_description( N_("MSN Now-Playing") )

    add_string( "msn-format", FORMAT_DEFAULT,
                FORMAT_TEXT, FORMAT_LONGTEXT, false )

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
        return VLC_ENOMEM;

    p_intf->p_sys->psz_format = var_InheritString( p_intf, "msn-format" );
    if( !p_intf->p_sys->psz_format )
        p_intf->p_sys->psz_format = strdup( FORMAT_DEFAULT );

    msg_Dbg( p_intf, "using MSN format: %s", p_intf->p_sys->psz_format );

    p_playlist = pl_Get( p_intf );
    var_AddCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_AddCallback( p_playlist, "item-current", ItemChange, p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist = pl_Get( p_this );

    /* clear the MSN stuff ... else it looks like we're still playing
     * something although VLC (or the MSN plugin) is closed */
    SendToMSN( "\\0Music\\01\\0\\0\\0\\0\\0\\0\\0" );

    var_DelCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_DelCallback( p_playlist, "item-current", ItemChange, p_intf );

    /* Destroy structure */
    free( p_intf->p_sys->psz_format );
    free( p_intf->p_sys );
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    (void)psz_var;    (void)oldval;    (void)newval;
    intf_thread_t *p_intf = (intf_thread_t *)param;
    char psz_tmp[MSN_MAX_LENGTH];
    input_thread_t *p_input =  playlist_CurrentInput( (playlist_t *) p_this );

    if( !p_input ) return VLC_SUCCESS;

    if( p_input->b_dead || !input_GetItem(p_input)->psz_name )
    {
        /* Not playing anything ... */
        SendToMSN( "\\0Music\\01\\0\\0\\0\\0\\0\\0\\0" );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Playing something ... */
    char *psz_artist = input_item_GetArtist( input_GetItem( p_input ) );
    char *psz_album = input_item_GetAlbum( input_GetItem( p_input ) );
    char *psz_title = input_item_GetTitleFbName( input_GetItem( p_input ) );

    snprintf( psz_tmp,
              MSN_MAX_LENGTH,
              "\\0Music\\01\\0%s\\0%s\\0%s\\0%s\\0\\0\\0",
              p_intf->p_sys->psz_format,
              psz_artist ? psz_artist : "",
              psz_title ? psz_title : "",
              psz_album );
    free( psz_title );
    free( psz_artist );
    free( psz_album );

    SendToMSN( (const char*)psz_tmp );
    vlc_object_release( p_input );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendToMSN
 *****************************************************************************/
static int SendToMSN( const char *psz_msg )
{
    COPYDATASTRUCT msndata;
    HWND msnui = NULL;

    wchar_t *wmsg = ToWide( psz_msg );
    if( unlikely(wmsg == NULL) )
        return VLC_ENOMEM;

    msndata.dwData = 0x547;
    msndata.lpData = wmsg;
    msndata.cbData = (wcslen(wmsg) + 1) * 2;

    while( ( msnui = FindWindowEx( NULL, msnui, "MsnMsgrUIManager", NULL ) ) )
    {
        SendMessage(msnui, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&msndata);
    }
    free( wmsg );
    return VLC_SUCCESS;
}
