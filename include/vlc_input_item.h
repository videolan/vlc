/*****************************************************************************
 * vlc_input_item.h: Core input item
 *****************************************************************************
 * Copyright (C) 1999-2009 VLC authors and VideoLAN
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
#include <vlc_list.h>

#include <string.h>

typedef struct input_item_opaque input_item_opaque_t;
typedef struct input_item_slave input_item_slave_t;
typedef struct input_preparser_callbacks_t input_preparser_callbacks_t;

struct info_t
{
    char *psz_name;            /**< Name of this info */
    char *psz_value;           /**< Value of the info */
    struct vlc_list node;
};

#define info_foreach(info, cat) vlc_list_foreach(info, cat, node)

struct info_category_t
{
    char   *psz_name;      /**< Name of this category */
    struct vlc_list infos; /**< Infos in the category */
    struct vlc_list node;  /**< node, to put this category in a list */
};

/**
 * Returns true if the category is hidden
 *
 * Infos from hidden categories should not be displayed directly by UI modules.
 */
static inline bool info_category_IsHidden(info_category_t *cat)
{
    return cat->psz_name[0] == '.';
}

enum input_item_type_e
{
    ITEM_TYPE_UNKNOWN,
    ITEM_TYPE_FILE,
    ITEM_TYPE_DIRECTORY,
    ITEM_TYPE_DISC,
    ITEM_TYPE_CARD,
    ITEM_TYPE_STREAM,
    ITEM_TYPE_PLAYLIST,
    ITEM_TYPE_NODE,

    /* This one is not a real type but the number of input_item types. */
    ITEM_TYPE_NUMBER
};

/**
 * Describes an input and is used to spawn input_thread_t objects.
 */
struct input_item_t
{
    char       *psz_name;            /**< text describing this item */
    char       *psz_uri;             /**< mrl of this item */

    int        i_options;            /**< Number of input options */
    char       **ppsz_options;       /**< Array of input options */
    uint8_t    *optflagv;            /**< Some flags of input options */
    unsigned   optflagc;
    input_item_opaque_t *opaques;    /**< List of opaque pointer values */

    vlc_tick_t i_duration;           /**< Duration in vlc ticks */


    struct vlc_list categories;      /**< List of categories */

    int         i_es;                /**< Number of es format descriptions */
    es_format_t **es;                /**< Es formats */

    input_stats_t *p_stats;          /**< Statistics */

    vlc_meta_t *p_meta;

    int         i_epg;               /**< Number of EPG entries */
    vlc_epg_t   **pp_epg;            /**< EPG entries */
    int64_t     i_epg_time;          /** EPG timedate as epoch time */
    const vlc_epg_t *p_epg_table;    /** running/selected program cur/next EPG table */

    int         i_slaves;            /**< Number of slaves */
    input_item_slave_t **pp_slaves;  /**< Slave entries that will be loaded by
                                          the input_thread */

    vlc_event_manager_t event_manager;

    vlc_mutex_t lock;                 /**< Lock for the item */

    enum input_item_type_e i_type;   /**< Type (file, disc, ... see input_item_type_e) */
    bool        b_net;               /**< Net: always true for TYPE_STREAM, it
                                          depends for others types */
    bool        b_error_when_reading;/**< Error When Reading */

    int         i_preparse_depth;    /**< How many level of sub items can be preparsed:
                                          -1: recursive, 0: none, >0: n levels */

    bool        b_preparse_interact; /**< Force interaction with the user when
                                          preparsing.*/

    void        *libvlc_owner;       /**< LibVLC private data, can only be set
                                          before events are registered. */
};

#define INPUT_ITEM_URI_NOP "vlc://nop" /* dummy URI for node/directory items */

/* placeholder duration for items with no known duration at time of creation
 * it may remain the duration for items like a node/directory */
#define INPUT_DURATION_UNSET      VLC_TICK_INVALID
#define INPUT_DURATION_INDEFINITE (-1) /* item with a known indefinite duration (live/continuous source) */

enum input_item_net_type
{
    ITEM_NET_UNKNOWN,
    ITEM_NET,
    ITEM_LOCAL
};

