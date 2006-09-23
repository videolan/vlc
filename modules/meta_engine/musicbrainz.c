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
#include <vlc_meta_engine.h>

#include "musicbrainz/mb_c.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int FindMeta( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
/*    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );*/
    set_shortname( N_( "MusicBrainz" ) );
    set_description( _("MusicBrainz meta data") );

    set_capability( "meta engine", 80 );
    set_callbacks( FindMeta, NULL );
vlc_module_end();

/*****************************************************************************
 *****************************************************************************/
static int FindMeta( vlc_object_t *p_this )
{
    meta_engine_t *p_me = (meta_engine_t *)p_this;
    input_item_t *p_item = p_me->p_item;

    char *psz_title = NULL;
    char *psz_artist = NULL;
    char *psz_album = NULL;

    char psz_buf[256];
    char psz_data[256];
    char i_album_count, i;
    char *ppsz_args[4];

    if( !p_item->p_meta ) return VLC_EGENERIC;
    psz_artist = p_item->p_meta->psz_artist;
    psz_album = p_item->p_meta->psz_album;
    psz_title = p_item->psz_name;

    if( !psz_artist || !psz_album )
        return VLC_EGENERIC;
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
        msg_Err( p_me, "Query failed: %s\n", psz_buf );
        mb_Delete( p_mb );
        return VLC_EGENERIC;
    }

    i_album_count = mb_GetResultInt( p_mb, MBE_GetNumAlbums );
    if( i_album_count < 1 )
    {
        msg_Err( p_me, "No albums found.\n" );
        mb_Delete( p_mb );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_me, "Found %d albums.\n", i_album_count );

    for( i = 1; i <= i_album_count; i++ )
    {
        mb_Select( p_mb, MBS_Rewind );
        mb_Select1( p_mb, MBS_SelectAlbum, i );

        mb_GetResultData( p_mb, MBE_AlbumGetAlbumId, psz_data, 256 );
        mb_GetIDFromURL( p_mb, psz_data, psz_buf, 256 );
        msg_Dbg( p_me, "Album Id: %s", psz_buf );

        if( mb_GetResultData( p_mb, MBE_AlbumGetAmazonAsin, psz_buf, 256 ) )
        {
            msg_Dbg( p_me, "Amazon ASIN: %s", psz_buf );
            sprintf( psz_data, "http://images.amazon.com/images/P/%s.01._SCLZZZZZZZ_.jpg", psz_buf );
            vlc_meta_SetArtURL( p_item->p_meta, psz_data );
            break;
        }
    }
#ifdef WIN32
    mb_WSAInit( p_mb );
#endif

    mb_Delete( p_mb );

    return VLC_SUCCESS;
}
