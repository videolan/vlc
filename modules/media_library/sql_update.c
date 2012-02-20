/*****************************************************************************
 * sql_update.c: SQL-based media library: all database update functions
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


/**
 * @brief Generic update in Media Library database
 *
 * @param p_ml the media library object
 * @param selected_type the type of the element we're selecting
 * @param where the list of ids or uri to change
 * @param changes list of changes to make in the entries
 * @return VLC_SUCCESS or VLC_EGENERIC
 * @note This function is transactional
 */
int Update( media_library_t *p_ml, ml_select_e selected_type,
        const char* psz_lvalue, ml_ftree_t *where, vlc_array_t *changes )
{
    int i_ret = VLC_EGENERIC;
    char *psz_query = NULL;
    char *psz_id_query = NULL;
    char **pp_results = NULL;
    int i_rows = 0, i_cols = 0;

    i_ret = BuildUpdate( p_ml, &psz_query, &psz_id_query,
                         psz_lvalue, selected_type, where, changes );

    if( i_ret != VLC_SUCCESS )
    {
        msg_Err(p_ml,"Failed to generate update query" );
        return i_ret;
    }
    i_ret = VLC_EGENERIC;

    Begin( p_ml );
    if( QuerySimple( p_ml, "%s", psz_query ) != VLC_SUCCESS )
    {
        msg_Err( p_ml, "Couldn't run the generated update query successfully" );
        goto quitdelete;
    }

    /* Get the updated IDs to send events! */
    if( Query( p_ml, &pp_results, &i_rows, &i_cols, psz_id_query )
            != VLC_SUCCESS )
        goto quitdelete;

    i_ret = VLC_SUCCESS;
quitdelete:
    if( i_ret != VLC_SUCCESS )
        Rollback( p_ml );
    else
    {
        Commit( p_ml );
        if( i_rows > 0 )
        {
            for( int i = 0; i < i_rows; i++ )
            {
                var_SetInteger( p_ml, "media-meta-change",
                        atoi( pp_results[i*i_cols] ) );
            }
        }
    }
    FreeSQLResult( p_ml, pp_results );
    free( psz_id_query );
    free( psz_query );
    return i_ret;
}

#define SET_STR( a )                                                        \
if( !psz_set[i_type] )                                                      \
{                                                                           \
    psz_set[i_type] = sql_Printf( p_ml->p_sys->p_sql, a, find->value.str ); \
    if( !psz_set[i_type] )                                                  \
        goto quit_buildupdate;                                              \
}                                                                           \
break;

#define SET_INT( a )                                                        \
if( !psz_set[i_type] )                                                      \
{                                                                           \
    psz_set[i_type] = sql_Printf( p_ml->p_sys->p_sql, a, find->value.i );   \
    if( !psz_set[i_type] )                                                  \
        goto quit_buildupdate;                                              \
}                                                                           \
break;

