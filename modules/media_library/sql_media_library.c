/*****************************************************************************
 * sql_media_library.c: SQL-based media library
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

#include "sql_media_library.h"

static const char* ppsz_AudioExtensions[] = { EXTENSIONS_AUDIO_CSV, NULL };
static const char* ppsz_VideoExtensions[] = { EXTENSIONS_VIDEO_CSV, NULL };

#define MEDIA_LIBRARY_PATH_TEXT N_( "Filename of the SQLite database" )
#define MEDIA_LIBRARY_PATH_LONGTEXT N_( "Path to the file containing " \
                                        "the SQLite database" )

#define IGNORE_TEXT N_( "Ignored extensions in the media library" )
#define IGNORE_LONGTEXT N_( "Files with these extensions will not be added to"\
                            " the media library when scanning directories." )

#define RECURSIVE_TEXT N_( "Subdirectory recursive scanning" )
#define RECURSIVE_LONGTEXT N_( "When scanning a directory, scan also all its"\
        " subdirectories." )



/*****************************************************************************
 * Static functions
 *****************************************************************************/

/* Module entry point and exit point */
static int load( vlc_object_t* );
static void unload( vlc_object_t* );

static int CreateInputItemFromMedia( input_item_t **pp_item,
                                     ml_media_t *p_media );


struct ml_table_elt
{
    int column_id;
    const char* column_name;
};

static int compare_ml_elts( const void *a, const void *b )
{
    return strcmp( ( (struct ml_table_elt* )a )->column_name,
            ( ( struct ml_table_elt* )b )->column_name );
}

static const struct ml_table_elt ml_table_map[]=
{
    { ML_ALBUM_COVER,    "album_cover" },
    { ML_ALBUM_ID,       "album_id" },
    { ML_ALBUM,          "album_title" },
    { ML_COMMENT,        "comment" },
    { ML_COVER,          "cover" },
    { ML_DIRECTORY,      "directory_id" },
    { ML_DISC_NUMBER,    "disc" },
    { ML_DURATION,       "duration" },
    { ML_EXTRA,          "extra" },
    { ML_FILESIZE,       "filesize" },
    { ML_FIRST_PLAYED,   "first_played" },
    { ML_GENRE,          "genre" },
    { ML_ID,             "id" },
    { ML_IMPORT_TIME,    "import_time" },
    { ML_LANGUAGE,       "language" },
    { ML_LAST_PLAYED,    "last_played" },
    { ML_LAST_SKIPPED,   "last_skipped" },
    { ML_ORIGINAL_TITLE, "original_title" },
    { ML_PEOPLE_ID,      "people_id" },
    { ML_PEOPLE,         "people_name" },
    { ML_PEOPLE_ROLE,    "people_role" },
    { ML_PLAYED_COUNT,   "played_count" },
    { ML_PREVIEW,        "preview" },
    { ML_SCORE,          "score" },
    { ML_SKIPPED_COUNT,  "skipped_count" },
    { ML_TITLE,          "title" },
    { ML_TRACK_NUMBER,   "track" },
    { ML_TYPE,           "type" },
    { ML_URI,            "uri" },
    { ML_VOTE,           "vote" },
    { ML_YEAR,           "year" }
};

/*****************************************************************************
 * Module description
 *****************************************************************************/
vlc_module_begin()
    set_shortname( "Media Library" )
    set_description( _( "Media Library based on a SQL based database" ) )
    set_capability( "media-library", 1 )
    set_callbacks( load, unload )
    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_MISC )
    add_string( "ml-filename", "vlc-media-library.db",
            MEDIA_LIBRARY_PATH_TEXT, MEDIA_LIBRARY_PATH_LONGTEXT, false )
    add_string( "ml-username", "",  N_( "Username for the database" ),
            N_( "Username for the database" ), false )
    add_string( "ml-password", "",  N_( "Password for the database" ),
            N_( "Password for the database" ), false )
    add_integer( "ml-port", 0,
            N_( "Port for the database" ), N_("Port for the database"), false )
    add_bool( "ml-recursive-scan", true, RECURSIVE_TEXT,
            RECURSIVE_LONGTEXT, false )
    add_bool( "ml-auto-add", true,  N_("Auto add new medias"),
            N_( "Automatically add new medias to ML" ), false )
vlc_module_end()


/**
 * @brief Load module
 * @param obj Parent object
 */
static int load( vlc_object_t *obj )
{
    msg_Dbg( obj, "loading media library module" );

    media_library_t *p_ml = ( media_library_t * ) obj;
    p_ml->p_sys = ( media_library_sys_t* )
                        calloc( 1, sizeof( media_library_sys_t ) );
    if( !p_ml->p_sys )
        return VLC_ENOMEM;

    p_ml->functions.pf_Find               = FindVa;
    p_ml->functions.pf_FindAdv            = FindAdv;
    p_ml->functions.pf_Control            = Control;
    p_ml->functions.pf_InputItemFromMedia = GetInputItemFromMedia;
    p_ml->functions.pf_Update             = Update;
    p_ml->functions.pf_Delete             = Delete;
    p_ml->functions.pf_GetMedia           = GetMedia;

    vlc_mutex_init( &p_ml->p_sys->lock );

    /* Initialise Sql module */
    if ( InitDatabase( p_ml ) != VLC_SUCCESS )
    {
        vlc_mutex_destroy( &p_ml->p_sys->lock );
        //free( p_ml->p_sys ); // FIXME: Freed in InitDatase ?!?
        return VLC_EGENERIC;
    }

    /* Initialise the media pool */
    ARRAY_INIT( p_ml->p_sys->mediapool );
    vlc_mutex_init( &p_ml->p_sys->pool_mutex );

    /* Create variables system */
    var_Create( p_ml, "media-added", VLC_VAR_INTEGER );
    var_Create( p_ml, "media-deleted", VLC_VAR_INTEGER );
    var_Create( p_ml, "media-meta-change", VLC_VAR_INTEGER );

    /* Launching the directory monitoring thread */
    monitoring_thread_t *p_mon =
            vlc_object_create( p_ml, sizeof( monitoring_thread_t ) );
    if( !p_mon )
    {
        vlc_mutex_destroy( &p_ml->p_sys->lock );
        sql_Destroy( p_ml->p_sys->p_sql );
        free( p_ml->p_sys );
        return VLC_ENOMEM;
    }
    p_ml->p_sys->p_mon = p_mon;

    p_mon->p_ml = p_ml;

    if( vlc_clone( &p_mon->thread, RunMonitoringThread, p_mon,
                VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_ml, "cannot spawn the media library monitoring thread" );
        vlc_mutex_destroy( &p_ml->p_sys->lock );
        sql_Destroy( p_ml->p_sys->p_sql );
        free( p_ml->p_sys );
        vlc_object_release( p_mon );
        return VLC_EGENERIC;
    }
    /* Starting the watching system (starts a thread) */
    watch_Init( p_ml );

    msg_Dbg( p_ml, "Media library module loaded successfully" );

    return VLC_SUCCESS;
}


