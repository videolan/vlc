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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_META_H
#define _VLC_META_H 1

#include <vlc_arrays.h>

#define VLC_META_TYPE_COUNT 17

typedef enum vlc_meta_type_t
{
    vlc_meta_Title = 0,
    vlc_meta_Artist,
    vlc_meta_Genre,
    vlc_meta_Copyright,
    vlc_meta_Album,
    vlc_meta_TrackNumber,
    vlc_meta_Description,
    vlc_meta_Rating,
    vlc_meta_Date,
    vlc_meta_Setting,
    vlc_meta_URL,
    vlc_meta_Language,
    vlc_meta_NowPlaying,
    vlc_meta_Publisher,
    vlc_meta_EncodedBy,
    vlc_meta_ArtworkURL,
    vlc_meta_TrackID
} vlc_meta_type_t;

/* Returns a localizes string describing the meta */
VLC_EXPORT(const char *, input_MetaTypeToLocalizedString, ( vlc_meta_type_t meta_type ) );

#define ITEM_PREPARSED      0x01
#define ITEM_META_FETCHED   0x02
#define ITEM_ARTURL_FETCHED 0x04
#define ITEM_ART_FETCHED    0x08
#define ITEM_ART_NOTFOUND   0x10

struct vlc_meta_t
{
    char * ppsz_meta[VLC_META_TYPE_COUNT];

    vlc_dictionary_t extra_tags;

    int i_status;

};

/* Setters for meta.
 * Warning: Make sure to use the input_item meta setters (defined in vlc_input.h)
 * instead of those one. */
#define vlc_meta_SetTitle( meta, b )       vlc_meta_Set( meta, vlc_meta_Title, b )
#define vlc_meta_SetArtist( meta, b )      vlc_meta_Set( meta, vlc_meta_Artist, b )
#define vlc_meta_SetGenre( meta, b )       vlc_meta_Set( meta, vlc_meta_Genre, b )
#define vlc_meta_SetCopyright( meta, b )   vlc_meta_Set( meta, vlc_meta_Copyright, b )
#define vlc_meta_SetAlbum( meta, b )       vlc_meta_Set( meta, vlc_meta_Album, b )
#define vlc_meta_SetTracknum( meta, b )    vlc_meta_Set( meta, vlc_meta_TrackNumber, b )
#define vlc_meta_SetDescription( meta, b ) vlc_meta_Set( meta, vlc_meta_Description, b )
#define vlc_meta_SetRating( meta, b )      vlc_meta_Set( meta, vlc_meta_Rating, b )
#define vlc_meta_SetDate( meta, b )        vlc_meta_Set( meta, vlc_meta_Date, b )
#define vlc_meta_SetSetting( meta, b )     vlc_meta_Set( meta, vlc_meta_Setting, b )
#define vlc_meta_SetURL( meta, b )         vlc_meta_Set( meta, vlc_meta_URL, b )
#define vlc_meta_SetLanguage( meta, b )    vlc_meta_Set( meta, vlc_meta_Language, b )
#define vlc_meta_SetNowPlaying( meta, b )  vlc_meta_Set( meta, vlc_meta_NowPlaying, b )
#define vlc_meta_SetPublisher( meta, b )   vlc_meta_Set( meta, vlc_meta_Publisher, b )
#define vlc_meta_SetEncodedBy( meta, b )   vlc_meta_Set( meta, vlc_meta_EncodedBy, b )
#define vlc_meta_SetArtURL( meta, b )      vlc_meta_Set( meta, vlc_meta_ArtworkURL, b )
#define vlc_meta_SetTrackID( meta, b )     vlc_meta_Set( meta, vlc_meta_TrackID, b )

static inline void vlc_meta_Set( vlc_meta_t * p_meta, vlc_meta_type_t meta_type, const char * psz_val )
{
    free( p_meta->ppsz_meta[meta_type] );
    p_meta->ppsz_meta[meta_type] = psz_val ? strdup( psz_val ) : NULL;
}

static inline const char * vlc_meta_Get( const vlc_meta_t * p_meta, vlc_meta_type_t meta_type )
{
    return p_meta->ppsz_meta[meta_type];
}

static inline vlc_meta_t *vlc_meta_New( void )
{
    vlc_meta_t *m = (vlc_meta_t*)malloc( sizeof( vlc_meta_t ) );
    if( !m ) return NULL;
    memset( m->ppsz_meta, 0, sizeof(m->ppsz_meta) );
    m->i_status = 0;
    vlc_dictionary_init( &m->extra_tags, 0 );
    return m;
}

static inline void vlc_meta_Delete( vlc_meta_t *m )
{
    int i;
    for( i = 0; i < 0; i++ )
        free( m->ppsz_meta[i] );
    vlc_dictionary_clear( &m->extra_tags );
    free( m );
}

static inline void vlc_meta_AddExtra( vlc_meta_t *m, const char *psz_name, const char *psz_value )
{
    char * psz_oldvalue = (char *)vlc_dictionary_value_for_key( &m->extra_tags, psz_name );
    if( psz_oldvalue != kVLCDictionaryNotFound )
    {
        free( psz_oldvalue );
        vlc_dictionary_remove_value_for_key( &m->extra_tags, psz_name );
    }
    vlc_dictionary_insert( &m->extra_tags, psz_name, strdup(psz_value) );
}