/* TODO: Build smarter updates by using IN () */
static int BuildWhere( media_library_t* p_ml, char **ppsz_where, ml_ftree_t *tree )
{
    assert( ppsz_where );
    char* psz_left = NULL;
    char* psz_right = NULL;
    int i_ret = VLC_SUCCESS;
    switch( tree->op )
    {
        case ML_OP_AND:
        case ML_OP_OR:
            i_ret = BuildWhere( p_ml, &psz_left, tree->left );
            if( i_ret != VLC_SUCCESS )
                goto quit_buildwhere;
            i_ret = BuildWhere( p_ml, &psz_right, tree->right );
            if( i_ret != VLC_SUCCESS )
                goto quit_buildwhere;
            if( psz_left == NULL || psz_right == NULL )
            {
                msg_Err( p_ml, "Couldn't build AND/OR for Update statement" );
                i_ret = VLC_EGENERIC;
                goto quit_buildwhere;
            }
            if( asprintf( ppsz_where, "( %s %s %s )", psz_left,
                ( tree->op == ML_OP_AND ? "AND" : "OR" ), psz_right ) == -1 )
            {
                i_ret = VLC_ENOMEM;
                goto quit_buildwhere;
            }
            break;
        case ML_OP_NOT:
            i_ret = BuildWhere( p_ml, &psz_left, tree->left );
            if( i_ret != VLC_SUCCESS )
                goto quit_buildwhere;
            if( psz_left == NULL )
            {
                msg_Err( p_ml, "Couldn't build NOT for Update statement" );
                i_ret = VLC_EGENERIC;
                goto quit_buildwhere;
            }
            if( asprintf( ppsz_where, "( NOT %s )", psz_left ) == -1 )
            {
                i_ret = VLC_ENOMEM;
                goto quit_buildwhere;
            }
            break;
        case ML_OP_SPECIAL:
            msg_Err( p_ml, "Couldn't build special for Update statement" );
            break;
        case ML_OP_NONE:
            switch( tree->criteria )
            {
                case ML_ID:
                    assert( tree->comp == ML_COMP_EQUAL );
                    *ppsz_where = sql_Printf( p_ml->p_sys->p_sql, "media.id = %d",
                                                tree->value.i );
                    if( *ppsz_where == NULL )
                        goto quit_buildwhere;
                    break;
                case ML_URI:
                    assert( tree->comp == ML_COMP_EQUAL );
                    *ppsz_where = sql_Printf( p_ml->p_sys->p_sql, "media.uri = %q",
                                                tree->value.str );
                    if( *ppsz_where == NULL )
                        goto quit_buildwhere;
                    break;
                default:
                    msg_Err( p_ml, "Trying to update with unsupported condition" );
                    break;
            }
    }
quit_buildwhere:
    return i_ret;
}

