/*****************************************************************************
 * acoustid.c: AcoustId webservice parser
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "json_helper.h"
#include "acoustid.h"

/*****************************************************************************
 * Requests lifecycle
 *****************************************************************************/
void acoustid_result_release( acoustid_result_t * r )
{
    free( r->psz_id );
    for ( unsigned int i=0; i<r->recordings.count; i++ )
    {
        free( r->recordings.p_recordings[ i ].psz_artist );
        free( r->recordings.p_recordings[ i ].psz_title );
    }
    free( r->recordings.p_recordings );
}

static void parse_artists( const json_value *node, acoustid_mb_result_t *record )
{
    /* take only main */
    if ( !node || node->type != json_array || node->u.array.length < 1 )
        return;
    record->psz_artist = json_dupstring( node->u.array.values[ 0 ], "name" );
}

static void parse_recordings( vlc_object_t *p_obj, const json_value *node, acoustid_result_t *p_result )
{
    if ( !node || node->type != json_array ) return;
    p_result->recordings.p_recordings = calloc( node->u.array.length, sizeof(acoustid_mb_result_t) );
    if ( ! p_result->recordings.p_recordings ) return;
    p_result->recordings.count = node->u.array.length;

    for( unsigned int i=0; i<node->u.array.length; i++ )
    {
        acoustid_mb_result_t *record = & p_result->recordings.p_recordings[ i ];
        const json_value *recordnode = node->u.array.values[ i ];
        if ( !recordnode || recordnode->type != json_object )
            break;
        record->psz_title = json_dupstring( recordnode, "title" );
        const json_value *value = json_getbyname( recordnode, "id" );
        if ( value && value->type == json_string )
        {
            size_t i_len = strlen( value->u.string.ptr );
            i_len = __MIN( i_len, MB_ID_SIZE );
            memcpy( record->s_musicbrainz_id, value->u.string.ptr, i_len );
        }
        parse_artists( json_getbyname( recordnode, "artists" ), record );
        msg_Dbg( p_obj, "recording %d title %s %36s %s", i, record->psz_title,
                 record->s_musicbrainz_id, record->psz_artist );
    }
}

static bool ParseJson( vlc_object_t *p_obj, const void *p_buffer, acoustid_results_t *p_results )
{
    json_value *root = json_parse_document( p_obj, p_buffer );
    if( !root )
        return false;

    const json_value *node = json_getbyname( root, "status" );
    if ( !node || node->type != json_string )
    {
        msg_Warn( p_obj, "status node not found or invalid" );
        goto error;
    }
    if ( strcmp( node->u.string.ptr, "ok" ) != 0 )
    {
        msg_Warn( p_obj, "Bad request status" );
        goto error;
    }
    node = json_getbyname( root, "results" );
    if ( !node || node->type != json_array )
    {
        msg_Warn( p_obj, "Bad results array or no results" );
        goto error;
    }
    p_results->p_results = calloc( node->u.array.length, sizeof(acoustid_result_t) );
    if ( ! p_results->p_results ) goto error;
    p_results->count = node->u.array.length;
    for( unsigned int i=0; i<node->u.array.length; i++ )
    {
        const json_value *resultnode = node->u.array.values[i];
        if ( resultnode && resultnode->type == json_object )
        {
            acoustid_result_t *p_result = & p_results->p_results[i];
            const json_value *value = json_getbyname( resultnode, "score" );
            if ( value && value->type == json_double )
                p_result->d_score = value->u.dbl;
            p_result->psz_id = json_dupstring( resultnode, "id" );
            parse_recordings( p_obj, json_getbyname( resultnode, "recordings" ), p_result );
        }
    }
    json_value_free( root );
    return true;

error:
    if ( root ) json_value_free( root );
    return false;
}

int acoustid_lookup_fingerprint( const acoustid_config_t *p_cfg, acoustid_fingerprint_t *p_data )
{
    if ( !p_data->psz_fingerprint )
        return VLC_SUCCESS;

    char *psz_url;
    if( p_cfg->psz_server )
    {
        if( unlikely(asprintf( &psz_url, "https://%s/v2/lookup"
                               "?client=%s"
                               "&meta=recordings+tracks+usermeta+releases"
                               "&duration=%d"
                               "&fingerprint=%s",
                               p_cfg->psz_server,
                               p_cfg->psz_apikey ? p_cfg->psz_apikey : "",
                               p_data->i_duration,
                               p_data->psz_fingerprint ) < 1 ) )
             return VLC_EGENERIC;
    }
    else /* Use VideoLAN anonymized requests proxy */
    {
        if( unlikely(asprintf( &psz_url, "https://" ACOUSTID_ANON_SERVER
                               ACOUSTID_ANON_SERVER_PATH
                               "?meta=recordings+tracks+usermeta+releases"
                               "&duration=%d"
                               "&fingerprint=%s",
                               p_data->i_duration,
                               p_data->psz_fingerprint ) < 1 ) )
             return VLC_EGENERIC;
    }

    msg_Dbg( p_cfg->p_obj, "Querying AcoustID from %s", psz_url );
    void *p_buffer = json_retrieve_document( p_cfg->p_obj, psz_url );
    free( psz_url );
    if( !p_buffer )
        return VLC_EGENERIC;

    if ( ParseJson( p_cfg->p_obj, p_buffer, & p_data->results ) )
        msg_Dbg( p_cfg->p_obj, "results count == %d", p_data->results.count );
    else
        msg_Dbg( p_cfg->p_obj, "No results" );
    free( p_buffer );

    return VLC_SUCCESS;
}
