/*****************************************************************************
 * sql_search.c: SQL-based media library: all find/get functions
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN team and AUTHORS
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

int Find( media_library_t *p_ml, vlc_array_t *p_result_array, ... )
{
    va_list args;
    int returned;

    va_start( args, p_result_array );
    returned = FindVa( p_ml, p_result_array, args );
    va_end( args );

    return returned;
}

/**
 * @brief Generic find in Media Library, returns arrays of psz or int
 *
 * @param p_ml the media library object
 * @param result A pointer to a result array
 * @param criterias list of criterias used in SELECT
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int FindVa( media_library_t *p_ml,
            vlc_array_t *p_result_array, va_list criterias )
{
    int i_ret = VLC_SUCCESS;
    char *psz_query;
    ml_result_type_e result_type;
    char **pp_results = NULL;
    int i_cols, i_rows;

    if( !p_result_array )
        return VLC_EGENERIC;

    i_ret = BuildSelectVa( p_ml, &psz_query, &result_type, criterias );
    if( i_ret != VLC_SUCCESS )
        return i_ret;

    if( Query( p_ml, &pp_results, &i_rows, &i_cols, "%s", psz_query )
        != VLC_SUCCESS )
    {
        msg_Err( p_ml, "Error occurred while making the query to the database" );
        return VLC_EGENERIC;
    }

    i_ret = SQLToResultArray( p_ml, p_result_array, pp_results, i_rows, i_cols,
                              result_type );

    free( psz_query);
    FreeSQLResult( p_ml, pp_results );

    return i_ret;
}

/**
 * @brief Generic find in Media Library, returns arrays of psz or int
 *
 * @param p_ml the media library object
 * @param result a pointer to a result array
 * @param selected_type the type of the element we're selecting
 * @param criterias list of criterias used in SELECT
 * @return VLC_SUCCESS or VLC_EGENERIC
 */

int FindAdv( media_library_t *p_ml, vlc_array_t *p_result_array,
             ml_select_e selected_type, const char* psz_lvalue, ml_ftree_t *tree )
{
    int i_ret = VLC_SUCCESS;
    char *psz_query;
    ml_result_type_e result_type;
    char **pp_results = NULL;
    int i_cols, i_rows;

    if( !p_result_array )
        return VLC_EGENERIC;

    i_ret = BuildSelect( p_ml, &psz_query, &result_type, psz_lvalue,
                         selected_type, tree );

    if( i_ret != VLC_SUCCESS )
        return i_ret;

    if( Query( p_ml, &pp_results, &i_rows, &i_cols, "%s", psz_query )
        != VLC_SUCCESS )
    {
        msg_Err( p_ml, "Error occurred while making the query to the database" );
        return VLC_EGENERIC;
    }

    i_ret = SQLToResultArray( p_ml, p_result_array, pp_results, i_rows, i_cols,
                              result_type );

    free( psz_query);
    FreeSQLResult( p_ml, pp_results );

    return i_ret;
}

/**
 * @brief Generic SELECT query builder with va_list parameter
 *
 * @param p_ml This media_library_t object
 * @param ppsz_query *ppsz_query will contain query
 * @param p_result_type see enum ml_result_type_e
 * @param criterias list of criterias used in SELECT
 * @return VLC_SUCCESS or a VLC error code
 * NOTE va_list criterias must end with ML_END or this will fail (segfault)
 *
 * This function handles results of only one column (or two if ID is included),
 * of 'normal' types: int and strings
 */
int BuildSelectVa( media_library_t *p_ml, char **ppsz_query,
                   ml_result_type_e *p_result_type, va_list criterias )
{
    int i_continue = 1;
    ml_ftree_t* p_ftree = NULL;
    char* psz_lvalue = NULL;

    /* Get the name of the data we want */
    ml_select_e selected_type = va_arg( criterias, int );
    if( selected_type == ML_PEOPLE || selected_type == ML_PEOPLE_ID ||
            selected_type == ML_PEOPLE_ROLE )
        psz_lvalue = va_arg( criterias, char * );

    /* Loop on every arguments */
    while( i_continue )
    {
        ml_ftree_t *p_find = ( ml_ftree_t* ) calloc( 1, sizeof( ml_ftree_t ) );
        if( !p_find )
            return VLC_ENOMEM;
        p_find->criteria = va_arg( criterias, int );
        p_find->comp = ML_COMP_EQUAL;
        switch( p_find->criteria )
        {
            case ML_SORT_ASC:
                p_ftree = ml_FtreeSpecAsc( p_ftree, va_arg( criterias, char* ) ); break;
            case ML_SORT_DESC:
                p_ftree = ml_FtreeSpecDesc( p_ftree, va_arg( criterias, char* ) ); break;
            case ML_DISTINCT:
                p_ftree = ml_FtreeSpecDistinct( p_ftree ); break;
            case ML_LIMIT:
                p_ftree = ml_FtreeSpecLimit( p_ftree, va_arg( criterias, int ) );
                break;
            case ML_ARTIST:
                /* This is OK because of a shallow free find */
                p_find->lvalue.str = (char *)ML_PERSON_ARTIST;
                p_find->value.str = va_arg( criterias, char* );
                p_ftree = ml_FtreeFastAnd( p_ftree, p_find );
                break;
            case ML_PEOPLE:
                p_find->lvalue.str = va_arg( criterias, char* );
                p_find->value.str = va_arg( criterias, char* );
                p_ftree = ml_FtreeFastAnd( p_ftree, p_find );
                break;
            case ML_PEOPLE_ID:
                p_find->lvalue.str = va_arg( criterias, char* );
                p_find->value.i = va_arg( criterias, int );
                p_ftree = ml_FtreeFastAnd( p_ftree, p_find );
                break;
            case ML_END:
                i_continue = 0;
                break;
            default:
                switch( ml_AttributeIsString( p_find->criteria ) )
                {
                    case 0:
                        p_find->value.i = va_arg( criterias, int );
                        break;
                    case 1:
                        p_find->value.str = va_arg( criterias, char* );
                        break;
                }
                p_ftree = ml_FtreeFastAnd( p_ftree, p_find );
                break;
        }
    }

    int i_ret = BuildSelect( p_ml, ppsz_query, p_result_type, psz_lvalue,
                             selected_type, p_ftree );

    ml_ShallowFreeFindTree( p_ftree );
    return i_ret;
}

