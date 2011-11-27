/*****************************************************************************
 * vlc_media_library.h: SQL-based media library
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju at gmail dot com>
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

#ifndef VLC_MEDIA_LIBRARY_H
# define VLC_MEDIA_LIBRARY_H

# ifdef __cplusplus
extern "C" {
# endif

#include <vlc_common.h>
#include <vlc_playlist.h>

/*****************************************************************************
 * ML Enums
 *****************************************************************************/

#define ML_PERSON_ARTIST        "Artist"
#define ML_PERSON_ALBUM_ARTIST  "Album Artist"
#define ML_PERSON_ENCODER       "Encoder"
#define ML_PERSON_PUBLISHER     "Publisher"


#define ml_priv( gc, t ) ((t *)(((char *)(gc)) - offsetof(t, ml_gc_data)))

/** List of Query select types.
 * In a query array or variable argument list, each select type is followed
 * by an argument (X) of variable type (char* or int, @see ml_element_t).
 * These types can be used either in the query list or in the result array.
 * Some types are reserved for the result array:
 */
typedef enum
{
    ML_ALBUM = 1,              /**< Album Title */
    ML_ALBUM_ID,               /**< Album ID */
    ML_ALBUM_COVER,            /**< Album Cover art url */
    /* FIXME: Remove ML_ARTIST */
    ML_ARTIST,                 /**< Artist, interpreted as ML_PEOPLE
                                    && ML_PEOPLE_ROLE = ML_PERSON_ARTIST */
    ML_ARTIST_ID,              /**< Artist ID, interpreted as ML_PEOPLE_ID
                                    && ML_PEOPLE_ROLE = ML_PERSON_ARTIST */
    ML_COMMENT,                /**< Comment about media */
    ML_COUNT_MEDIA,            /**< Number of medias */
    ML_COUNT_ALBUM,            /**< Number of albums */
    ML_COUNT_PEOPLE,           /**< Number of people */
    ML_COVER,                  /**< Cover art url */
    ML_DURATION,               /**< Duration in ms */
    ML_DISC_NUMBER,            /**< Disc number of the track */
    ML_EXTRA,                  /**< Extra/comment (string) on the media */
    ML_FIRST_PLAYED,           /**< First time media was played */
    ML_FILESIZE,               /**< Size of the media file */
    ML_GENRE,                  /**< Genre of the media (if any) */
    ML_ID,                     /**< Media ID */
    ML_IMPORT_TIME,            /**< Date when media was imported */
    ML_LANGUAGE,               /**< Language */
    ML_LAST_PLAYED,            /**< Last play UNIX timestamp */
    ML_LAST_SKIPPED,           /**< Time when media was last skipped */
    ML_ORIGINAL_TITLE,         /**< Media original title (if any) */
    ML_PEOPLE,                 /**< Any People associated with this media */
    ML_PEOPLE_ID,              /**< Id of a person */
    ML_PEOPLE_ROLE,            /**< Person role */
    ML_PLAYED_COUNT,           /**< Media play count */
    ML_PREVIEW,                /**< Url of the video preview */
    ML_SKIPPED_COUNT,          /**< Number of times skipped */
    ML_SCORE,                  /**< Computed media score */
    ML_TITLE,                  /**< Media title */
    ML_TRACK_NUMBER,           /**< Media track number (if any) */
    ML_TYPE,                   /**< Media type. @see ml_type_e */
    ML_URI,                    /**< Media full URI. */
    ML_VOTE,                   /**< Media user vote value */
    ML_YEAR,                   /**< Media publishing year */
    ML_DIRECTORY,              /**< Monitored directory */
    ML_MEDIA,                  /**< Full media descriptor. @see ml_media_t */
    ML_MEDIA_SPARSE,           /**< Sparse media. @see ml_media_t */
    ML_MEDIA_EXTRA,            /**< Sparse + Extra = Full media */

    /* Some special elements */
    ML_LIMIT     = -1,         /**< Limit a query to X results */
    ML_SORT_DESC = -2,         /**< Sort a query descending on argument X */
    ML_SORT_ASC  = -3,         /**< Sort a query ascending on argument X */
    ML_DISTINCT  = -4,         /**< Add DISTINCT to SELECT statements. */
    ML_END       = -42         /**< End of argument list */
} ml_select_e;

/** Media types (audio, video, etc...) */
typedef enum
{
    ML_UNKNOWN   = 0,       /**< Unknown media type */
    ML_AUDIO     = 1 << 0,  /**< Audio only media */
    ML_VIDEO     = 1 << 1,  /**< Video media. May contain audio channels */
    ML_STREAM    = 1 << 2,  /**< Streamed media = not a local file */
    ML_NODE      = 1 << 3,  /**< Nodes like simple nodes, directories, playlists, etc */
    ML_REMOVABLE = 1 << 4,  /**< Removable media: CD/DVD/Card/... */
} ml_type_e;

/** Query result item/list type: integers, strings, medias, timestamps */
typedef enum {
    ML_TYPE_INT,        /**< Object is an int */
    ML_TYPE_PSZ,        /**< A string char* */
    ML_TYPE_TIME,       /**< A timestamp mtime_t */
    ML_TYPE_MEDIA,      /**< A pointer to a media ml_media_t* */
} ml_result_type_e;

/** Arguments for VLC Control for the media library */
typedef enum
{
    ML_SET_DATABASE,      /**< arg1 = char *psz_host
                               arg2 = int i_port
                               arg3 = char *psz_user
                               arg4 = char *psz_pass */
    ML_INIT_DATABASE,     /**< No arg */
    ML_ADD_INPUT_ITEM,    /**< arg1 = input_item_t* */
    ML_ADD_PLAYLIST_ITEM, /**< arg1 = playlist_item_t * */
    ML_ADD_MONITORED,     /**< arg1 = char* */
    ML_DEL_MONITORED,     /**< arg1 = char* */
    ML_GET_MONITORED,     /**< arg1 = vlc_array_t* */
} ml_control_e;

/* Operations that can be specified between find conditions */
typedef enum
{
    ML_OP_NONE = 0,       /**< This is to specify an actual condition */
    ML_OP_AND,            /**< AND condition */
    ML_OP_OR,             /**< OR condition */
    ML_OP_NOT,            /**< NOT condition */
    ML_OP_SPECIAL         /**< This is for inclusion of
                            *  special stuffs like LIMIT */
} ml_op_e;

