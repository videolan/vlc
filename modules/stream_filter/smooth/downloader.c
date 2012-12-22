/*****************************************************************************
 * downloader.c: download thread
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_common.h>
#include <assert.h>
#include <vlc_stream.h>
#include <vlc_es.h>

#include "smooth.h"
#include "../../demux/mp4/libmp4.h"

static char *ConstructUrl( const char *template, const char *base_url,
        const uint64_t bandwidth, const uint64_t start_time )
{
    char *frag, *end, *qual;
    char *url_template = strdup( template );
    char *saveptr = NULL;

    qual = strtok_r( url_template, "{", &saveptr );
    strtok_r( NULL, "}", &saveptr );
    frag = strtok_r( NULL, "{", &saveptr );
    strtok_r( NULL, "}", &saveptr );
    end = strtok_r( NULL, "", &saveptr );
    char *url = NULL;

    if( asprintf( &url, "%s/%s%"PRIu64"%s%"PRIu64"%s", base_url, qual,
                bandwidth, frag, start_time, end) < 0 )
    {
       return NULL;
    }

    free( url_template );
    return url;
}

static chunk_t * chunk_Get( sms_stream_t *sms, const int64_t start_time )
{
    int len = vlc_array_count( sms->chunks );
    for( int i = 0; i < len; i++ )
    {
        chunk_t * chunk = vlc_array_item_at_index( sms->chunks, i );
        if( !chunk ) return NULL;

        if( chunk->start_time <= start_time &&
                chunk->start_time + chunk->duration > start_time )
        {
            return chunk;
        }
    }
    return NULL;
}

static unsigned set_track_id( chunk_t *chunk, const unsigned tid )
{
    uint32_t size, type;
    if( !chunk->data )
        return 0;
    uint8_t *slice = chunk->data;
    if( !slice )
        return 0;

    SMS_GET4BYTES( size );
    SMS_GETFOURCC( type );
    assert( type == ATOM_moof );

    SMS_GET4BYTES( size );
    SMS_GETFOURCC( type );
    assert( type == ATOM_mfhd );
    slice += size - 8;

    SMS_GET4BYTES( size );
    SMS_GETFOURCC( type );
    assert( type == ATOM_traf );

    SMS_GET4BYTES( size );
    SMS_GETFOURCC( type );
    if( type != ATOM_tfhd )
        return 0;

    unsigned ret = bswap32( ((uint32_t *)slice)[1] );
    ((uint32_t *)slice)[1] = bswap32( tid );

    return ret;
}

static int sms_Download( stream_t *s, chunk_t *chunk, char *url )
{
    stream_sys_t *p_sys = s->p_sys;

    stream_t *p_ts = stream_UrlNew( s, url );
    free( url );
    if( p_ts == NULL )
        return VLC_EGENERIC;

    int64_t size = stream_Size( p_ts );

    chunk->size = size;
    chunk->offset = p_sys->download.next_chunk_offset;
    p_sys->download.next_chunk_offset += chunk->size;

    chunk->data = malloc( size );

    if( chunk->data == NULL )
    {
        stream_Delete( p_ts );
        return VLC_ENOMEM;
    }

    int read = stream_Read( p_ts, chunk->data, size );
    if( read < size )
    {
        msg_Warn( s, "sms_Download: I requested %"PRIi64" bytes, "\
                "but I got only %i", size, read );
        chunk->data = realloc( chunk->data, read );
    }

    stream_Delete( p_ts );

    vlc_mutex_lock( &p_sys->download.lock_wait );
    int index = es_cat_to_index( chunk->type );
    p_sys->download.lead[index] += chunk->duration;
    vlc_mutex_unlock( &p_sys->download.lock_wait );

    return VLC_SUCCESS;
}

#ifdef DISABLE_BANDWIDTH_ADAPTATION
static unsigned BandwidthAdaptation( stream_t *s,
        sms_stream_t *sms, uint64_t *bandwidth )
{
    return sms->download_qlvl;
}
#else

static unsigned BandwidthAdaptation( stream_t *s,
        sms_stream_t *sms, uint64_t bandwidth )
{
    if( sms->type != VIDEO_ES )
        return sms->download_qlvl;

    uint64_t bw_candidate = 0;
    quality_level_t *qlevel;
    unsigned ret = sms->download_qlvl;

    for( unsigned i = 0; i < sms->qlevel_nb; i++ )
    {
        qlevel = vlc_array_item_at_index( sms->qlevels, i );
        if( unlikely( !qlevel ) )
        {
            msg_Err( s, "Could no get %uth quality level", i );
            return 0;
        }

        if( qlevel->Bitrate < (bandwidth - bandwidth / 3) &&
                qlevel->Bitrate > bw_candidate )
        {
            bw_candidate = qlevel->Bitrate;
            ret = qlevel->id;
        }
    }

    return ret;
}
#endif

static int get_new_chunks( stream_t *s, chunk_t *ck )
{
    stream_sys_t *p_sys = s->p_sys;

    uint8_t *slice = ck->data;
    if( !slice )
        return VLC_EGENERIC;
    uint8_t version, fragment_count;
    uint32_t size, type, flags;
    sms_stream_t *sms;
    UUID_t uuid;
    TfrfBoxDataFields_t *tfrf_df;

    sms = SMS_GET_SELECTED_ST( ck->type );

    SMS_GET4BYTES( size );
    SMS_GETFOURCC( type );
    assert( type == ATOM_moof );

    SMS_GET4BYTES( size );
    SMS_GETFOURCC( type );
    assert( type == ATOM_mfhd );
    slice += size - 8;

    SMS_GET4BYTES( size );
    SMS_GETFOURCC( type );
    assert( type == ATOM_traf );

    for(;;)
    {
        SMS_GET4BYTES( size );
        assert( size > 1 );
        SMS_GETFOURCC( type );
        if( type == ATOM_mdat )
        {
            msg_Err( s, "No uuid box found :-(" );
            return VLC_EGENERIC;
        }
        else if( type == ATOM_uuid )
        {
            GetUUID( &uuid, slice);
            if( !CmpUUID( &uuid, &TfrfBoxUUID ) )
                break;
        }
        slice += size - 8;
    }

    slice += 16;
    SMS_GET1BYTE( version );
    SMS_GET3BYTES( flags );
    SMS_GET1BYTE( fragment_count );

    tfrf_df = calloc( fragment_count, sizeof( TfrfBoxDataFields_t ) );
    if( unlikely( tfrf_df == NULL ) )
        return VLC_EGENERIC;

    for( uint8_t i = 0; i < fragment_count; i++ )
    {
        SMS_GET4or8BYTES( tfrf_df[i].i_fragment_abs_time );
        SMS_GET4or8BYTES( tfrf_df[i].i_fragment_duration );
    }

    msg_Dbg( s, "read box: \"tfrf\" version %d, flags 0x%x, "\
            "fragment count %"PRIu8, version, flags, fragment_count );

    for( uint8_t i = 0; i < fragment_count; i++ )
    {
        int64_t dur = tfrf_df[i].i_fragment_duration;
        int64_t stime = tfrf_df[i].i_fragment_abs_time;
        msg_Dbg( s, "\"tfrf\" fragment duration %"PRIu64", "\
                    "fragment abs time %"PRIu64, dur, stime);

        if( !chunk_Get( sms, stime + dur ) )
            chunk_New( sms, dur, stime );
    }
    free( tfrf_df );

    return VLC_SUCCESS;
}

#define STRA_SIZE 334
#define SMOO_SIZE (STRA_SIZE * 3 + 24) /* 1026 */