/**
 * @brief Append a string and format it using SQL vmprintf
 **/
static int AppendStringFmtVa( media_library_t *p_ml,
                              char **ppsz_dst, const char *psz_fmt,
                              va_list args )
{
    char *psz_tmp = NULL, *psz_fullexp = NULL;

    assert( ppsz_dst != NULL );

    if( !( *ppsz_dst ) )
    {
        /* New expression */
        *ppsz_dst = sql_VPrintf( p_ml->p_sys->p_sql, psz_fmt, args );
        if( !( *ppsz_dst ) )
            return VLC_ENOMEM;
    }
    else
    {
        /* Create new expression B */
        psz_tmp = sql_VPrintf( p_ml->p_sys->p_sql, psz_fmt, args );
        if( !psz_tmp )
            return VLC_ENOMEM;

        if( asprintf( &psz_fullexp, "%s%s", *ppsz_dst, psz_tmp ) == -1 )
        {
            free( psz_tmp );
            return VLC_ENOMEM;
        }

        free( *ppsz_dst );
        *ppsz_dst = psz_fullexp;
    }

    return VLC_SUCCESS;
}

static int AppendStringFmt( media_library_t *p_ml,
                            char **ppsz_dst, const char *psz_fmt, ... )
{
    va_list args;
    va_start( args, psz_fmt );
    int i_ret = AppendStringFmtVa( p_ml, ppsz_dst, psz_fmt, args );
    va_end( args );
    return i_ret;
}

/* Early Declaration of Where String Generator */
static int BuildWhere( media_library_t* p_ml, char **ppsz_where, ml_ftree_t* tree,
       char** sort, int* limit, const char** distinct, char*** pppsz_frompersons,
       int* i_frompersons, int* join );

#   define table_media             (1 << 0)
#   define table_album             (1 << 1)
#   define table_people            (1 << 2)
#   define table_extra             (1 << 3)

