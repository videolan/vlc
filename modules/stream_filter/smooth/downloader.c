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

#include "smooth.h"

#include <assert.h>
#include <vlc_stream.h>
#include <vlc_charset.h>

#include "../../demux/mp4/libmp4.h"

static bool Replace( char **ppsz_string, off_t off, const char *psz_old,
                     const char *psz_new )
{
    const size_t i_oldlen = strlen( psz_old );
    const size_t i_newlen = strlen( psz_new );
    size_t i_stringlen = strlen( *ppsz_string );

    if ( i_newlen > i_oldlen )
    {
        i_stringlen += i_newlen - i_oldlen;
        char *psz_realloc = realloc( *ppsz_string, i_stringlen + 1 );
        if( !psz_realloc )
            return false;
        *ppsz_string = psz_realloc;
    }
    memmove( *ppsz_string + off + i_newlen,
             *ppsz_string + off + i_oldlen,
             i_stringlen - off - i_newlen );
    strncpy( *ppsz_string + off, psz_new, i_newlen );
    (*ppsz_string)[i_stringlen] = 0;

    return true;
}

static char *ConstructUrl( const char *psz_template, const char *psz_base_url,
                           const quality_level_t *p_qlevel, const uint64_t i_start_time )
{
    char *psz_path = strdup( psz_template );
    if ( !psz_path )
        return NULL;

    char *psz_start;
    while( true )
    {
        if ( (psz_start = strstr( psz_path, "{bitrate}" )) )
        {
            char *psz_bitrate = NULL;
            if ( us_asprintf( &psz_bitrate, "%u", p_qlevel->Bitrate ) < 0 ||
                 ! Replace( &psz_path, psz_start - psz_path, "{bitrate}", psz_bitrate ) )
            {
                free( psz_bitrate );
                free( psz_path );
                return false;
            }
            free( psz_bitrate );
        }
        else if ( (psz_start = strstr( psz_path, "{start time}" )) ||
                  (psz_start = strstr( psz_path, "{start_time}" )) )
        {
            psz_start[6] = ' ';
            char *psz_starttime = NULL;
            if ( us_asprintf( &psz_starttime, "%"PRIu64, i_start_time ) < 0 ||
                 ! Replace( &psz_path, psz_start - psz_path, "{start time}", psz_starttime ) )
            {
                free( psz_starttime );
                free( psz_path );
                return false;
            }
            free( psz_starttime );
        }
        else if ( (psz_start = strstr( psz_path, "{CustomAttributes}" )) )
        {
            char *psz_attributes = NULL;
            FOREACH_ARRAY( const custom_attrs_t *p_attrs, p_qlevel->custom_attrs )
            if ( asprintf( &psz_attributes,
                           psz_attributes ? "%s,%s=%s" : "%s%s=%s",
                           psz_attributes ? psz_attributes : "",
                           p_attrs->psz_key, p_attrs->psz_value ) < 0 )
                    break;
            FOREACH_END()
            if ( !psz_attributes ||
                 ! Replace( &psz_path, psz_start - psz_path, "{CustomAttributes}", psz_attributes ) )
            {
                free( psz_attributes );
                free( psz_path );
                return false;
            }
            free( psz_attributes );
        }
        else
            break;
    }

    char *psz_url;
    if( asprintf( &psz_url, "%s/%s", psz_base_url, psz_path ) < 0 )
    {
        free( psz_path );
        return NULL;
    }
    free( psz_path );
    return psz_url;
}

static chunk_t * chunk_Get( sms_stream_t *sms, const uint64_t start_time )
{
    chunk_t *p_chunk = sms->p_chunks;
    while( p_chunk )
    {
        if( p_chunk->start_time <= start_time &&
                p_chunk->start_time + p_chunk->duration > start_time )
            break;
        p_chunk = p_chunk->p_next;
    }
    return p_chunk;
}

static unsigned set_track_id( chunk_t *chunk, const unsigned tid )
{
    uint32_t size, type;
    if( !chunk->data || chunk->size < 32 )
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
    stream_t *p_ts = stream_UrlNew( s, url );
    free( url );
    if( p_ts == NULL )
        return VLC_EGENERIC;

    int64_t size = stream_Size( p_ts );
    if ( size < 0 )
    {
        stream_Delete( p_ts );
        return VLC_EGENERIC;
    }

    uint8_t *p_data = malloc( size );
    if( p_data == NULL )
    {
        stream_Delete( p_ts );
        return VLC_ENOMEM;
    }

    int read = stream_Read( p_ts, p_data, size );
    if( read < size && read > 0 )
    {
        msg_Warn( s, "sms_Download: I requested %"PRIi64" bytes, "\
                "but I got only %i", size, read );
        p_data = realloc( p_data, read );
    }

    chunk->data = p_data;
    chunk->size = (uint64_t) (read > 0 ? read : 0);

    stream_Delete( p_ts );

    return VLC_SUCCESS;
}