/* SmooBox is a very simple MP4 box, used only to pass information
 * to the demux layer. As this box is not aimed to travel accross networks,
 * simplicity of the design is better than compactness */
static int build_smoo_box( stream_t *s, uint8_t *smoo_box )
{
    stream_sys_t *p_sys = s->p_sys;
    sms_stream_t *sms = NULL;
    uint32_t FourCC;

    /* smoo */
    memset( smoo_box, 0, SMOO_SIZE );
    smoo_box[2] = (SMOO_SIZE & 0xff00)>>8;
    smoo_box[3] = SMOO_SIZE & 0xff;
    smoo_box[4] = 'u';
    smoo_box[5] = 'u';
    smoo_box[6] = 'i';
    smoo_box[7] = 'd';

    /* UUID is e1da72ba-24d7-43c3-a6a5-1b5759a1a92c */
    ((uint32_t *)smoo_box)[2] = bswap32( 0xe1da72ba );
    ((uint32_t *)smoo_box)[3] = bswap32( 0x24d743c3 );
    ((uint32_t *)smoo_box)[4] = bswap32( 0xa6a51b57 );
    ((uint32_t *)smoo_box)[5] = bswap32( 0x59a1a92c );

    uint8_t *stra_box;
    for( int i = 0; i < 3; i++ )
    {
        sms = NULL;
        int cat = UNKNOWN_ES;
        stra_box = smoo_box + i * STRA_SIZE;

        stra_box[26] = (STRA_SIZE & 0xff00)>>8;
        stra_box[27] = STRA_SIZE & 0xff;
        stra_box[28] = 'u';
        stra_box[29] = 'u';
        stra_box[30] = 'i';
        stra_box[31] = 'd';

        /* UUID is b03ef770-33bd-4bac-96c7-bf25f97e2447 */
        ((uint32_t *)stra_box)[8] = bswap32( 0xb03ef770 );
        ((uint32_t *)stra_box)[9] = bswap32( 0x33bd4bac );
        ((uint32_t *)stra_box)[10] = bswap32( 0x96c7bf25 );
        ((uint32_t *)stra_box)[11] = bswap32( 0xf97e2447 );

        cat = index_to_es_cat( i );
        stra_box[48] = cat;
        sms = SMS_GET_SELECTED_ST( cat );

        stra_box[49] = 0; /* reserved */
        if( sms == NULL )
            continue;
        stra_box[50] = (sms->id & 0xff00)>>8;
        stra_box[51] = sms->id & 0xff;

        ((uint32_t *)stra_box)[13] = bswap32( sms->timescale );
        ((uint64_t *)stra_box)[7] = bswap64( p_sys->vod_duration );

        quality_level_t * qlvl = get_qlevel( sms, sms->download_qlvl );

        FourCC = qlvl->FourCC ? qlvl->FourCC : sms->default_FourCC;
        ((uint32_t *)stra_box)[16] = bswap32( FourCC );
        ((uint32_t *)stra_box)[17] = bswap32( qlvl->Bitrate );
        ((uint32_t *)stra_box)[18] = bswap32( qlvl->MaxWidth );
        ((uint32_t *)stra_box)[19] = bswap32( qlvl->MaxHeight );
        ((uint32_t *)stra_box)[20] = bswap32( qlvl->SamplingRate );
        ((uint32_t *)stra_box)[21] = bswap32( qlvl->Channels );
        ((uint32_t *)stra_box)[22] = bswap32( qlvl->BitsPerSample );
        ((uint32_t *)stra_box)[23] = bswap32( qlvl->AudioTag );
        ((uint16_t *)stra_box)[48] = bswap16( qlvl->nBlockAlign );

        if( !qlvl->CodecPrivateData )
            continue;
        stra_box[98] = stra_box[99] = stra_box[100] = 0; /* reserved */
        assert( strlen( qlvl->CodecPrivateData ) < 512 );
        stra_box[101] = strlen( qlvl->CodecPrivateData ) / 2;
        uint8_t *binary_cpd = decode_string_hex_to_binary( qlvl->CodecPrivateData );
        memcpy( stra_box + 102, binary_cpd, stra_box[101] );
        free( binary_cpd );
    }

    return VLC_SUCCESS;
}