static void PackFromPersons( char*** pppsz_frompersons, int i_num_frompersons )
{
    for( int i = 0; i < i_num_frompersons; i++ )
    {
        if( *pppsz_frompersons[i] == NULL )
            continue;
        for( int j = i+1; j < i_num_frompersons; j++ )
        {
            if( strcmp( *pppsz_frompersons[i], *pppsz_frompersons[j] ) == 0 )
            {
                *pppsz_frompersons[j] = NULL;
            }
        }
    }
}
/**
 * @brief Generic SELECT query builder
 *
 * @param p_ml This media_library_t object
 * @param ppsz_query *ppsz_query will contain query
 * @param p_result_type see enum ml_result_type_e
 * @param selected_type the type of the element we're selecting
 * @param tree the find tree
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int BuildSelect( media_library_t *p_ml,
                 char **ppsz_query, ml_result_type_e *p_result_type,
                 const char *psz_selected_type_lvalue, ml_select_e selected_type,
                 ml_ftree_t *tree )
{
    /* Basic verification */
    if( !ppsz_query )
        return VLC_EGENERIC;

    int i_ret = VLC_SUCCESS;
    char *psz_query = NULL;

    /* Building psz_query :
    psz_query = "SELECT psz_distinct psz_select
                 FROM psz_from [JOIN psz_join ON psz_on]
                 [JOIN psz_join2 ON psz_on2]
                 [WHERE psz_where[i] [AND psz_where[j] ...]]
                 [LIMIT psz_limit] [ORDER BY psz_select psz_sort] ;"
    */
    char *psz_select             = NULL;
    const char *psz_distinct     = ""; /* "DISTINCT" or "" */

    /* FROM */
    char *psz_from               = NULL;
    int i_from                   = 0; /* Main select table */

    char **ppsz_frompersons      = NULL;
    int  i_num_frompersons       = 0;
    char *psz_peoplerole         = NULL; /* Person to get selected */

    /* JOIN ... ON ... */
    char *psz_join               = NULL;
    char *psz_join2              = NULL;
    char *psz_on                 = NULL;
    char *psz_on2                = NULL;
    int i_join                   = 0;    /* Tables that need to be joined */

    /* String buffers */
    char *psz_where              = NULL;
    char *psz_sort               = NULL; /* ASC or DESC or NULL */
    char *psz_tmp                = NULL;

    int i_limit                  = 0;

    /* Build the WHERE condition */
    BuildWhere( p_ml, &psz_where, tree, &psz_sort, &i_limit,
            &psz_distinct, &ppsz_frompersons, &i_num_frompersons, &i_join );

    PackFromPersons( &ppsz_frompersons, i_num_frompersons );

    /* What is the result type? */
    ml_result_type_e res_type   = ML_TYPE_PSZ;

    /* SELECT, FROM */
    /* Note that a DISTINCT select makes id of result non sense */
    switch( selected_type )
    {
    case ML_ALBUM:
        psz_select   = ( !*psz_distinct ) ?
                        strdup( "album.id, album.title AS album_title" )
                        : strdup( "album.title AS album_title" );
        i_from       = table_album;
        break;
    case ML_ALBUM_COVER:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "album.id, album.cover" ) : strdup( "album.cover" );
        i_from       = table_album;
        break;
    case ML_ALBUM_ID:
        psz_select   = strdup( "album.id" );
        psz_distinct = "DISTINCT";
        i_from       = table_album;
        res_type     = ML_TYPE_INT;
        break;
    case ML_ARTIST:
        psz_select   = ( !*psz_distinct ) ?
                        strdup( "people_Artist.id, people_Artist.name" )
                        : strdup( "people_Artist.name" );
        i_from       = table_people;
        psz_peoplerole = strdup( ML_PERSON_ARTIST );
        break;
    case ML_ARTIST_ID:
        psz_select   = strdup( "people_Artist.id" );
        psz_distinct = "DISTINCT";
        i_from       = table_people;
        res_type     = ML_TYPE_INT;
        psz_peoplerole = strdup( ML_PERSON_ARTIST );
        break;
    case ML_COVER:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "media.id, media.cover" ) : strdup( "media.cover" );
        i_from       = table_media;
        break;
    case ML_COMMENT:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "media.id, extra.comment" ) : strdup( "extra.comment" );
        i_from       = table_extra;
        break;
    case ML_GENRE:
        psz_select   = ( !*psz_distinct ) ?
               strdup( "media.id, media.genre" ) : strdup( "media.genre" );
        i_from       = table_media;
        break;
    case ML_COUNT_MEDIA:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "COUNT()" ) : strdup( "COUNT( DISTINCT media.id )" );
        i_from       = table_media;
        res_type     = ML_TYPE_INT;
        break;
    case ML_COUNT_ALBUM:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "COUNT()" ) : strdup( "COUNT( DISTINCT album.id )" );
        i_from       = table_album;
        res_type     = ML_TYPE_INT;
        break;
    case ML_COUNT_PEOPLE:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "COUNT()" ) : strdup( "COUNT( DISTINCT people.id )" );
        i_from       = table_people;
        res_type     = ML_TYPE_INT;
        break;
    case ML_FILESIZE:
        psz_select   = strdup( "media.filesize" );
        i_from       = table_media;
        res_type     = ML_TYPE_INT;
        break;
    case ML_ID:
        psz_select   = strdup( "media.id" ); /* ID: must be distinct */
        psz_distinct = "DISTINCT";
        i_from       = table_media;
        res_type     = ML_TYPE_INT;
        break;
    case ML_LANGUAGE:
        psz_select   = strdup( "extra.language" );
        psz_distinct = "DISTINCT";
        i_from       = table_extra;
        break;
    case ML_MEDIA_SPARSE:
        i_ret = AppendStringFmt( p_ml, &psz_select, "media.id AS id,"
                "media.uri AS uri,"
                "media.type AS type,"
                "media.title AS title,"
                "media.duration AS duration,"
                "media.original_title AS original_title,"
                "media.album_id AS album_id,"
                "media.cover AS cover,"
                "media.preview AS preview,"
                "media.disc AS disc,"
                "media.track AS track,"
                "media.year AS year,"
                "media.genre AS genre,"
                "media.played_count AS played_count,"
                "media.last_played AS last_played,"
                "media.first_played AS first_played,"
                "media.import_time AS import_time,"
                "media.skipped_count AS skipped_count,"
                "media.last_skipped AS last_skipped,"
                "media.vote AS vote,"
                "media.score AS score,"
                "media.comment AS comment,"
                "media.filesize AS filesize,"
                "album.title AS album_title,"
                "album.cover AS album_cover,"
        "(SELECT name FROM media_to_people JOIN people "
        "ON (people_id = id) WHERE media_id = media.id AND role = %Q LIMIT 1) AS people_%s",
        ML_PERSON_ARTIST, ML_PERSON_ARTIST );
        if( i_ret != VLC_SUCCESS )
            goto exit;
        i_from       = table_media;
        i_join      |= ( table_album | table_people );
        psz_distinct = "DISTINCT";
        res_type     = ML_TYPE_MEDIA;
        break;
    case ML_MEDIA:
        /* Who said this was over-complicated ?? */
        /* Yea right. */
        psz_select   = strdup( "media.id AS id,"
                "media.uri AS uri,"
                "media.type AS type,"
                "media.title AS title,"
                "media.duration AS duration,"
                "media.original_title AS original_title,"
                "media.album_id AS album_id,"
                "media.cover AS cover,"
                "media.preview AS preview,"
                "media.disc as disc,"
                "media.track AS track,"
                "media.year AS year,"
                "media.genre AS genre,"
                "media.played_count AS played_count,"
                "media.last_played AS last_played,"
                "media.first_played AS first_played,"
                "media.import_time AS import_time,"
                "media.last_skipped AS last_skipped,"
                "media.skipped_count AS skipped_count,"
                "media.vote AS vote,"
                "media.score AS score,"
                "media.comment AS comment,"
                "media.filesize AS filesize,"
                "album.title AS album_title,"
                "album.cover AS album_cover,"
                "people.id AS people_id,"
                "people.name AS people_name,"
                "people.role AS people_role,"
                "extra.language AS language,"
                "extra.extra AS extra" );
        i_from       = table_media;
        i_join      |= ( table_album | table_people | table_extra );
        psz_distinct = "DISTINCT";
        res_type     = ML_TYPE_MEDIA;
        break;
    case ML_MEDIA_EXTRA:
        psz_select = strdup( "media.id AS id,"
                "people.id AS people_id,"
                "people.name AS people_name,"
                "people.role AS people_role,"
                "extra.extra AS extra,"
                "extra.language AS language" );
        i_from       = table_media;
        i_join      |= ( table_album | table_people | table_extra );
        psz_distinct = "DISTINCT";
        res_type     = ML_TYPE_MEDIA;
        break;
    case ML_ORIGINAL_TITLE:
        psz_select   = ( !*psz_distinct ) ?
               strdup( "media.id, media.original_title" ) : strdup( "media.original_title" );
        i_from       = table_media;
        break;
    /* For people, if lvalue = "", then we want ANY people. */
    case ML_PEOPLE:
        assert( psz_selected_type_lvalue );
        i_ret = AppendStringFmt( p_ml, &psz_select, "people%s%s.name",
           *psz_selected_type_lvalue ? "_" : "",
           *psz_selected_type_lvalue ? psz_selected_type_lvalue : "" );
        if( i_ret != VLC_SUCCESS )
            goto exit;
        if( *psz_distinct )
        {
            i_ret = AppendStringFmt( p_ml, &psz_select, ", people%s%s.name",
               *psz_selected_type_lvalue ? "_" : "",
               *psz_selected_type_lvalue ? psz_selected_type_lvalue : "" );
            if( i_ret != VLC_SUCCESS )
                goto exit;
        }
        i_from         = table_people;
        psz_peoplerole = strdup( psz_selected_type_lvalue );
        break;
    case ML_PEOPLE_ID:
        assert( psz_selected_type_lvalue );
        i_ret = AppendStringFmt( p_ml, &psz_select, "people%s%s.id",
           *psz_selected_type_lvalue ? "_" : "",
           *psz_selected_type_lvalue ? psz_selected_type_lvalue : "" );
        if( i_ret != VLC_SUCCESS )
            goto exit;
        if( *psz_distinct )
        {
            i_ret = AppendStringFmt( p_ml, &psz_select, ", people%s%s.id",
               *psz_selected_type_lvalue ? "_" : "",
               *psz_selected_type_lvalue ? psz_selected_type_lvalue : "" );
            if( i_ret != VLC_SUCCESS )
                goto exit;
        }
        psz_distinct   = "DISTINCT";
        i_from         = table_people;
        psz_peoplerole = strdup( psz_selected_type_lvalue );
        res_type       = ML_TYPE_INT;
        break;
    case ML_PEOPLE_ROLE:
        psz_select   = strdup( "people.role" );
        psz_distinct = "DISTINCT";
        i_from       = table_people;
        break;
    case ML_TITLE:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "media.id, media.title" ) : strdup( "media.title" );
        i_from       = table_media;
        break;
    case ML_TYPE:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "media.id, media.type" ): strdup( "media.type" );
        i_from       = table_media;
        res_type     = ML_TYPE_INT;
        break;
    case ML_URI:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "media.id, media.uri" ) : strdup( "media.uri" );
        i_from       = table_media;
        break;
    case ML_VOTE:
        psz_select   = ( !*psz_distinct ) ?
                strdup( "media.id, media.vote" ) : strdup( "media.vote" );
        i_from       = table_media;
        res_type     = ML_TYPE_INT;
        break;
    case ML_YEAR:
        psz_select   = ( !*psz_distinct ) ?
               strdup( "media.id, media.year" ) : strdup( "media.year" );
        i_from       = table_media;
        res_type     = ML_TYPE_INT;
        break;
    case ML_LIMIT:
    case ML_SORT_DESC:
    case ML_SORT_ASC:
    case ML_END:
    default:
        msg_Dbg( p_ml, "unknown select (%d) in BuildSelect", selected_type );
        return VLC_EGENERIC;
    }

    /* Let's build full psz_query ! */
    i_ret = VLC_SUCCESS;

    /* Figure out select and join tables */
    switch( i_from )
    {
        case table_media:
            break;
        case table_album:
            switch( i_join )
            {
                case 0: break;
                case 2: i_join = 0; break;
                case 1:
                case 3: i_from = table_media; i_join = table_album; break;
                case 4:
                case 5:
                case 6:
                case 7: i_from = table_media; i_join = table_album | table_people; break;
                case 8:
                case 9:
                case 10:
                case 11: i_from = table_media; i_join = table_extra | table_album; break;
                case 12:
                case 13:
                case 14:
                case 15: i_from = table_media; i_join = table_extra | table_album | table_people; break;
                default: break;
            }
            break;
        case table_people:
            switch( i_join )
            {
                case 0: break;
                case 1: i_from = table_media; i_join = table_people; break;
                case 2:
                case 3: i_from = table_media; i_join = table_album | table_people; break;
                case 4:
                    /* Determine if a join from media is required */
                    if( i_num_frompersons > 1 )
                        i_from = table_media;
                    else
                        i_join = 0;
                    break;
                case 5: i_from = table_media; i_join = table_people; break;
                case 6:
                case 7: i_from = table_media; i_join = table_album | table_people; break;
                case 8:
                case 9: i_from = table_media; i_join = table_people | table_extra; break;
                case 10:
                case 11: i_from = table_media; i_join = table_people | table_album | table_extra; break;
                case 12:
                case 13: i_from = table_media; i_join = table_people | table_extra; break;
                case 14:
                case 15: i_from = table_media; i_join = table_people | table_album | table_extra; break;
                default: break;
            }
            break;
        case table_extra:
            switch( i_join )
            {
                case 0: break;
                case 1: i_from = table_media; i_join = table_extra; break;
                case 2:
                case 3: i_from = table_media; i_join = table_extra | table_album; break;
                case 4:
                case 5: i_from = table_media; i_join = table_extra | table_people; break;
                case 6:
                case 7: i_from = table_media; i_join = table_extra | table_people | table_album; break;
                case 8: i_from = table_extra; i_join = 0; break;
                case 9: i_from = table_media; i_join = table_extra; break;
                case 10:
                case 11: i_from = table_media; i_join = table_extra | table_album; break;
                case 12:
                case 13: i_from = table_media; i_join = table_extra | table_people; break;
                case 14:
                case 15: i_from = table_media; i_join = table_extra | table_people | table_album; break;
                default: break;
            }
            break;
        default: msg_Warn( p_ml, "You can't be selecting from this table!!" );
                 i_ret = VLC_EGENERIC;
                 goto exit;
    }

    assert( !( i_from & table_album && i_join & table_album ) );
    assert( !( i_from & table_people && i_join & table_people ) );
    assert( !( i_from & table_extra && i_join & table_extra ) );

    /* Generate FROM - psz_from */
    if( i_from == table_media )
        i_ret = AppendStringFmt( p_ml, &psz_from, "media" );
    else if( i_from == table_album )
        i_ret = AppendStringFmt( p_ml, &psz_from, "album" );
    else if( i_from == table_extra )
        i_ret = AppendStringFmt( p_ml, &psz_from, "extra" );
    else if( i_from == table_people )
    {
        i_ret = AppendStringFmt( p_ml, &psz_from, "people AS people%s%s",
                psz_peoplerole ? "_" : "", psz_peoplerole );
        if( i_ret < 0 ) goto exit;

        /* The ugly next statement is only required if persons are being
         * selected. Otherwise the joins will handle this */
        if( psz_peoplerole && *psz_peoplerole )
        {
            i_ret = AppendStringFmt( p_ml, &psz_where, "%s people_%s.role = %Q ",
                                     ( psz_where && *psz_where ) ? " AND" : "",
                                     psz_peoplerole, psz_peoplerole );
            if( i_ret < 0 ) goto exit;
        }
    }
    if( i_ret < 0 ) goto exit;

    i_ret = AppendStringFmt( p_ml, &psz_query,
                             "SELECT %s %s ", psz_distinct, psz_select );
    if( i_ret < 0 ) goto exit;

    i_ret = AppendStringFmt( p_ml, &psz_query, "FROM %s ", psz_from );
    if( i_ret < 0 ) goto exit;

    /* Create join conditions */
    if( i_join & table_people )
    {
        /* we can join psz_peoplerole safely because
         * if i_join = people, then i_from != people */
        bool join = true;
        for( int i = 0; i < i_num_frompersons ; i++ )
        {
            /* We assume ppsz_frompersons has unique entries and
             * if ppsz_frompersons[i] is empty(but not NULL), then it
             * means we accept any role */
            if( ppsz_frompersons[i] && *ppsz_frompersons[i] )
            {
                if( strcmp( psz_peoplerole, ppsz_frompersons[i] ) == 0 )
                    join = false;
                AppendStringFmt( p_ml, &psz_join, "%smedia_to_people AS people_%sx ",
                        psz_join == NULL ? "" : ",", ppsz_frompersons[i] );
                /* This is possible because from is usually the media table */
                AppendStringFmt( p_ml, &psz_on, "%speople_%sx.media_id = media.id ",
                        psz_on == NULL ? "" : " AND ", ppsz_frompersons[i] );
                AppendStringFmt( p_ml, &psz_join2, "%speople AS people_%s ",
                        psz_join2 == NULL ? "" : ",", ppsz_frompersons[i] );
                AppendStringFmt( p_ml, &psz_on2, "%s ( people_%sx.people_id = people_%s.id AND "
                        "people_%s.role = %Q )", psz_on2 == NULL ? "" : " AND ",
                        ppsz_frompersons[i], ppsz_frompersons[i],
                        ppsz_frompersons[i], ppsz_frompersons[i]  );
            }
            else if( ppsz_frompersons[i] )
            {
                if( strcmp( psz_peoplerole, ppsz_frompersons[i] ) == 0 )
                    join = false;
                AppendStringFmt( p_ml, &psz_join, "%smedia_to_people AS peoplex ",
                        psz_join == NULL ? "" : "," );
                /* This is possible because from is usually the media table */
                AppendStringFmt( p_ml, &psz_on, "%speoplex.media_id = media.id ",
                        psz_on == NULL ? "" : " AND " );
                AppendStringFmt( p_ml, &psz_join2, "%speople AS people ",
                        psz_join2 == NULL ? "" : "," );
                AppendStringFmt( p_ml, &psz_on2, "%s peoplex.people_id = people.id",
                        psz_on2 == NULL ? "" : " AND " );
            }
        }
        if( join )
        {
            if( psz_peoplerole && *psz_peoplerole )
            {
                AppendStringFmt( p_ml, &psz_join, "%smedia_to_people AS people_%sx ",
                        psz_join == NULL ? "" : ",", psz_peoplerole );
                /* This is possible because from is always the media table */
                AppendStringFmt( p_ml, &psz_on, "%speople_%sx.media_id = media.id ",
                        psz_on == NULL ? "" : " AND ", psz_peoplerole );
                AppendStringFmt( p_ml, &psz_join2, "%speople AS people_%s ",
                        psz_join2 == NULL ? "" : ",", psz_peoplerole );
                AppendStringFmt( p_ml, &psz_on2, "%s ( people_%sx.people_id = people_%s.id AND "
                        "people_%s.role = %Q )", psz_on2 == NULL ? "" : " AND ",
                        psz_peoplerole, psz_peoplerole,
                        psz_peoplerole, psz_peoplerole );
            }
            else
            {
                AppendStringFmt( p_ml, &psz_join, "%smedia_to_people AS peoplex ",
                        psz_join == NULL ? "" : "," );
                /* This is possible because from is usually the media table */
                AppendStringFmt( p_ml, &psz_on, "%speoplex.media_id = media.id ",
                        psz_on == NULL ? "" : " AND " );
                AppendStringFmt( p_ml, &psz_join2, "%speople ",
                        psz_join2 == NULL ? "" : "," );
                AppendStringFmt( p_ml, &psz_on2, "%s peoplex.people_id = people.id",
                        psz_on2 == NULL ? "" : " AND " );

            }
        }
    }
    if( i_join & table_album )
    {
        AppendStringFmt( p_ml, &psz_join, "%salbum", psz_join == NULL ? "" : "," );
        AppendStringFmt( p_ml, &psz_on, "%s album.id = media.album_id ",
                psz_on == NULL ? "" : " AND " );
    }
    if( i_join & table_extra )
    {
        AppendStringFmt( p_ml, &psz_join, "%sextra", psz_join == NULL ? "" : "," );
        AppendStringFmt( p_ml, &psz_on, "%s extra.id = media.id ",
                psz_on == NULL ? "" : " AND " );
    }

    /* Complete the join clauses */
    if( psz_join )
    {
        AppendStringFmt( p_ml, &psz_query,
                         "JOIN %s ON %s ", psz_join, psz_on );
    }
    if( psz_join2 )
    {
        AppendStringFmt( p_ml, &psz_query,
                         "JOIN %s ON %s ", psz_join2, psz_on2 );
    }
    if( psz_where && *psz_where )
    {
        AppendStringFmt( p_ml, &psz_query,
                         "WHERE %s ", psz_where );
    }
    /* TODO: FIXME: Limit on media objects doesn't work! */
    if( i_limit )
    {
        AppendStringFmt( p_ml, &psz_query,
                         "LIMIT %d ", i_limit );
    }

    if( psz_sort )
    {
        AppendStringFmt( p_ml, &psz_query,
                         "ORDER BY %s %s", psz_select, psz_sort );
    }

    if( i_ret > 0 ) i_ret = VLC_SUCCESS;

    if( p_result_type ) *p_result_type = res_type;
    if( !psz_query )    i_ret = VLC_EGENERIC;
    else                *ppsz_query = strdup( psz_query );

