/*****************************************************************************
 * musicbrainz.c
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc_meta.h>

#include "musicbrainz/mb_c.h"


/*****************************************************************************
 * intf_sys_t: description and status of log interface
 *****************************************************************************/
struct intf_sys_t
{
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_shortname( N_( "MusicBrainz" ) );
    set_description( _("MusicBrainz meta data") );

    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist;

    MALLOC_ERR( p_intf->p_sys, intf_sys_t );

    p_playlist = pl_Yield( p_intf );
    var_AddCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_AddCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    pl_Release( p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist = pl_Yield( p_this );

    var_DelCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_DelCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    pl_Release( p_this );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    char *psz_title = NULL;
    char *psz_artist = NULL;
    char *psz_album = NULL;
    input_thread_t *p_input;
    playlist_t *p_playlist = pl_Yield( p_this );

    p_input = p_playlist->p_input;
    pl_Release( p_this );

    if( !p_input ) return VLC_SUCCESS;
    vlc_object_yield( p_input );

    if( p_input->b_dead || !p_input->input.p_item->psz_name )
    {
        /* Not playing anything ... */
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Playing something ... */
    psz_artist = p_input->input.p_item->p_meta->psz_artist;
    psz_album = p_input->input.p_item->p_meta->psz_album;
    psz_title = p_input->input.p_item->psz_name;

    if( psz_artist && psz_album /* && psz_title */ )
    {
        musicbrainz_t p_mb;
        char psz_buf[256];
        char psz_data[256];
        char i_album_count, i;
        char *ppsz_args[4];

        fprintf( stdout,"[33;1m--> %s %s %s[0m\n", psz_artist, psz_album, psz_title );
        p_mb = mb_New();
#ifdef WIN32
        mb_WSAInit( p_mb );
#endif
        mb_SetDepth( p_mb, 2 );
        ppsz_args[0] = psz_album;
        ppsz_args[1] = psz_artist;
        ppsz_args[2] = NULL;
        if( !mb_QueryWithArgs( p_mb,
            "<mq:FindAlbum>\n" \
            "   <mq:depth>@DEPTH@</mq:depth>\n" \
            "   <mq:maxItems>@MAX_ITEMS@</mq:maxItems>\n" \
            "   <mq:albumName>@1@</mq:albumName>\n" \
            "   <mq:artistName>@2@</mq:artistName>\n" \
            "</mq:FindAlbum>\n", ppsz_args ) )
        {
            mb_GetQueryError( p_mb, psz_buf, 256 );
            msg_Err( p_intf, "Query failed: %s\n", psz_buf );
            mb_Delete( p_mb );
            vlc_object_release( p_input );
            return VLC_EGENERIC;
        }

        i_album_count = mb_GetResultInt( p_mb, MBE_GetNumAlbums );
        if( i_album_count < 1 )
        {
            msg_Err( p_intf, "No albums found.\n" );
            mb_Delete( p_mb );
            vlc_object_release( p_input );
            return VLC_EGENERIC;
        }

        msg_Dbg( p_intf, "Found %d albums.\n", i_album_count );

        for( i = 1; i <= i_album_count; i++ )
        {
            mb_Select( p_mb, MBS_Rewind );
            mb_Select1( p_mb, MBS_SelectAlbum, i );

            mb_GetResultData( p_mb, MBE_AlbumGetAlbumId, psz_data, 256 );
            mb_GetIDFromURL( p_mb, psz_data, psz_buf, 256 );
            msg_Dbg( p_intf, "Album Id: %s", psz_buf );

            if( mb_GetResultData( p_mb, MBE_AlbumGetAmazonAsin, psz_buf, 256 ) )
            {
                msg_Dbg( p_intf, "Amazon ASIN: %s", psz_buf );
                msg_Dbg( p_intf, "Album art url: " "http://images.amazon.com/images/P/%s.01._AA240_SCLZZZZZZZ_.jpg", psz_buf );
                break;
            }
        }
#ifdef WIN32
        mb_WSAInit( p_mb );
#endif

        mb_Delete( p_mb );

    }

    vlc_object_release( p_input );

    return VLC_SUCCESS;
}