#ifdef DISABLE_BANDWIDTH_ADAPTATION
static quality_level_t *
BandwidthAdaptation( stream_t *s, sms_stream_t *sms,
                     uint64_t obw, uint64_t i_duration,
                     bool b_starved )
{
    VLC_UNUSED(obw);
    VLC_UNUSED(s);
    VLC_UNUSED(i_duration);
    VLC_UNUSED(b_starved);
    return sms->current_qlvl;
}
#else

static quality_level_t *
BandwidthAdaptation( stream_t *s, sms_stream_t *sms,
                     uint64_t obw, uint64_t i_duration,
                     bool b_starved )
{
    quality_level_t *ret = NULL;

    assert( sms->current_qlvl );
    if ( sms->qlevels.i_size < 2 )
        return sms->qlevels.p_elems[0];

    if ( b_starved )
    {
        //TODO: do something on starvation post first buffering
        //   s->p_sys->i_probe_length *= 2;
    }

    /* PASS 1 */
    quality_level_t *lowest = sms->qlevels.p_elems[0];
    FOREACH_ARRAY( quality_level_t *qlevel, sms->qlevels );
    if ( qlevel->Bitrate >= obw )
    {
        qlevel->i_validation_length -= i_duration;
        qlevel->i_validation_length = __MAX(qlevel->i_validation_length, - s->p_sys->i_probe_length);
    }
    else
    {
        qlevel->i_validation_length += i_duration;
        qlevel->i_validation_length = __MIN(qlevel->i_validation_length, s->p_sys->i_probe_length);
    }
    if ( qlevel->Bitrate < lowest->Bitrate )
        lowest = qlevel;
    FOREACH_END();

    /* PASS 2 */
    if ( sms->current_qlvl->i_validation_length == s->p_sys->i_probe_length )
    {
        /* might upgrade */
        ret = sms->current_qlvl;
    }
    else if ( sms->current_qlvl->i_validation_length >= 0 )
    {
        /* do nothing */
        ret = sms->current_qlvl;
        msg_Dbg( s, "bw current:%uKB/s avg:%"PRIu64"KB/s qualified %"PRId64"%%",
                (ret->Bitrate) / (8 * 1024),
                 obw / (8 * 1024),
                 ( ret->i_validation_length*1000 / s->p_sys->i_probe_length ) /10 );
        return ret;
    }
    else
    {
        /* downgrading */
        ret = lowest;
    }

    /* might upgrade */
    FOREACH_ARRAY( quality_level_t *qlevel, sms->qlevels );
    if( qlevel->Bitrate <= obw &&
            ret->Bitrate <= qlevel->Bitrate &&
            qlevel->i_validation_length >= 0 &&
            qlevel->i_validation_length >= ret->i_validation_length )
    {
        ret = qlevel;
    }
    FOREACH_END();

    msg_Dbg( s, "bw reselected:%uKB/s avg:%"PRIu64"KB/s qualified %"PRId64"%%",
            (ret->Bitrate) / (8 * 1024),
             obw / (8 * 1024),
             ( ret->i_validation_length*1000 / s->p_sys->i_probe_length ) /10 );

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
        uint64_t dur = tfrf_df[i].i_fragment_duration;
        uint64_t stime = tfrf_df[i].i_fragment_abs_time;
        msg_Dbg( s, "\"tfrf\" fragment duration %"PRIu64", "\
                    "fragment abs time %"PRIu64, dur, stime);

        if( !chunk_Get( sms, stime + dur ) )
            chunk_AppendNew( sms, dur, stime );
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

        const quality_level_t *qlvl = sms->current_qlvl;
        if ( qlvl )
        {
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
            stra_box[101] = strlen( qlvl->CodecPrivateData ) / 2;
            if ( stra_box[101] > STRA_SIZE - 102 )
                stra_box[101] = STRA_SIZE - 102;
            uint8_t *binary_cpd = decode_string_hex_to_binary( qlvl->CodecPrivateData );
            memcpy( stra_box + 102, binary_cpd, stra_box[101] );
            free( binary_cpd );
        }
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

    assert( sms->p_nextdownload );
    assert( sms->p_nextdownload->data == NULL );
    assert( sms->current_qlvl );

    int index = es_cat_to_index( sms->type );
    if ( unlikely( index == -1 ) )
    {
        msg_Err( s, "invalid stream type" );
        return VLC_EGENERIC;
    }

    chunk_t *chunk = sms->p_nextdownload;
    if( !chunk )
    {
        msg_Warn( s, "Could not find a chunk for stream %s", sms->name );
        return VLC_EGENERIC;
    }
    if( chunk->data != NULL )
    {
        /* Segment already downloaded */
        msg_Warn( s, "Segment already downloaded" );
        return VLC_SUCCESS;
    }

    chunk->type = sms->type;

    char *url = ConstructUrl( sms->url_template, p_sys->download.base_url,
                              sms->current_qlvl, chunk->start_time );
    if( !url )
    {
        msg_Err( s, "ConstructUrl returned NULL" );
        return VLC_EGENERIC;
    }

    /* sanity check - can we download this chunk on time? */
    if( (sms->i_obw > 0) && (sms->current_qlvl->Bitrate > 0) )
    {
        /* duration in ms */
        unsigned chunk_duration = chunk->duration * 1000 / sms->timescale;
        uint64_t size = chunk_duration * sms->current_qlvl->Bitrate / 1000; /* bits */
        unsigned estimated = size * 1000 / sms->i_obw;
        if( estimated > chunk_duration )
        {
            msg_Warn( s,"downloading of chunk @%"PRIu64" would take %d ms, "
                        "which is longer than its playback (%d ms)",
                        chunk->start_time, estimated, chunk_duration );
        }
    }

    mtime_t duration = mdate();
    if( sms_Download( s, chunk, url ) != VLC_SUCCESS )
    {
        msg_Err( s, "downloaded chunk @%"PRIu64" from stream %s at quality"
                    " %u *failed*", chunk->start_time, sms->name, sms->current_qlvl->Bitrate );
        return VLC_EGENERIC;
    }
    duration = mdate() - duration;

    unsigned real_id = set_track_id( chunk, sms->id );
    if( real_id == 0)
    {
        msg_Err( s, "tfhd box not found or invalid chunk" );
        return VLC_EGENERIC;
    }

    if( p_sys->b_live )
    {
        get_new_chunks( s, chunk );
    }

    msg_Info( s, "downloaded chunk @%"PRIu64" from stream %s at quality %u",
                 chunk->start_time, sms->name, sms->current_qlvl->Bitrate );

    if (likely( duration ))
        bw_stats_put( sms, chunk->size * 8 * CLOCK_FREQ / duration ); /* bits / s */

    /* Track could get disabled in mp4 demux if we trigger adaption too soon.
       And we don't need adaptation on last chunk */
    if( sms->p_chunks == NULL || sms->p_chunks == sms->p_lastchunk )
        return VLC_SUCCESS;

    bool b_starved = false;
    vlc_mutex_lock( &p_sys->playback.lock );
    if ( &p_sys->playback.b_underrun )
    {
        p_sys->playback.b_underrun = false;
        bw_stats_underrun( sms );
        b_starved = true;
    }
    vlc_mutex_unlock( &p_sys->playback.lock );

    quality_level_t *new_qlevel = BandwidthAdaptation( s, sms, sms->i_obw,
                                                       duration, b_starved );
    assert(new_qlevel);

    if( sms->qlevels.i_size < 2 )
    {
        assert(new_qlevel == sms->current_qlvl);
        return VLC_SUCCESS;
    }

    vlc_mutex_lock( &p_sys->playback.lock );
    if ( p_sys->playback.init.p_datachunk == NULL && /* Don't chain/nest reinits */
         new_qlevel != sms->current_qlvl )
    {
        msg_Warn( s, "detected %s bandwidth (%u) stream",
                  (new_qlevel->Bitrate >= sms->current_qlvl->Bitrate) ? "faster" : "lower",
                  new_qlevel->Bitrate );

        quality_level_t *qlvl_backup = sms->current_qlvl;
        sms->current_qlvl = new_qlevel;
        chunk_t *new_init_ck = build_init_chunk( s );
        if( new_init_ck )
        {
            p_sys->playback.init.p_datachunk = new_init_ck;
            p_sys->playback.init.p_startchunk = chunk->p_next; /* to send before that one */
            assert( chunk->p_next && chunk != sms->p_lastchunk );
        }
        else
            sms->current_qlvl = qlvl_backup;
    }
    vlc_mutex_unlock( &p_sys->playback.lock );

    return VLC_SUCCESS;
}