enum slave_type
{
    SLAVE_TYPE_SPU,
    SLAVE_TYPE_GENERIC, /* audio, video or subtitle not matched in SLAVE_SPU_EXTENSIONS */
};

enum slave_priority
{
    SLAVE_PRIORITY_MATCH_NONE = 1,
    SLAVE_PRIORITY_MATCH_RIGHT,
    SLAVE_PRIORITY_MATCH_LEFT,
    SLAVE_PRIORITY_MATCH_ALL,
    SLAVE_PRIORITY_USER
};

/* Extensions must be in alphabetical order */
#define MASTER_EXTENSIONS \
    "asf", "avi", "divx", \
    "f4v", "flv", "m1v", \
    "m2v", "m4v", "mkv", \
    "mov", "mp2", "mp2v", \
    "mp4", "mp4v", "mpe", \
    "mpeg", "mpeg1", "mpeg2", \
    "mpeg4", "mpg", "mpv2", \
    "mxf", "ogv", "ogx", \
    "ps", "vro","webm", \
    "wmv", "wtv"

#define SLAVE_SPU_EXTENSIONS \
    "aqt", "ass",  "cdg", \
    "dks", "idx", "jss", \
    "mpl2", "mpsub", "pjs", \
    "psb", "rt", "sami", "sbv", \
    "scc", "smi", "srt", \
    "ssa",  "stl", "sub", \
    "tt", "ttml", "usf", \
    "vtt", "webvtt"

#define SLAVE_AUDIO_EXTENSIONS \
    "aac", "ac3", "dts", \
    "dtshd", "eac3", "flac", \
    "m4a", "mp3", "pcm" \

struct input_item_slave
{
    enum slave_type     i_type;     /**< Slave type (spu, audio) */
    enum slave_priority i_priority; /**< Slave priority */
    bool                b_forced;   /**< Slave should be selected */
    char                psz_uri[];  /**< Slave mrl */
};

struct input_item_node_t
{
    input_item_t *         p_item;
    int                    i_children;
    input_item_node_t      **pp_children;
};

VLC_API void input_item_CopyOptions( input_item_t *p_child, input_item_t *p_parent );
VLC_API void input_item_SetName( input_item_t *p_item, const char *psz_name );

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
 * Remove a node from its parent.
 */
VLC_API void input_item_node_RemoveNode( input_item_node_t *parent,
                                         input_item_node_t *child );

/**
 * Delete a node created with input_item_node_Create() and all its children.
 */
VLC_API void input_item_node_Delete( input_item_node_t *p_node );

/**
 * Option flags
 */
enum input_item_option_e
{
    /* Allow VLC to trust the given option.
     * By default options are untrusted */
    VLC_INPUT_OPTION_TRUSTED = 0x2,

    /* Add the option, unless the same option
     * is already present. */
    VLC_INPUT_OPTION_UNIQUE  = 0x100,
};

/**
 * This function allows to add an option to an existing input_item_t.
 */
VLC_API int input_item_AddOption(input_item_t *, const char *, unsigned i_flags );
/**
 * This function add several options to an existing input_item_t.
 */
VLC_API int input_item_AddOptions(input_item_t *, int i_options,
                                  const char *const *ppsz_options,
                                  unsigned i_flags );
VLC_API int input_item_AddOpaque(input_item_t *, const char *, void *);

void input_item_ApplyOptions(vlc_object_t *, input_item_t *);

VLC_API bool input_item_slave_GetType(const char *, enum slave_type *);

VLC_API input_item_slave_t *input_item_slave_New(const char *, enum slave_type,
                                               enum slave_priority);
#define input_item_slave_Delete(p_slave) free(p_slave)

/**
 * This function allows adding a slave to an existing input item.
 * The slave is owned by the input item after this call.
 */
VLC_API int input_item_AddSlave(input_item_t *, input_item_slave_t *);