/* Comparison operators used in a single find condition */
typedef enum
{
    ML_COMP_NONE = 0,
    ML_COMP_LESSER,              ///< <
    ML_COMP_LESSER_OR_EQUAL,     ///< <=
    ML_COMP_EQUAL,               ///< ==
    ML_COMP_GREATER_OR_EQUAL,    ///< >=
    ML_COMP_GREATER,             ///< >
    ML_COMP_HAS,                 ///< "Contains", equivalent to SQL "LIKE %x%"
    ML_COMP_STARTS_WITH,         ///< Equivalent to SQL "LIKE %x"
    ML_COMP_ENDS_WITH,           ///< Equivalent to SQL "LIKE x%"
} ml_comp_e;

/*****************************************************************************
 * ML Structures and types
 *****************************************************************************/

typedef struct media_library_t media_library_t;
typedef struct media_library_sys_t media_library_sys_t;

typedef struct ml_media_t      ml_media_t;
typedef struct ml_result_t     ml_result_t;
typedef struct ml_element_t    ml_element_t;
typedef struct ml_person_t     ml_person_t;
typedef struct ml_ftree_t      ml_ftree_t;


typedef struct ml_gc_object_t
{
    vlc_spinlock_t spin;
    bool           pool;
    uintptr_t      refs;
    void          (*pf_destructor) (struct ml_gc_object_t *);
} ml_gc_object_t;

#define ML_GC_MEMBERS ml_gc_object_t ml_gc_data;

/** Main structure of the media library. VLC object. */
struct media_library_t
{
    VLC_COMMON_MEMBERS

    module_t             *p_module;  /**< the media library module */
    media_library_sys_t  *p_sys;     /**< internal struture */

    /** Member functions */
    struct
    {
        /**< Search in the database */
        int ( * pf_Find )            ( media_library_t *p_media_library,
                                       vlc_array_t *p_result_array,
                                       va_list args );

        /**< Search in the database using an array of arguments */
        int ( * pf_FindAdv )         ( media_library_t *p_media_library,
                                       vlc_array_t *p_result_array,
                                       ml_select_e selected_type,
                                       const char *psz_lvalue,
                                       ml_ftree_t *tree );

        /**< Update the database using an array of arguments */
        int ( * pf_Update )          ( media_library_t *p_media_library,
                                       ml_select_e selected_type,
                                       const char *psz_lvalue,
                                       ml_ftree_t *where,
                                       vlc_array_t *changes );

        /**< Delete many medias in the database */
        int ( * pf_Delete )    ( media_library_t *p_media_library,
                                       vlc_array_t *p_array );

        /**< Control the media library */
        int ( * pf_Control ) ( media_library_t *p_media_library,
                               int i_query, va_list args );

        /**< Create associated input item */
        input_item_t* ( * pf_InputItemFromMedia ) (
                    media_library_t *p_media_library, int i_media );

        /**< Get a media */
        ml_media_t* ( * pf_GetMedia ) (
                    media_library_t *p_media_library, int i_media,
                    ml_select_e select, bool reload );
    } functions;
};


/**
 * @brief Structure to describe a media
 *
 * This is the main structure holding the meta data in ML.
 * @see b_sparse indicates whether the media struct has valid values
 * in its Extra fields. Otherwise, it must be loaded with the API
 * function.
 * @see i_id indicates whether this struct is saved in the ML if i_id > 0
 * Otherwise, it can be added to the database
 */
struct ml_media_t
{
    ML_GC_MEMBERS
    vlc_mutex_t     lock;               /**< Mutex for multithreaded access */
    bool            b_sparse;           /**< Specifies if media is loaded fully */
    ml_type_e       i_type;             /**< Type of the media (ml_type_e) */
    int8_t          i_vote;             /**< User vote */
    int16_t         i_disc_number;      /**< Disc number of media */
    int16_t         i_track_number;     /**< Track number */
    int16_t         i_year;             /**< Year of release */
    int32_t         i_id;               /**< Media ID in the database */
    int32_t         i_score;            /**< Score computed about the media */
    int32_t         i_album_id;         /**< Album id */
    int32_t         i_played_count;     /**< How many time the media was played */
    int32_t         i_skipped_count;    /**< No. of times file was skipped */
    int32_t         i_bitrate;          /**< Extra: Bitrate of the media */
    int32_t         i_samplerate;       /**< Extra: Samplerate of the media */
    int32_t         i_bpm;              /**< Extra: Beats per minute */
    char            *psz_uri;           /**< URI to find the media */
    char            *psz_title;         /**< Title of the media */
    char            *psz_orig_title;    /**< Original title (mainly for movies) */
    char            *psz_album;         /**< Name of the album */
    char            *psz_cover;         /**< URI of the cover */
    char            *psz_genre;         /**< Genre of the media */
    char            *psz_preview;       /**< Preview thumbnail for video, if any */
    char            *psz_comment;       /**< Comment or description about media */
    char            *psz_language;      /**< Extra: Language */
    char            *psz_extra;         /**< Extra: Some extra datas like lyrics */
    ml_person_t     *p_people;          /**< Extra: People associated with this
                                             media This meta holds only one
                                             artist if b_sparse = true */
    int64_t         i_filesize;         /**< Size of the file */
    mtime_t         i_duration;         /**< Duration in microseconds */
    mtime_t         i_last_played;      /**< Time when the media was last played */
    mtime_t         i_last_skipped;     /**< Time when the media was last skipped */
    mtime_t         i_first_played;     /**< First played */
    mtime_t         i_import_time;      /**< Time when media was added */

};


/**
 * @brief Main communication struct between GUI and sql_media_library.
 * Generic representation of an ML/SQL query result.
 */
struct ml_result_t
{
    int32_t          id;        /**< Media/Album/Artist... ID (if any) */
    ml_result_type_e type;      /**< Type of value */
    union
    {
        /* Classical results */
        int             i;
        char           *psz;
        mtime_t         time;

        /* Complex result: media descriptor */
        ml_media_t     *p_media;
    } value;                    /**< Value of the result obtained */
};


