/*****************************************************************************
 * sorting.h : commun sorting & column display code
 ****************************************************************************
 * Copyright © 2008 the VideoLAN team and AUTHORS
 * $Id$
 *
 * Authors: Rafaël Carré <funman@videolanorg>
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

#ifndef VLC_QT_SORTING_H_
#define VLC_QT_SORTING_H_

#include <vlc_media_library.h>
#include <vlc_input_item.h>

/* You can use these numbers with | and & to determine what you want to show */
enum
{
    COLUMN_TITLE          = 0x0001,
    COLUMN_DURATION       = 0x0002,
    COLUMN_ARTIST         = 0x0004,
    COLUMN_GENRE          = 0x0008,
    COLUMN_ALBUM          = 0x0010,
    COLUMN_TRACK_NUMBER   = 0x0020,
    COLUMN_DESCRIPTION    = 0x0040,
    COLUMN_URI            = 0x0080,
    COLUMN_NUMBER         = 0x0100,
    COLUMN_RATING         = 0x0200,
    COLUMN_COVER          = 0x0400,
    COLUMN_DISC_NUMBER    = 0x0800,
    COLUMN_DATE           = 0x1000,

    /* Add new entries here and update the COLUMN_END value*/

    COLUMN_END            = 0x2000
};

#define COLUMN_DEFAULT (COLUMN_TITLE|COLUMN_DURATION|COLUMN_ALBUM)

/* Return the title of a column */
static inline const char * psz_column_title( uint32_t i_column )
{
    switch( i_column )
    {
    case COLUMN_NUMBER:          return _("ID");
    case COLUMN_TITLE:           return VLC_META_TITLE;
    case COLUMN_DURATION:        return _("Duration");
    case COLUMN_ARTIST:          return VLC_META_ARTIST;
    case COLUMN_GENRE:           return VLC_META_GENRE;
    case COLUMN_ALBUM:           return VLC_META_ALBUM;
    case COLUMN_TRACK_NUMBER:    return VLC_META_TRACK_NUMBER;
    case COLUMN_DESCRIPTION:     return VLC_META_DESCRIPTION;
    case COLUMN_URI:             return _("URI");
    case COLUMN_RATING:          return VLC_META_RATING;
    case COLUMN_COVER:           return _("Cover");
    case COLUMN_DISC_NUMBER:     return VLC_META_DISCNUMBER;
    case COLUMN_DATE:            return VLC_META_DATE;
    default: abort();
    }
}

/* Return the meta data associated with an item for a column
 * Returned value has to be freed */
static inline char * psz_column_meta( input_item_t *p_item, uint32_t i_column )
{
    int i_duration;
    char psz_duration[MSTRTIME_MAX_SIZE];
    switch( i_column )
    {
    case COLUMN_NUMBER:
        return NULL;
    case COLUMN_TITLE:
        return input_item_GetTitleFbName( p_item );
    case COLUMN_DURATION:
        i_duration = SEC_FROM_VLC_TICK( input_item_GetDuration( p_item ) );
        if( i_duration == 0 ) return NULL;
        secstotimestr( psz_duration, i_duration );
        return strdup( psz_duration );
    case COLUMN_ARTIST:
        return input_item_GetArtist( p_item );
    case COLUMN_GENRE:
        return input_item_GetGenre( p_item );
    case COLUMN_ALBUM:
        return input_item_GetAlbum( p_item );
    case COLUMN_TRACK_NUMBER:
        return input_item_GetTrackNum( p_item );
    case COLUMN_DESCRIPTION:
        return input_item_GetDescription( p_item );
    case COLUMN_URI:
        return input_item_GetURI( p_item );
    case COLUMN_RATING:
        return input_item_GetRating( p_item );
    case COLUMN_COVER:
        return input_item_GetArtworkURL( p_item );
    case COLUMN_DISC_NUMBER:
        return input_item_GetDiscNumber( p_item );
    case COLUMN_DATE:
        return input_item_GetDate( p_item );
    default:
        abort();
    }
}

/* Return the playlist sorting mode for a given column */
static inline int i_column_sorting( uint32_t i_column )
{
    switch( i_column )
    {
    case COLUMN_NUMBER:         return SORT_ID;
    case COLUMN_TITLE:          return SORT_TITLE_NODES_FIRST;
    case COLUMN_DURATION:       return SORT_DURATION;
    case COLUMN_ARTIST:         return SORT_ARTIST;
    case COLUMN_GENRE:          return SORT_GENRE;
    case COLUMN_ALBUM:          return SORT_ALBUM;
    case COLUMN_TRACK_NUMBER:   return SORT_TRACK_NUMBER;
    case COLUMN_DESCRIPTION:    return SORT_DESCRIPTION;
    case COLUMN_URI:            return SORT_URI;
    case COLUMN_RATING:         return SORT_RATING;
    case COLUMN_DISC_NUMBER:    return SORT_DISC_NUMBER;
    case COLUMN_DATE:           return SORT_DATE;
    default: abort();
    }
}

#endif
