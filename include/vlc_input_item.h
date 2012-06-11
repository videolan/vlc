/*****************************************************************************
 * vlc_input_item.h: Core input item
 *****************************************************************************
 * Copyright (C) 1999-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_INPUT_ITEM_H
#define VLC_INPUT_ITEM_H 1

/**
 * \file
 * This file defines functions, structures and enums for input items in vlc
 */

#include <vlc_meta.h>
#include <vlc_epg.h>
#include <vlc_events.h>

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
    int        i_id;                 /**< Identifier of the item */

    char       *psz_name;            /**< text describing this item */
    char       *psz_uri;             /**< mrl of this item */

    int        i_options;            /**< Number of input options */
    char       **ppsz_options;       /**< Array of input options */
    uint8_t    *optflagv;            /**< Some flags of input options */
    unsigned   optflagc;

    mtime_t    i_duration;           /**< Duration in microseconds */


    int        i_categories;         /**< Number of info categories */
    info_category_t **pp_categories; /**< Pointer to the first info category */

    int         i_es;                /**< Number of es format descriptions */
    es_format_t **es;                /**< Es formats */

    input_stats_t *p_stats;          /**< Statistics */
    int           i_nb_played;       /**< Number of times played */

    vlc_meta_t *p_meta;

    int         i_epg;               /**< Number of EPG entries */
    vlc_epg_t   **pp_epg;            /**< EPG entries */

    vlc_event_manager_t event_manager;

    vlc_mutex_t lock;                 /**< Lock for the item */

    uint8_t     i_type;              /**< Type (file, disc, ... see input_item_type_e) */
    bool        b_fixed_name;        /**< Can the interface change the name ?*/
    bool        b_error_when_reading;/**< Error When Reading */
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

struct input_item_node_t
{
    input_item_t *         p_item;
    int                    i_children;
    input_item_node_t      **pp_children;
    input_item_node_t      *p_parent;
};

VLC_API void input_item_CopyOptions( input_item_t *p_parent, input_item_t *p_child );
VLC_API void input_item_SetName( input_item_t *p_item, const char *psz_name );

/**
 * Add one subitem to this item
 *
 * This won't hold the item, but can tell to interested third parties
 * Like the playlist, that there is a new sub item. With this design
 * It is not the input item's responsability to keep all the ref of
 * the input item children.
 *
 * Sends a vlc_InputItemSubItemTreeAdded and a vlc_InputItemSubItemAdded event
 */
VLC_API void input_item_PostSubItem( input_item_t *p_parent, input_item_t *p_child );

/**
 * Start adding multiple subitems.
 *
 * Create a root node to hold a tree of subitems for given item
 */
VLC_API input_item_node_t * input_item_node_Create( input_item_t *p_input ) VLC_USED;

/**
 * Add a new child node to this parent node that will point to this subitem.
 */
VLC_API input_item_node_t * input_item_node_AppendItem( input_item_node_t *p_node, input_item_t *p_item );

/**
 * Add an already created node to children of this parent node.
 */
VLC_API void input_item_node_AppendNode( input_item_node_t *p_parent, input_item_node_t *p_child );

/**
 * Delete a node created with input_item_node_Create() and all its children.
 */
VLC_API void input_item_node_Delete( input_item_node_t *p_node );

/**
 * End adding multiple subitems.
 *
 * Sends a vlc_InputItemSubItemTreeAdded event to notify that the item pointed to
 * by the given root node has created new subitems that are pointed to by all the
 * children of the node.
 *
 * Also sends vlc_InputItemSubItemAdded event for every child under the given root node;
 *
 * In the end deletes the node and all its children nodes.
 */
VLC_API void input_item_node_PostAndDelete( input_item_node_t *p_node );


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
VLC_API int input_item_AddOption(input_item_t *, const char *, unsigned i_flags );

/* */
VLC_API bool input_item_HasErrorWhenReading( input_item_t * );
VLC_API void input_item_SetMeta( input_item_t *, vlc_meta_type_t meta_type, const char *psz_val );
VLC_API bool input_item_MetaMatch( input_item_t *p_i, vlc_meta_type_t meta_type, const char *psz );
VLC_API char * input_item_GetMeta( input_item_t *p_i, vlc_meta_type_t meta_type ) VLC_USED;
VLC_API char * input_item_GetName( input_item_t * p_i ) VLC_USED;
VLC_API char * input_item_GetTitleFbName( input_item_t * p_i ) VLC_USED;
VLC_API char * input_item_GetURI( input_item_t * p_i ) VLC_USED;
VLC_API void input_item_SetURI( input_item_t * p_i, const char *psz_uri );
VLC_API mtime_t input_item_GetDuration( input_item_t * p_i );
VLC_API void input_item_SetDuration( input_item_t * p_i, mtime_t i_duration );
VLC_API bool input_item_IsPreparsed( input_item_t *p_i );
VLC_API bool input_item_IsArtFetched( input_item_t *p_i );

