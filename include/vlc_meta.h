/*****************************************************************************
 * vlc_meta.h: Stream meta-data
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef _VLC_META_H
#define _VLC_META_H 1

/* VLC meta name */
#define VLC_META_INFO_CAT           N_("Meta-information")
#define VLC_META_TITLE              N_("Title")
#define VLC_META_AUTHOR             N_("Author")
#define VLC_META_ARTIST             N_("Artist")
#define VLC_META_GENRE              N_("Genre")
#define VLC_META_COPYRIGHT          N_("Copyright")
#define VLC_META_COLLECTION         N_("Album/movie/show title")
#define VLC_META_SEQ_NUM            N_("Track number/position in set")
#define VLC_META_DESCRIPTION        N_("Description")
#define VLC_META_RATING             N_("Rating")
#define VLC_META_DATE               N_("Date")
#define VLC_META_SETTING            N_("Setting")
#define VLC_META_URL                N_("URL")
#define VLC_META_LANGUAGE           N_("Language")
#define VLC_META_NOW_PLAYING        N_("Now Playing")
#define VLC_META_PUBLISHER          N_("Publisher")
#define VLC_META_ENCODED_BY         N_("Encoded by")

#define VLC_META_ART_URL            N_("Art URL")

#define VLC_META_CODEC_NAME         N_("Codec Name")
#define VLC_META_CODEC_DESCRIPTION  N_("Codec Description")

#define ITEM_PREPARSED      0x01
#define ITEM_META_FETCHED   0x02
#define ITEM_ARTURL_FETCHED 0x04
#define ITEM_ART_FETCHED    0x08

struct vlc_meta_t
{
    char *psz_title;
    char *psz_author;
    char *psz_artist;
    char *psz_genre;
    char *psz_copyright;
    char *psz_album;
    char *psz_tracknum;
    char *psz_description;
    char *psz_rating;
    char *psz_date;
    char *psz_setting;
    char *psz_url;
    char *psz_language;
    char *psz_nowplaying;
    char *psz_publisher;
    char *psz_encodedby;
    char *psz_arturl;

    int i_status;
#if 0
    /* track meta information */
    int         i_track;
    vlc_meta_t  **track;
#endif
};