/**
 * @brief Generic UPDATE query builder
 *
 * @param p_ml This media_library_t object
 * @param ppsz_query *ppsz_query will contain query for update
 * @param ppsz_id_query will contain query to get the ids of updated files
 * @param selected_type the type of the element we're selecting
 * @param where parse tree of where condition
 * @param changes list of changes to make in the entries
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int BuildUpdate( media_library_t *p_ml,
                 char **ppsz_query, char **ppsz_id_query,
                 const char *psz_lvalue,
                 ml_select_e selected_type,
                 ml_ftree_t *where, vlc_array_t *changes )
{
    assert( ppsz_query );
    assert( ppsz_id_query );

    *ppsz_query = NULL;
    int i_type;
    int i_index;
    int i_ret = VLC_ENOMEM;

    char *psz_table = NULL;
    /* TODO: Hack? */
    char *psz_set[ ML_DIRECTORY + 1 ] = { NULL };
    char *psz_fullset = NULL;
    char *psz_extra = NULL; /*<< For an update to extra table */

    char *psz_where = NULL;
    char *psz_tmp = NULL;

    int *pi_padd_ids = NULL;
    int i_people_add = 0;

    int i_album_id = 0;
    char *psz_album = NULL;
    char *psz_cover = NULL;


    if( !where )
    {
        msg_Warn( p_ml, "You're trying to update no rows."
               "Trying to guess update based on uri" );
    }

    /* Create the id/uri lists for WHERE part of the query */
    i_ret = BuildWhere( p_ml, &psz_where, where );
    if( i_ret != VLC_SUCCESS )
        goto quit_buildupdate;
    i_ret = VLC_ENOMEM;

    /** Firstly, choose the right table */
    switch( selected_type )
    {
        case ML_ALBUM:
            psz_table = strdup( "album" );
            break;
        case ML_PEOPLE:
            psz_table = strdup( "people" );
            break;
        case ML_MEDIA:
            psz_table = strdup( "media" );
            break;
        default:
            msg_Err( p_ml, "Not a valid element to Update!" );
            i_ret = VLC_EGENERIC;
            goto quit_buildupdate;
            break;
    }

    if( !psz_table )
        return VLC_ENOMEM;

    /** Secondly, build the SET part of the query */
    for( i_index = 0; i_index < vlc_array_count( changes ); i_index++ )
    {
        ml_element_t *find = ( ml_element_t * )
                vlc_array_item_at_index( changes, i_index );
        i_type = find->criteria;

        switch( i_type )
        {
        case ML_ALBUM:
            if( selected_type == ML_ALBUM )
            {
                if( !psz_set[i_type] )
                {
                    psz_set[i_type] = sql_Printf( p_ml->p_sys->p_sql,
                                                  "title = %Q",
                                                  find->value.str );
                    if( !psz_set[i_type] )
                    {
                        msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                                __FILE__, __LINE__ );
                        goto quit_buildupdate;
                    }
                }
            }
            else if( selected_type == ML_MEDIA )
            {
                if( !psz_album )
                    psz_album = find->value.str;
            }
            else
                assert( 0 );
            break;
        case ML_ALBUM_ID:
            assert( selected_type != ML_ALBUM );
            if( selected_type == ML_MEDIA )
            {
                if( i_album_id <= 0 )
                {
                    i_album_id = find->value.i;
                    if( !psz_set[i_type] )
                    {
                        psz_set[i_type] = sql_Printf( p_ml->p_sys->p_sql,
                                                      "album_id = '%d'",
                                                      find->value.i );
                        if( !psz_set[i_type] )
                        {
                            msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                                __FILE__, __LINE__ );
                            goto quit_buildupdate;
                        }
                    }
                }
            }
            break;
        case ML_PEOPLE:
            if( selected_type == ML_MEDIA )
            {
                pi_padd_ids = (int*) realloc( pi_padd_ids , ( ++i_people_add * sizeof(int) ) );
                pi_padd_ids[ i_people_add - 1 ] = ml_GetInt( p_ml, ML_PEOPLE_ID,
                                            find->lvalue.str, ML_PEOPLE, find->lvalue.str,
                                            find->value.str );
                if( pi_padd_ids[ i_people_add - 1 ] <= 0 )
                {
                    AddPeople( p_ml, find->value.str, find->lvalue.str );
                    pi_padd_ids[ i_people_add - 1 ] = ml_GetInt( p_ml, ML_PEOPLE_ID,
                                            find->lvalue.str, ML_PEOPLE, find->lvalue.str,
                                            find->value.str );
                }
            }
            else if( strcmp( psz_lvalue, find->lvalue.str ) )
            {
                msg_Err( p_ml, "Trying to update a different person type" );
                return VLC_EGENERIC;
            }
            else
            {
                if( !psz_set[i_type] ) psz_set[i_type] =
                    sql_Printf( p_ml->p_sys->p_sql, "name = %Q", find->value.str );
            }
            break;
        case ML_PEOPLE_ID:
            /* TODO: Implement smarter updates for this case? */
            assert( selected_type == ML_MEDIA );
            if( selected_type == ML_MEDIA )
            {
                bool b_update = true;
                for( int i = 0; i < i_people_add; i++ )
                {
                   if( pi_padd_ids[ i ] == find->value.i )
                   {
                      b_update = false;
                      break;
                   }
                }
                if( b_update )
                {
                    pi_padd_ids = (int *)realloc( pi_padd_ids, ( ++i_people_add * sizeof(int) ) );
                    pi_padd_ids[ i_people_add - 1 ] = find->value.i;
                }
            }
            break;
        case ML_PEOPLE_ROLE:
            msg_Dbg( p_ml, "Can't update role" );
            break;
        case ML_COMMENT:
            assert( selected_type == ML_MEDIA );
            SET_STR( "comment = %Q" );
        case ML_COVER:
            assert( selected_type == ML_ALBUM || selected_type == ML_MEDIA );
            psz_cover = find->value.str;
            SET_STR( "cover = %Q" );
        case ML_DISC_NUMBER:
            assert( selected_type == ML_MEDIA );
            SET_INT( "disc = '%d'" );
        case ML_DURATION:
            assert( selected_type == ML_MEDIA );
            SET_INT( "duration = '%d'" );
        case ML_EXTRA:
            assert( selected_type == ML_MEDIA );
            SET_STR( "extra = %Q" );
        case ML_FIRST_PLAYED:
            assert( selected_type == ML_MEDIA );
            SET_INT( "first_played =='%d'" );
        case ML_GENRE:
            assert( selected_type == ML_MEDIA );
            SET_STR( "genre = %Q" );
        /* ID cannot be updated */
        /* Import time can't be updated */
        case ML_LAST_PLAYED:
            assert( selected_type == ML_MEDIA );
            SET_INT( "last_played = '%d'" );
        case ML_ORIGINAL_TITLE:
            assert( selected_type == ML_MEDIA );
            SET_STR( "original_title = %Q" );
        case ML_PLAYED_COUNT:
            assert( selected_type == ML_MEDIA );
            SET_INT( "played_count = '%d'" );
        case ML_PREVIEW:
            assert( selected_type == ML_MEDIA );
            SET_STR( "preview = %Q" );
        case ML_SKIPPED_COUNT:
            assert( selected_type == ML_MEDIA );
            SET_INT( "skipped_count = '%d'" );
        case ML_SCORE:
            assert( selected_type == ML_MEDIA );
            SET_INT( "score = '%d'" );
        case ML_TITLE:
            assert( selected_type == ML_MEDIA );
            SET_STR( "title = %Q" );
        case ML_TRACK_NUMBER:
            assert( selected_type == ML_MEDIA );
            SET_INT( "track = '%d'" );
        case ML_TYPE:
            assert( selected_type == ML_MEDIA );
            if( !psz_set[i_type] ) psz_set[i_type] =
                sql_Printf( p_ml->p_sys->p_sql, "type = '%d'", find->value.i );
            break;
        case ML_URI:
            assert( selected_type == ML_MEDIA );
            if( !psz_set[i_type] )
            {
                psz_set[i_type] = sql_Printf( p_ml->p_sys->p_sql,
                                              "uri = %Q",
                                              find->value.str );
            }
            break;
        case ML_VOTE:
            assert( selected_type == ML_MEDIA );
            SET_INT( "vote = '%d'" );
        case ML_YEAR:
            assert( selected_type == ML_MEDIA );
            SET_INT( "year = '%d'" );
        case ML_END:
            goto exitfor;
        default:
            msg_Warn( p_ml, "Invalid type for update : %d", i_type );
        }
    }