#define INPUT_META( name ) \
static inline \
void input_item_Set ## name (input_item_t *p_input, const char *val) \
{ \
    input_item_SetMeta (p_input, vlc_meta_ ## name, val); \
} \
static inline \
char *input_item_Get ## name (input_item_t *p_input) \
{ \
    return input_item_GetMeta (p_input, vlc_meta_ ## name); \
}

INPUT_META(Title)
INPUT_META(Artist)
INPUT_META(Genre)
INPUT_META(Copyright)
INPUT_META(Album)
INPUT_META(TrackNumber)
INPUT_META(Description)
INPUT_META(Rating)
INPUT_META(Date)
INPUT_META(Setting)
INPUT_META(URL)
INPUT_META(Language)
INPUT_META(NowPlaying)
INPUT_META(Publisher)
INPUT_META(EncodedBy)
INPUT_META(ArtworkURL)
INPUT_META(TrackID)
INPUT_META(TrackTotal)

#define input_item_SetTrackNum input_item_SetTrackNumber
#define input_item_GetTrackNum input_item_GetTrackNumber
#define input_item_SetArtURL   input_item_SetArtworkURL
#define input_item_GetArtURL   input_item_GetArtworkURL

VLC_API char * input_item_GetInfo( input_item_t *p_i, const char *psz_cat,const char *psz_name ) VLC_USED;
VLC_API int input_item_AddInfo( input_item_t *p_i, const char *psz_cat, const char *psz_name, const char *psz_format, ... ) VLC_FORMAT( 4, 5 );
VLC_API int input_item_DelInfo( input_item_t *p_i, const char *psz_cat, const char *psz_name );
VLC_API void input_item_ReplaceInfos( input_item_t *, info_category_t * );
VLC_API void input_item_MergeInfos( input_item_t *, info_category_t * );

/**
 * This function creates a new input_item_t with the provided information.
 *
 * XXX You may also use input_item_New or input_item_NewExt as they need
 * less arguments.
 */
VLC_API input_item_t * input_item_NewWithType( const char *psz_uri, const char *psz_name, int i_options, const char *const *ppsz_options, unsigned i_option_flags, mtime_t i_duration, int i_type ) VLC_USED;

/**
 * This function creates a new input_item_t with the provided information.
 *
 * Provided for convenience.
 */
VLC_API input_item_t * input_item_NewExt( const char *psz_uri, const char *psz_name, int i_options, const char *const *ppsz_options, unsigned i_option_flags, mtime_t i_duration ) VLC_USED;

/**
 * This function creates a new input_item_t with the provided information.
 *
 * Provided for convenience.
 */
#define input_item_New( a,b ) input_item_NewExt( a, b, 0, NULL, 0, -1 )

/**
 * This function creates a new input_item_t as a copy of another.
 */
VLC_API input_item_t * input_item_Copy(input_item_t * ) VLC_USED;

/** Holds an input item, i.e. creates a new reference. */
VLC_API input_item_t *input_item_Hold(input_item_t *);

/** Releases an input item, i.e. decrements its reference counter. */
VLC_API void input_item_Release(input_item_t *);

/* Historical hack... */
#define vlc_gc_incref(i) input_item_Hold(i)
#define vlc_gc_decref(i) input_item_Release(i)

/******************
 * Input stats
 ******************/
struct input_stats_t
{
    vlc_mutex_t         lock;

    /* Input */
    int64_t i_read_packets;
    int64_t i_read_bytes;
    float f_input_bitrate;
    float f_average_input_bitrate;

    /* Demux */
    int64_t i_demux_read_packets;
    int64_t i_demux_read_bytes;
    float f_demux_bitrate;
    float f_average_demux_bitrate;
    int64_t i_demux_corrupted;
    int64_t i_demux_discontinuity;

    /* Decoders */
    int64_t i_decoded_audio;
    int64_t i_decoded_video;

    /* Vout */
    int64_t i_displayed_pictures;
    int64_t i_lost_pictures;

    /* Sout */
    int64_t i_sent_packets;
    int64_t i_sent_bytes;
    float f_send_bitrate;

    /* Aout */
    int64_t i_played_abuffers;
    int64_t i_lost_abuffers;
};

#endif
