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

#include <vlc_common.h>
#include <vlc_stream.h>
#include <limits.h>
#include <vlc_memory.h>

#include <vlc/vlc.h>
#include "acoustid.h"
#include "json.h"

/*****************************************************************************
 * Requests lifecycle
 *****************************************************************************/
void free_acoustid_result_t( acoustid_result_t * r )
{
    free( r->psz_id );
    for ( unsigned int i=0; i<r->recordings.count; i++ )
    {
        free( r->recordings.p_recordings[ i ].psz_artist );
        free( r->recordings.p_recordings[ i ].psz_title );
    }
    free( r->recordings.p_recordings );
}

static json_value * jsongetbyname( json_value *object, const char *psz_name )
{
    if ( object->type != json_object ) return NULL;
    for ( unsigned int i=0; i < object->u.object.length; i++ )
        if ( strcmp( object->u.object.values[i].name, psz_name ) == 0 )
            return object->u.object.values[i].value;
    return NULL;
}

static void parse_artists( json_value *node, musicbrainz_recording_t *record )
{
    /* take only main */
    if ( !node || node->type != json_array || node->u.array.length < 1 ) return;
    json_value *artistnode = node->u.array.values[ 0 ];
    json_value *value = jsongetbyname( artistnode, "name" );
    if ( value && value->type == json_string )
        record->psz_artist = strdup( value->u.string.ptr );
}

static void parse_recordings( vlc_object_t *p_obj, json_value *node, acoustid_result_t *p_result )
{
    if ( !node || node->type != json_array ) return;
    p_result->recordings.p_recordings = calloc( node->u.array.length, sizeof(musicbrainz_recording_t) );
    if ( ! p_result->recordings.p_recordings ) return;
    p_result->recordings.count = node->u.array.length;

    for( unsigned int i=0; i<node->u.array.length; i++ )
    {
        musicbrainz_recording_t *record = & p_result->recordings.p_recordings[ i ];
        json_value *recordnode = node->u.array.values[ i ];
        if ( !recordnode || recordnode->type != json_object ) break;
        json_value *value = jsongetbyname( recordnode, "title" );
        if ( value && value->type == json_string )
            record->psz_title = strdup( value->u.string.ptr );
        value = jsongetbyname( recordnode, "id" );
        if ( value && value->type == json_string )
            strncpy( record->sz_musicbrainz_id, value->u.string.ptr, MB_ID_SIZE );
        parse_artists( jsongetbyname( recordnode, "artists" ), record );
        msg_Dbg( p_obj, "recording %d title %s %36s %s", i, record->psz_title, record->sz_musicbrainz_id, record->psz_artist );
    }
}

static bool ParseJson( vlc_object_t *p_obj, char *psz_buffer, acoustid_results_t *p_results )
{
    json_settings settings;
    char psz_error[128];
    memset (&settings, 0, sizeof (json_settings));
    json_value *root = json_parse_ex( &settings, psz_buffer, psz_error );
    if ( root == NULL )
    {
        msg_Warn( p_obj, "Can't parse json data: %s", psz_error );
        goto error;
    }
    if ( root->type != json_object )
    {
        msg_Warn( p_obj, "wrong json root node" );
        goto error;
    }
    json_value *node = jsongetbyname( root, "status" );
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
    node = jsongetbyname( root, "results" );
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
        json_value *resultnode = node->u.array.values[i];
        if ( resultnode && resultnode->type == json_object )
        {
            acoustid_result_t *p_result = & p_results->p_results[i];
            json_value *value = jsongetbyname( resultnode, "score" );
            if ( value && value->type == json_double )
                p_result->d_score = value->u.dbl;
            value = jsongetbyname( resultnode, "id" );
            if ( value && value->type == json_string )
                p_result->psz_id = strdup( value->u.string.ptr );
            parse_recordings( p_obj, jsongetbyname( resultnode, "recordings" ), p_result );
        }
    }
    json_value_free( root );
    return true;

error:
    if ( root ) json_value_free( root );
    return false;
}

struct webrequest_t
{
    stream_t *p_stream;
    char *psz_url;
    char *p_buffer;
};

static void cancelDoAcoustIdWebRequest( void *p_arg )
{
    struct webrequest_t *p_request = (struct webrequest_t *) p_arg;
    if ( p_request->p_stream )
        stream_Delete( p_request->p_stream );
    if ( p_request->psz_url )
        free( p_request->psz_url );
    if ( p_request->p_buffer )
        free( p_request->p_buffer );
}

int DoAcoustIdWebRequest( vlc_object_t *p_obj, acoustid_fingerprint_t *p_data )
{
    int i_ret;
    int i_status;
    struct webrequest_t request = { NULL, NULL, NULL };

    if ( !p_data->psz_fingerprint ) return VLC_SUCCESS;

    i_ret = asprintf( & request.psz_url,
              "http://fingerprint.videolan.org/acoustid.php?meta=recordings+tracks+usermeta+releases&duration=%d&fingerprint=%s",
              p_data->i_duration, p_data->psz_fingerprint );
    if ( i_ret < 1 ) return VLC_EGENERIC;

    vlc_cleanup_push( cancelDoAcoustIdWebRequest, &request );

    msg_Dbg( p_obj, "Querying AcoustID from %s", request.psz_url );
    request.p_stream = stream_UrlNew( p_obj, request.psz_url );
    if ( !request.p_stream )
    {
        i_status = VLC_EGENERIC;
        goto cleanup;
    }

    /* read answer */
    i_ret = 0;
    for( ;; )
    {
        int i_read = 65536;

        if( i_ret >= INT_MAX - i_read )
            break;

        request.p_buffer = realloc_or_free( request.p_buffer, 1 + i_ret + i_read );
        if( !request.p_buffer )
        {
            i_status = VLC_ENOMEM;
            goto cleanup;
        }

        i_read = stream_Read( request.p_stream, &request.p_buffer[i_ret], i_read );
        if( i_read <= 0 )
            break;

        i_ret += i_read;
    }
    stream_Delete( request.p_stream );
    request.p_stream = NULL;
    request.p_buffer[ i_ret ] = 0;

    int i_canc = vlc_savecancel();
    if ( ParseJson( p_obj, request.p_buffer, & p_data->results ) )
    {
        msg_Dbg( p_obj, "results count == %d", p_data->results.count );
    } else {
        msg_Dbg( p_obj, "No results" );
    }
    vlc_restorecancel( i_canc );
    i_status = VLC_SUCCESS;

cleanup:
    vlc_cleanup_run( );
    return i_status;
}