exitfor:

    /* TODO: Album artist. Verify albumart */
    if( i_album_id <= 0 || ( psz_album && *psz_album ) )
    {
        i_album_id = ml_GetAlbumId( p_ml, psz_album );
        if( i_album_id < 0 ) //0 is Unknown
        {
            i_ret = AddAlbum( p_ml, psz_album, psz_cover, 0 );
            if( i_ret != VLC_SUCCESS )
            {

                msg_Err( p_ml, "Couldn't AddAlbum at BuildUpdate():(%s, %d)",
                        __FILE__, __LINE__ );
                goto quit_buildupdate;
            }
            i_album_id = ml_GetAlbumId( p_ml, psz_album );
            if( i_album_id < 0 )
                goto quit_buildupdate;
        }
        psz_set[ML_ALBUM_ID] = sql_Printf( p_ml->p_sys->p_sql,
                                      "album_id = '%d'", i_album_id );
        if( !psz_set[ML_ALBUM_ID] )
        {
            msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                    __FILE__, __LINE__ );
            goto quit_buildupdate;
        }
    }

    for( unsigned i = 0; i <= ML_DIRECTORY; i++ )
    {
        if( psz_set[i] )
        {
            if( i == ML_EXTRA || i == ML_LANGUAGE )
            {
                free( psz_tmp );
                if( asprintf( &psz_tmp, "%s%s%s", psz_extra ? psz_extra : "",
                               psz_extra ? ", ": "",  psz_set[i] ) == -1 )
                {
                    msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                            __FILE__, __LINE__ );
                    goto quit_buildupdate;
                }
                free( psz_extra );
                psz_extra = strdup( psz_tmp );
            }
            else
            {
                free( psz_tmp );
                if( asprintf( &psz_tmp, "%s%s%s", psz_fullset ? psz_fullset : "",
                               psz_fullset ? ", ": "",  psz_set[i] ) == -1 )
                {
                    msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                            __FILE__, __LINE__ );
                    goto quit_buildupdate;
                }
                free( psz_fullset );
                psz_fullset = strdup( psz_tmp );
            }
        }
    }
    i_ret = VLC_SUCCESS;

    /** Now build the right WHERE condition */
    assert( psz_where && *psz_where );

    /** Finally build the full query */
    /** Pass if we have some people to add - Indirect update */
    if( !psz_fullset && i_people_add == 0 )
    {
        i_ret = VLC_EGENERIC;
        msg_Err( p_ml, "Nothing found to create update at BuildUpdate():(%s, %d)",
                        __FILE__, __LINE__ );
        goto quit_buildupdate;
    }

    if( psz_fullset ){
        if( asprintf( ppsz_query, "UPDATE %s SET %s WHERE %s", psz_table,
                      psz_fullset, psz_where ) == -1 )
        {
            msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                            __FILE__, __LINE__ );
            goto quit_buildupdate;
        }
    }

    if( selected_type == ML_MEDIA )
    {
        if( psz_extra )
        {
            if( asprintf( &psz_tmp, "%s; UPDATE extra SET %s WHERE %s",
                        *ppsz_query, psz_extra, psz_where ) == -1 )
            {
                msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                            __FILE__, __LINE__ );
                goto quit_buildupdate;
            }
            free( *ppsz_query );
            *ppsz_query = psz_tmp;
            psz_tmp = NULL;
        }
        char* psz_idstring = NULL;
        if( i_people_add > 0 )
        {
            for( int i = 0; i < i_people_add; i++ )
            {
                if( asprintf( &psz_tmp, "%s%s%d", psz_idstring == NULL? "" : psz_idstring,
                            psz_idstring == NULL ? "" : ",", pi_padd_ids[i] ) == -1 )
                {
                    free( psz_tmp );
                    free( psz_idstring );
                    msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                            __FILE__, __LINE__ );
                    goto quit_buildupdate;
                }
                free( psz_idstring );
                psz_idstring = psz_tmp;
                psz_tmp = NULL;
            }
            /* Delete all connections with people whom we will update now! */
            if( asprintf( &psz_tmp, "%s;DELETE FROM media_to_people WHERE EXISTS "
                    "(SELECT media.id, people.id FROM media JOIN media_to_people "
                    "AS temp ON media.id = temp.media_id "
                    "JOIN people ON temp.people_id = people.id "
                    "WHERE %s AND people.role IN "
                    "(SELECT people.role FROM people WHERE people.id IN (%s)) "
                    "AND people.id NOT IN (%s) "
                    "AND temp.media_id = media_to_people.media_id AND "
                    "temp.people_id = media_to_people.people_id )",
                    *ppsz_query == NULL ? "": *ppsz_query, psz_where,
                    psz_idstring, psz_idstring ) == -1 )
            {
                free( psz_idstring );
                msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                                __FILE__, __LINE__ );
                goto quit_buildupdate;
            }
            free( *ppsz_query );
            *ppsz_query = psz_tmp;
            psz_tmp = NULL;
            free( psz_idstring );
        }
        for( int i = 0; i < i_people_add ; i++ )
        {
            if( pi_padd_ids[i] > 0 )
            {
                /* OR IGNORE will avoid errors from collisions from old media
                 * Perhaps this hack can be fixed...FIXME */
                if( asprintf( &psz_tmp, "%s;INSERT OR IGNORE into media_to_people "
                "(media_id,people_id) SELECT media.id, %d FROM media WHERE %s",
                *ppsz_query == NULL ? "" : *ppsz_query, pi_padd_ids[i],
                psz_where ) == -1 )
                {
                    msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                                    __FILE__, __LINE__ );
                    goto quit_buildupdate;
                }
                FREENULL( *ppsz_query );
                *ppsz_query = psz_tmp;
                psz_tmp = NULL;
            }
        }
    }

    if( asprintf( ppsz_id_query, "SELECT id AS %s_id FROM %s WHERE %s",
                psz_table, psz_table, psz_where ) == -1 )
    {
        msg_Err( p_ml, "Couldn't create string at BuildUpdate():(%s, %d)",
                        __FILE__, __LINE__ );
        goto quit_buildupdate;
    }