/**
 * @brief Element of a query: criteria type/value pair
 * Used for update and delete queries
 */
struct ml_element_t
{
    ml_select_e    criteria;    /**< SELECT criteria type. @see ml_select_e */
    union
    {
        int     i;
        char*   str;
    } value;                    /**< SELECT criteria value (string or int) */
    union
    {
        int     i;
        char*   str;
    } lvalue;                   /**< Refer to @see ml_ftree_t lvalue docs */
};

/**
 * Binary tree used to parse the WHERE condition for a search
 *
 * Let [expr] indicate a valid expression
 * [expr] = [expr] AND [expr], where the left and right are respective
 * [expr] = [expr] OR [expr]
 * [expr] = [expr] NOT [NULL]
 * [expr] = [expr] SPEC [spec_expr]
 * [expr] = [criteria=val]
 * [spec_expr] = [DISTINCT/LIMIT/ASC/DESC = val ]
 */
struct ml_ftree_t
{
    ml_op_e         op;         /**< Operator. ML_OP_NONE means this is a leaf
                                  *  node. Criteria and value gives its data.
                                  *  ML_OP_SPECIAL specifies a special node
                                  *  that does not form a part of the WHERE.
                                  *  The right node consists of the data
                                  *  with its criteria set to the special val
                                  *  and the left node is the corresponding
                                  *  subtree of the parent node.
                                  *  ML_OP_NOT only left sub tree is considered
                                  *  ML_OP_AND and ML_OP_OR consider both
                                  *  left and right subtrees */
    ml_ftree_t      *left;      /**< Left child of Bin tree */
    ml_ftree_t      *right;     /**< Right child of Bin tree */
    ml_select_e     criteria;   /**< SELECT criteria type @see ml_select_e
                                  *  The criteria value is considered only when
                                  *  op = ML_OP_NONE i.e. in leaf nodes */
    ml_comp_e       comp;       /**< Condition between type and value */
    union
    {
        int     i;
        char    *str;
    } value;                    /**< SELECT criteria value ( string or int ) */
    union
    {
        int     i;
        char    *str;
    } lvalue;                   /**< Used as key value for people types/roles.
                                     An empty string "" denotes ANY person role.
                                     NULL is used for all other criterias */
};


/**
 * Person class. Implemented as a linked list
 */
struct ml_person_t
{
    char               *psz_role;   /**< Type of person */
    char               *psz_name;   /**< Name of the person */
    int                 i_id;       /**< ID in the database */
    ml_person_t        *p_next;     /**< Next person in list */
};


/*****************************************************************************
 * ML Function headers
 *****************************************************************************/

/**
 * @brief Acquire a reference to the media library singleton
 * @param p_this The object holding the media library
 * @return The media library object. NULL if the media library
 * object could not be loaded
 */
VLC_API media_library_t* ml_Get( vlc_object_t* p_this );
#define ml_Get( a ) ml_Get( VLC_OBJECT(a) )

/**
 * @brief Create a Media Library VLC object.
 * @param p_this Parent to attach the ML object to.
 * @param psz_name Name for the module
 * @return The ML object.
 */
VLC_API media_library_t* ml_Create( vlc_object_t *p_this, char* psz_name );

/**
 * @brief Destructor for the Media library singleton
 * @param p_this Parent the ML object is attached to
 */
VLC_API void ml_Destroy( vlc_object_t* p_this );

/**
 * @brief Control the Media Library
 * @param p_media_library the media library object
 * @param i_type one of ml_control_e values @see ml_control_e.
 * @param ... optional arguments.
 * @return VLC_SUCCESS or an error
 */
static inline int ml_ControlVa( media_library_t *p_media_library,
                                ml_control_e i_type, va_list args )
{
    return p_media_library->functions.pf_Control( p_media_library,
                                                  i_type,
                                                  args );
}

/**
 * @brief Control the Media Library
 * @param i_type one of ml_control_e values @see ml_control_e.
 * Variable arguments list equivalent
 */
#define ml_Control( a, b, args... )     __ml_Control( a, b, ## args )
static inline int __ml_Control( media_library_t *p_media_library,
                                ml_control_e i_type, ... )
{
    va_list args;
    int returned;

    va_start( args, i_type );
    returned = ml_ControlVa( p_media_library, i_type, args );
    va_end( args );

    return returned;
}

/**
 * @brief Determine an attribute's type (int or string)
 * @param meta Attribute to test @see ml_select_e
 * @return -1 if invalid, 0 if this is an integer, 1 if this is a string
 */
static inline int ml_AttributeIsString( ml_select_e meta )
{
    switch( meta )
    {
    /* Strings */
    case ML_ALBUM:
    case ML_ARTIST:
    case ML_COMMENT:
    case ML_COVER:
    case ML_EXTRA:
    case ML_GENRE:
    case ML_LANGUAGE:
    case ML_PREVIEW:
    case ML_PEOPLE:
    case ML_PEOPLE_ROLE:
    case ML_ORIGINAL_TITLE:
    case ML_TITLE:
    case ML_URI:
        return 1;

    /* Integers */
    case ML_ALBUM_ID:
    case ML_ARTIST_ID:
    case ML_DURATION:
    case ML_DISC_NUMBER:
    case ML_COUNT_MEDIA:
    case ML_COUNT_ALBUM:
    case ML_COUNT_PEOPLE:
    case ML_FILESIZE:
    case ML_FIRST_PLAYED:
    case ML_ID:
    case ML_IMPORT_TIME:
    case ML_LAST_PLAYED:
    case ML_LIMIT:
    case ML_PLAYED_COUNT:
    case ML_PEOPLE_ID:
    case ML_SCORE:
    case ML_SKIPPED_COUNT:
    case ML_TRACK_NUMBER:
    case ML_TYPE:
    case ML_VOTE:
    case ML_YEAR:
        return 0;

    /* Invalid or no following value (in a SELECT statement) */
    default:
        return -1;
    }
}

/* Reference Counting Functions */
/**
 * @brief Increment reference count of media
 * @param p_media The media object
 */