exit:
    free( psz_query );
    free( psz_where );
    free( psz_tmp   );
    free( psz_from  );
    free( psz_join  );
    free( psz_select );
    free( psz_join2 );
    free( psz_on    );
    free( psz_on2   );
    free( psz_peoplerole );
    free( ppsz_frompersons );

    if( i_ret != VLC_SUCCESS )
        msg_Warn( p_ml, "an unknown error occurred (%d)", i_ret );

    return i_ret;
}

#undef CASE_INT
#define CASE_INT( casestr, fmt, table )                                     \
case casestr:                                                               \
assert( tree->comp != ML_COMP_HAS && tree->comp != ML_COMP_STARTS_WITH      \
        && tree->comp != ML_COMP_ENDS_WITH );                               \
*ppsz_where = sql_Printf( p_ml->p_sys->p_sql, "%s %s %d", fmt,              \
    tree->comp == ML_COMP_LESSER ? "<" :                                    \
    tree->comp == ML_COMP_LESSER_OR_EQUAL ? "<=" :                          \
    tree->comp == ML_COMP_GREATER ? ">" :                                   \
    tree->comp == ML_COMP_GREATER_OR_EQUAL ? ">=" : "=", tree->value.i );   \
if( *ppsz_where == NULL )                                                   \
    goto parsefail;                                                         \