static inline void vlc_meta_Merge( vlc_meta_t *dst, const vlc_meta_t *src )
{
    char ** ppsz_all_keys;
    int i;

    if( !dst || !src ) return;

    for( i = 0; i < VLC_META_TYPE_COUNT; i++ )
    {
        if( src->ppsz_meta[i] )
        {
            free( dst->ppsz_meta[i] );
            dst->ppsz_meta[i] = strdup( src->ppsz_meta[i] );
        }
    }

    /* XXX: If speed up are needed, it is possible */
    ppsz_all_keys = vlc_dictionary_all_keys( &src->extra_tags );
    for( i = 0; ppsz_all_keys[i]; i++ )
    {
        /* Always try to remove the previous value */
        vlc_dictionary_remove_value_for_key( &dst->extra_tags, ppsz_all_keys[i] );
        void * p_value = vlc_dictionary_value_for_key( &src->extra_tags, ppsz_all_keys[i] );
        vlc_dictionary_insert( &dst->extra_tags, ppsz_all_keys[i], p_value );
        free( ppsz_all_keys[i] );
    }
    free( ppsz_all_keys );
}

/* Shortcuts for the AddInfo */
#define VLC_META_INFO_CAT           N_("Meta-information")
#define VLC_META_TITLE              input_MetaTypeToLocalizedString( vlc_meta_Title )
#define VLC_META_DURATION           N_( "Duration" )
#define VLC_META_ARTIST             input_MetaTypeToLocalizedString( vlc_meta_Artist )
#define VLC_META_GENRE              input_MetaTypeToLocalizedString( vlc_meta_Genre )
#define VLC_META_COPYRIGHT          input_MetaTypeToLocalizedString( vlc_meta_Copyright )
#define VLC_META_COLLECTION         input_MetaTypeToLocalizedString( vlc_meta_Album )
#define VLC_META_SEQ_NUM            input_MetaTypeToLocalizedString( vlc_meta_TrackNumber )
#define VLC_META_DESCRIPTION        input_MetaTypeToLocalizedString( vlc_meta_Description )
#define VLC_META_RATING             input_MetaTypeToLocalizedString( vlc_meta_Rating )
#define VLC_META_DATE               input_MetaTypeToLocalizedString( vlc_meta_Date )
#define VLC_META_SETTING            input_MetaTypeToLocalizedString( vlc_meta_Setting )
#define VLC_META_URL                input_MetaTypeToLocalizedString( vlc_meta_URL )
#define VLC_META_LANGUAGE           input_MetaTypeToLocalizedString( vlc_meta_Language )
#define VLC_META_NOW_PLAYING        input_MetaTypeToLocalizedString( vlc_meta_NowPlaying )
#define VLC_META_PUBLISHER          input_MetaTypeToLocalizedString( vlc_meta_Publisher )
#define VLC_META_ENCODED_BY         input_MetaTypeToLocalizedString( vlc_meta_EncodedBy )
#define VLC_META_ART_URL            input_MetaTypeToLocalizedString( vlc_meta_ArtworkURL )
#define VLC_META_TRACKID            input_MetaTypeToLocalizedString( vlc_meta_TrackID )

enum {
    ALBUM_ART_WHEN_ASKED,
    ALBUM_ART_WHEN_PLAYED,
    ALBUM_ART_ALL
};

struct meta_export_t
{
    input_item_t *p_item;
    const char *psz_file;
};

#define VLC_META_ENGINE_TRACKID         0x00000001
#define VLC_META_ENGINE_TITLE           0x00000002
#define VLC_META_ENGINE_DURATION        0x00000004
#define VLC_META_ENGINE_ARTIST          0x00000008
#define VLC_META_ENGINE_GENRE           0x00000010
#define VLC_META_ENGINE_COPYRIGHT       0x00000020
#define VLC_META_ENGINE_COLLECTION      0x00000040
#define VLC_META_ENGINE_SEQ_NUM         0x00000080
#define VLC_META_ENGINE_DESCRIPTION     0x00000100
#define VLC_META_ENGINE_RATING          0x00000200
#define VLC_META_ENGINE_DATE            0x00000400
#define VLC_META_ENGINE_URL             0x00000800
#define VLC_META_ENGINE_LANGUAGE        0x00001000

#define VLC_META_ENGINE_ART_URL         0x00002000

#if 0 /* unused (yet?) */
#define VLC_META_ENGINE_MB_ARTIST_ID    0x00002000
#define VLC_META_ENGINE_MB_RELEASE_ID   0x00004000
#define VLC_META_ENGINE_MB_TRACK_ID     0x00008000
#define VLC_META_ENGINE_MB_TRM_ID       0x00010000
#endif

typedef struct meta_engine_sys_t meta_engine_sys_t;

struct meta_engine_t
{
    VLC_COMMON_MEMBERS

    module_t *p_module;

    uint32_t i_mandatory; /**< Stuff which we really need to get */
    uint32_t i_optional; /**< Stuff which we'd like to have */

    input_item_t *p_item;
};

VLC_EXPORT(uint32_t, input_CurrentMetaFlags,( const vlc_meta_t *p_meta ) );

#endif