#ifndef NDEBUG
    msg_Dbg( p_ml, "updated media where %s", psz_where );
#endif
    goto quit_buildupdate_success;

quit_buildupdate:
    msg_Warn( p_ml, "BuildUpdate() could not generate update sql query" );
quit_buildupdate_success:
    free( psz_tmp );
    free( psz_table );
    free( psz_fullset );
    free( psz_extra );
    free( pi_padd_ids );
    for( int i = 0; i <= ML_DIRECTORY; i++ )
        free( psz_set[ i ] );

    return i_ret;
}

#undef SET_STR
#undef SET_INT

/**
 * @brief Update a ml_media_t
 *
 * @param p_ml the media library object
 * @param p_media media to synchronise in the database
 * @return VLC_SUCCESS or VLC_EGENERIC
 * @note: the media id may be 0, in this case, the update is based
 *        on the url (less powerful). This function is threadsafe
 *
 * This synchronises all non NULL and non zero fields of p_media
 * Synchronization of album and people is TODO
 */
int UpdateMedia( media_library_t *p_ml, ml_media_t *p_media )
{
    assert( p_media->i_id || ( p_media->psz_uri && *p_media->psz_uri ) );
    vlc_array_t *changes = vlc_array_new();
    ml_element_t *find = NULL;
    int i_ret = VLC_EGENERIC;

    ml_LockMedia( p_media );
#define APPEND_ICHANGES( cond, crit ) \
    if( cond ) { \
        find = ( ml_element_t* ) calloc( 1, sizeof( ml_element_t ) ); \
        find->criteria = crit; \
        find->value.i = cond; \
        vlc_array_append( changes, find ); \
    }
#define APPEND_SCHANGES( cond, crit ) \
    if( cond ) { \
        find = ( ml_element_t* ) calloc( 1, sizeof( ml_element_t ) ); \
        find->criteria = crit; \
        find->value.str = cond; \
        vlc_array_append( changes, find ); \
    }

    APPEND_SCHANGES( p_media->psz_title, ML_TITLE );
    APPEND_ICHANGES( p_media->i_type, ML_TYPE );
    APPEND_ICHANGES( p_media->i_duration, ML_DURATION );
    APPEND_SCHANGES( p_media->psz_preview, ML_PREVIEW );
    APPEND_SCHANGES( p_media->psz_cover, ML_COVER );
    APPEND_ICHANGES( p_media->i_disc_number, ML_DISC_NUMBER );
    APPEND_ICHANGES( p_media->i_track_number, ML_TRACK_NUMBER );
    APPEND_ICHANGES( p_media->i_year, ML_YEAR);
    APPEND_SCHANGES( p_media->psz_genre, ML_GENRE );
    APPEND_ICHANGES( p_media->i_album_id, ML_ALBUM_ID );
    APPEND_SCHANGES( p_media->psz_album, ML_ALBUM );
    APPEND_ICHANGES( p_media->i_skipped_count, ML_SKIPPED_COUNT );
    APPEND_ICHANGES( p_media->i_last_skipped, ML_LAST_SKIPPED );
    APPEND_ICHANGES( p_media->i_played_count, ML_PLAYED_COUNT );
    APPEND_ICHANGES( p_media->i_last_played, ML_LAST_PLAYED );
    APPEND_ICHANGES( p_media->i_first_played, ML_FIRST_PLAYED );
    APPEND_ICHANGES( p_media->i_vote, ML_VOTE );
    APPEND_ICHANGES( p_media->i_score, ML_SCORE );
    APPEND_SCHANGES( p_media->psz_comment, ML_COMMENT );
    APPEND_SCHANGES( p_media->psz_extra, ML_EXTRA );
    APPEND_SCHANGES( p_media->psz_language, ML_LANGUAGE );

    if( p_media->psz_uri && p_media->i_id )
    {
        find = ( ml_element_t* ) calloc( 1, sizeof( ml_element_t ) );
        find->criteria = ML_URI;
        find->value.str = p_media->psz_uri;
        vlc_array_append( changes, find );
    }
    /*TODO: implement extended meta */
    /* We're not taking import time! Good */

#undef APPEND_ICHANGES
#undef APPEND_SCHANGES
    ml_person_t* person = p_media->p_people;
    while( person )
    {
        if( person->i_id > 0 )
        {
            find = ( ml_element_t* ) calloc( 1, sizeof( ml_element_t ) );
            find->criteria = ML_PEOPLE_ID;
            find->lvalue.str = person->psz_role;
            find->value.i = person->i_id;
            vlc_array_append( changes, find );
        }
        else if( person->psz_name && *person->psz_name )
        {
            find = ( ml_element_t* ) calloc( 1, sizeof( ml_element_t ) );
            find->criteria = ML_PEOPLE;
            find->lvalue.str = person->psz_role;
            find->value.str = person->psz_name;
            vlc_array_append( changes, find );
        }
        person = person->p_next;
    }

    ml_ftree_t* p_where = NULL;
    ml_ftree_t* p_where_elt = ( ml_ftree_t* ) calloc( 1, sizeof( ml_ftree_t ) );
    if( p_media->i_id )
    {
        p_where_elt->criteria = ML_ID;
        p_where_elt->value.i = p_media->i_id ;
        p_where_elt->comp = ML_COMP_EQUAL;
        p_where = ml_FtreeFastAnd( p_where, p_where_elt );
    }
    else if( p_media->psz_uri )
    {
        p_where_elt->criteria = ML_URI;
        p_where_elt->value.str = p_media->psz_uri;
        p_where_elt->comp = ML_COMP_EQUAL;
        p_where = ml_FtreeFastAnd( p_where, p_where_elt );
    }
    else
    {
        goto quit1;
    }
    i_ret = Update( p_ml, ML_MEDIA, NULL, p_where, changes );

quit1:
    ml_FreeFindTree( p_where );
    for( int i = 0; i < vlc_array_count( changes ); i++ )
        /* Note: DO NOT free the strings because
         * they belong to the ml_media_t object */
        free( vlc_array_item_at_index( changes, i ) );
    vlc_array_destroy( changes );
    ml_UnlockMedia( p_media );
    return i_ret;
}

/**
 * @brief Update an album's cover art
 * @param p_ml The Media Library
 * @param i_album_id Album's ID
 * @param psz_cover New cover art
 * @return VLC success/error code
 **/
int SetArtCover( media_library_t *p_ml,
                   int i_album_id, const char *psz_cover )
{
    assert( i_album_id != 0 );
    assert( psz_cover != NULL );

    char *psz_query = sql_Printf( p_ml->p_sys->p_sql,
              "UPDATE album SET cover = %Q WHERE id = '%d'",
              psz_cover, i_album_id );

    if( !psz_query )
        return VLC_ENOMEM;

    if( QuerySimple( p_ml, "%s", psz_query ) != VLC_SUCCESS )
    {
        msg_Warn( p_ml, "Could not update the album's cover art "
                "(Database error)" );
        free( psz_query );
        return VLC_EGENERIC;
    }

    free( psz_query );
    return VLC_SUCCESS;
}