/**
 * @brief Unload module
 *
 * @param obj the media library object
 * @return Nothing
 */
static void unload( vlc_object_t *obj )
{
    media_library_t *p_ml = ( media_library_t* ) obj;

    /* Stopping the watching system */
    watch_Close( p_ml );

    /* Stop the monitoring thread */
    vlc_cancel( p_ml->p_sys->p_mon->thread );
    vlc_join( p_ml->p_sys->p_mon->thread, NULL );
    vlc_object_release( p_ml->p_sys->p_mon );

    /* Destroy the variable */
    var_Destroy( p_ml, "media-meta-change" );
    var_Destroy( p_ml, "media-deleted" );
    var_Destroy( p_ml, "media-added" );

    /* Empty the media pool */
    ml_media_t* item;
    FOREACH_ARRAY( item, p_ml->p_sys->mediapool )
        ml_gc_decref( item );
    FOREACH_END()
    vlc_mutex_destroy( &p_ml->p_sys->pool_mutex );

    sql_Destroy( p_ml->p_sys->p_sql );

    vlc_mutex_destroy( &p_ml->p_sys->lock );

    free( p_ml->p_sys );
}

/**
 * @brief Get results of an SQL-Query on the database (please : free the result)
 *
 * @param p_ml the media library object
 * @param ppp_res char *** in which to store the table of results (allocated)
 * @param pi_rows resulting row number in table
 * @param pi_cols resulting column number in table
 * @param psz_fmt query command with printf-like format enabled
 * @param va_args format the command
 * @return VLC_SUCCESS or a VLC error code
 */
int Query( media_library_t *p_ml,
              char ***ppp_res, int *pi_rows, int *pi_cols,
              const char *psz_fmt, ... )
{
    va_list argp;
    va_start( argp, psz_fmt );

    int i_ret = QueryVa( p_ml, ppp_res, pi_rows, pi_cols, psz_fmt, argp );

    va_end( argp );
    return i_ret;
}

/**
 * @brief Get results of an SQL-Query on the database (please : free the result)
 *
 * @param p_ml the media library object
 * @param ppp_res char *** in which to store the table of results (allocated)
 * @param pi_rows resulting row number in table
 * @param pi_cols resulting column number in table
 * @param psz_fmt query command with printf-like format enabled
 * @param va_args format the command
 * @return VLC_SUCCESS or a VLC error code
 */
int QueryVa( media_library_t *p_ml, char ***ppp_res,
                      int *pi_rows, int *pi_cols, const char *psz_fmt,
                      va_list argp )
{
    assert( p_ml );
    if( !ppp_res || !psz_fmt ) return VLC_EGENERIC;

    char *psz_query = sql_VPrintf( p_ml->p_sys->p_sql, psz_fmt, argp );
    if( !psz_query )
        return VLC_ENOMEM;

    int i_ret = sql_Query( p_ml->p_sys->p_sql, psz_query,
                           ppp_res, pi_rows, pi_cols );

    free( psz_query );
    return i_ret;
}

/**
 * @brief Do a SQL-query without any data coming back
 *
 * @param p_ml the media library object
 * @param psz_fmt query command with printf-like format enabled
 * @param va_args format the command
 * @return VLC_SUCCESS or a VLC error code
 */
int QuerySimple( media_library_t *p_ml,
                    const char *psz_fmt, ... )
{
    va_list argp;
    va_start( argp, psz_fmt );

    int i_ret = QuerySimpleVa( p_ml, psz_fmt, argp );

    va_end( argp );
    return i_ret;
}

/**
 * @brief Do a SQL-query without any data coming back
 *
 * @param p_ml the media library object
 * @param psz_fmt query command with printf-like format enabled
 * @param argp format the command
 * @return VLC_SUCCESS or a VLC error code
 */
int QuerySimpleVa( media_library_t *p_ml,
                      const char *psz_fmt, va_list argp )
{
    assert( p_ml );

    int i_ret = VLC_SUCCESS;
    int i_rows, i_cols;
    char **pp_results = NULL;

    i_ret = QueryVa( p_ml, &pp_results, &i_rows, &i_cols, psz_fmt, argp );

    FreeSQLResult( p_ml, pp_results );
    va_end( argp );

    return i_ret;
}

/**
 * @brief Transforms a string to a ml_result_t, with given type and id (as psz)
 *
 * @param res the result of the function
 * @param psz string to transform into a result
 * @param psz_id id as a string
 * @param result_type type of the result
 * @return ID or a VLC error code
 */
int StringToResult( ml_result_t *p_result, const char *psz,
                    const char *psz_id, ml_result_type_e result_type )
{
    memset( &p_result->value, 0, sizeof( p_result->value ) );

    p_result->id = psz_id ? atoi( psz_id ) : 0;
    p_result->type = result_type;

    switch( result_type )
    {
        case ML_TYPE_INT:
            p_result->value.i = psz ? atoi( psz ) : 0;
            break;

        case ML_TYPE_TIME:
            p_result->value.time = psz ? ( mtime_t ) atoi( psz )
                                       : ( mtime_t ) 0LL;
            break;

        case ML_TYPE_PSZ:
            p_result->value.psz = psz ? strdup( psz ) : NULL;
            break;

        case ML_TYPE_MEDIA:
        default:
            /* This is an error */
            return VLC_EGENERIC;
    }

    return p_result->id;
}


/**
 * @brief fills an ml_result_array_t with result of an SQL query
 *
 * @param p_ml the media library object
 * @param p_media ml_result_array_t object to fill
 * @param pp_results result of sql query
 * @param i_rows row number
 * @param i_cols column number
 * @param result_type type of the result
 * @return VLC_SUCCESS or a VLC error code
 **/