/* */
VLC_API bool input_item_HasErrorWhenReading( input_item_t * );
VLC_API void input_item_SetMeta( input_item_t *, vlc_meta_type_t meta_type, const char *psz_val );
VLC_API bool input_item_MetaMatch( input_item_t *p_i, vlc_meta_type_t meta_type, const char *psz );
VLC_API char * input_item_GetMeta( input_item_t *p_i, vlc_meta_type_t meta_type ) VLC_USED;
VLC_API const char *input_item_GetMetaLocked(input_item_t *, vlc_meta_type_t meta_type);
VLC_API char * input_item_GetName( input_item_t * p_i ) VLC_USED;
VLC_API char * input_item_GetTitleFbName( input_item_t * p_i ) VLC_USED;
VLC_API char * input_item_GetURI( input_item_t * p_i ) VLC_USED;
VLC_API char * input_item_GetNowPlayingFb( input_item_t *p_item ) VLC_USED;
VLC_API void input_item_SetURI( input_item_t * p_i, const char *psz_uri );
VLC_API vlc_tick_t input_item_GetDuration( input_item_t * p_i );
VLC_API void input_item_SetDuration( input_item_t * p_i, vlc_tick_t i_duration );
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
INPUT_META(AlbumArtist)
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
INPUT_META(ESNowPlaying)
INPUT_META(Publisher)
INPUT_META(EncodedBy)
INPUT_META(ArtworkURL)
INPUT_META(TrackID)
INPUT_META(TrackTotal)
INPUT_META(Director)
INPUT_META(Season)
INPUT_META(Episode)
INPUT_META(ShowName)
INPUT_META(Actors)
INPUT_META(DiscNumber)
INPUT_META(DiscTotal)

#define input_item_SetTrackNum input_item_SetTrackNumber
#define input_item_GetTrackNum input_item_GetTrackNumber
#define input_item_SetArtURL   input_item_SetArtworkURL
#define input_item_GetArtURL   input_item_GetArtworkURL

VLC_API char * input_item_GetInfo( input_item_t *p_i, const char *psz_cat,const char *psz_name ) VLC_USED;
VLC_API char * input_item_GetInfoLocked( input_item_t *p_i, const char *psz_cat,const char *psz_name );
VLC_API int input_item_AddInfo( input_item_t *p_i, const char *psz_cat, const char *psz_name, const char *psz_format, ... ) VLC_FORMAT( 4, 5 );
VLC_API int input_item_DelInfo( input_item_t *p_i, const char *psz_cat, const char *psz_name );
VLC_API void input_item_ReplaceInfos( input_item_t *, info_category_t * );
VLC_API void input_item_MergeInfos( input_item_t *, info_category_t * );

#define input_item_AddStat(item, type, value) \
    input_item_AddInfo(item, ".stat", type, "%" PRIu64, (uint64_t) value)

/**
 * This function creates a new input_item_t with the provided information.
 *
 * XXX You may also use input_item_New, as they need less arguments.
 */
VLC_API input_item_t * input_item_NewExt( const char *psz_uri,
                                          const char *psz_name,
                                          vlc_tick_t i_duration, enum input_item_type_e i_type,
                                          enum input_item_net_type i_net ) VLC_USED;

#define input_item_New( psz_uri, psz_name ) \
    input_item_NewExt( psz_uri, psz_name, INPUT_DURATION_UNSET, ITEM_TYPE_UNKNOWN, ITEM_NET_UNKNOWN )

#define input_item_NewCard( psz_uri, psz_name ) \
    input_item_NewExt( psz_uri, psz_name, INPUT_DURATION_INDEFINITE, ITEM_TYPE_CARD, ITEM_LOCAL )

#define input_item_NewDisc( psz_uri, psz_name, i_duration ) \
    input_item_NewExt( psz_uri, psz_name, i_duration, ITEM_TYPE_DISC, ITEM_LOCAL )

#define input_item_NewStream( psz_uri, psz_name, i_duration ) \
    input_item_NewExt( psz_uri, psz_name, i_duration, ITEM_TYPE_STREAM, ITEM_NET )

#define input_item_NewDirectory( psz_uri, psz_name, i_net ) \
    input_item_NewExt( psz_uri, psz_name, INPUT_DURATION_UNSET, ITEM_TYPE_DIRECTORY, i_net )

#define input_item_NewFile( psz_uri, psz_name, i_duration, i_net ) \
    input_item_NewExt( psz_uri, psz_name, i_duration, ITEM_TYPE_FILE, i_net )