static chunk_t *build_init_chunk( stream_t *s )
{
    chunk_t *ret = calloc( 1, sizeof( chunk_t ) );
    if( unlikely( ret == NULL ) )
        goto build_init_chunk_error;

    ret->size = SMOO_SIZE;
    ret->data = malloc( SMOO_SIZE );
    if( !ret->data )
        goto build_init_chunk_error;

    if( build_smoo_box( s, ret->data ) == VLC_SUCCESS)
        return ret;

    free( ret->data );
build_init_chunk_error:
    free( ret );
    msg_Err( s, "build_init_chunk failed" );
    return NULL;
}

static int Download( stream_t *s, sms_stream_t *sms )
{
    stream_sys_t *p_sys = s->p_sys;

    int index = es_cat_to_index( sms->type );
    int64_t start_time = p_sys->download.lead[index];

    quality_level_t *qlevel = get_qlevel( sms, sms->download_qlvl );
    if( unlikely( !qlevel ) )
    {
        msg_Err( s, "Could not get quality level id %u", sms->download_qlvl );
        return VLC_EGENERIC;
    }


    chunk_t *chunk = chunk_Get( sms, start_time );
    if( !chunk )
    {
        msg_Warn( s, "Could not find a chunk for stream %s, "\
                "start time = %"PRIu64"", sms->name, start_time );
        return VLC_EGENERIC;
    }
    if( chunk->data != NULL )
    {
        /* Segment already downloaded */
        msg_Warn( s, "Segment already downloaded" );
        return VLC_SUCCESS;
    }

    chunk->type = sms->type;

    char *url = ConstructUrl( sms->url_template, p_sys->base_url,
                                  qlevel->Bitrate, chunk->start_time );
    if( !url )
    {
        msg_Err( s, "ConstructUrl returned NULL" );
        return VLC_EGENERIC;
    }

    /* sanity check - can we download this chunk on time? */
    uint64_t avg_bw = sms_queue_avg( p_sys->bws );
    if( (avg_bw > 0) && (qlevel->Bitrate > 0) )
    {
        /* duration in ms */
        unsigned chunk_duration = chunk->duration * 1000 / sms->timescale;
        uint64_t size = chunk_duration * qlevel->Bitrate / 1000; /* bits */
        unsigned estimated = size * 1000 / avg_bw;
        if( estimated > chunk_duration )
        {
            msg_Warn( s,"downloading of chunk %d would take %d ms, "\
                    "which is longer than its playback (%d ms)",
                        chunk->sequence, estimated, chunk_duration );
        }
    }

    mtime_t start = mdate();
    if( sms_Download( s, chunk, url ) != VLC_SUCCESS )
    {
        msg_Err( s, "downloaded chunk %u from stream %s at quality\
            %u failed", chunk->sequence, sms->name, qlevel->Bitrate );
        return VLC_EGENERIC;
    }
    mtime_t duration = mdate() - start;

    unsigned real_id = set_track_id( chunk, sms->id );
    if( real_id == 0)
    {
        msg_Err( s, "tfhd box not found or invalid chunk" );
        return VLC_EGENERIC;
    }

    //msg_Dbg( s, "chunk ID was %i and is now %i", real_id, sms->id );

    if( p_sys->b_live )
        get_new_chunks( s, chunk );

    vlc_mutex_lock( &p_sys->download.lock_wait );
    vlc_array_append( p_sys->download.chunks, chunk );
    vlc_cond_signal( &p_sys->download.wait );
    vlc_mutex_unlock( &p_sys->download.lock_wait );

    msg_Info( s, "downloaded chunk %d from stream %s at quality %u",
                chunk->sequence, sms->name, qlevel->Bitrate );

    uint64_t actual_lead = chunk->start_time + chunk->duration;
    int ind = es_cat_to_index( sms->type );
    p_sys->download.ck_index[ind] = chunk->sequence;
    p_sys->download.lead[ind] = __MIN( p_sys->download.lead[ind], actual_lead );

    if( sms->type == VIDEO_ES ||
            ( !SMS_GET_SELECTED_ST( VIDEO_ES ) && sms->type == AUDIO_ES ) )
    {
        p_sys->playback.toffset = __MIN( p_sys->playback.toffset,
                                            (uint64_t)chunk->start_time );
    }

    unsigned dur_ms = __MAX( 1, duration / 1000 );
    uint64_t bw = chunk->size * 8 * 1000 / dur_ms; /* bits / s */
    if( sms_queue_put( p_sys->bws, bw ) != VLC_SUCCESS )
        return VLC_EGENERIC;
    avg_bw = sms_queue_avg( p_sys->bws );

    if( sms->type != VIDEO_ES )
        return VLC_SUCCESS;

    /* Track could get disabled in mp4 demux if we trigger adaption too soon. */
    if( chunk->sequence <= 1 )
        return VLC_SUCCESS;

    unsigned new_qlevel_id = BandwidthAdaptation( s, sms, avg_bw );
    quality_level_t *new_qlevel = get_qlevel( sms, new_qlevel_id );
    if( unlikely( !new_qlevel ) )
    {
        msg_Err( s, "Could not get quality level id %u", new_qlevel_id );
        return VLC_EGENERIC;
    }

    if( new_qlevel->Bitrate != qlevel->Bitrate )
    {
        msg_Warn( s, "detected %s bandwidth (%u) stream",
                 (new_qlevel->Bitrate >= qlevel->Bitrate) ? "faster" : "lower",
                 new_qlevel->Bitrate );

        sms->download_qlvl = new_qlevel_id;
    }

    if( new_qlevel->MaxWidth != qlevel->MaxWidth ||
        new_qlevel->MaxHeight != qlevel->MaxHeight )
    {
        chunk_t *new_init_ck = build_init_chunk( s );
        if( !new_init_ck )
        {
            return VLC_EGENERIC;
        }

        new_init_ck->offset = p_sys->download.next_chunk_offset;
        p_sys->download.next_chunk_offset += new_init_ck->size;

        vlc_mutex_lock( &p_sys->download.lock_wait );
        vlc_array_append( p_sys->download.chunks, new_init_ck );
        vlc_array_append( p_sys->init_chunks, new_init_ck );
        vlc_mutex_unlock( &p_sys->download.lock_wait );
    }
    return VLC_SUCCESS;
}

