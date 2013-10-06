/*****************************************************************************
 * smooth.c: Smooth Streaming stream filter
 *****************************************************************************
 * Copyright (C) 1996-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Frédéric Yhuel <fyhuel _AT_ viotech _DOT_ net>
 * Heavily inspired by HLS module of Jean-Paul Saman
 * <jpsaman _AT_ videolan _DOT_ org>
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

#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <assert.h>
#include <inttypes.h>

#include <vlc_xml.h>
#include <vlc_charset.h>
#include <vlc_stream.h>
#include <vlc_es.h>
#include <vlc_codecs.h>

#include "smooth.h"
#include "../../demux/mp4/libmp4.h"

/* I make the assumption that when the demux want to do a *time* seek,
 * then p_sys->download->boffset > FAKE_STREAM_SIZE, and thus FAKE_STREAM_SIZE
 * should be small enough. 1000 seems to be a sensible choice. See also
 * chunk_Seek() comments to understand properly */
#define FAKE_STREAM_SIZE 1000
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
    set_description( N_("Smooth Streaming") )
    set_shortname( "Smooth Streaming")
    add_shortcut( "smooth" )
    set_capability( "stream_filter", 30 )
    set_callbacks( Open, Close )
vlc_module_end()

static int   Read( stream_t *, void *, unsigned );
static int   Peek( stream_t *, const uint8_t **, unsigned );
static int   Control( stream_t *, int , va_list );

static bool isSmoothStreaming( stream_t *s )
{
    const uint8_t *peek;
    const char *needle = "<SmoothStreamingMedia";
    const char *encoding = NULL;
    bool ret = false;

    int i_size = stream_Peek( s->p_source, &peek, 512 );
    if( i_size < 512 )
        return false;

    char *peeked = malloc( 512 );
    if( unlikely( !peeked ) )
        return false;

    memcpy( peeked, peek, 512 );
    peeked[511] = peeked[510] = '\0';

    if( strstr( (const char *)peeked, needle ) != NULL )
        ret = true;
    else
    /* maybe it's utf-16 encoding, should we also test other encodings? */
    {
        if( !memcmp( peeked, "\xFF\xFE", 2 ) )
            encoding = "UTF-16LE";
        else if( !memcmp( peeked, "\xFE\xFF", 2 ) )
            encoding = "UTF-16BE";
        else
        {
            free( peeked );
            return false;
        }
        peeked = FromCharset( encoding, peeked, 512 );

        if( peeked != NULL && strstr( peeked, needle ) != NULL )
            ret = true;
    }
    free( peeked );
    return ret;
}

#if 0
static void print_chunk( stream_t *s, chunk_t *ck )
{
    msg_Info( s, "chunk %u type %i: duration is %"PRIu64", stime is %"PRIu64", "\
            "size is %i, offset is %"PRIu64", read_pos is %i.",
            ck->sequence, ck->type, ck->duration,
            ck->start_time, ck->size, ck->offset, ck->read_pos );
}
#endif