static inline bool reached_download_limit( sms_stream_t *sms, unsigned int i_max_chunks,
                                           uint64_t i_max_time )
{
    if ( !sms->p_nextdownload )
        return true;

    if ( sms->p_playback == sms->p_nextdownload ) /* chunk can be > max_time */
        return false;

    const chunk_t *p_head = sms->p_playback;
    if ( !p_head )
        return true;

    unsigned int i_chunk_count = 0;
    uint64_t i_total_time = 0;
    const chunk_t *p_tail = sms->p_nextdownload;
    while( p_head )
    {
        i_total_time += p_head->duration * CLOCK_FREQ / sms->timescale;
        if ( i_max_time && i_total_time >= i_max_time )
        {
            return true;
        }
        else if ( i_max_chunks && i_chunk_count >= i_max_chunks )
        {
            return true;
        }

        if ( p_head == p_tail )
            break;

        p_head = p_head->p_next;
        i_chunk_count += 1;
    }

    return false;
}

static bool all_reached_download_limit( stream_sys_t *p_sys, unsigned int i_max_chunks,
                                        uint64_t i_max_time )
{
    FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms_selected );
    vlc_mutex_lock( &sms->chunks_lock );
    bool b_ret = reached_download_limit( sms, i_max_chunks, i_max_time );
    vlc_mutex_unlock( &sms->chunks_lock );
    if ( ! b_ret )
        return false;
    FOREACH_END();
    return true;
}

