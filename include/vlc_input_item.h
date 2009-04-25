/*****************************************************************************
 * vlc_input_item.h: Core input item
 *****************************************************************************
 * Copyright (C) 1999-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC__INPUT_ITEM_H
#define VLC__INPUT_ITEM_H 1

/**
 * \file
 * This file defines functions, structures and enums for input items in vlc
 */

#include <vlc_meta.h>

#include <string.h>

/*****************************************************************************
 * input_item_t: Describes an input and is used to spawn input_thread_t objects
 *****************************************************************************/
struct info_t
{
    char *psz_name;            /**< Name of this info */
    char *psz_value;           /**< Value of the info */
};

struct info_category_t
{
    char   *psz_name;      /**< Name of this category */
    int    i_infos;        /**< Number of infos in the category */
    struct info_t **pp_infos;     /**< Pointer to an array of infos */
};

struct input_item_t
{
    VLC_GC_MEMBERS
    int        i_id;                 /**< Identifier of the item */

    char       *psz_name;            /**< text describing this item */
    char       *psz_uri;             /**< mrl of this item */
    bool       b_fixed_name;        /**< Can the interface change the name ?*/

    int        i_options;            /**< Number of input options */
    char       **ppsz_options;       /**< Array of input options */
    uint8_t    *optflagv;            /**< Some flags of input options */
    unsigned   optflagc;

    mtime_t    i_duration;           /**< Duration in milliseconds*/

    uint8_t    i_type;               /**< Type (file, disc, ... see input_item_type_e) */
    bool b_prefers_tree;             /**< Do we prefer being displayed as tree*/

    int        i_categories;         /**< Number of info categories */
    info_category_t **pp_categories; /**< Pointer to the first info category */

    int         i_es;                /**< Number of es format descriptions */
    es_format_t **es;                /**< Es formats */

    input_stats_t *p_stats;          /**< Statistics */
    int           i_nb_played;       /**< Number of times played */

    bool          b_error_when_reading;       /**< Error When Reading */

    vlc_meta_t *p_meta;

    vlc_event_manager_t event_manager;

    vlc_mutex_t lock;                 /**< Lock for the item */
};

enum input_item_type_e
{
    ITEM_TYPE_UNKNOWN,
    ITEM_TYPE_FILE,
    ITEM_TYPE_DIRECTORY,
    ITEM_TYPE_DISC,
    ITEM_TYPE_CDDA,
    ITEM_TYPE_CARD,
    ITEM_TYPE_NET,
    ITEM_TYPE_PLAYLIST,
    ITEM_TYPE_NODE,

    /* This one is not a real type but the number of input_item types. */
    ITEM_TYPE_NUMBER
};

VLC_EXPORT( void, input_item_CopyOptions, ( input_item_t *p_parent, input_item_t *p_child ) );
VLC_EXPORT( void, input_item_SetName, ( input_item_t *p_item, const char *psz_name ) );

/* This won't hold the item, but can tell to interested third parties
 * Like the playlist, that there is a new sub item. With this design
 * It is not the input item's responsability to keep all the ref of
 * the input item children. */
VLC_EXPORT( void, input_item_AddSubItem, ( input_item_t *p_parent, input_item_t *p_child ) );


/**
 * Option flags
 */
enum input_item_option_e
{
    /* Allow VLC to trust the given option.
     * By default options are untrusted */
    VLC_INPUT_OPTION_TRUSTED = 0x2,

    /* Change the value associated to an option if already present, otherwise
     * add the option */
    VLC_INPUT_OPTION_UNIQUE  = 0x100,
};

/**
 * This function allows to add an option to an existing input_item_t.
 */
VLC_EXPORT( int,  input_item_AddOption, (input_item_t *, const char *, unsigned i_flags ) );

/* */
VLC_EXPORT( bool, input_item_HasErrorWhenReading, ( input_item_t * ) );
VLC_EXPORT( void, input_item_SetMeta, ( input_item_t *, vlc_meta_type_t meta_type, const char *psz_val ));
VLC_EXPORT( bool, input_item_MetaMatch, ( input_item_t *p_i, vlc_meta_type_t meta_type, const char *psz ) );
VLC_EXPORT( char *, input_item_GetMeta, ( input_item_t *p_i, vlc_meta_type_t meta_type ) );
VLC_EXPORT( char *, input_item_GetName, ( input_item_t * p_i ) );
VLC_EXPORT( char *, input_item_GetTitleFbName, ( input_item_t * p_i ) );
VLC_EXPORT( char *, input_item_GetURI, ( input_item_t * p_i ) );
VLC_EXPORT( void,   input_item_SetURI, ( input_item_t * p_i, const char *psz_uri ));
VLC_EXPORT(mtime_t, input_item_GetDuration, ( input_item_t * p_i ) );
VLC_EXPORT( void,   input_item_SetDuration, ( input_item_t * p_i, mtime_t i_duration ));
VLC_EXPORT( bool,   input_item_IsPreparsed, ( input_item_t *p_i ));
VLC_EXPORT( bool,   input_item_IsArtFetched, ( input_item_t *p_i ));


