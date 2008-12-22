/*****************************************************************************
 * meta.c : Metadata handling
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>
#include "input_internal.h"
#include "../playlist/art.h"

/* FIXME bad name convention */
const char * input_MetaTypeToLocalizedString( vlc_meta_type_t meta_type )
{
    switch( meta_type )
    {
    case vlc_meta_Title:        return _("Title");
    case vlc_meta_Artist:       return _("Artist");
    case vlc_meta_Genre:        return _("Genre");
    case vlc_meta_Copyright:    return _("Copyright");
    case vlc_meta_Album:        return _("Album");
    case vlc_meta_TrackNumber:  return _("Track number");
    case vlc_meta_Description:  return _("Description");
    case vlc_meta_Rating:       return _("Rating");
    case vlc_meta_Date:         return _("Date");
    case vlc_meta_Setting:      return _("Setting");
    case vlc_meta_URL:          return _("URL");
    case vlc_meta_Language:     return _("Language");
    case vlc_meta_NowPlaying:   return _("Now Playing");
    case vlc_meta_Publisher:    return _("Publisher");
    case vlc_meta_EncodedBy:    return _("Encoded by");
    case vlc_meta_ArtworkURL:   return _("Artwork URL");
    case vlc_meta_TrackID:      return _("Track ID");

    default: abort();
    }
};

void input_ExtractAttachmentAndCacheArt( input_thread_t *p_input )
{
    input_item_t *p_item = p_input->p->p_item;

    /* */
    const char *psz_arturl = vlc_meta_Get( p_item->p_meta, vlc_meta_ArtworkURL );
    if( !psz_arturl || strncmp( psz_arturl, "attachment://", strlen("attachment://") ) )
    {
        msg_Err( p_input, "internal input error with input_ExtractAttachmentAndCacheArt" );
        return;
    }

    playlist_t *p_playlist = pl_Hold( p_input );
    if( !p_playlist )
        return;


    if( input_item_IsArtFetched( p_item ) )
    {
        /* XXX Weird, we should not have end up with attachment:// art url unless there is a race
         * condition */
        msg_Warn( p_input, "internal input error with input_ExtractAttachmentAndCacheArt" );
        playlist_FindArtInCache( p_item );
        pl_Release( p_playlist );
        return;
    }

    /* */
    input_attachment_t *p_attachment = NULL;
    for( int i_idx = 0; i_idx < p_input->p->i_attachment; i_idx++ )
    {
        if( !strcmp( p_input->p->attachment[i_idx]->psz_name,
                     &psz_arturl[strlen("attachment://")] ) )
        {
            p_attachment = p_input->p->attachment[i_idx];
            break;
        }
    }
    if( !p_attachment || p_attachment->i_data <= 0 )
    {
        msg_Warn( p_input, "internal input error with input_ExtractAttachmentAndCacheArt" );
        pl_Release( p_playlist );
        return;
    }

    /* */
    const char *psz_type = NULL;
    if( !strcmp( p_attachment->psz_mime, "image/jpeg" ) )
        psz_type = ".jpg";
    else if( !strcmp( p_attachment->psz_mime, "image/png" ) )
        psz_type = ".png";

    /* */
    playlist_SaveArt( p_playlist, p_item,
                      p_attachment->p_data, p_attachment->i_data, psz_type );

    pl_Release( p_playlist );
}