int SQLToResultArray( media_library_t *p_ml, vlc_array_t *p_result_array,
                      char **pp_results, int i_rows, int i_cols,
                      ml_result_type_e result_type )
{
    assert( p_ml );
    if( !p_result_array )
        return VLC_EGENERIC;
    if( i_cols == 0 )   /* No result */
        return VLC_SUCCESS;
    if( i_cols < 0 )
    {
        msg_Err( p_ml, "negative number of columns in result ?" );
        return VLC_EGENERIC;
    }

    if( i_cols == 1 )
    {
        for( int i = 1; i <= i_rows; i++ )
        {
            ml_result_t *res = ( ml_result_t* )
                                    calloc( 1, sizeof( ml_result_t ) );
            if( !res )
                return VLC_ENOMEM;
            StringToResult( res, pp_results[ i ], NULL, result_type );
            vlc_array_append( p_result_array, res );
        }
    }
    /* FIXME?: Assuming all double column results are id - result pairs */
    else if( ( i_cols == 2 ) )
    {
        for( int i = 1; i <= i_rows; i++ )
        {
            ml_result_t *res = ( ml_result_t* )
                                    calloc( 1, sizeof( ml_result_t ) );
            if( !res )
                return VLC_ENOMEM;
            StringToResult( res, pp_results[ i * 2 + 1], pp_results[ i * 2 ],
                            result_type );
            vlc_array_append( p_result_array, res );
        }
    }
    else if( result_type == ML_TYPE_MEDIA )
    {
        return SQLToMediaArray( p_ml, p_result_array,
                                pp_results, i_rows, i_cols );
    }
    else
    {
        msg_Err( p_ml, "unable to convert SQL result to a ml_result_t array" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}


/**
 * @brief fills a vlc_array_t with results of an SQL query
 *        medias in ml_result_t
 *
 * @param p_ml the media library object
 * @param p_array array to fill with ml_media_t elements (might be initialized)
 * @param pp_results result of sql query
 * @param i_rows row number
 * @param i_cols column number
 * @return VLC_SUCCESS or a VLC error code
 * Warning: this returns VLC_EGENERIC if i_rows == 0 (empty result)
 **/
int SQLToMediaArray( media_library_t *p_ml, vlc_array_t *p_result_array,
                     char **pp_results, int i_rows, int i_cols )
{
    int i_ret = VLC_SUCCESS;
    assert( p_ml );

    #define res( i, j ) ( pp_results[ i * i_cols + j ] )
    #define atoinull( a ) ( (a) ? atoi( a ) : 0 )
    #define strdupnull( a ) ( (a) ? strdup( a ) : NULL )

    if( i_rows == 0 )
        return VLC_EGENERIC;

    if( !p_result_array || !pp_results || i_rows < 0 || i_cols <= 0 )
    {
        msg_Warn( p_ml, "bad arguments (%s:%d)", __FILE__, __LINE__ );
        return VLC_EGENERIC;
    }

    vlc_array_t* p_intermediate_array = vlc_array_new();

    /* Analyze first row */
    int *indexes = ( int* ) calloc( i_cols + 1, sizeof( int ) );
    if( !indexes )
    {
        vlc_array_destroy( p_intermediate_array );
        return VLC_ENOMEM;
    }

    const int count = sizeof( ml_table_map )/ sizeof( struct ml_table_elt );
    for( int col = 0; col < i_cols; col++ )
    {
        struct ml_table_elt key, *result = NULL;
        key.column_name = res( 0, col );
        result = bsearch( &key, ml_table_map, count,
                sizeof( struct ml_table_elt ), compare_ml_elts );

        if( !result )
            msg_Warn( p_ml, "unknown column: %s", res( 0, col ) );
        else
            indexes[col] = result->column_id;
    }

    /* Read rows 1 to i_rows */
    ml_media_t  *p_media  = NULL;
    ml_result_t *p_result = NULL;

    for( int row = 1; ( row <= i_rows ) && ( i_ret == VLC_SUCCESS ); row++ )
    {
        p_media = media_New( p_ml, 0, ML_MEDIA, false );
        if( !p_media )
        {
            free( indexes );
            i_ret = VLC_ENOMEM;
            goto quit_sqlmediaarray;
        }
        p_result = ( ml_result_t * ) calloc( 1, sizeof( ml_result_t ) );
        if( !p_result )
        {
            ml_gc_decref( p_media );
            free( indexes );
            i_ret = VLC_ENOMEM;
            goto quit_sqlmediaarray;
        }

        char* psz_append_pname = NULL;
        char* psz_append_prole = NULL;
        int i_append_pid = 0;

#define SWITCH_INT( key, value ) case key: \
        p_media-> value = atoinull( res( row, col ) );
#define SWITCH_PSZ( key, value ) case key: \
        p_media-> value = strdupnull( res( row, col ) );

        ml_LockMedia( p_media );
        for( int col = 0; ( col < i_cols ) && ( i_ret == VLC_SUCCESS ); col++ )
        {
            switch( indexes[ col ] )
            {
                SWITCH_INT( ML_ALBUM_ID, i_album_id );
                SWITCH_PSZ( ML_ALBUM, psz_album );
                SWITCH_PSZ( ML_COMMENT, psz_comment );
                SWITCH_INT( ML_DISC_NUMBER, i_disc_number );
                SWITCH_INT( ML_DURATION, i_duration );
                SWITCH_PSZ( ML_EXTRA, psz_extra );
                SWITCH_INT( ML_FILESIZE, i_filesize );
                SWITCH_INT( ML_FIRST_PLAYED, i_first_played );
                SWITCH_PSZ( ML_GENRE, psz_genre);
                SWITCH_INT( ML_IMPORT_TIME, i_import_time );
                SWITCH_PSZ( ML_LANGUAGE, psz_language );
                SWITCH_INT( ML_LAST_PLAYED, i_last_played );
                SWITCH_INT( ML_LAST_SKIPPED, i_last_skipped );
                SWITCH_PSZ( ML_ORIGINAL_TITLE, psz_orig_title );
                SWITCH_INT( ML_PLAYED_COUNT, i_played_count );
                SWITCH_PSZ( ML_PREVIEW, psz_preview );
                SWITCH_INT( ML_SCORE, i_score );
                SWITCH_INT( ML_SKIPPED_COUNT, i_skipped_count );
                SWITCH_PSZ( ML_TITLE, psz_title );
                SWITCH_INT( ML_TRACK_NUMBER, i_track_number );
                SWITCH_INT( ML_TYPE, i_type );
                SWITCH_INT( ML_VOTE, i_vote);
                SWITCH_INT( ML_YEAR, i_year );
            case ML_ALBUM_COVER:
                /* See ML_COVER */
                // Discard attachment://
                if( !p_media->psz_cover || !*p_media->psz_cover
                 || !strncmp( p_media->psz_cover, "attachment://", 13 ) )
                {
                    free( p_media->psz_cover );
                    p_media->psz_cover = strdupnull( res( row, col ) );
                }
                break;
            case ML_PEOPLE:
                psz_append_pname = strdupnull( res( row, col ) );
                break;
            case ML_PEOPLE_ID:
                i_append_pid = atoinull( res( row, col ) );
                break;
            case ML_PEOPLE_ROLE:
                psz_append_prole = strdupnull( res( row, col ) );
                break;
            case ML_COVER:
                /* See ML_ALBUM_COVER */
                if( !p_media->psz_cover || !*p_media->psz_cover
                     || !strncmp( p_media->psz_cover, "attachment://", 13 ) )
                {
                    free( p_media->psz_cover );
                    p_media->psz_cover = strdupnull( res( row, col ) );
                }
                break;
            case ML_ID:
                p_media->i_id = atoinull( res( row, col ) );
                if( p_media->i_id <= 0 )
                    msg_Warn( p_ml, "entry with id null or inferior to zero" );
                break;
            case ML_URI:
                p_media->psz_uri = strdupnull( res( row, col ) );
                if( !p_media->psz_uri )
                    msg_Warn( p_ml, "entry without uri" );
                break;
            case ML_DIRECTORY:
                break; // The column directory_id is'nt part of the media model
            default:
                msg_Warn( p_ml, "unknown element, row %d column %d (of %d) - %s - %s",
                        row, col, i_cols, res( 0 , col ), res( row, col ) );
                break;
            }
        }

#undef SWITCH_INT
#undef SWITCH_PSZ
        int i_appendrow;
        ml_result_t* p_append = NULL;
        for( i_appendrow = 0; i_appendrow < vlc_array_count( p_intermediate_array ); i_appendrow++ )
        {
            p_append = ( ml_result_t* )
                vlc_array_item_at_index( p_intermediate_array, i_appendrow );
            if( p_append->id == p_media->i_id )
                break;
        }
        if( i_appendrow == vlc_array_count( p_intermediate_array ) )
        {
            p_result->id   = p_media->i_id;
            p_result->type = ML_TYPE_MEDIA;
            p_result->value.p_media = p_media;
            if( psz_append_pname && i_append_pid && psz_append_prole )
                ml_CreateAppendPersonAdv( &(p_result->value.p_media->p_people),
                        psz_append_prole, psz_append_pname, i_append_pid );
            vlc_array_append( p_intermediate_array, p_result );
            ml_UnlockMedia( p_media );
        }
        else /* This is a repeat row and the people need to be put together */
        {
            free( p_result );
            ml_LockMedia( p_append->value.p_media );
            if( psz_append_pname && i_append_pid && psz_append_prole )
                ml_CreateAppendPersonAdv( &(p_append->value.p_media->p_people),
                        psz_append_prole, psz_append_pname, i_append_pid );
            ml_UnlockMedia( p_append->value.p_media );
            ml_UnlockMedia( p_media );
            ml_gc_decref( p_media );
        }
        FREENULL( psz_append_prole );
        FREENULL( psz_append_pname );
        i_append_pid = 0;
    }
    p_media = NULL;
    free( indexes );

    /* Now check if these medias are already on the pool, and sync */
    for( int i = 0; i < vlc_array_count( p_intermediate_array ); i++ )
    {
        p_result =
            ( ml_result_t* )vlc_array_item_at_index( p_intermediate_array, i );
        p_media = p_result->value.p_media;
        ml_media_t* p_poolmedia = pool_GetMedia( p_ml, p_result->id );
        /* TODO: Pool_syncMedia might be cleaner? */

        p_result = ( ml_result_t* ) calloc( 1, sizeof( ml_result_t * ) );
        if( !p_result )
            goto quit_sqlmediaarray;
        if( p_poolmedia )
        {
            /* TODO: This might cause some weird stuff to occur w/ GC? */
            ml_CopyMedia( p_poolmedia, p_media );
            p_result->id = p_poolmedia->i_id;
            p_result->type = ML_TYPE_MEDIA;
            p_result->value.p_media = p_poolmedia;
            vlc_array_append( p_result_array, p_result );
        }
        else
        {
            i_ret = pool_InsertMedia( p_ml, p_media, false );
            if( i_ret == VLC_SUCCESS )
            {
                ml_gc_incref( p_media );
                p_result->id = p_media->i_id;
                p_result->type = ML_TYPE_MEDIA;
                p_result->value.p_media = p_media;
                vlc_array_append( p_result_array, p_result );
            }
        }
    }

    #undef strdupnull
    #undef atoinull
    #undef res
quit_sqlmediaarray:
    for( int k = 0; k < vlc_array_count( p_intermediate_array ); k++ )
    {
        ml_result_t* temp = ((ml_result_t*)vlc_array_item_at_index( p_intermediate_array, k ));
        ml_FreeResult( temp );
    }
    vlc_array_destroy( p_intermediate_array );
    return i_ret;
}


/**
 * @brief Returns (unique) ID of media with specified URI
 *
 * @param p_ml the media library object
 * @param psz_uri URI to look for
 * @return i_id: (first) ID found, VLC_EGENERIC in case of error
 * NOTE: Normally, there should not be more than one ID with one URI
 */
int GetMediaIdOfURI( media_library_t *p_ml, const char *psz_uri )
{
    int i_ret = VLC_EGENERIC;
    vlc_array_t *p_array = vlc_array_new();
    i_ret = Find( p_ml, p_array, ML_ID, ML_URI, psz_uri, ML_LIMIT, 1, ML_END );
    if( ( i_ret == VLC_SUCCESS )
        && ( vlc_array_count( p_array ) > 0 )
        && vlc_array_item_at_index( p_array, 0 ) )
    {
        i_ret = ( (ml_result_t*)vlc_array_item_at_index( p_array, 0 ) )
                        ->value.i;
    }
    else
    {
        i_ret = VLC_EGENERIC;
    }
    vlc_array_destroy( p_array );
    return i_ret;
}


/**
 * @brief Control function for media library
 *
 * @param p_ml Media library handle
 * @param i_query query type
 * @param args query arguments
 * @return VLC_SUCCESS if ok
 */
int Control( media_library_t *p_ml, int i_query, va_list args )
{
    switch( i_query )
    {
    case ML_ADD_INPUT_ITEM:
    {
        input_item_t *p_item = (input_item_t *)va_arg( args, input_item_t * );
        return AddInputItem( p_ml, p_item );
    }

    case ML_ADD_PLAYLIST_ITEM:
    {
        playlist_item_t *p_item = (playlist_item_t *)va_arg( args, playlist_item_t * );
        return AddPlaylistItem( p_ml, p_item );
    }

    case ML_ADD_MONITORED:
    {
        char *psz_dir = (char *)va_arg( args, char * );
        return AddDirToMonitor( p_ml, psz_dir );
    }

    case ML_GET_MONITORED:
    {
        vlc_array_t *p_array = (vlc_array_t *)va_arg( args, vlc_array_t * );
        return ListMonitoredDirs( p_ml, p_array );
    }

    case ML_DEL_MONITORED:
    {
        char *psz_dir = (char *)va_arg( args, char * );
        return RemoveDirToMonitor( p_ml, psz_dir );
    }

    default:
        return VLC_EGENERIC;
    }
}


/**
 * @brief Create a new (empty) database. The database might be initialized
 *
 * @param p_ml This ML
 * @return VLC_SUCCESS or VLC_EGENERIC
 * @note This function is transactional
 */
int CreateEmptyDatabase( media_library_t *p_ml )
{
    assert( p_ml );
    int i_ret = VLC_SUCCESS;
    msg_Dbg( p_ml, "creating a new (empty) database" );

    Begin( p_ml );

    /* Albums */
    i_ret= QuerySimple( p_ml,
                        "CREATE TABLE album ( "
                        "id INTEGER PRIMARY KEY,"
                        "album_artist_id INTEGER,"
                        "title VARCHAR(1024),"
                        "cover VARCHAR(1024) )" );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* Add "unknown" entry to albums */
    i_ret = QuerySimple( p_ml,
                        "INSERT INTO album ( id, title, cover, album_artist_id ) "
                        "VALUES ( 0, 'Unknown', '', 0 )" );

    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* Main media table */
    i_ret= QuerySimple( p_ml,
                        "CREATE TABLE media ( "
                        "id INTEGER PRIMARY KEY,"
                        "timestamp INTEGER,"            /* File timestamp */
                        "uri VARCHAR(1024),"
                        "type INTEGER,"
                        "title VARCHAR(1024),"
                        "original_title VARCHAR(1024),"
                        "album_id INTEGER,"
                        "cover VARCHAR(1024),"
                        "preview VARCHAR(1024),"        /* Video preview */
                        "track INTEGER,"                /* Track number */
                        "disc INTEGER,"                 /* Disc number */
                        "year INTEGER,"
                        "genre VARCHAR(1024),"
                        "vote INTEGER,"                 /* Rating/Stars */
                        "score INTEGER,"                /* ML score/rating */
                        "comment VARCHAR(1024),"        /* Comment */
                        "filesize INTEGER,"
                        /* Dates and times */
                        "duration INTEGER,"             /* Length of media */
                        "played_count INTEGER,"
                        "last_played DATE,"
                        "first_played DATE,"
                        "import_time DATE,"
                        "skipped_count INTEGER,"
                        "last_skipped DATE,"
                        "directory_id INTEGER,"
                        "CONSTRAINT associated_album FOREIGN KEY(album_id) "
            "REFERENCES album(id) ON DELETE SET DEFAULT ON UPDATE RESTRICT)" );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* People */
    i_ret = QuerySimple( p_ml,
                        "CREATE TABLE people ( "
                        "id INTEGER PRIMARY KEY,"
                        "name VARCHAR(1024) ,"
                        "role VARCHAR(1024) )" );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* Media to people */
    i_ret = QuerySimple( p_ml,
                        "CREATE TABLE media_to_people ( "
                        "media_id INTEGER, "
                        "people_id INTEGER, "
                        "PRIMARY KEY( media_id, people_id ), "
                        "CONSTRAINT associated_people FOREIGN KEY(people_id) "
            "REFERENCES people(id) ON DELETE SET DEFAULT ON UPDATE RESTRICT, "
                        "CONSTRAINT associated_media FOREIGN KEY(media_id) "
            "REFERENCES media(id) ON DELETE CASCADE ON UPDATE RESTRICT )" );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* Add "unknown" entry to people */
    i_ret = QuerySimple( p_ml,
                        "INSERT INTO people ( id, name, role ) "
                        "VALUES ( 0, 'Unknown', NULL )" );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* recursive is set to 1 if the directory is added to the database
       by recursion and 0 if not */
    i_ret = QuerySimple( p_ml,
                        "CREATE TABLE directories ( "
                        "id INTEGER PRIMARY KEY,"
                        "uri VARCHAR(1024),"
                        "timestamp INTEGER,"
                        "recursive INTEGER )" );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* Create information table
     * This table should have one row and the version number is the version
     * of the database
     * Other information may be stored here at later stages */
    i_ret = QuerySimple( p_ml,
                        "CREATE TABLE information ( "
                        "version INTEGER PRIMARY KEY )" );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* Insert current DB version */
    i_ret = QuerySimple( p_ml,
                        "INSERT INTO information ( version ) "
                        "VALUES ( %d )", ML_DBVERSION );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* Text data: song lyrics or subtitles */
    i_ret = QuerySimple( p_ml,
                        "CREATE TABLE extra ( "
                        "id INTEGER PRIMARY KEY,"
                        "extra TEXT,"
                        "language VARCHAR(256),"
                        "bitrate INTEGER,"
                        "samplerate INTEGER,"
                        "bpm INTEGER )" );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;

    /* Emulating foreign keys with triggers */
    /* Warning: Lots of SQL */
    if( !strcmp( module_get_name( p_ml->p_sys->p_sql->p_module, false ),
        "SQLite" ) )
    {
    i_ret = QuerySimple( p_ml,
    "\nCREATE TRIGGER genfkey1_insert_referencing BEFORE INSERT ON \"media\" WHEN\n"
    "    new.\"album_id\" IS NOT NULL AND NOT EXISTS (SELECT 1 FROM \"album\" WHERE new.\"album_id\" == \"id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey1_insert_referencing failed. Cannot insert album_id into media. Album did not exist');\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER genfkey1_update_referencing BEFORE\n"
    "    UPDATE OF album_id ON \"media\" WHEN \n"
    "    new.\"album_id\" IS NOT NULL AND \n"
    "    NOT EXISTS (SELECT 1 FROM \"album\" WHERE new.\"album_id\" == \"id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey1_update_referencing failed. Cannot update album_id in media. Album did not exist');\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER genfkey1_delete_referenced BEFORE DELETE ON \"album\" WHEN\n"
    "    EXISTS (SELECT 1 FROM \"media\" WHERE old.\"id\" == \"album_id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey1_delete_referenced failed. Cannot delete album, media still exist');\n"
    "END;\n"
    "\n"
    "\n"
    "CREATE TRIGGER genfkey1_update_referenced AFTER\n"
    "    UPDATE OF id ON \"album\" WHEN \n"
    "    EXISTS (SELECT 1 FROM \"media\" WHERE old.\"id\" == \"album_id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey1_update_referenced failed. Cannot change album id in album, media still exist');\n"
    "END;\n"
    "\n"
    "\n"
    "CREATE TRIGGER genfkey2_insert_referencing BEFORE INSERT ON \"media_to_people\" WHEN \n"
    "    new.\"media_id\" IS NOT NULL AND NOT EXISTS (SELECT 1 FROM \"media\" WHERE new.\"media_id\" == \"id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey2_insert_referencing failed. Cannot insert into media_to_people, that media does not exist');\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER genfkey2_update_referencing BEFORE\n"
    "    UPDATE OF media_id ON \"media_to_people\" WHEN \n"
    "    new.\"media_id\" IS NOT NULL AND \n"
    "    NOT EXISTS (SELECT 1 FROM \"media\" WHERE new.\"media_id\" == \"id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey2_update_referencing failed. Cannot update media_to_people, that media does not exist');\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER genfkey2_delete_referenced BEFORE DELETE ON \"media\" WHEN\n"
    "    EXISTS (SELECT 1 FROM \"media_to_people\" WHERE old.\"id\" == \"media_id\")\n"
    "BEGIN\n"
    "  DELETE FROM \"media_to_people\" WHERE \"media_id\" = old.\"id\";\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER genfkey2_update_referenced AFTER\n"
    "    UPDATE OF id ON \"media\" WHEN \n"
    "    EXISTS (SELECT 1 FROM \"media_to_people\" WHERE old.\"id\" == \"media_id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey2_update_referenced failed. Cannot update media id, refs still exist in media_to_people');\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER genfkey3_insert_referencing BEFORE INSERT ON \"media_to_people\" WHEN \n"
    "    new.\"people_id\" IS NOT NULL AND NOT EXISTS (SELECT 1 FROM \"people\" WHERE new.\"people_id\" == \"id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey3_insert_referencing failed. Cannot insert into media_to_people, people does not exist');\n"
    "END;\n"
    "CREATE TRIGGER genfkey3_update_referencing BEFORE\n"
    "    UPDATE OF people_id ON \"media_to_people\" WHEN \n"
    "    new.\"people_id\" IS NOT NULL AND \n"
    "    NOT EXISTS (SELECT 1 FROM \"people\" WHERE new.\"people_id\" == \"id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey3_update_referencing failed. Cannot update media_to_people, people does not exist');\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER genfkey3_delete_referenced BEFORE DELETE ON \"people\" WHEN\n"
    "    EXISTS (SELECT 1 FROM \"media_to_people\" WHERE old.\"id\" == \"people_id\")\n"
    "BEGIN\n"
    "  UPDATE media_to_people SET people_id = 0 WHERE people_id == old.\"id\";\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER genfkey3_update_referenced AFTER\n"
    "    UPDATE OF id ON \"people\" WHEN \n"
    "    EXISTS (SELECT 1 FROM \"media_to_people\" WHERE old.\"id\" == \"people_id\")\n"
    "BEGIN\n"
    "  SELECT RAISE(ABORT, 'constraint genfkey3_update_referenced failed. Cannot update people_id, people does not exist');\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER keep_people_clean AFTER \n"
    "    DELETE ON \"media_to_people\"\n"
    "    WHEN NOT EXISTS( SELECT 1 from \"media_to_people\" WHERE old.\"people_id\" == \"people_id\" )\n"
    "BEGIN\n"
    "    DELETE FROM people WHERE people.id = old.\"people_id\" AND people.id != 0;\n"
    "END;\n"
    "\n"
    "CREATE TRIGGER keep_album_clean AFTER\n"
    "    DELETE ON \"media\"\n"
    "    WHEN NOT EXISTS( SELECT 1 FROM \"media\" WHERE old.\"album_id\" == \"album_id\" )\n"
    "BEGIN\n"
    "    DELETE FROM album WHERE album.id = old.\"album_id\" AND album.id != 0;\n"
    "END;" );
    if( i_ret != VLC_SUCCESS )
        goto quit_createemptydatabase;
    }

quit_createemptydatabase:
    if( i_ret == VLC_SUCCESS )
        Commit( p_ml );
    else
        Rollback( p_ml );
    return VLC_SUCCESS;
}


/**
 * @brief Initiates database (create the database and the tables if needed)
 *
 * @param p_ml This ML
 * @return VLC_SUCCESS or an error code
 */
int InitDatabase( media_library_t *p_ml )
{
    assert( p_ml );
    msg_Dbg( p_ml, "initializing database" );

    /* Select database name */
    char *psz_dbhost = NULL, *psz_user = NULL, *psz_pass = NULL;
    int i_port = 0;
    psz_dbhost = config_GetPsz( p_ml, "ml-filename" );
    psz_user = config_GetPsz( p_ml, "ml-username" );
    psz_pass = config_GetPsz( p_ml, "ml-password" );
    i_port = config_GetInt( p_ml, "ml-port" );

    /* Let's consider that a filename with a DIR_SEP is a full URL */
    if( strchr( psz_dbhost, DIR_SEP_CHAR ) == NULL )
    {
        char *psz_datadir = config_GetUserDir( VLC_DATA_DIR );
        char *psz_tmp = psz_dbhost;
        if( asprintf( &psz_dbhost, "%s" DIR_SEP "%s",
                      psz_datadir, psz_tmp ) == -1 )
        {
            free( psz_datadir );
            free( psz_tmp );
            return VLC_ENOMEM;
        }
        free( psz_datadir );
        free( psz_tmp );
    }

    p_ml->p_sys->p_sql = sql_Create( p_ml, NULL, psz_dbhost, i_port, psz_user,
                                     psz_pass );
    if( !p_ml->p_sys->p_sql )
    {
        vlc_mutex_destroy( &p_ml->p_sys->lock );
        free( p_ml->p_sys );
        return VLC_EGENERIC;
    }

    /* Let's check if tables exist */
    int i_version = GetDatabaseVersion( p_ml );
    if( i_version <= 0 )
        CreateEmptyDatabase( p_ml );
    else if( i_version != ML_DBVERSION )
        return VLC_EGENERIC;

    /**
     * The below code ensures that correct code is written
     * when database versions are changed
     */

#if ML_DBVERSION != 1
#error "ML versioning code needs to be updated. Is this done correctly?"
#endif

    msg_Dbg( p_ml, "ML initialized" );
    return VLC_SUCCESS;
}

/**
 * @brief Gets the current version number from the database
 *
 * @param p_ml media library object
 * @return version number of the current db. <= 0 on error.
 */
int GetDatabaseVersion( media_library_t *p_ml )
{
    int i_rows, i_cols;
    char **pp_results;
    int i_return;
    i_return = Query( p_ml, &pp_results, &i_rows, &i_cols,
        "SELECT version FROM information ORDER BY version DESC LIMIT 1" );
    if( i_return != VLC_SUCCESS )
        i_return = -1;
    else
        i_return = atoi( pp_results[ 1 ] );

    FreeSQLResult( p_ml, pp_results );

    return i_return;
}

 /**
 * @brief Object constructor for ml_media_t
 * @param p_ml The media library object
 * @param id If 0, this item isn't in database. If non zero, it is and
 * it will be a singleton
 * @param select Type of object
 * @param reload Whether to reload from database
 */
ml_media_t* GetMedia( media_library_t* p_ml, int id,
                        ml_select_e select, bool reload )
{
    assert( id > 0 );
    assert( select == ML_MEDIA || select == ML_MEDIA_SPARSE );
    int i_ret = VLC_SUCCESS;
    ml_media_t* p_media = NULL;
    if( !reload )
    {
        p_media = pool_GetMedia( p_ml, id );
        if( !p_media )
            reload = true;
        else
        {
            ml_LockMedia( p_media );
            if( p_media->b_sparse && select == ML_MEDIA )
                reload = true;
            /* Utilise ML_MEDIA_EXTRA load? TODO */
            ml_UnlockMedia( p_media );
            ml_gc_incref( p_media );
        }
    }
    else
    {
        vlc_array_t *p_array = vlc_array_new();
        i_ret = ml_Find( p_ml, p_array, select, ML_ID, id );
        assert( vlc_array_count( p_array ) == 1 );
        if( ( i_ret == VLC_SUCCESS )
            && ( vlc_array_count( p_array ) > 0 )
            && vlc_array_item_at_index( p_array, 0 ) )
        {
            p_media = ((ml_result_t*)vlc_array_item_at_index( p_array, 0 ))->value.p_media;
            ml_gc_incref( p_media );
            ml_FreeResult( vlc_array_item_at_index( p_array, 0 ) );
        }
        vlc_array_destroy( p_array );
        if( select == ML_MEDIA )
            p_media->b_sparse = false;
        else
            p_media->b_sparse = true;
    }
    return p_media;
}
/**
 * @brief Create an input item from media (given its ID)
 *
 * @param p_ml This media_library_t object
 * @param i_media Media ID
 * @return input_item_t* created
 *
 * @note This is a public function (pf_InputItemFromMedia)
 * The input_item will have a refcount at 2 (1 for the ML, 1 for you)
 */
input_item_t* GetInputItemFromMedia( media_library_t *p_ml, int i_media )
{
    input_item_t *p_item = NULL;

    p_item = watch_get_itemOfMediaId( p_ml, i_media );
    if( !p_item )
    {
        ml_media_t* p_media = media_New( p_ml, i_media, ML_MEDIA, true );
        if( p_media == NULL )
            return NULL;
        CreateInputItemFromMedia( &p_item, p_media );
        watch_add_Item( p_ml, p_item, p_media );
        ml_gc_decref( p_media );
    }

    return p_item;
}

/**
 * @brief Copy an input_item_t to a ml_media_t
 * @param p_media Destination
 * @param p_item Source
 * @note Media ID will not be set! This function is threadsafe. Leaves
 * unsyncable items alone
 */
void CopyInputItemToMedia( ml_media_t *p_media, input_item_t *p_item )
{
    ml_LockMedia( p_media );
#if 0
    // unused meta :
    input_item_GetCopyright( item )
    input_item_GetRating( item ) /* TODO */
    input_item_GetGetting( item )
    input_item_GetNowPlaying( item )
    input_item_GetTrackID( item )
    input_item_GetSetting( item )
#endif
    p_media->psz_title      = input_item_GetTitle       ( p_item );
    p_media->psz_uri        = input_item_GetURL         ( p_item );
    if( !p_media->psz_uri )
        p_media->psz_uri    = strdup( p_item->psz_uri );
    p_media->psz_album      = input_item_GetAlbum       ( p_item );
    p_media->psz_cover      = input_item_GetArtURL      ( p_item );
    p_media->psz_genre      = input_item_GetGenre       ( p_item );
    p_media->psz_language   = input_item_GetLanguage    ( p_item );
    p_media->psz_comment    = input_item_GetDescription ( p_item );
    char *psz_track         = input_item_GetTrackNum    ( p_item );
    p_media->i_track_number = psz_track ? atoi( psz_track ) : 0;
    free( psz_track );
    char *psz_date          = input_item_GetDate( p_item );
    p_media->i_year         = psz_date ? atoi( psz_date ) : 0;
    free( psz_date );
    p_media->i_duration     = p_item->i_duration;

    /* People */
    char *psz_tmp = input_item_GetArtist( p_item );
    if( psz_tmp )
        ml_CreateAppendPersonAdv( &p_media->p_people, ML_PERSON_ARTIST,
                                     psz_tmp, 0 );
    free( psz_tmp );
    psz_tmp = input_item_GetPublisher( p_item );
    if( psz_tmp )
        ml_CreateAppendPersonAdv( &p_media->p_people, ML_PERSON_PUBLISHER,
                                    psz_tmp, 0 );
    free( psz_tmp );
    psz_tmp = input_item_GetEncodedBy( p_item );
    if( psz_tmp )
        ml_CreateAppendPersonAdv( &p_media->p_people, ML_PERSON_ENCODER,
                                    psz_tmp, 0 );
    free( psz_tmp );

    /* Determine input type: audio, video, stream */
    /* First read input type */
    switch( p_item->i_type )
    {
    case ITEM_TYPE_FILE:
        p_media->i_type |= 0;
        break;
    case ITEM_TYPE_DISC:
    case ITEM_TYPE_CARD:
        p_media->i_type |= ML_REMOVABLE;
        break;
    case ITEM_TYPE_CDDA:
    case ITEM_TYPE_NET:
        p_media->i_type |= ML_STREAM;
        break;
    case ITEM_TYPE_PLAYLIST:
    case ITEM_TYPE_NODE:
    case ITEM_TYPE_DIRECTORY:
        p_media->i_type |= ML_NODE;
        break;
    case ITEM_TYPE_NUMBER:
    case ITEM_TYPE_UNKNOWN:
    default:
        p_media->i_type |= ML_UNKNOWN;
        break;
    }

    /* Then try to guess if this is a video or not */
    /* Check file extension, and guess if this is a video or an audio media
       Note: this test is not very good, but it's OK for normal files */
    char *psz_ext = strrchr( p_item->psz_uri, '.' );
    if( psz_ext && strlen( psz_ext ) < 5 )
    {
        bool b_ok = false;
        psz_ext++;
        for( unsigned i = 0; ppsz_AudioExtensions[i]; i++ )
        {
            if( strcasecmp( psz_ext, ppsz_AudioExtensions[i] ) == 0 )
            {
                p_media->i_type |= ML_AUDIO;
                b_ok = true;
                break;
            }
        }
        if( !b_ok )
        {
            for( unsigned i = 0; ppsz_VideoExtensions[i]; i++ )
            {
                if( strcasecmp( psz_ext, ppsz_VideoExtensions[i] ) == 0 )
                {
                    p_media->i_type |= ML_VIDEO;
                    break;
                }
            }
        }
    }
    ml_UnlockMedia( p_media );
}

/**
 * @brief Copy a ml_media_t to an input_item_t
 * @param p_item Destination
 * @param p_media Source
 */
void CopyMediaToInputItem( input_item_t *p_item, ml_media_t *p_media )
{
    ml_LockMedia( p_media );
    if( p_media->psz_title && *p_media->psz_title )
        input_item_SetTitle( p_item, p_media->psz_title );
    if( p_media->psz_uri && *p_media->psz_uri && !strncmp( p_media->psz_uri, "http", 4 ) )
        input_item_SetURL( p_item, p_media->psz_uri );
    if( p_media->psz_album && *p_media->psz_album )
        input_item_SetAlbum( p_item, p_media->psz_album );
    if( p_media->psz_cover && *p_media->psz_cover )
        input_item_SetArtURL( p_item, p_media->psz_cover );
    if( p_media->psz_genre && *p_media->psz_genre )
        input_item_SetGenre( p_item, p_media->psz_genre );
    if( p_media->psz_language && *p_media->psz_language )
        input_item_SetLanguage( p_item, p_media->psz_language );
    if( p_media->psz_comment && *p_media->psz_comment )
        input_item_SetDescription( p_item, p_media->psz_comment );
    if( p_media->i_track_number )
    {
        char *psz_track;
        if( asprintf( &psz_track, "%d", p_media->i_track_number ) != -1 )
            input_item_SetTrackNum( p_item, psz_track );
        free( psz_track );
    }
    if( p_media->i_year )
    {
        char *psz_date;
        if( asprintf( &psz_date, "%d", p_media->i_year ) != -1 )
            input_item_SetDate( p_item, psz_date );
        free( psz_date );
    }
    p_item->i_duration = p_media->i_duration;
    ml_person_t *person = p_media->p_people;
    while( person )
    {
        if( !strcmp( person->psz_role, ML_PERSON_ARTIST ) )
            input_item_SetArtist( p_item, person->psz_name );
        else if( !strcmp( person->psz_role, ML_PERSON_PUBLISHER ) )
            input_item_SetPublisher( p_item, person->psz_name );
        else if( !strcmp( person->psz_role, ML_PERSON_ENCODER ) )
            input_item_SetEncodedBy( p_item, person->psz_name );
        person = person->p_next;
    }
    ml_UnlockMedia( p_media );
}

/**
 * @brief Copy a ml_media_t to an input_item_t
 * @param pp_item A pointer to a new input_item (return value)
 * @param p_media The media to copy as an input item
 * @note This function is threadsafe
 */
static int CreateInputItemFromMedia( input_item_t **pp_item,
                                     ml_media_t *p_media )
{
    *pp_item = input_item_New( p_media->psz_uri, p_media->psz_title );
                               /* ITEM_TYPE_FILE ); */
    if( !*pp_item )
        return VLC_EGENERIC;
    CopyMediaToInputItem( *pp_item, p_media );
    return VLC_SUCCESS;
}

/**
 * @brief Find the media_id associated to an input item
 * @param p_ml This
 * @param p_item Input item to look for
 * @return Media ID or <= 0 if not found
 */
int GetMediaIdOfInputItem( media_library_t *p_ml, input_item_t *p_item )
{
    int i_media_id = watch_get_mediaIdOfItem( p_ml, p_item );
    if( i_media_id <= 0 )
    {
        i_media_id = GetMediaIdOfURI( p_ml, p_item->psz_uri );
    }
    return i_media_id;
}


