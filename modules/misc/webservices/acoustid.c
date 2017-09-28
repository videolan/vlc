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
        {
            size_t i_len = strlen( value->u.string.ptr );
            i_len = __MIN( i_len, MB_ID_SIZE );
            memcpy( record->s_musicbrainz_id, value->u.string.ptr, i_len );
        }
        parse_artists( jsongetbyname( recordnode, "artists" ), record );
        msg_Dbg( p_obj, "recording %d title %s %36s %s", i, record->psz_title, record->s_musicbrainz_id, record->psz_artist );
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

int DoAcoustIdWebRequest( vlc_object_t *p_obj, acoustid_fingerprint_t *p_data )
{
    if ( !p_data->psz_fingerprint ) return VLC_SUCCESS;

    char *psz_url;
    if( unlikely(asprintf( &psz_url, "https://fingerprint.videolan.org/"
                           "acoustid.php?meta=recordings+tracks+usermeta+"
                           "releases&duration=%d&fingerprint=%s",
                           p_data->i_duration, p_data->psz_fingerprint ) < 1 ) )
         return VLC_EGENERIC;

    msg_Dbg( p_obj, "Querying AcoustID from %s", psz_url );
    int i_saved_flags = p_obj->obj.flags;
    p_obj->obj.flags |= OBJECT_FLAGS_NOINTERACT;

    stream_t *p_stream = vlc_stream_NewURL( p_obj, psz_url );

    free( psz_url );
    p_obj->obj.flags = i_saved_flags;
    if ( p_stream == NULL )
        return VLC_EGENERIC;

    stream_t *p_chain = vlc_stream_FilterNew( p_stream, "inflate" );
    if( p_chain )
        p_stream = p_chain;

    /* read answer */
    char *p_buffer = NULL;
    int i_ret = 0;
    for( ;; )
    {
        int i_read = 65536;

        if( i_ret >= INT_MAX - i_read )
            break;

        p_buffer = realloc_or_free( p_buffer, 1 + i_ret + i_read );
        if( unlikely(p_buffer == NULL) )
        {
            vlc_stream_Delete( p_stream );
            return VLC_ENOMEM;
        }

        i_read = vlc_stream_Read( p_stream, &p_buffer[i_ret], i_read );
        if( i_read <= 0 )
            break;

        i_ret += i_read;
    }
    vlc_stream_Delete( p_stream );
    p_buffer[i_ret] = 0;

    if ( ParseJson( p_obj, p_buffer, & p_data->results ) )
        msg_Dbg( p_obj, "results count == %d", p_data->results.count );
    else
        msg_Dbg( p_obj, "No results" );

    return VLC_SUCCESS;
}
