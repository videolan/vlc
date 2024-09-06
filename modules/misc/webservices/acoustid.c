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
#include <vlc_messages.h>

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

static void parse_artists( const struct json_array *artists, acoustid_mb_result_t *record )
{
    /* take only main */
    if ( artists->size < 1)
        return;
    if (artists->entries[0].type == JSON_OBJECT) {
        record->psz_artist = json_dupstring( &artists->entries[ 0 ].object, "name" );
    }
}

static void parse_recordings( vlc_object_t *p_obj, const struct json_object *object,
                              acoustid_result_t *p_result )
{
    const struct json_array *recordings = json_get_array(object, "recordings");
    if (recordings == NULL)
        return;
    p_result->recordings.p_recordings = calloc( recordings->size, sizeof(acoustid_mb_result_t) );
    if ( ! p_result->recordings.p_recordings ) return;
    p_result->recordings.count = recordings->size;

    for( unsigned int i = 0; i < recordings->size; i++ )
    {
        acoustid_mb_result_t *record = & p_result->recordings.p_recordings[ i ];
        struct json_value *recordnode = &recordings->entries[ i ];
        if ( recordnode->type != JSON_OBJECT )
            break;
        record->psz_title = json_dupstring( &recordnode->object, "title" );
        const char *id = json_get_str( &recordnode->object, "id" );
        if ( id != NULL )
        {
            size_t i_len = strlen( id );
            i_len = __MIN( i_len, MB_ID_SIZE );
            memcpy( record->s_musicbrainz_id, id, i_len );
        }
        const struct json_array *artists = json_get_array(&recordnode->object,
                                                          "artists");
        parse_artists( artists, record );
        msg_Dbg( p_obj, "recording %d title %s %36s %s", i, record->psz_title,
                 record->s_musicbrainz_id, record->psz_artist );
    }
}

static bool ParseJson( vlc_object_t *p_obj, const void *p_buffer, size_t i_buffer,
                       acoustid_results_t *p_results )
{
    struct json_helper_sys sys;
    sys.logger = p_obj->logger;
    sys.buffer = p_buffer;
    sys.size = i_buffer;

    struct json_object json;
    int val = json_parse(&sys, &json);
    if (val) {
        msg_Dbg( p_obj, "error: could not parse json!");
        return false;
    }

    const char *status = json_get_str(&json, "status");
    if (status == NULL)
    {
        msg_Warn( p_obj, "status node not found or invalid" );
        json_free(&json);
        return false;
    }
    if (strcmp(status, "ok") != 0)
    {
        msg_Warn( p_obj, "Bad request status" );
        json_free(&json);
        return false;
    }
    const struct json_array *results = json_get_array(&json, "results");
    if (results == NULL)
    {
        msg_Warn( p_obj, "Bad results array or no results" );
        json_free(&json);
        return false;
    }
    p_results->p_results = calloc(results->size, sizeof(acoustid_result_t));
    if ( ! p_results->p_results ) {
        json_free(&json);
        return false;
    }
    p_results->count = results->size;
    for( unsigned int i=0; i<results->size; i++ )
    {
        const struct json_value *resultnode = &results->entries[i];
        if ( resultnode->type == JSON_OBJECT )
        {
            acoustid_result_t *p_result = & p_results->p_results[i];
            double score = json_get_num(&resultnode->object, "score");
            if (score != NAN)
                p_result->d_score = score;
            p_result->psz_id = json_dupstring(&resultnode->object, "id");
            parse_recordings( p_obj, &resultnode->object, p_result );
        }
    }
    json_free(&json);
    return true;
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
    size_t i_buffer;
    void *p_buffer = json_retrieve_document( p_cfg->p_obj, psz_url, &i_buffer );
    free( psz_url );
    if( !p_buffer )
        return VLC_EGENERIC;

    if ( ParseJson( p_cfg->p_obj, p_buffer, i_buffer, & p_data->results ) )
        msg_Dbg( p_cfg->p_obj, "results count == %d", p_data->results.count );
    else
        msg_Dbg( p_cfg->p_obj, "No results" );
    free( p_buffer );

    return VLC_SUCCESS;
}