/* Returns the first download chunk by time in the download queue */
static sms_stream_t *next_download_stream( stream_sys_t *p_sys )
{
    sms_stream_t *p_candidate = NULL;
    FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms_selected );
    vlc_mutex_lock( &sms->chunks_lock );
    if ( !sms->p_nextdownload )
    {
        vlc_mutex_unlock( &sms->chunks_lock );
        continue;
    }
    if ( p_candidate == NULL ||
         sms->p_nextdownload->start_time < p_candidate->p_nextdownload->start_time )
        p_candidate = sms;
    vlc_mutex_unlock( &sms->chunks_lock );
    FOREACH_END();
    return p_candidate;
}

void* sms_Thread( void *p_this )
{
    stream_t *s = (stream_t *)p_this;
    stream_sys_t *p_sys = s->p_sys;

    int canc = vlc_savecancel();

    chunk_t *init_ck = build_init_chunk( s );
    if( !init_ck )
        goto cancel;

    vlc_mutex_lock( &p_sys->playback.lock );
    p_sys->playback.init.p_datachunk = init_ck;
    p_sys->playback.init.p_startchunk = NULL; /* before any */
    vlc_mutex_unlock( &p_sys->playback.lock );
    vlc_cond_signal( &p_sys->playback.wait ); /* demuxer in Open() can start reading */

    int64_t i_pts_delay = 0;

    /* Buffer up first chunks */
    int i_initial_buffering = p_sys->download.lookahead_count;
    if ( !i_initial_buffering )
        i_initial_buffering = 3;
    for( int i = 0; i < i_initial_buffering; i++ )
    {
        FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms_selected );
        vlc_mutex_lock( &sms->chunks_lock );
        if ( sms->p_nextdownload )
        {
            mtime_t duration = mdate();
            if( Download( s, sms ) != VLC_SUCCESS )
            {
                vlc_mutex_unlock( &sms->chunks_lock );
                goto cancel;
            }
            duration = mdate() - duration;
            if (likely( duration ))
                bw_stats_put( sms, sms->p_nextdownload->size * 8 * CLOCK_FREQ / duration ); /* bits / s */
            sms->p_nextdownload = sms->p_nextdownload->p_next;
        }
        vlc_mutex_unlock( &sms->chunks_lock );
        FOREACH_END();
    }
    vlc_cond_signal( &p_sys->playback.wait );

    while( !p_sys->b_close )
    {
        if ( !p_sys->b_live || !p_sys->download.lookahead_count )
            stream_Control( s, STREAM_GET_PTS_DELAY, &i_pts_delay );

        vlc_mutex_lock( &p_sys->lock );
        while( all_reached_download_limit( p_sys,
                                           p_sys->download.lookahead_count,
                                           i_pts_delay ) )
        {
            vlc_cond_wait( &p_sys->download.wait, &p_sys->lock );
            if( p_sys->b_close )
                break;
        }

        if( p_sys->b_close )
        {
            vlc_mutex_unlock( &p_sys->lock );
            break;
        }

        sms_stream_t *sms = next_download_stream( p_sys );
        if ( sms )
        {
            vlc_mutex_lock( &sms->chunks_lock );
            vlc_mutex_unlock( &p_sys->lock );

            if( Download( s, sms ) != VLC_SUCCESS )
            {
                vlc_mutex_unlock( &sms->chunks_lock );
                break;
            }
            vlc_cond_signal( &p_sys->playback.wait );

            if ( sms->p_nextdownload ) /* could have been modified by seek */
                sms->p_nextdownload = sms->p_nextdownload->p_next;

            vlc_mutex_unlock( &sms->chunks_lock );
        }
        else
            vlc_mutex_unlock( &p_sys->lock );

        vlc_testcancel();
    }

cancel:
    p_sys->b_error = true;
    msg_Dbg(s, "Canceling download thread!");
    vlc_restorecancel( canc );
    return NULL;
}