#define input_item_SetTitle( item, b )       input_item_SetMeta( item, vlc_meta_Title, b )
#define input_item_SetArtist( item, b )      input_item_SetMeta( item, vlc_meta_Artist, b )
#define input_item_SetGenre( item, b )       input_item_SetMeta( item, vlc_meta_Genre, b )
#define input_item_SetCopyright( item, b )   input_item_SetMeta( item, vlc_meta_Copyright, b )
#define input_item_SetAlbum( item, b )       input_item_SetMeta( item, vlc_meta_Album, b )
#define input_item_SetTrackNum( item, b )    input_item_SetMeta( item, vlc_meta_TrackNumber, b )
#define input_item_SetDescription( item, b ) input_item_SetMeta( item, vlc_meta_Description, b )
#define input_item_SetRating( item, b )      input_item_SetMeta( item, vlc_meta_Rating, b )
#define input_item_SetDate( item, b )        input_item_SetMeta( item, vlc_meta_Date, b )
#define input_item_SetSetting( item, b )     input_item_SetMeta( item, vlc_meta_Setting, b )
#define input_item_SetURL( item, b )         input_item_SetMeta( item, vlc_meta_URL, b )
#define input_item_SetLanguage( item, b )    input_item_SetMeta( item, vlc_meta_Language, b )
#define input_item_SetNowPlaying( item, b )  input_item_SetMeta( item, vlc_meta_NowPlaying, b )
#define input_item_SetPublisher( item, b )   input_item_SetMeta( item, vlc_meta_Publisher, b )
#define input_item_SetEncodedBy( item, b )   input_item_SetMeta( item, vlc_meta_EncodedBy, b )
#define input_item_SetArtURL( item, b )      input_item_SetMeta( item, vlc_meta_ArtworkURL, b )
#define input_item_SetTrackID( item, b )     input_item_SetMeta( item, vlc_meta_TrackID, b )

#define input_item_GetTitle( item )          input_item_GetMeta( item, vlc_meta_Title )
#define input_item_GetArtist( item )         input_item_GetMeta( item, vlc_meta_Artist )
#define input_item_GetGenre( item )          input_item_GetMeta( item, vlc_meta_Genre )
#define input_item_GetCopyright( item )      input_item_GetMeta( item, vlc_meta_Copyright )
#define input_item_GetAlbum( item )          input_item_GetMeta( item, vlc_meta_Album )
#define input_item_GetTrackNum( item )       input_item_GetMeta( item, vlc_meta_TrackNumber )
#define input_item_GetDescription( item )    input_item_GetMeta( item, vlc_meta_Description )
#define input_item_GetRating( item )         input_item_GetMeta( item, vlc_meta_Rating )
#define input_item_GetDate( item )           input_item_GetMeta( item, vlc_meta_Date )
#define input_item_GetGetting( item )        input_item_GetMeta( item, vlc_meta_Getting )
#define input_item_GetURL( item )            input_item_GetMeta( item, vlc_meta_URL )
#define input_item_GetLanguage( item )       input_item_GetMeta( item, vlc_meta_Language )
#define input_item_GetNowPlaying( item )     input_item_GetMeta( item, vlc_meta_NowPlaying )
#define input_item_GetPublisher( item )      input_item_GetMeta( item, vlc_meta_Publisher )
#define input_item_GetEncodedBy( item )      input_item_GetMeta( item, vlc_meta_EncodedBy )
#define input_item_GetArtURL( item )         input_item_GetMeta( item, vlc_meta_ArtworkURL )
#define input_item_GetTrackID( item )        input_item_GetMeta( item, vlc_meta_TrackID )
#define input_item_GetSetting( item )        input_item_GetMeta( item, vlc_meta_Setting )

VLC_EXPORT( char *, input_item_GetInfo, ( input_item_t *p_i, const char *psz_cat,const char *psz_name ) );
VLC_EXPORT( int, input_item_AddInfo, ( input_item_t *p_i, const char *psz_cat, const char *psz_name, const char *psz_format, ... ) LIBVLC_FORMAT( 4, 5 ) );
VLC_EXPORT( int, input_item_DelInfo, ( input_item_t *p_i, const char *psz_cat, const char *psz_name ) );

/**
 * This function creates a new input_item_t with the provided informations.
 *
 * XXX You may also use input_item_New or input_item_NewExt as they need
 * less arguments.
 */
VLC_EXPORT( input_item_t *, input_item_NewWithType, ( vlc_object_t *, const char *psz_uri, const char *psz_name, int i_options, const char *const *ppsz_options, unsigned i_option_flags, mtime_t i_duration, int i_type ) );

/**
 * This function creates a new input_item_t with the provided informations.
 *
 * Provided for convenience.
 */
#define input_item_NewExt(a,b,c,d,e,f,g) __input_item_NewExt( VLC_OBJECT(a),b,c,d,e,f,g)
VLC_EXPORT( input_item_t *, __input_item_NewExt, (vlc_object_t *, const char *psz_uri, const char *psz_name, int i_options, const char *const *ppsz_options, unsigned i_option_flags, mtime_t i_duration ) );

/**
 * This function creates a new input_item_t with the provided informations.
 *
 * Provided for convenience.
 */
#define input_item_New( a,b,c ) input_item_NewExt( a, b, c, 0, NULL, 0, -1 )

/******************
 * Input stats
 ******************/
struct input_stats_t
{
    vlc_mutex_t         lock;

    /* Input */
    int i_read_packets;
    int i_read_bytes;
    float f_input_bitrate;
    float f_average_input_bitrate;

    /* Demux */
    int i_demux_read_packets;
    int i_demux_read_bytes;
    float f_demux_bitrate;
    float f_average_demux_bitrate;
    int i_demux_corrupted;
    int i_demux_discontinuity;

    /* Decoders */
    int i_decoded_audio;
    int i_decoded_video;

    /* Vout */
    int i_displayed_pictures;
    int i_lost_pictures;

    /* Sout */
    int i_sent_packets;
    int i_sent_bytes;
    float f_send_bitrate;

    /* Aout */
    int i_played_abuffers;
    int i_lost_abuffers;
};

#endif