static int parse_Manifest( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;
    xml_t *vlc_xml = NULL;
    xml_reader_t *vlc_reader = NULL;
    int type = UNKNOWN_ES;
    const char *name, *value;
    stream_t *st = s->p_source;
    msg_Dbg( s, "Manifest parsing\n" );

    vlc_xml = xml_Create( st );
    if( !vlc_xml )
    {
        msg_Err( s, "Failed to open XML parser" );
        return VLC_EGENERIC;
    }

    vlc_reader = xml_ReaderCreate( vlc_xml, st );
    if( !vlc_reader )
    {
        msg_Err( s, "Failed to open source for parsing" );
        xml_Delete( vlc_xml );
        return VLC_EGENERIC;
    }

    const char *node;
    uint8_t *WaveFormatEx;
    sms_stream_t *sms = NULL;
    quality_level_t *ql = NULL;
    int64_t start_time = 0, duration = 0;
    int64_t computed_start_time = 0, computed_duration = 0;
    unsigned next_track_id = 1;
    unsigned next_qid = 1;
    int loop_count = 0;
    bool b_weird = false;

#define TIMESCALE 10000000
    while( (type = xml_ReaderNextNode( vlc_reader, &node )) > 0 )
    {
        switch( type )
        {
            case XML_READER_STARTELEM:

                if( !strcmp( node, "SmoothStreamingMedia" ) )
                {
                    while( (name = xml_ReaderNextAttr( vlc_reader, &value )) )
                    {
                        if( !strcmp( name, "Duration" ) )
                            p_sys->vod_duration = strtoull( value, NULL, 10 );
                        if( !strcmp( name, "TimeScale" ) )
                            p_sys->timescale = strtoull( value, NULL, 10 );
                    }
                    if( !p_sys->timescale )
                        p_sys->timescale = TIMESCALE;
                }


                if( !strcmp( node, "StreamIndex" ) )
                {
                    sms = sms_New();
                    if( unlikely( !sms ) )
                        return VLC_ENOMEM;
                    sms->id = next_track_id;
                    next_track_id++;

                    while( (name = xml_ReaderNextAttr( vlc_reader, &value )) )
                    {
                        if( !strcmp( name, "Type" ) )
                        {
                            if( !strcmp( value, "video" ) )
                                sms->type = VIDEO_ES;
                            else if( !strcmp( value, "audio" ) )
                                sms->type = AUDIO_ES;
                            else if( !strcmp( value, "text" ) )
                                sms->type = SPU_ES;
                        }

                        if( !strcmp( name, "Name" ) )
                            sms->name = strdup( value );
                        if( !strcmp( name, "TimeScale" ) )
                            sms->timescale = strtoull( value, NULL, 10 );
                        if( !strcmp( name, "FourCC" ) )
                            sms->default_FourCC =
                                VLC_FOURCC( value[0], value[1], value[2], value[3] );

                        if( !strcmp( name, "Chunks" ) )
                        {
                            sms->vod_chunks_nb = strtol( value, NULL, 10 );
                            if( sms->vod_chunks_nb == 0 ) /* live */
                                sms->vod_chunks_nb = UINT32_MAX;
                        }

                        if( !strcmp( name, "QualityLevels" ) )
                            sms->qlevel_nb = strtoul( value, NULL, 10 );
                        if( !strcmp( name, "Url" ) )
                            sms->url_template = strdup(value);
                    }

                    if( sms && !sms->timescale )
                        sms->timescale = TIMESCALE;
                    if( !sms->name )
                    {
                        if( sms->type == VIDEO_ES )
                            sms->name = strdup( "video" );
                        else if( sms->type == AUDIO_ES )
                            sms->name = strdup( "audio" );
                        else if( sms->type == SPU_ES )
                            sms->name = strdup( "text" );
                    }

                    vlc_array_append( p_sys->sms_streams, sms );
                }

                if( !strcmp( node, "QualityLevel" ) )
                {
                    ql = ql_New();
                    if( !ql )
                        return VLC_ENOMEM;
                    ql->id = next_qid;
                    next_qid++;
                    while( (name = xml_ReaderNextAttr( vlc_reader, &value )) )
                    {
                        if( !strcmp( name, "Index" ) )
                            ql->Index = strtol( value, NULL, 10 );
                        if( !strcmp( name, "Bitrate" ) )
                            ql->Bitrate = strtoull( value, NULL, 10 );
                        if( !strcmp( name, "PacketSize" ) )
                            ql->nBlockAlign = strtoull( value, NULL, 10 );
                        if( !strcmp( name, "FourCC" ) )
                            ql->FourCC = VLC_FOURCC( value[0], value[1],
                                                     value[2], value[3] );
                        if( !strcmp( name, "CodecPrivateData" ) )
                            ql->CodecPrivateData = strdup( value );
                        if( !strcmp( name, "WaveFormatEx" ) )
                        {
                            WaveFormatEx = decode_string_hex_to_binary( value );
                            uint16_t data_len = ((uint16_t *)WaveFormatEx)[8];
                            ql->CodecPrivateData = strndup( value + 36, data_len * 2 );

                            uint16_t wf_tag = ((uint16_t *)WaveFormatEx)[0];
                            wf_tag_to_fourcc( wf_tag, &ql->FourCC, NULL );

                            ql->Channels = ((uint16_t *)WaveFormatEx)[1];
                            ql->SamplingRate = ((uint32_t *)WaveFormatEx)[1];
                            ql->nBlockAlign = ((uint16_t *)WaveFormatEx)[6];
                            ql->BitsPerSample = ((uint16_t *)WaveFormatEx)[7];
                            free( WaveFormatEx );
                        }
                        if( !strcmp( name, "MaxWidth" ) || !strcmp( name, "Width" ) )
                            ql->MaxWidth = strtoul( value, NULL, 10 );
                        if( !strcmp( name, "MaxHeight" ) || !strcmp( name, "Height" ) )
                            ql->MaxHeight = strtoul( value, NULL, 10 );
                        if( !strcmp( name, "Channels" ) )
                            ql->Channels = strtoul( value, NULL, 10 );
                        if( !strcmp( name, "SamplingRate" ) )
                            ql->SamplingRate = strtoul( value, NULL, 10 );
                        if( !strcmp( name, "BitsPerSample" ) )
                            ql->BitsPerSample = strtoul( value, NULL, 10 );
                    }
                    vlc_array_append( sms->qlevels, ql );
                }

                if( !strcmp( node, "c" ) )
                {
                    loop_count++;
                    start_time = duration = -1;
                    while( (name = xml_ReaderNextAttr( vlc_reader, &value )) )
                    {
                        if( !strcmp( name, "t" ) )
                            start_time = strtoull( value, NULL, 10 );
                        if( !strcmp( name, "d" ) )
                            duration = strtoull( value, NULL, 10 );
                    }
                    if( start_time == -1 )
                    {
                        assert( duration != -1 );
                        computed_start_time += computed_duration;
                        computed_duration = duration;
                    }
                    else if( duration == -1 )
                    {
                        assert( start_time != -1 );
                        /* Handle weird Manifests which give only the start time
                         * of the first segment. In those cases, we have to look
                         * at the start time of the second segment to compute
                         * the duration of the first one. */
                        if( loop_count == 1 )
                        {
                            b_weird = true;
                            computed_start_time = start_time;
                            continue;
                        }

                        computed_duration = start_time - computed_start_time;
                        if( !b_weird )
                            computed_start_time = start_time;
                    }
                    else
                    {
                        if( b_weird )
                            computed_duration = start_time - computed_start_time;
                        else
                        {
                            computed_start_time = start_time;
                            computed_duration = duration;
                        }
                    }

                    if( unlikely( chunk_New( sms, computed_duration,
                                        computed_start_time ) == NULL ) )
                    {
                        return VLC_ENOMEM;
                    }
                    if( b_weird && start_time != -1 )
                        computed_start_time = start_time;
                }
                break;

            case XML_READER_ENDELEM:
                if( strcmp( node, "StreamIndex" ) )
                    break;

                computed_start_time = 0;
                computed_duration = 0;
                loop_count = 0;
                if( b_weird && !chunk_New( sms, computed_duration, computed_start_time ) )
                    return VLC_ENOMEM;

                b_weird = false;
                next_qid = 1;

                if( sms->qlevel_nb == 0 )
                    sms->qlevel_nb = vlc_array_count( sms->qlevels );
                break;

            case XML_READER_NONE:
                break;
            case XML_READER_TEXT:
                break;
            default:
                return VLC_EGENERIC;
        }
    }
#undef TIMESCALE

    xml_ReaderDelete( vlc_reader );
    xml_Delete( vlc_xml );

    return VLC_SUCCESS;
}

