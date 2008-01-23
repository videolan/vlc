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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>

#include "musicbrainz/mb_c.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int FindArt( vlc_object_t * );
static int FindMetaMBId( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_shortname( N_( "MusicBrainz" ) );
    set_description( _("MusicBrainz meta data") );

    set_capability( "meta fetcher", 10 );
        /* This meta fetcher module only retrieves the musicbrainz track id
         * and stores it
         * TODO:
         *  - Actually do it
         *  - Also store the album id
         * */
        set_callbacks( FindMetaMBId, NULL );
    add_submodule();
        /* This art finder module fetches the album ID from musicbrainz and
         * uses it to fetch the amazon ASIN from musicbrainz.
         * TODO:
         *  - Add ability to reuse MB album ID if we already have it
         */
        set_capability( "art finder", 80 );
        set_callbacks( FindArt, NULL );
vlc_module_end();

/*****************************************************************************
 *****************************************************************************/

static int GetData( vlc_object_t *p_obj, input_item_t *p_item,
                    vlc_bool_t b_art )
{
    char psz_buf[256];
    char psz_data[256];
    char i_album_count, i;
    char *ppsz_args[4];
    vlc_bool_t b_art_found = VLC_FALSE;

    char *psz_artist;
    char *psz_album;

    psz_artist = input_item_GetArtist( p_item );
    psz_album = input_item_GetAlbum( p_item );

    if( !psz_artist || !psz_album )
    {
        free( psz_artist );
        free( psz_album );
        return VLC_EGENERIC;
    }

    musicbrainz_t p_mb;

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
        msg_Err( p_obj, "Query failed: %s", psz_buf );
        mb_Delete( p_mb );
        free( psz_artist );
        free( psz_album );
        return VLC_EGENERIC;
    }
    free( psz_artist );
    free( psz_album );

    i_album_count = mb_GetResultInt( p_mb, MBE_GetNumAlbums );
    if( i_album_count < 1 )
    {
        mb_Delete( p_mb );
        return VLC_EGENERIC;
    }

    /** \todo Get the MB Track ID and store it */
    msg_Dbg( p_obj, "found %d albums.\n", i_album_count );

    for( i = 1; i <= i_album_count; i++ )
    {
        mb_Select( p_mb, MBS_Rewind );
        mb_Select1( p_mb, MBS_SelectAlbum, i );

        mb_GetResultData( p_mb, MBE_AlbumGetAlbumId, psz_data, 256 );
        mb_GetIDFromURL( p_mb, psz_data, psz_buf, 256 );
        msg_Dbg( p_obj, "album Id: %s", psz_buf );


        if( !b_art )
            break;

        if( mb_GetResultData( p_mb, MBE_AlbumGetAmazonAsin, psz_buf, 256 ) )
        {
            msg_Dbg( p_obj, "Amazon ASIN: %s", psz_buf );
            snprintf( psz_data, 255,
                    "http://images.amazon.com/images/P/%s.01._SCLZZZZZZZ_.jpg",
                    psz_buf );
            msg_Dbg( p_obj, "Album art URL: %s", psz_data );
            input_item_SetArtURL( p_item, psz_data );
            b_art_found = VLC_TRUE;
            break;
        }
    }
#ifdef WIN32
    mb_WSAInit( p_mb );
#endif
    mb_Delete( p_mb );

    if( !b_art )
        return VLC_SUCCESS;
    else
        return b_art_found ? VLC_SUCCESS : VLC_EGENERIC;
}

static int FindMetaMBId( vlc_object_t *p_this )
{
    meta_engine_t *p_me = (meta_engine_t *)p_this;
    input_item_t *p_item = p_me->p_item;
    int i_ret = GetData( VLC_OBJECT(p_me), p_item,
                         p_me->i_mandatory & VLC_META_ENGINE_ART_URL );

    if( !i_ret )
    {
        uint32_t i_meta = input_CurrentMetaFlags( input_item_GetMetaObject( p_item ) );
        p_me->i_mandatory &= ~i_meta;
        p_me->i_optional &= ~i_meta;
        return p_me->i_mandatory ? VLC_EGENERIC : VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static int FindArt( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t *)p_this;
    input_item_t *p_item = (input_item_t *)(p_playlist->p_private);
    assert( p_item );

    return GetData( VLC_OBJECT(p_playlist), p_item, VLC_TRUE );
}
