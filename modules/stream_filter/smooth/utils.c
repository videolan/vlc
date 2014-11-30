/*****************************************************************************
 * utils.c: misc. stuff
 *****************************************************************************
 * Copyright (C) 1996-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Frédéric Yhuel <fyhuel _AT_ viotech _DOT_ net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *****************************************************************************/

#include "smooth.h"

#include <vlc_es.h>
#include <vlc_block.h>
#include <assert.h>

static int hex_digit( const char c )
{
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= '0' && c<= '9')
        return c - '0';
    else
        return -1;
}

uint8_t *decode_string_hex_to_binary( const char *psz_src )
{
    int i = 0, j = 0, first_digit, second_digit;
    int i_len = strlen( psz_src );
    assert( i_len % 2 == 0 );
    uint8_t *p_data = malloc( i_len / 2 );

    if( !p_data )
        return NULL;

    while( i < i_len )
    {
        first_digit = hex_digit( psz_src[i++] );
        second_digit = hex_digit( psz_src[i++] );
        assert( first_digit >= 0 && second_digit >= 0 );
        p_data[j++] = ( first_digit << 4 ) | second_digit;
    }

    return p_data;
}

quality_level_t * ql_New( void )
{
    quality_level_t *ql = calloc( 1, sizeof( quality_level_t ) );
    if( unlikely( !ql ) ) return NULL;

    ql->Index = -1;
    ARRAY_INIT(ql->custom_attrs);
    return ql;
}

void ql_Free( quality_level_t *qlevel )
{
    free( qlevel->CodecPrivateData );
    FOREACH_ARRAY( custom_attrs_t *p_attrs, qlevel->custom_attrs )
        free( p_attrs->psz_key );
        free( p_attrs->psz_value );
    FOREACH_END()
    ARRAY_RESET(qlevel->custom_attrs);
    free( qlevel );
    qlevel = NULL;
}

chunk_t *chunk_AppendNew( sms_stream_t* sms, uint64_t duration,
                          uint64_t start_time )
{
    chunk_t *chunk = calloc( 1, sizeof( chunk_t ) );
    if( unlikely( chunk == NULL ) )
        return NULL;

    chunk->duration = duration;
    chunk->start_time = start_time;
    chunk->type = UNKNOWN_ES;
    if ( sms->p_lastchunk )
    {
        assert(sms->p_chunks);
        sms->p_lastchunk->p_next = chunk;
    }
    else
    {
        assert(!sms->p_chunks);
        sms->p_chunks = chunk;
        /* Everything starts from first chunk */
        sms->p_nextdownload = chunk;
        sms->p_playback = chunk;
    }
    sms->p_lastchunk = chunk;

    return chunk;
}

void chunk_Free( chunk_t *chunk )
{
    free( chunk->data );
    free( chunk );
}

sms_stream_t * sms_New( void )
{
    sms_stream_t *sms = calloc( 1, sizeof( sms_stream_t ) );
    if( unlikely( !sms ) ) return NULL;

    ARRAY_INIT( sms->qlevels );
    sms->type = UNKNOWN_ES;
    vlc_mutex_init( &sms->chunks_lock );
    return sms;
}

void sms_Free( sms_stream_t *sms )
{
    if ( !sms )
        return;
    FOREACH_ARRAY( quality_level_t *qlevel, sms->qlevels );
    if( qlevel )
        ql_Free( qlevel );
    FOREACH_END();
    ARRAY_RESET( sms->qlevels );

    vlc_mutex_lock( &sms->chunks_lock );
    while( sms->p_chunks )
    {
        chunk_t *p_chunk = sms->p_chunks;
        sms->p_chunks = sms->p_chunks->p_next;
        chunk_Free( p_chunk );
    }
    vlc_mutex_unlock( &sms->chunks_lock );

    vlc_mutex_destroy( &sms->chunks_lock );
    free( sms->name );
    free( sms->url_template );
    free( sms );
}

void bw_stats_put( sms_stream_t *sms, const uint64_t bw )
{
    /* overall bw update */
    if ( bw >= sms->i_obw )
        sms->i_obw = sms->i_obw + ( bw - sms->i_obw ) /
                    (sms->i_obw_samples + 1);
    else
        sms->i_obw = sms->i_obw - ( sms->i_obw - bw ) /
                    (sms->i_obw_samples + 1);
    sms->i_obw_samples++;
    /* limited history bw stats update */
    if ( sms->rgi_bw[0] == 0 )
    { /* first stats */
        for( int i=0; i<SMS_BW_SHORTSTATS; i++ )
            sms->rgi_bw[i] = bw;
    }
    else
    {
        sms->rgi_tidx = (sms->rgi_tidx + 1) % SMS_BW_SHORTSTATS;
        sms->rgi_bw[sms->rgi_tidx] = bw;
    }
}

uint64_t bw_stats_avg( sms_stream_t *sms )
{
    uint64_t sum = sms->rgi_bw[0];
    for( int i=1; i<SMS_BW_SHORTSTATS; i++ )
        sum += sms->rgi_bw[i];
    return sum / SMS_BW_SHORTSTATS;
}

void bw_stats_underrun( sms_stream_t *sms )
{
    sms->i_obw = bw_stats_avg( sms );
    sms->i_obw_samples = SMS_BW_SHORTSTATS;
}

sms_stream_t * sms_get_stream_by_cat( stream_sys_t *p_sys, int i_cat )
{
    assert( p_sys->sms_selected.i_size >= 0 && p_sys->sms_selected.i_size <= 3 );
    FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms_selected );
    if( sms->type == i_cat )
        return sms;
    FOREACH_END();
    return NULL;
}

bool no_more_chunks( stream_sys_t *p_sys )
{
    FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms_selected );
    if ( sms->p_playback )
    {
        return false;
    }
    FOREACH_END();
    return true;
}

void resetChunksState( stream_sys_t *p_sys )
{
    FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms_selected );
    vlc_mutex_lock( &sms->chunks_lock );
    chunk_t *p_chunk = sms->p_playback;
    while( p_chunk )
    {
        FREENULL( p_chunk->data );
        p_chunk->offset = CHUNK_OFFSET_UNSET;
        p_chunk->size = 0;
        p_chunk->read_pos = 0;
        if ( p_chunk == sms->p_nextdownload )
            break;
        p_chunk = p_chunk->p_next;
    }
    sms->p_playback = NULL;
    sms->p_nextdownload = NULL;
    vlc_mutex_unlock( &sms->chunks_lock );
    FOREACH_END();
}