/**
 * This function creates a new input_item_t as a copy of another.
 */
VLC_API input_item_t * input_item_Copy(input_item_t * ) VLC_USED;

/** Holds an input item, i.e. creates a new reference. */
VLC_API input_item_t *input_item_Hold(input_item_t *);

/** Releases an input item, i.e. decrements its reference counter. */
VLC_API void input_item_Release(input_item_t *);

/**
 * Record prefix string.
 * TODO make it configurable.
 */
#define INPUT_RECORD_PREFIX "vlc-record-%Y-%m-%d-%Hh%Mm%Ss-$ N-$ p"

/**
 * This function creates a sane filename path.
 */
VLC_API char * input_item_CreateFilename( input_item_t *,
                                          const char *psz_path, const char *psz_prefix,
                                          const char *psz_extension ) VLC_USED;

/**
 * input item parser opaque structure
 */
typedef struct input_item_parser_id_t input_item_parser_id_t;

/**
 * input item parser callbacks
 */
typedef struct input_item_parser_cbs_t
{
    /**
     * Event received when the parser ends
     *
     * @note This callback is mandatory.
     *
     * @param item the parsed item
     * @param status VLC_SUCCESS in case of success, an error otherwise
     * @param userdata user data set by input_item_Parse()
     */
    void (*on_ended)(input_item_t *item, int status, void *userdata);

    /**
     * Event received when a new subtree is added
     *
     * @note This callback is optional.
     *
     * @param item the parsed item
     * @param subtree sub items of the current item
     * @param userdata user data set by input_item_Parse()
     */
    void (*on_subtree_added)(input_item_t *item, input_item_node_t *subtree, void *userdata);
} input_item_parser_cbs_t;

/**
 * Parse an item asynchronously
 *
 * @note The parsing is done asynchronously. The user can call
 * input_item_parser_id_Interrupt() before receiving the on_ended() event in
 * order to interrupt it.
 *
 * @param item the item to parse
 * @param parent the parent obj
 * @param cbs callbacks to be notified of the end of the parsing
 * @param userdata opaque data used by parser callbacks
 *
 * @return a parser instance or NULL in case of error, the parser needs to be
 * released with input_item_parser_id_Release()
 */
VLC_API input_item_parser_id_t *
input_item_Parse(input_item_t *item, vlc_object_t *parent,
                 const input_item_parser_cbs_t *cbs, void *userdata) VLC_USED;

/**
 * Interrupts & cancels the parsing
 *
 * @note The parser still needs to be released with input_item_parser_id_Release
 * afterward.
 * @note Calling this function will cause the on_ended callback to be invoked.
 *
 * @param parser the parser to interrupt
 */
VLC_API void
input_item_parser_id_Interrupt(input_item_parser_id_t *parser);

/**
 * Release (and interrupt if needed) a parser
 *
 * @param parser the parser returned by input_item_Parse
 */
VLC_API void
input_item_parser_id_Release(input_item_parser_id_t *parser);

typedef enum input_item_meta_request_option_t
{
    META_REQUEST_OPTION_NONE          = 0x00,
    META_REQUEST_OPTION_SCOPE_LOCAL   = 0x01,
    META_REQUEST_OPTION_SCOPE_NETWORK = 0x02,
    META_REQUEST_OPTION_SCOPE_ANY     = 0x03,
    META_REQUEST_OPTION_FETCH_LOCAL   = 0x04,
    META_REQUEST_OPTION_FETCH_NETWORK = 0x08,
    META_REQUEST_OPTION_FETCH_ANY     = 0x0C,
    META_REQUEST_OPTION_DO_INTERACT   = 0x10,
    META_REQUEST_OPTION_NO_SKIP       = 0x20,
} input_item_meta_request_option_t;

/* status of the on_preparse_ended() callback */
enum input_item_preparse_status
{
    ITEM_PREPARSE_SKIPPED,
    ITEM_PREPARSE_FAILED,
    ITEM_PREPARSE_TIMEOUT,
    ITEM_PREPARSE_DONE
};