static inline int64_t get_lead( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;
    int64_t lead = 0;
    int64_t alead = p_sys->download.lead[es_cat_to_index( AUDIO_ES )];
    int64_t vlead = p_sys->download.lead[es_cat_to_index( VIDEO_ES )];
    bool video = SMS_GET_SELECTED_ST( VIDEO_ES ) ? true : false;
    bool audio = SMS_GET_SELECTED_ST( AUDIO_ES ) ? true : false;

    if( video && audio )
        lead = __MIN( vlead, alead );
    else if( video )
        lead = vlead;
    else
        lead = alead;

    lead -= p_sys->playback.toffset;
    return lead;
}

static int next_track( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;
    uint64_t tmp, min = 0;
    int cat, ret = UNKNOWN_ES;
    for( int i = 0; i < 3; i++ )
    {
        tmp = p_sys->download.lead[i];
        cat = index_to_es_cat( i );
        if( (!min || tmp < min) && SMS_GET_SELECTED_ST( cat ) )
        {
            min = tmp;
            ret = cat;
        }
    }
    return ret;
}

void* sms_Thread( void *p_this )
{
    stream_t *s = (stream_t *)p_this;
    stream_sys_t *p_sys = s->p_sys;
    sms_stream_t *sms = NULL;
    chunk_t *chunk;

    int canc = vlc_savecancel();

    /* We compute the average bandwidth of the 4 last downloaded
     * chunks, but feel free to replace '4' by whatever you wish */
    p_sys->bws = sms_queue_init( 4 );
    if( !p_sys->bws )
        goto cancel;

    chunk_t *init_ck = build_init_chunk( s );
    if( !init_ck )
        goto cancel;

    vlc_mutex_lock( &p_sys->download.lock_wait );
    vlc_array_append( p_sys->download.chunks, init_ck );
    vlc_array_append( p_sys->init_chunks, init_ck );
    vlc_mutex_unlock( &p_sys->download.lock_wait );

    p_sys->download.next_chunk_offset = init_ck->size;

    /* XXX Sometimes, the video stream is cut into pieces of one exact length,
     * while the audio stream fragments can't be made to match exactly,
     * and for some reason the n^th advertised video fragment is related to
     * the n+1^th advertised audio chunk or vice versa */

    int64_t start_time = 0, lead = 0;

    for( int i = 0; i < 3; i++ )
    {
        sms = SMS_GET_SELECTED_ST( index_to_es_cat( i ) );
        if( sms )
        {
            chunk = vlc_array_item_at_index( sms->chunks, 0 );
            p_sys->download.lead[i] = chunk->start_time + p_sys->timescale / 1000;
            if( !start_time )
                start_time = chunk->start_time;

            if( Download( s, sms ) != VLC_SUCCESS )
                goto cancel;
        }
    }

    while( 1 )
    {
        /* XXX replace magic number 10 by a value depending on
         * LookAheadFragmentCount and DVRWindowLength */
        vlc_mutex_lock( &p_sys->download.lock_wait );

        if( p_sys->b_close )
        {
            vlc_mutex_unlock( &p_sys->download.lock_wait );
            break;
        }

        lead = get_lead( s );

        while( lead > 10 * p_sys->timescale + start_time || NO_MORE_CHUNKS )
        {
            vlc_cond_wait( &p_sys->download.wait, &p_sys->download.lock_wait );
            lead = get_lead( s );

            if( p_sys->b_close )
                break;
        }

        if( p_sys->b_tseek )
        {
            int count = vlc_array_count( p_sys->download.chunks );
            chunk_t *ck = NULL;
            for( int i = 0; i < count; i++ )
            {
                ck = vlc_array_item_at_index( p_sys->download.chunks, i );
                if( unlikely( !ck ) )
                    goto cancel;
                ck->read_pos = 0;
                if( ck->data == NULL )
                    continue;
                FREENULL( ck->data );
            }

            vlc_array_destroy( p_sys->download.chunks );
            p_sys->download.chunks = vlc_array_new();

            p_sys->playback.toffset = p_sys->time_pos;
            for( int i = 0; i < 3; i++ )
            {
                p_sys->download.lead[i] = p_sys->time_pos;
                p_sys->download.ck_index[i] = 0;
            }
            p_sys->download.next_chunk_offset = 0;

            p_sys->playback.boffset = 0;
            p_sys->playback.index = 0;

            chunk_t *new_init_ck = build_init_chunk( s );
            if( !new_init_ck )
                goto cancel;

            new_init_ck->offset = p_sys->download.next_chunk_offset;
            p_sys->download.next_chunk_offset += new_init_ck->size;

            vlc_array_append( p_sys->download.chunks, new_init_ck );
            vlc_array_append( p_sys->init_chunks, new_init_ck );
            p_sys->b_tseek = false;
        }
        vlc_mutex_unlock( &p_sys->download.lock_wait );

        sms = SMS_GET_SELECTED_ST( next_track( s ) );
        if( Download( s, sms ) != VLC_SUCCESS )
            goto cancel;
    }

cancel:
    p_sys->b_error = true;
    msg_Warn(s, "Canceling download thread!");
    vlc_restorecancel( canc );
    return NULL;
}