static inline void ml_gc_incref( ml_media_t* p_media )
{
    ml_gc_object_t* p_gc = &p_media->ml_gc_data;
    if( p_gc == NULL )
        return;

    vlc_spin_lock (&p_gc->spin);
    ++p_gc->refs;
    vlc_spin_unlock (&p_gc->spin);
}

/**
 * @brief Decrease reference count of media
 * @param p_media The media object
 */
static inline void ml_gc_decref( ml_media_t* p_media )
{
    /* The below code is from vlc_release(). */
    unsigned refs;
    bool pool;
    ml_gc_object_t* p_gc = &p_media->ml_gc_data;
    if( p_gc == NULL )
        return;

    vlc_spin_lock (&p_gc->spin);
    refs = --p_gc->refs;
    pool = p_gc->pool;
    vlc_spin_unlock (&p_gc->spin);

    if( refs == 0 && !pool )
    {
        vlc_spin_destroy (&p_gc->spin);
        p_gc->pf_destructor (p_gc);
    }
}

/*****************************************************************************
 * ML Free Functions
 *****************************************************************************/

/**
 * @brief Free a person object
 * @param p_media Person object to free
 * @note This function is NOT threadsafe
 */
static inline void ml_FreePeople( ml_person_t *p_person )
{
    if( p_person == NULL )
        return;
    ml_FreePeople( p_person->p_next );
    free( p_person->psz_name );
    free( p_person->psz_role );
    free( p_person );
}

/**
 * @brief Free only the content of a media. @see ml_media_t
 * @param p_media Media object
 * @note This function is NOT threadsafe.
 */
static inline void ml_FreeMediaContent( ml_media_t *p_media )
{
    free( p_media->psz_uri );
    free( p_media->psz_title );
    free( p_media->psz_orig_title );
    free( p_media->psz_cover );
    free( p_media->psz_comment );
    free( p_media->psz_extra );
    free( p_media->psz_genre );
    free( p_media->psz_album );
    free( p_media->psz_preview );
    free( p_media->psz_language );
    ml_FreePeople( p_media->p_people );
    p_media->b_sparse = true;
    p_media->i_id = 0;
    p_media->i_type = ML_UNKNOWN;
    p_media->i_album_id = 0;
    p_media->i_disc_number = 0;
    p_media->i_track_number = 0;
    p_media->i_year = 0;
    p_media->i_vote = 0;
    p_media->i_score = 0;
    p_media->i_filesize = 0;
    p_media->i_duration = 0;
    p_media->i_played_count = 0;
    p_media->i_last_played = 0;
    p_media->i_skipped_count = 0;
    p_media->i_last_skipped = 0;
    p_media->i_first_played = 0;
    p_media->i_import_time = 0;
    p_media->i_bitrate = 0;
    p_media->i_samplerate = 0;
    p_media->i_bpm = 0;
}

/**
 * @brief Free a result item. @see ml_result_t
 * @param p_result Result item to free
 * @note This will free any strings and decref medias.
 */
static inline void ml_FreeResult( ml_result_t *p_result )
{
    if( p_result )
    {
        switch( p_result->type )
        {
            case ML_TYPE_PSZ:
                free( p_result->value.psz );
                break;
            case ML_TYPE_MEDIA:
                ml_gc_decref( p_result->value.p_media );
                break;
            default:
                break;
        }
        free( p_result );
    }
}


/**
 * @brief Free a ml_element_t item.
 * @param p_find Find object to free
 * @see ml_element_t */
static inline void ml_FreeElement( ml_element_t *p_elt )
{
    if( p_elt )
    {
        if( ml_AttributeIsString( p_elt->criteria ) )
        {
            free( p_elt->value.str );
        }
        if( p_elt->criteria == ML_PEOPLE )
        {
            free( p_elt->lvalue.str );
        }
        free( p_elt );
    }
}


/**
 * @brief Destroy a vlc_array_t of ml_result_t
 * @param ml_result_array The result array to free
 * @note Frees all results and contents of the results
 */
static inline void ml_DestroyResultArray( vlc_array_t *p_result_array )
{
    for( int i = 0; i < vlc_array_count( p_result_array ); i++ )
    {
        ml_FreeResult( ( ml_result_t* ) vlc_array_item_at_index(
                p_result_array, i ) );
    }
}



/*****************************************************************************
 * ML Object Management Functions
 *****************************************************************************/

/** Helpers for locking and unlocking */
#define ml_LockMedia( a )      vlc_mutex_lock( &a->lock )
#define ml_UnlockMedia( a )    vlc_mutex_unlock( &a->lock )

/**
 * @brief Object constructor for ml_media_t
 * @param p_ml The media library object
 * @param id If 0, this item isn't in database. If non zero, it is and
 * it will be a singleton
 * @param select Type of object
 * @param reload Whether to reload from database
 */
VLC_API ml_media_t *media_New( media_library_t* p_ml, int id,
        ml_select_e select, bool reload );


/* Forward declaration */
static inline int ml_CopyPersons( ml_person_t** a, ml_person_t* b );

/**
 * @brief Copy all members of a ml_media_t to another.
 * @param b Destination media, already allocated
 * @param a Source media, cannot be NULL, const
 * @note This does not check memory allocation (for strdup). It is threadsafe
 * @todo Free b content, before inserting a?
 */