typedef struct input_preparser_callbacks_t {
    void (*on_preparse_ended)(input_item_t *, enum input_item_preparse_status status, void *userdata);
    void (*on_subtree_added)(input_item_t *, input_item_node_t *subtree, void *userdata);
} input_preparser_callbacks_t;

typedef struct input_fetcher_callbacks_t {
    void (*on_art_fetch_ended)(input_item_t *, bool fetched, void *userdata);
} input_fetcher_callbacks_t;

VLC_API int libvlc_MetadataRequest( libvlc_int_t *, input_item_t *,
                                    input_item_meta_request_option_t,
                                    const input_preparser_callbacks_t *cbs,
                                    void *cbs_userdata,
                                    int, void * );
VLC_API int libvlc_ArtRequest(libvlc_int_t *, input_item_t *,
                              input_item_meta_request_option_t,
                              const input_fetcher_callbacks_t *cbs,
                              void *cbs_userdata );
VLC_API void libvlc_MetadataCancel( libvlc_int_t *, void * );

/******************
 * Input stats
 ******************/
struct input_stats_t
{
    /* Input */
    int64_t i_read_packets;
    int64_t i_read_bytes;
    float f_input_bitrate;

    /* Demux */
    int64_t i_demux_read_packets;
    int64_t i_demux_read_bytes;
    float f_demux_bitrate;
    int64_t i_demux_corrupted;
    int64_t i_demux_discontinuity;

    /* Decoders */
    int64_t i_decoded_audio;
    int64_t i_decoded_video;

    /* Vout */
    int64_t i_displayed_pictures;
    int64_t i_late_pictures;
    int64_t i_lost_pictures;

    /* Aout */
    int64_t i_played_abuffers;
    int64_t i_lost_abuffers;
};

/**
 * Access pf_readdir helper struct
 * \see vlc_readdir_helper_init()
 * \see vlc_readdir_helper_additem()
 * \see vlc_readdir_helper_finish()
 */
struct vlc_readdir_helper
{
    input_item_node_t *p_node;
    void **pp_slaves;
    size_t i_slaves;
    void **pp_dirs;
    size_t i_dirs;
    int i_sub_autodetect_fuzzy;
    bool b_show_hiddenfiles;
    bool b_flatten;
    char *psz_ignored_exts;
};

/**
 * Init a vlc_readdir_helper struct
 *
 * \param p_rdh need to be cleaned with vlc_readdir_helper_finish()
 * \param p_node node that will be used to add items
 */
VLC_API void vlc_readdir_helper_init(struct vlc_readdir_helper *p_rdh,
                                     vlc_object_t *p_obj, input_item_node_t *p_node);
#define vlc_readdir_helper_init(p_rdh, p_obj, p_node) \
    vlc_readdir_helper_init(p_rdh, VLC_OBJECT(p_obj), p_node)

/**
 * Finish adding items to the node
 *
 * \param b_success if true, items of the node will be sorted.
 */
VLC_API void vlc_readdir_helper_finish(struct vlc_readdir_helper *p_rdh, bool b_success);

/**
 * Add a new input_item_t entry to the node of the vlc_readdir_helper struct.
 *
 * \param p_rdh previously inited vlc_readdir_helper struct
 * \param psz_uri uri of the new item
 * \param psz_flatpath flattened path of the new item. If not NULL, this
 *        function will create an input item for each sub folders (separated
 *        by '/') of psz_flatpath (so, this will un-flatten the folder
 *        hierarchy). Either psz_flatpath or psz_filename must be valid.
 * \param psz_filename file name of the new item. If NULL, the file part of path
 *        will be used as a filename. Either psz_flatpath or psz_filename must
 *        be valid.
 * \param i_type see \ref input_item_type_e
 * \param i_net see \ref input_item_net_type
 * \param[out] created_item if an input item is created. The item should not be
 * released and is valid until vlc_readdir_helper_finish() is called.
 * \param status VLC_SUCCESS in case of success, an error otherwise. Parsing
 * should be aborted in case of error.
 */
VLC_API int vlc_readdir_helper_additem(struct vlc_readdir_helper *p_rdh,
                                       const char *psz_uri, const char *psz_flatpath,
                                       const char *psz_filename,
                                       int i_type, int i_net, input_item_t **created_item);

#endif