#define vlc_meta_Set( meta,var,val ) { \
    if( meta->psz_##var ) free( meta->psz_##var ); \
    meta->psz_##var = strdup( val ); }

#define vlc_meta_SetTitle( meta, b ) vlc_meta_Set( meta, title, b );
#define vlc_meta_SetArtist( meta, b ) vlc_meta_Set( meta, artist, b );
#define vlc_meta_SetAuthor( meta, b ) vlc_meta_Set( meta, author, b );
#define vlc_meta_SetGenre( meta, b ) vlc_meta_Set( meta, genre, b );
#define vlc_meta_SetCopyright( meta, b ) vlc_meta_Set( meta, copyright, b );
#define vlc_meta_SetAlbum( meta, b ) vlc_meta_Set( meta, album, b );
#define vlc_meta_SetTracknum( meta, b ) vlc_meta_Set( meta, tracknum, b );
#define vlc_meta_SetDescription( meta, b ) vlc_meta_Set( meta, description, b );
#define vlc_meta_SetRating( meta, b ) vlc_meta_Set( meta, rating, b );
#define vlc_meta_SetDate( meta, b ) vlc_meta_Set( meta, date, b );
#define vlc_meta_SetSetting( meta, b ) vlc_meta_Set( meta, setting, b );
#define vlc_meta_SetURL( meta, b ) vlc_meta_Set( meta, url, b );
#define vlc_meta_SetLanguage( meta, b ) vlc_meta_Set( meta, language, b );
#define vlc_meta_SetNowPlaying( meta, b ) vlc_meta_Set( meta, nowplaying, b );
#define vlc_meta_SetPublisher( meta, b ) vlc_meta_Set( meta, publisher, b );
#define vlc_meta_SetEncodedBy( meta, b ) vlc_meta_Set( meta, encodedby, b );
#define vlc_meta_SetArtURL( meta, b ) vlc_meta_Set( meta, arturl, b );

static inline vlc_meta_t *vlc_meta_New( void )
{
    vlc_meta_t *m = (vlc_meta_t*)malloc( sizeof( vlc_meta_t ) );
    if( !m ) return NULL;
    m->psz_title = NULL;
    m->psz_author = NULL;
    m->psz_artist = NULL;
    m->psz_genre = NULL;
    m->psz_copyright = NULL;
    m->psz_album = NULL;
    m->psz_tracknum = NULL;
    m->psz_description = NULL;
    m->psz_rating = NULL;
    m->psz_date = NULL;
    m->psz_setting = NULL;
    m->psz_url = NULL;
    m->psz_language = NULL;
    m->psz_nowplaying = NULL;
    m->psz_publisher = NULL;
    m->psz_encodedby = NULL;
    m->psz_arturl = NULL;
    m->i_status = 0;
    return m;
}

static inline void vlc_meta_Delete( vlc_meta_t *m )
{
    free( m->psz_title );
    free( m->psz_author );
    free( m->psz_artist );
    free( m->psz_genre );
    free( m->psz_copyright );
    free( m->psz_album );
    free( m->psz_tracknum );
    free( m->psz_description );
    free( m->psz_rating );
    free( m->psz_date );
    free( m->psz_setting );
    free( m->psz_url );
    free( m->psz_language );
    free( m->psz_nowplaying );
    free( m->psz_publisher );
    free( m->psz_encodedby );
    free( m->psz_arturl );

    free( m );
}

static inline void vlc_meta_Merge( vlc_meta_t *dst, vlc_meta_t *src )
{
    if( !dst || !src ) return;
#define COPY_FIELD( a ) \
    if( src->psz_ ## a ) { \
        if( dst->psz_ ## a ) free( dst->psz_## a ); \
        dst->psz_##a = strdup( src->psz_##a ); \
    }
    COPY_FIELD( title );
    COPY_FIELD( author );
    COPY_FIELD( artist );
    COPY_FIELD( genre );
    COPY_FIELD( copyright );
    COPY_FIELD( album );
    COPY_FIELD( tracknum );
    COPY_FIELD( description );
    COPY_FIELD( rating );
    COPY_FIELD( date );
    COPY_FIELD( setting );
    COPY_FIELD( url );
    COPY_FIELD( language );
    COPY_FIELD( nowplaying );
    COPY_FIELD( publisher );
    COPY_FIELD( encodedby );
    COPY_FIELD( arturl );
}
    /** \todo Track meta */

enum {
    ALBUM_ART_NEVER,
    ALBUM_ART_WHEN_ASKED,
    ALBUM_ART_WHEN_PLAYED,
    ALBUM_ART_ALL };

struct meta_export_t
{
    input_item_t *p_item;
    const char *psz_file;
};

#define VLC_META_ENGINE_TITLE           0x00000001
#define VLC_META_ENGINE_AUTHOR          0x00000002
#define VLC_META_ENGINE_ARTIST          0x00000004
#define VLC_META_ENGINE_GENRE           0x00000008
#define VLC_META_ENGINE_COPYRIGHT       0x00000010
#define VLC_META_ENGINE_COLLECTION      0x00000020
#define VLC_META_ENGINE_SEQ_NUM         0x00000040
#define VLC_META_ENGINE_DESCRIPTION     0x00000080
#define VLC_META_ENGINE_RATING          0x00000100
#define VLC_META_ENGINE_DATE            0x00000200
#define VLC_META_ENGINE_URL             0x00000400
#define VLC_META_ENGINE_LANGUAGE        0x00000800

#define VLC_META_ENGINE_ART_URL         0x00001000

#define VLC_META_ENGINE_MB_ARTIST_ID    0x00002000
#define VLC_META_ENGINE_MB_RELEASE_ID   0x00004000
#define VLC_META_ENGINE_MB_TRACK_ID     0x00008000
#define VLC_META_ENGINE_MB_TRM_ID       0x00010000

typedef struct meta_engine_sys_t meta_engine_sys_t;

struct meta_engine_t
{
    VLC_COMMON_MEMBERS

    module_t *p_module;

    uint32_t i_mandatory; /**< Stuff which we really need to get */
    uint32_t i_optional; /**< Stuff which we'd like to have */

    input_item_t *p_item;
};

VLC_EXPORT( uint32_t, input_GetMetaEngineFlags, ( vlc_meta_t *p_meta ) );

#endif