static inline int ml_CopyMedia( ml_media_t *b, ml_media_t *a )
{
    if( !a || !b ) return VLC_EGENERIC;
    if( a == b ) return VLC_SUCCESS;
    ml_LockMedia( a );
    ml_LockMedia( b );
    b->b_sparse = a->b_sparse;
    b->i_id = a->i_id;
    b->i_type = a->i_type;
    b->i_album_id = a->i_album_id;
    b->i_disc_number = a->i_disc_number;
    b->i_track_number = a->i_track_number;
    b->i_year = a->i_year;
    b->i_vote = a->i_vote;
    b->i_score = a->i_score;
    b->i_filesize = a->i_filesize;
    b->i_duration = a->i_duration;
    b->i_played_count = a->i_played_count;
    b->i_last_played = a->i_last_played;
    b->i_skipped_count = a->i_skipped_count;
    b->i_last_skipped = a->i_last_skipped;
    b->i_first_played = a->i_first_played;
    b->i_import_time = a->i_import_time;
    b->i_bitrate = a->i_bitrate;
    b->i_samplerate = a->i_samplerate;
    b->i_bpm = a->i_bpm;
    free( b->psz_uri );
    if( a->psz_uri )
        b->psz_uri = strdup( a->psz_uri );
    free( b->psz_title );
    if( a->psz_title )
        b->psz_title = strdup( a->psz_title );
    free( b->psz_orig_title );
    if( a->psz_orig_title )
        b->psz_orig_title = strdup( a->psz_orig_title );
    free( b->psz_album );
    if( a->psz_album )
        b->psz_album = strdup( a->psz_album );
    free( b->psz_cover );
    if( a->psz_cover )
        b->psz_cover = strdup( a->psz_cover );
    free( b->psz_genre );
    if( a->psz_genre )
        b->psz_genre = strdup( a->psz_genre );
    free( b->psz_comment );
    if( a->psz_comment )
        b->psz_comment = strdup( a->psz_comment );
    free( b->psz_extra );
    if( a->psz_extra )
        b->psz_extra = strdup( a->psz_extra );
    free( b->psz_preview );
    if( a->psz_preview )
        b->psz_preview = strdup( a->psz_preview );
    free( b->psz_language );
    if( a->psz_language )
        b->psz_language = strdup( a->psz_language );
    ml_FreePeople( b->p_people );
    if( a->p_people )        ml_CopyPersons( &( b->p_people ), a->p_people );
    ml_UnlockMedia( b );
    ml_UnlockMedia( a );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ML Find Tree Related Functions
 *****************************************************************************/
#define ml_FreeFindTree( tree )          ml_GenericFreeFindTree( tree, true )
#define ml_ShallowFreeFindTree( tree )   ml_GenericFreeFindTree( tree, false )
/**
 * @brief Free a find tree
 * @param Find tree to free
 * @param true to free any associated strings, false to not free them
 */
static inline void ml_GenericFreeFindTree( ml_ftree_t* tree, bool freestrings )
{
    if( tree == NULL )
        return;
    if( tree->left )
    {
        ml_GenericFreeFindTree( tree->left, freestrings );
        free( tree->left );
    }
    if( tree->right )
    {
        ml_GenericFreeFindTree( tree->right, freestrings );
        free( tree->right );
    }
    if( tree->op == ML_OP_NONE && ml_AttributeIsString( tree->criteria )
            && freestrings)
    {
        free( tree->value.str );
        if( tree->criteria == ML_PEOPLE )
            free( tree->lvalue.str );
    }
}

/**
 * @brief Checks if a given find tree has leaf nodes
 * @param Find tree
 * @return Number of leaf nodes
 */
static inline int ml_FtreeHasOp( ml_ftree_t* tree )
{
    if( tree == NULL )
        return 0;
    if( tree->criteria > 0 && tree->op == ML_OP_NONE )
        return 1;
    else
        return ml_FtreeHasOp( tree->left ) + ml_FtreeHasOp( tree->right );
}


/**
 * @brief Connect up a find tree
 * @param op operator to connect with
 * If op = ML_OP_NONE, then you are connecting to a tree consisting of
 * only SPECIAL nodes.
 * If op = ML_OP_NOT, then right MUST be NULL
 * op must not be ML_OP_SPECIAL, @see ml_FtreeSpec
 * @param left part of the tree
 * @param right part of the tree
 * @return Pointer to new tree
 * @note Use the helpers!
 */
VLC_API ml_ftree_t *ml_OpConnectChilds( ml_op_e op, ml_ftree_t* left,
        ml_ftree_t* right );

/**
 * @brief Attaches a special node to a tree
 * @param tree Tree to attach special node to
 * @param crit Criteria may be SORT_ASC, SORT_DESC, LIMIT or DISTINCT
 * @param limit Limit used if LIMIT criteria used
 * @param Sort string used if SORT criteria is used
 * @return Pointer to new tree
 * @note Use the helpers
 */
VLC_API ml_ftree_t *ml_FtreeSpec( ml_ftree_t* tree,
                                          ml_select_e crit,
                                          int limit,
                                          char* sort );

/**
 * @brief This function gives quick sequential adding capability
 * @param left Tree to add to. This may be NULL
 * @param right Tree to append. May not be NULL
 * @return Pointer to new tree.*/
static inline ml_ftree_t* ml_FtreeFastAnd( ml_ftree_t* left,
                                           ml_ftree_t* right )
{
    if( ml_FtreeHasOp( left ) == 0 )
    {
        return ml_OpConnectChilds( ML_OP_NONE, left, right );
    }
    else
    {
        return ml_OpConnectChilds( ML_OP_AND, left, right );
    }
}
#define ml_FtreeAnd( left, right ) ml_OpConnectChilds( ML_OP_AND, left, right )
#define ml_FtreeOr( left, right )  ml_OpConnectChilds( ML_OP_OR, left, right )
#define ml_FtreeNot( left )        ml_OpConnectChilds( ML_OP_NOT, left, NULL )

#define ml_FtreeSpecAsc( tree, str )        ml_FtreeSpec( tree, ML_SORT_ASC, 0, str )
#define ml_FtreeSpecDesc( tree, str )       ml_FtreeSpec( tree, ML_SORT_DESC, 0, str )
#define ml_FtreeSpecLimit( tree, limit )    ml_FtreeSpec( tree, ML_LIMIT, limit, NULL )
#define ml_FtreeSpecDistinct( tree )        ml_FtreeSpec( tree, ML_DISTINCT, 0, NULL )


/*****************************************************************************
 * ML Core Functions
 *****************************************************************************/

/**
 * @brief Create input item from media
 * @param p_media_library This ML instance.
 * @param i_media_id ID of the media to use to create an input_item.
 * @return The media item.
 */
static inline input_item_t* ml_CreateInputItem(
        media_library_t *p_media_library, int i_media_id )
{
    return p_media_library->functions.pf_InputItemFromMedia( p_media_library,
                                                             i_media_id );
}

/**
 * @brief Search in the database according some criterias
 *
 * @param p_media_library the media library object
 * @param result a pointer to a result array
 * @param ... parameters to select the data
 * @return VLC_SUCCESS or an error
 */
static inline int __ml_Find( media_library_t *p_media_library,
                             vlc_array_t *p_result_array, ... )
{
    va_list args;
    int returned;

    va_start( args, p_result_array );
    returned = p_media_library->functions.pf_Find( p_media_library,
                                                   p_result_array, args );
    va_end( args );

    return returned;
}


/**
 * @brief Search in the database according some criterias (threaded)
 * @param p_media_library the media library object
 * @param result_array a pointer to a result array
 * @param result_type type of data to retrieve
 * @param psz_lvalue This should contain any necessary lvalue/key
 * for the given result_type. Used for ML_PEOPLE. Otherwise NULL
 * @param args parameters to select the data
 * @return VLC_SUCCESS or an error
 */
static inline int ml_FindAdv( media_library_t *p_media_library,
                              vlc_array_t *p_result_array,
                              ml_select_e result_type,
                              char* psz_lvalue,
                              ml_ftree_t *tree )
{
    return p_media_library->functions.pf_FindAdv( p_media_library,
                                                  p_result_array,
                                                  result_type,
                                                  psz_lvalue,
                                                  tree );
}


/**
 * @brief Find a value in the ML database, fill p_result with it.
 * @param p_media_library Media library object
 * @param p_result Object to put result into
 * @param Args [ SelectType [ PersonType ] Value ] ... ML_END
 * @note Do not use this function directly.
 */
static inline int __ml_GetValue( media_library_t *p_media_library,
                                  ml_result_t *p_result,
                                  va_list args )
{
    vlc_array_t *p_result_array = vlc_array_new();
    int i_ret = p_media_library->functions.pf_Find( p_media_library,
                                                    p_result_array,
                                                    args );
    if( i_ret != VLC_SUCCESS )
        goto exit;
    if( vlc_array_count( p_result_array ) > 0 )
        memcpy( p_result,
                ( ml_result_t* ) vlc_array_item_at_index( p_result_array, 0 ),
                sizeof( ml_result_t) );
    else
        i_ret = VLC_EGENERIC;

exit:
    /* Note: Do not free the results, because of memcpy */
    vlc_array_destroy( p_result_array );
    return i_ret;
}

/**
 * @brief Search an INTEGER in the database
 * This uses a Query but returns only one integer (>0), or an error code.
 *
 * @param p_media_library the media library object
 * @param va_args parameters to select the data
 * @return Found INTEGER >= 0 or an error
 */
#define ml_GetInt( ml, ... ) __ml_GetInt( ml, __VA_ARGS__, ML_LIMIT, 1, ML_END )
static inline int __ml_GetInt( media_library_t *p_media_library, ... )
{
    va_list args;
    va_start( args, p_media_library );
    ml_result_t result;
    int i_ret = __ml_GetValue( p_media_library, &result, args );
    va_end( args );
    if( i_ret != VLC_SUCCESS )
        return i_ret;
    else
        return result.value.i;
}


/**
 * @brief Search a string (VARCHAR) in the database
 * This uses a Query but returns only one integer (>0), or an error code.
 *
 * @param p_media_library the media library object
 * @param va_args parameters to select the data
 * @return Found string, or NULL if not found or in case of error
 */
#define ml_FindPsz( ml, ... ) __ml_GetPsz( ml, __VA_ARGS__, ML_LIMIT, 1, ML_END )
static inline char* __ml_GetPsz( media_library_t *p_media_library, ... )
{
    va_list args;
    va_start( args, p_media_library );
    ml_result_t result;
    int i_ret = __ml_GetValue( p_media_library, &result, args );
    va_end( args );
    if( i_ret != VLC_SUCCESS )
        return NULL;
    else
        return result.value.psz; // no need to duplicate
}

/**
 * @brief Generic update in Media Library database
 *
 * @param p_media_library the media library object
 * @param selected_type the type of the element we're selecting
 * @param where list of ids/uris to be changed
 * @param changes list of changes to make in the entries
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int ml_Update( media_library_t *p_media_library,
                             ml_select_e selected_type,
                             const char* psz_lvalue,
                             ml_ftree_t *where,
                             vlc_array_t *changes )
{
    return p_media_library->functions.pf_Update( p_media_library,
                                                 selected_type, psz_lvalue,
                                                 where, changes );
}

/**
 * @brief Update a given table
 * @param p_media_library The media library object
 * @param selected_type The table to update
 * @param psz_lvalue The role of the person if selected_type = ML_PEOPLE
 * @param id The id of the row to update
 * @param ... The update data. [SelectType [RoleType] Value]
 */
VLC_API int ml_UpdateSimple( media_library_t *p_media_library,
                                     ml_select_e selected_type,
                                     const char* psz_lvalue,
                                     int id, ... );
#define ml_UpdateSimple( ml, sel, lval, id, ... ) \
        ml_UpdateSimple( ml, sel, lval, id, __VA_ARGS__, ML_END )

/**
 * @brief Generic DELETE function
 * Delete a media and all its references which don't point
 * to anything else.
 *
 * @param p_media_library This media_library_t object
 * @param id the id of the media to delete
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int
ml_DeleteSimple( media_library_t *p_media_library, int id )
{
    vlc_array_t* p_where = vlc_array_new();
    ml_element_t* p_find = (ml_element_t *) calloc( 1, sizeof( ml_element_t ) );
    p_find->criteria = ML_ID;
    p_find->value.i = id;
    vlc_array_append( p_where, p_find );
    int i_return =  p_media_library->functions.pf_Delete( p_media_library,
            p_where );
    free( p_find );
    vlc_array_destroy( p_where );
    return i_return;
}

/**
 * @brief Delete many medias in the media library
 * @param p_media_library Media library object
 * @param p_array Array of ids to delete
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int
ml_Delete( media_library_t *p_media_library, vlc_array_t* p_array )
{
    return p_media_library->functions.pf_Delete( p_media_library,
                                                        p_array );
}


/*****************************************************************************
 * ML Person Related Functions
 *****************************************************************************/

/**
 * @brief Create and append a person object to the given list
 * @param pp_person pointer to person list. Set the address to null to create new list
 * @param i_role The role of the person
 * @param psz_name The name string. Will be strdup'd
 * @param i_id The id in the database
 * @note This function is NOT thread safe. Please lock any associated media
 */
static inline int ml_CreateAppendPersonAdv( ml_person_t **pp_person,
        const char* psz_role, const char* psz_name, int i_id )
{
    if( i_id == 0 || !( psz_name && *psz_name && psz_role && *psz_role ) )
        return VLC_SUCCESS;
    if( !pp_person )
        return VLC_EGENERIC;
    if( *pp_person != NULL )
        return ml_CreateAppendPersonAdv( &((**pp_person).p_next),
                                         psz_role, psz_name, i_id);
    *pp_person = ( ml_person_t * ) calloc( 1, sizeof( ml_person_t ) );
    (*pp_person)->psz_name = (psz_name && *psz_name) ? strdup( psz_name ): NULL;
    (*pp_person)->psz_role = (psz_role && *psz_role) ? strdup( psz_role ): NULL;
    (*pp_person)->i_id = i_id;
    (*pp_person)->p_next = NULL;
    return VLC_SUCCESS;
}

/**
 * @brief Create and append a person object to the given list
 * @param pp_person pointer to person list.
 * Set the address to NULL to create a new list
 * @param personfrom Person object to copy from
 * @note Ignores the next variable and copies only the variables.
 * Uses ml_CreateAppendPersonAdv
 * @note This function is NOT threadsafe
 */
static inline int ml_CreateAppendPerson( ml_person_t **pp_person,
                                         ml_person_t *p_personfrom )
{
    return ml_CreateAppendPersonAdv( pp_person,
                                     p_personfrom->psz_role,
                                     p_personfrom->psz_name,
                                     p_personfrom->i_id );
}

/**
 * @brief Copy one person list into another
 * @param a To list
 * @param b From list
 * @note On errors, you have to free any allocated persons yourself
 * @note This function is NOT threadsafe. Please ensure your medias are locked
 */
static inline int ml_CopyPersons( ml_person_t** a, ml_person_t* b )
{
    int i_ret;
    while( b )
    {
        i_ret = ml_CreateAppendPerson( a, b );
        if( i_ret != VLC_SUCCESS )
            return i_ret;
        b = b->p_next;
    }
    return VLC_SUCCESS;
}


/**
 * @brief Returns a person list of given type
 * @param p_ml The ML object
 * @param p_media The Media object
 * @param i_type The person type
 * @note This function is thread safe
 */
VLC_API ml_person_t *ml_GetPersonsFromMedia( media_library_t* p_ml,
                                                    ml_media_t* p_media,
                                                    const char *psz_role );


#define ml_GetAlbumArtistsFromMedia( a, b ) ml_GetPersonsFromMedia( a, b, ML_PERSON_ALBUM_ARTIST );
#define ml_GetArtistsFromMedia( a, b )      ml_GetPersonsFromMedia( a, b, ML_PERSON_ARTIST );
#define ml_GetEncodersFromMedia( a, b )     ml_GetPersonsFromMedia( a, b, ML_PERSON_ENCODER );
#define ml_GetPublishersFromMedia( a, b )   ml_GetPersonsFromMedia( a, b, ML_PERSON_PUBLISHER );

/**
 * @brief Delete a certain type of people from a media
 * @param p_media Media to delete from
 * @param i_type Type of person to delete
 * @note This function is threadsafe
 */
VLC_API void ml_DeletePersonTypeFromMedia( ml_media_t* p_media,
                                                 const char *psz_role );


/**
 * @brief Creates and adds the playlist based on a given find tree
 * @param p_ml Media library object
 * @param p_tree Find tree to create SELECT
 */

VLC_API void ml_PlaySmartPlaylistBasedOn( media_library_t* p_ml,
                                                ml_ftree_t* p_tree );


/**
 * Convenience Macros
 */

/**
 * Get information using the *media* ID. This returns only 1 information.
 * @note You have to free the string returned (if that's a string!).
 */
#define ml_GetAlbumById( a, id )            ml_GetPsz( a, ML_ALBUM, ML_ID, id )
#define ml_GetArtistById( a, id )           ml_GetPsz( a, ML_PEOPLE, ML_PERSON_ARTIST, ML_ID, id )
#define ml_GetCoverUriById( a, id )         ml_GetPsz( a, ML_COVER, ML_ID, id )
#define ml_GetEncoderById( a, id )          ml_GetPsz( a, ML_PEOPLE, ML_PERSON_ENCODER, ML_ID, id )
#define ml_GetExtraById( a, id )            ml_GetPsz( a, ML_EXTRA, ML_ID, id )
#define ml_GetGenreById( a, id )            ml_GetPsz( a, ML_GENRE, ML_ID, id )
#define ml_GetOriginalTitleById( a, id )    ml_GetPsz( a, ML_ORIGINAL_TITLE, ML_ID, id )
#define ml_GetPublisherById( a, id )        ml_GetPsz( a, ML_PEOPLE, ML_PERSON_PUBLISHER, ML_ID, id )
#define ml_GetTitleById( a, id )            ml_GetPsz( a, ML_TITLE, ML_ID, id )
#define ml_GetUriById( a, id )              ml_GetPsz( a, ML_URI, ML_ID, id )

#define ml_GetAlbumIdById( a, id )          ml_GetInt( a, ML_ALBUM_ID, ML_ID, id )
#define ml_GetArtistIdById( a, id )         ml_GetInt( a, ML_PEOPLE_ID, ML_PERSON_ARTIST, ML_ID, id )
#define ml_GetDurationById( a, id )         ml_GetInt( a, ML_DURATION, ML_ID, id )
#define ml_GetEncoderIdById( a, id )        ml_GetInt( a, ML_PEOPLE_ID, ML_PERSON_ENCODER, ML_ID, id )
#define ml_GetLastPlayedById( a, id )       ml_GetInt( a, ML_LAST_PLAYED, ML_ID, id )
#define ml_GetPlayedCountById( a, id )      ml_GetInt( a, ML_PLAYED_COUNT, ML_ID, id )
#define ml_GetPublisherIdById( a, id )      ml_GetInt( a, ML_PEOPLE_ID, ML_PERSON_PUBLISHER, ML_ID, id )
#define ml_GetScoreById( a, id )            ml_GetInt( a, ML_SCORE, ML_ID, id )
#define ml_GetTrackNumberById( a, id )      ml_GetInt( a, ML_TRACK_NUMBER, ML_ID, id )
#define ml_GetTypeById( a, id )             ml_GetInt( a, ML_TYPE, ML_ID, id )
#define ml_GetYearById( a, id )             ml_GetInt( a, ML_YEAR, ML_ID, id )
#define ml_GetVoteById( a, id )             ml_GetInt( a, ML_VOTE, ML_ID, id )

/** Albums handling */
#define ml_GetAlbumId( a, b )               ml_GetInt( a, ML_ALBUM_ID, ML_ALBUM, b )

/** People handling */
#define ml_GetArtistId( a, b )              ml_GetInt( a, ML_PERSON_ID, ML_PERSON_ARTIST, ML_PERSON, ML_PERSON_ARTIST, b )
#define ml_GetEncoderId( a, b )             ml_GetInt( a, ML_PERSON_ID, ML_PERSON_ENCODER, ML_PERSON, ML_PERSON_ENCODER, b )
#define ml_GetPublisherId( a, b )           ml_GetInt( a, ML_PERSON_ID, ML_PERSON_PUBLISHER, ML_PERSON, ML_PERSON_PUBLISHER, b )

/** Counts handling */
#define ml_GetMediaCount( a, ... )          __ml_GetInt( a, ML_COUNT_MEDIA,      __VA_ARGS__, ML_END )
#define ml_GetAlbumCount( a, ... )          __ml_GetInt( a, ML_COUNT_ALBUM,      __VA_ARGS__, ML_END )
#define ml_GetPeopleCount( a, ... )         __ml_GetInt( a, ML_COUNT_PEOPLE,     __VA_ARGS__, ML_END )

#define ml_Find( a, b, ... )                __ml_Find( a, b, __VA_ARGS__, ML_END )

#define ml_FindAlbum( a, b, ... )           __ml_Find( a, b, ML_ALBUM,           __VA_ARGS__, ML_END )
#define ml_FindArtist( a, b, ... )          __ml_Find( a, b, ML_PERSON, ML_PERSON_ARTIST, __VA_ARGS__, ML_END )
#define ml_FindEncoder( a, b, ... )         __ml_Find( a, b, ML_PERSON, ML_PERSON_ENCODER, __VA_ARGS__, ML_END )
#define ml_FindGenre( a, b, ... )           __ml_Find( a, b, ML_GENRE,           __VA_ARGS__, ML_END )
#define ml_FindMedia( a, b, ... )           __ml_Find( a, b, ML_MEDIA,           __VA_ARGS__, ML_END )
#define ml_FindOriginalTitle( a, b, ... )   __ml_Find( a, b, ML_ORIGINAL_TITLE,  __VA_ARGS__, ML_END )
#define ml_FindPublisher( a, b, ... )       __ml_Find( a, b, ML_PERSON, ML_PERSON_PUBLISHER, __VA_ARGS__, ML_END )
#define ml_FindTitle( a, b, ... )           __ml_Find( a, b, ML_TITLE,           __VA_ARGS__, ML_END )
#define ml_FindType( a, b, ... )            __ml_Find( a, b, ML_TYPE,            __VA_ARGS__, ML_END )
#define ml_FindUri( a, b, ... )             __ml_Find( a, b, ML_URI,             __VA_ARGS__, ML_END )
#define ml_FindYear( a, b, ... )            __ml_Find( a, b, ML_YEAR,            __VA_ARGS__, ML_END )

#define ml_FindAllAlbums( a, b )            ml_FindAlbum( a, b,         ML_DISTINCT )
#define ml_FindAllArtists( a, b )           ml_FindArtist( a, b,        ML_DISTINCT )
#define ml_FindAllGenres( a, b )            ml_FindGenre( a, b,         ML_DISTINCT )
#define ml_FindAllMedias( a, b )            ml_FindMedia( a, b,         ML_DISTINCT )
#define ml_FindAllOriginalTitles( a, b )    ml_FindOriginalTitle( a, b, ML_DISTINCT )
#define ml_FindAllPublishers( a, b, ... )   ml_FindPublisher( a, b,     ML_DISTINCT )
#define ml_FindAllTitles( a, b )            ml_FindTitle( a, b,         ML_DISTINCT )
#define ml_FindAllTypes( a, b )             ml_FindType( a, b,          ML_DISTINCT )
#define ml_FindAllUris( a, b )              ml_FindUri( a, b,           ML_DISTINCT )
#define ml_FindAllYears( a, b )             ml_FindYear( a, b,          ML_DISTINCT )

#define ml_FindAlbumAdv( a, b, c )          ml_FindAdv( a, b, ML_ALBUM,         NULL, c )
#define ml_FindArtistAdv( a, b, c )         ml_FindAdv( a, b, ML_PERSON,        ML_PERSON_ARTIST, c )
#define ml_FindEncoderAdv( a, b, c )        ml_FindAdv( a, b, ML_PERSON,        ML_PERSON_ENCODER, c )
#define ml_FindGenreAdv( a, b, c )          ml_FindAdv( a, b, ML_GENRE,         NULL, c )
#define ml_FindMediaAdv( a, b, c )          ml_FindAdv( a, b, ML_MEDIA,         NULL, c )
#define ml_FindOriginalTitleAdv( a, b, c )  ml_FindAdv( a, b, ML_ORIGINAL_TITLE,NULL, c )
#define ml_FindPublisherAdv( a, b, c )      ml_FindAdv( a, b, ML_PUBLISHER,     ML_PERSON_PUBLISHER, c )
#define ml_FindTitleAdv( a, b, c )          ml_FindAdv( a, b, ML_TITLE,         NULL, c )
#define ml_FindTypeAdv( a, b, c )           ml_FindAdv( a, b, ML_TYPE,          NULL, c )
#define ml_FindUriAdv( a, b, c )            ml_FindAdv( a, b, ML_URI,           NULL, c )
#define ml_FindYearAdv( a, b, c )           ml_FindAdv( a, b, ML_YEAR,          NULL, c )



#ifdef __cplusplus
}
#endif /* C++ */

#endif /* VLC_MEDIA_LIBRARY_H */