static int Open( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys;

    if( !isSmoothStreaming( s ) )
        return VLC_EGENERIC;

    msg_Info( p_this, "Smooth Streaming (%s)", s->psz_path );

    s->p_sys = p_sys = calloc( 1, sizeof(*p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    char *uri = NULL;
    if( unlikely( asprintf( &uri, "%s://%s", s->psz_access, s->psz_path ) < 0 ) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    /* remove the last part of the url */
    char *pos = strrchr( uri, '/');
    *pos = '\0';
    p_sys->base_url = uri;

    /* XXX I don't know wether or not we should allow caching */
    p_sys->b_cache = false;

    p_sys->sms_streams = vlc_array_new();
    p_sys->selected_st = vlc_array_new();
    p_sys->download.chunks = vlc_array_new();
    p_sys->init_chunks = vlc_array_new();
    if( unlikely( !p_sys->sms_streams || !p_sys->download.chunks ||
                  !p_sys->selected_st || !p_sys->init_chunks ) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    /* Parse SMS ismc content. */
    if( parse_Manifest( s ) != VLC_SUCCESS )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( !p_sys->vod_duration )
       p_sys->b_live = true;

    p_sys->i_tracks = vlc_array_count( p_sys->sms_streams );

    /* Choose first video / audio / subtitle stream available */
    sms_stream_t *tmp = NULL, *selected = NULL;
    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        tmp = vlc_array_item_at_index( p_sys->sms_streams, i );
        selected = SMS_GET_SELECTED_ST( tmp->type );
        if( !selected )
            vlc_array_append( p_sys->selected_st, tmp );
    }

    /* Choose lowest quality for the first chunks */
    quality_level_t *wanted, *qlvl;
    sms_stream_t *sms = NULL;

    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        wanted = qlvl = NULL;
        sms = vlc_array_item_at_index( p_sys->sms_streams, i );
        wanted = vlc_array_item_at_index( sms->qlevels, 0 );
        for( unsigned i=1; i < sms->qlevel_nb; i++ )
        {
            qlvl = vlc_array_item_at_index( sms->qlevels, i );
            if( qlvl->Bitrate < wanted->Bitrate )
                wanted = qlvl;
        }
        sms->download_qlvl = wanted->id;
    }

    vlc_mutex_init( &p_sys->download.lock_wait );
    vlc_cond_init( &p_sys->download.wait );

    /* */
    s->pf_read = Read;
    s->pf_peek = Peek;
    s->pf_control = Control;

    if( vlc_clone( &p_sys->thread, sms_Thread, s, VLC_THREAD_PRIORITY_INPUT ) )
    {
        free( p_sys );
        vlc_mutex_destroy( &p_sys->download.lock_wait );
        vlc_cond_destroy( &p_sys->download.wait );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys = s->p_sys;

    vlc_mutex_lock( &p_sys->download.lock_wait );
    p_sys->b_close = true;
    /* Negate the condition variable's predicate */
    for( int i = 0; i < 3; i++ )
        p_sys->download.lead[i] = 0;
    p_sys->playback.toffset = 0;
    vlc_cond_signal(&p_sys->download.wait);
    vlc_mutex_unlock( &p_sys->download.lock_wait );

    vlc_join( p_sys->thread, NULL );
    vlc_mutex_destroy( &p_sys->download.lock_wait );
    vlc_cond_destroy( &p_sys->download.wait );

    /* Free sms streams */
    sms_stream_t *sms;
    for( int i = 0; i < vlc_array_count( p_sys->sms_streams ); i++ )
    {
        sms = vlc_array_item_at_index( p_sys->sms_streams, i );
        if( sms )
            sms_Free( sms );
    }
    /* Free downloaded chunks */
    chunk_t *chunk;
    for( int i = 0; i < vlc_array_count( p_sys->init_chunks ); i++ )
    {
        chunk = vlc_array_item_at_index( p_sys->init_chunks, i );
        chunk_Free( chunk );
    }

    sms_queue_free( p_sys->bws );
    vlc_array_destroy( p_sys->sms_streams );
    vlc_array_destroy( p_sys->selected_st );
    vlc_array_destroy( p_sys->download.chunks );
    vlc_array_destroy( p_sys->init_chunks );

    free( p_sys->base_url );
    free( p_sys );
}

static chunk_t *get_chunk( stream_t *s, const bool wait )
{
    stream_sys_t *p_sys = s->p_sys;
    unsigned count;
    chunk_t *chunk = NULL;

    vlc_mutex_lock( &p_sys->download.lock_wait );
    count = vlc_array_count( p_sys->download.chunks );
    while( p_sys->playback.index >= count || p_sys->b_tseek )
    {
        /* Yes I know, checking for p_sys->b_die is not reliable,
         * that's why vlc_object_alive() has been deprecated. But if I
         * understood well, there is no good solution with a stream_filter
         * module anyaway. */
        if( !wait || p_sys->b_error )
        {
            vlc_mutex_unlock( &p_sys->download.lock_wait );
            msg_Warn( s, "get_chunk failed! (playback index %u)",
                    p_sys->playback.index );
            return NULL;
        }
        if( NO_MORE_CHUNKS )
        {
            vlc_mutex_unlock( &p_sys->download.lock_wait );
            msg_Info( s, "No more chunks, end of the VOD" );
            return NULL;
        }

        msg_Dbg( s, "get_chunk is waiting !!!" );
        vlc_cond_timedwait( &p_sys->download.wait,
                &p_sys->download.lock_wait, mdate() + 500000 );
        count = vlc_array_count( p_sys->download.chunks );
        msg_Dbg( s, "count is %u, and index is %u", count, p_sys->playback.index );
    }
    chunk = vlc_array_item_at_index( p_sys->download.chunks, p_sys->playback.index );

    vlc_mutex_unlock( &p_sys->download.lock_wait );

    return chunk;
}

static int sms_Read( stream_t *s, uint8_t *p_read, int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    int copied = 0;
    chunk_t *chunk = NULL;

    do
    {
        chunk = get_chunk( s, true );
        if( !chunk )
            return copied;

        if( chunk->read_pos >= (int)chunk->size )
        {
            if( chunk->type == VIDEO_ES ||
                ( !SMS_GET_SELECTED_ST( VIDEO_ES ) && chunk->type == AUDIO_ES ) )
            {
                vlc_mutex_lock( &p_sys->download.lock_wait );
                p_sys->playback.toffset += chunk->duration;
                vlc_mutex_unlock( &p_sys->download.lock_wait );
                vlc_cond_signal( &p_sys->download.wait);
            }
            if( !p_sys->b_cache || p_sys->b_live )
            {
                FREENULL( chunk->data );
                chunk->read_pos = 0;
            }

            chunk->read_pos = 0;

            p_sys->playback.index += 1;
            msg_Dbg( s, "Incrementing playback index" );

            continue;
        }

        if( chunk->read_pos == 0 )
        {
            const char *verb = p_read == NULL ? "skipping" : "reading";
            msg_Dbg( s, "%s chunk %u (%u bytes), type %i",
                        verb, chunk->sequence, i_read, chunk->type );
            /* check integrity */
            uint32_t type;
            uint8_t *slice = chunk->data;
            SMS_GET4BYTES( type );
            SMS_GETFOURCC( type );
            assert( type == ATOM_moof || type == ATOM_uuid );
        }

        int len = -1;
        uint8_t *src = chunk->data + chunk->read_pos;
        if( i_read <= chunk->size - chunk->read_pos )
            len = i_read;
        else
            len = chunk->size - chunk->read_pos;

        if( len > 0 )
        {
            if( p_read ) /* otherwise caller skips data */
                memcpy( p_read + copied, src, len );
            chunk->read_pos += len;
            copied += len;
            i_read -= len;
        }


    } while ( i_read > 0 );

    return copied;
}

static int Read( stream_t *s, void *buffer, unsigned i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    int length = 0;

    if( p_sys->b_error )
        return 0;

    length = sms_Read( s, (uint8_t*) buffer, i_read );
    if( length < 0 )
        return 0;

    /* This call to sms_Read will increment p_sys->playback.index
     * in case the last chunk we read into is entirely read */
    sms_Read( s, NULL, 0 );

    p_sys->playback.boffset += length;
    if( (unsigned)length < i_read )
        msg_Warn( s, "could not read %i bytes, only %i!", i_read, length );

    return length;
}

/* The MP4 demux should never have to to peek outside the current chunk */
static int Peek( stream_t *s, const uint8_t **pp_peek, unsigned i_peek )
{
    chunk_t *chunk = get_chunk( s, true );
    if( !chunk || !chunk->data )
        return 0;

    int bytes = chunk->size - chunk->read_pos;
    assert( bytes > 0 );

    if( (unsigned)bytes < i_peek )
    {
        msg_Err( s, "could not peek %u bytes, only %i!", i_peek, bytes );
    }
    msg_Dbg( s, "peeking at chunk %u!", chunk->sequence );
    *pp_peek = chunk->data + chunk->read_pos;

    return bytes;
}

/* Normaly a stream_filter is not able to provide *time* seeking, since a
 * stream_filter operates on a byte stream. Thus, in order to circumvent this
 * limitation, I treat a STREAM_SET_POSITION request which value "pos" is less
 * than FAKE_STREAM_SIZE as a *time* seek request, and more precisely a request
 * to jump at time position: pos / FAKE_STREAM_SIZE * total_video_duration.
 * For exemple, it pos == 500, it would be interpreted as a request to jump at
 * the middle of the video.
 * If pos > 1000, it would be treated as a normal byte seek request. That means
 * the demux is not able to request a byte seek with 0 <= pos <= 1000
 * (unless it is in the current chunk), but that doesn't matter in practice.
 * Of course this a bit hack-ish, but if Smooth Streaming doesn't die, its
 * implementation will be moved to a access_demux module, and this hack won't
 * be needed anymore (among others). */
static int chunk_Seek( stream_t *s, const uint64_t pos )
{
    stream_sys_t *p_sys = s->p_sys;

    if( pos == p_sys->playback.boffset )
        return VLC_SUCCESS;

    chunk_t *chunk = get_chunk( s, false );
    if( chunk == NULL )
        return VLC_EGENERIC;

    bool inside_chunk = pos >= chunk->offset &&
            pos < (chunk->offset + chunk->size) ? true : false;

    if( inside_chunk )
    {
        chunk->read_pos = pos - chunk->offset;
        p_sys->playback.boffset = pos;
        return VLC_SUCCESS;
    }
    else
    {
        if( p_sys->b_live )
        {
            msg_Err( s, "Cannot seek outside the current chunk for a live stream" );
            return VLC_EGENERIC;
        }

        msg_Info( s, "Seeking outside the current chunk" );
        assert( pos <= FAKE_STREAM_SIZE );

        vlc_mutex_lock( &p_sys->download.lock_wait );

        p_sys->b_tseek = true;
        p_sys->time_pos = p_sys->vod_duration * pos / FAKE_STREAM_SIZE;
        for( int i = 0; i < 3; i++ )
            p_sys->download.lead[i] = 0;
        p_sys->playback.toffset = 0;

        vlc_cond_signal( &p_sys->download.wait);
        vlc_mutex_unlock( &p_sys->download.lock_wait );

        return VLC_SUCCESS;
    }
}

static int Control( stream_t *s, int i_query, va_list args )
{
    stream_sys_t *p_sys = s->p_sys;

    switch( i_query )
    {
        case STREAM_CAN_SEEK:
            *(va_arg( args, bool * )) = true;
            break;
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE: /* TODO */
        case STREAM_CAN_CONTROL_PACE:
            *(va_arg( args, bool * )) = false;
            break;
        case STREAM_GET_POSITION:
            *(va_arg( args, uint64_t * )) = p_sys->playback.boffset;
            break;
        case STREAM_SET_POSITION:
            {
                uint64_t pos = (uint64_t)va_arg(args, uint64_t);
                int ret = chunk_Seek(s, pos);
                if( ret == VLC_SUCCESS )
                    break;
                else
                    return VLC_EGENERIC;
            }
        case STREAM_GET_SIZE:
            *(va_arg( args, uint64_t * )) = FAKE_STREAM_SIZE;
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