*join |= table;                                                             \
break

#undef CASE_PSZ
#define CASE_PSZ( casestr, fmt, table )                                       \
case casestr:                                                                 \
    assert( tree->comp == ML_COMP_HAS || tree->comp == ML_COMP_EQUAL          \
        || tree->comp == ML_COMP_STARTS_WITH                                  \
        || tree->comp == ML_COMP_ENDS_WITH );                                 \
    *ppsz_where = sql_Printf( p_ml->p_sys->p_sql, "%s LIKE '%s%q%s'", fmt,    \
        tree->comp == ML_COMP_HAS                                             \
        || tree->comp == ML_COMP_STARTS_WITH? "%%" : "",                      \
            tree->value.str,                                                  \
        tree->comp == ML_COMP_HAS                                             \
        || tree->comp == ML_COMP_ENDS_WITH? "%%" : "" );                      \
    if( *ppsz_where == NULL )                                                 \
        goto parsefail;                                                       \
    *join |= table;                                                           \
    break

#define SLDPJ sort, limit, distinct, pppsz_frompersons, i_frompersons, join
static int BuildWhere( media_library_t* p_ml, char **ppsz_where, ml_ftree_t* tree,
       char** sort, int* limit, const char** distinct,
       char*** pppsz_frompersons, int* i_frompersons, int* join )
{
    assert( ppsz_where && sort && distinct );
    if( !tree ) /* Base case */
    {
        return VLC_SUCCESS;
    }

    int i_ret = VLC_EGENERIC;
    char* psz_left = NULL;
    char* psz_right = NULL;

    switch( tree->op )
    {
        case ML_OP_AND:
        case ML_OP_OR:
            i_ret = BuildWhere( p_ml, &psz_left, tree->left, SLDPJ );
            if( i_ret != VLC_SUCCESS )
                goto parsefail;
            i_ret = BuildWhere( p_ml, &psz_right, tree->right, SLDPJ );
            if( i_ret != VLC_SUCCESS )
                goto parsefail;
            if( psz_left == NULL || psz_right == NULL )
            {
                msg_Err( p_ml, "Parsing failed for AND/OR" );
                i_ret = VLC_EGENERIC;
                goto parsefail;
            }
            if( asprintf( ppsz_where, "( %s %s %s )", psz_left,
                ( tree->op == ML_OP_AND ? "AND" : "OR" ), psz_right ) == -1 )
            {
                i_ret = VLC_ENOMEM;
                goto parsefail;
            }
            break;
        case ML_OP_NOT:
            i_ret = BuildWhere( p_ml, &psz_left, tree->left, SLDPJ );
            if( i_ret != VLC_SUCCESS )
                goto parsefail;
            if( psz_left == NULL )
            {
                msg_Err( p_ml, "Parsing failed at NOT" );
                i_ret = VLC_EGENERIC;
                goto parsefail;
            }
            if( asprintf( ppsz_where, "( NOT %s )", psz_left ) == -1 )
            {
                i_ret = VLC_ENOMEM;
                goto parsefail;
            }
            break;
        case ML_OP_SPECIAL:
            i_ret = BuildWhere( p_ml, &psz_right, tree->right, SLDPJ );
            if( i_ret != VLC_SUCCESS )
                goto parsefail;
            i_ret = BuildWhere( p_ml, &psz_left, tree->left, SLDPJ );
            if( i_ret != VLC_SUCCESS )
                goto parsefail;
            /* Ignore right parse tree as this is a special node */
            *ppsz_where = strdup( psz_left ? psz_left : "" );
            if( !*ppsz_where )
            {
                i_ret = VLC_ENOMEM;
                goto parsefail;
            }
            break;
        case ML_OP_NONE:
            switch( tree->criteria )
            {
                case ML_PEOPLE:
                    assert( tree->comp == ML_COMP_HAS
                            || tree->comp == ML_COMP_EQUAL
                            || tree->comp == ML_COMP_STARTS_WITH
                            || tree->comp == ML_COMP_ENDS_WITH );
                    *ppsz_where = sql_Printf( p_ml->p_sys->p_sql,
                            "people%s%s.name LIKE '%s%q%s'",
                            tree->lvalue.str ? "_" : "",
                            tree->lvalue.str ? tree->lvalue.str : "",
                            tree->comp == ML_COMP_HAS
                            || tree->comp == ML_COMP_STARTS_WITH ? "%%" : "",
                            tree->value.str,
                            tree->comp == ML_COMP_HAS
                            || tree->comp == ML_COMP_ENDS_WITH ? "%%" : "" );
                    if( *ppsz_where == NULL )
                        goto parsefail;
                    *pppsz_frompersons = realloc( *pppsz_frompersons,
                            ++*i_frompersons * sizeof( char* ) );
                    *pppsz_frompersons[ *i_frompersons - 1 ] = tree->lvalue.str;
                    *join |= table_people;
                    break;
                case ML_PEOPLE_ID:
                    assert( tree->comp == ML_COMP_EQUAL );
                    *ppsz_where = sql_Printf( p_ml->p_sys->p_sql,
                                "( people%s%s.id = %d )", tree->lvalue.str ? "_":"",
                                tree->lvalue.str ? tree->lvalue.str:"",
                                tree->value.i );
                    if( *ppsz_where == NULL )
                        goto parsefail;
                    *pppsz_frompersons = realloc( *pppsz_frompersons,
                            ++*i_frompersons * sizeof( char* ) );
                    *pppsz_frompersons[ *i_frompersons - 1 ] = tree->lvalue.str;
                    *join |= table_people;
                    break;
                case ML_PEOPLE_ROLE:
                    assert( tree->comp == ML_COMP_HAS
                            || tree->comp == ML_COMP_EQUAL
                            || tree->comp == ML_COMP_STARTS_WITH
                            || tree->comp == ML_COMP_ENDS_WITH );
                    *ppsz_where = sql_Printf( p_ml->p_sys->p_sql,
                            "people%s%s.role LIKE '%s%q%s'",
                            tree->lvalue.str ? "_" : "",
                            tree->lvalue.str ? tree->lvalue.str : "",
                            tree->comp == ML_COMP_HAS
                            || tree->comp == ML_COMP_STARTS_WITH ? "%%" : "",
                            tree->value.str,
                            tree->comp == ML_COMP_HAS
                            || tree->comp == ML_COMP_ENDS_WITH ? "%%" : "" );
                    if( *ppsz_where == NULL )
                        goto parsefail;
                    *pppsz_frompersons = realloc( *pppsz_frompersons,
                            ++*i_frompersons * sizeof( char* ) );
                    *pppsz_frompersons[ *i_frompersons - 1 ] = tree->lvalue.str;
                    *join |= table_people;
                    break;
                CASE_PSZ( ML_ALBUM, "album.title", table_album );
                CASE_PSZ( ML_ALBUM_COVER, "album.cover", table_album );
                case ML_ALBUM_ID:
                    assert( tree->comp == ML_COMP_EQUAL );
                    *ppsz_where = sql_Printf( p_ml->p_sys->p_sql,
                            "album.id = %d", tree->value.i );
                    if( *ppsz_where == NULL )
                        goto parsefail;
                    *join |= table_album;
                    break;
                CASE_PSZ( ML_COMMENT, "media.comment", table_media );
                CASE_PSZ( ML_COVER, "media.cover", table_media );
                CASE_INT( ML_DURATION, "media.duration", table_media );
                CASE_PSZ( ML_EXTRA, "extra.extra", table_extra );
                CASE_INT( ML_FILESIZE, "media.filesize", table_media );
                CASE_PSZ( ML_GENRE, "media.genre", table_media );
                case ML_ID:
                    assert( tree->comp == ML_COMP_EQUAL );
                    *ppsz_where = sql_Printf( p_ml->p_sys->p_sql,
                            "media.id = %d", tree->value.i );
                    if( *ppsz_where == NULL )
                        goto parsefail;
                    *join |= table_media;
                    break;
                CASE_PSZ( ML_LANGUAGE, "extra.language", table_extra );
                CASE_INT( ML_LAST_PLAYED, "media.last_played", table_media );
                CASE_PSZ( ML_ORIGINAL_TITLE, "media.original_title", table_media );
                   msg_Warn( p_ml, "Deprecated Played Count tags" );
                CASE_INT( ML_PLAYED_COUNT, "media.played_count", table_media );
                CASE_INT( ML_SCORE, "media.score", table_media );
                CASE_PSZ( ML_TITLE, "media.title", table_media );
                CASE_INT( ML_TRACK_NUMBER, "media.track", table_media);
                CASE_INT( ML_TYPE, "media.type", table_media );
                CASE_PSZ( ML_URI, "media.uri", table_media );
                CASE_INT( ML_VOTE, "media.vote", table_media );
                CASE_INT( ML_YEAR, "media.year", table_media );
                case ML_LIMIT:
                    if( !*limit )
                        *limit = tree->value.i;
                    else
                        msg_Warn( p_ml, "Double LIMIT found" );
                    break;
                case ML_SORT_DESC:
                    *sort = sql_Printf( p_ml->p_sys->p_sql, "%s%s%s DESC ",
                                        sort ? *sort : "", sort ? ", " : "",
                                        tree->value.str );
                    if( *sort == NULL )
                        goto parsefail;
                    break;
                case ML_SORT_ASC:
                    *sort = sql_Printf( p_ml->p_sys->p_sql, "%s%s%s ASC ",
                                        sort ? *sort : "", sort ? ", " : "",
                                        tree->value.str );
                    if( *sort == NULL )
                        goto parsefail;
                    break;
                case ML_DISTINCT:
                    if( !**distinct )
                        *distinct = "DISTINCT";
                    else
                        msg_Warn( p_ml, "Double DISTINCT found!" );
                    break;
                default:
                    msg_Err( p_ml, "Invalid select type or unsupported: %d", tree->criteria );
            }
            break;
        default:
            msg_Err( p_ml, "Broken find tree!" );
            i_ret = VLC_EGENERIC;
            goto parsefail;
    }

    i_ret = VLC_SUCCESS;
parsefail:
    free( psz_left );
    free( psz_right );
    return i_ret;
}


#   undef CASE_INT
#   undef CASE_PSZ

#   undef table_media
#   undef table_album
#   undef table_people
#   undef table_extra
