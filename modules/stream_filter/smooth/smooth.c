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

#include "smooth.h"

#include <vlc_plugin.h>
#include <vlc_xml.h>
#include <vlc_charset.h>
#include <vlc_stream.h>
#include <vlc_es.h>
#include <vlc_codecs.h>

#include <limits.h>
#include <assert.h>
#include <inttypes.h>

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
    int i_size = stream_Peek( s->p_source, &peek, 512 );
    if( i_size < 512 )
        return false;

    char *peeked = malloc( 512 );
    if( unlikely( peeked == NULL ) )
        return false;

    memcpy( peeked, peek, 512 );
    peeked[511] = peeked[510] = '\0';

    char *str;

    if( !memcmp( peeked, "\xFF\xFE", 2 ) )
    {
        str = FromCharset( "UTF-16LE", peeked, 512 );
        free( peeked );
    }
    else if( !memcmp( peeked, "\xFE\xFF", 2 ) )
    {
        str = FromCharset( "UTF-16BE", peeked, 512 );
        free( peeked );
    }
    else
        str = peeked;

    if( str == NULL )
        return false;

    bool ret = strstr( str, "<SmoothStreamingMedia" ) != NULL;
    free( str );
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

static void cleanup_attributes(custom_attrs_t **cp)
{
    if( !*cp )
        return;

    free( (*cp)->psz_key );
    free( (*cp)->psz_value );
    FREENULL( *cp );
}

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
    custom_attrs_t *cp = NULL;
    int64_t start_time = 0, duration = 0;
    int64_t computed_start_time = 0, computed_duration = 0;
    unsigned next_track_id = 1;
    int loop_count = 0;
    bool b_weird = false;
    int ret = VLC_SUCCESS;

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
                        else if( !strcmp( name, "TimeScale" ) )
                            p_sys->timescale = strtoul( value, NULL, 10 );
                        else if ( !strcmp( name, "LookAheadFragmentCount" ) )
                            p_sys->download.lookahead_count = strtoul( value, NULL, 10 );
                    }
                    if( !p_sys->timescale )
                        p_sys->timescale = TIMESCALE;
                }
                else if( !strcmp( node, "StreamIndex" ) )
                {
                    sms_Free( sms );
                    sms = sms_New();
                    if( unlikely( !sms ) )
                    {
                        ret = VLC_ENOMEM;
                        goto cleanup;
                    }
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

                        else if( !strcmp( name, "Name" ) )
                            sms->name = strdup( value );
                        else if( !strcmp( name, "TimeScale" ) )
                            sms->timescale = strtoull( value, NULL, 10 );
                        else if( !strcmp( name, "FourCC" ) )
                            sms->default_FourCC =
                                VLC_FOURCC( value[0], value[1], value[2], value[3] );

                        else if( !strcmp( name, "Chunks" ) )
                        {
                            sms->vod_chunks_nb = strtoul( value, NULL, 10 );
                            if( sms->vod_chunks_nb == 0 ) /* live */
                                sms->vod_chunks_nb = UINT32_MAX;
                        }

                        else if( !strcmp( name, "QualityLevels" ) )
                            sms->qlevel_nb = strtoul( value, NULL, 10 );
                        else if( !strcmp( name, "Url" ) )
                            sms->url_template = strdup(value);
                    }

                    if( !sms->timescale )
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
                }
                else if ( !strcmp( node, "CustomAttributes" ) )
                {
                    if (!sms || !ql || cp)
                        break;
                    cp = (custom_attrs_t *) calloc( 1, sizeof(*cp) );
                    if( unlikely( !cp ) )
                    {
                        ret = VLC_ENOMEM;
                        goto cleanup;
                    }
                }
                else if ( !strcmp( node, "Attribute" ) )
                {
                    if (!sms || !ql || !cp)
                        break;
                    while( (name = xml_ReaderNextAttr( vlc_reader, &value )) )
                    {
                        if( !strcmp( name, "Name" ) && !cp->psz_key )
                            cp->psz_key = strdup( value );
                        else
                        if( !strcmp( name, "Value" ) && !cp->psz_value )
                            cp->psz_value = strdup( value );
                    }
                }
                else if( !strcmp( node, "QualityLevel" ) )
                {
                    if ( !sms )
                        break;

                    ql = ql_New();
                    if( !ql )
                    {
                        ret = VLC_ENOMEM;
                        goto cleanup;
                    }

                    while( (name = xml_ReaderNextAttr( vlc_reader, &value )) )
                    {
                        if( !strcmp( name, "Index" ) )
                            ql->Index = strtol( value, NULL, 10 );
                        else if( !strcmp( name, "Bitrate" ) )
                            ql->Bitrate = strtoul( value, NULL, 10 );
                        else if( !strcmp( name, "PacketSize" ) )
                            ql->nBlockAlign = strtoul( value, NULL, 10 );
                        else if( !strcmp( name, "FourCC" ) )
                            ql->FourCC = VLC_FOURCC( value[0], value[1],
                                                     value[2], value[3] );
                        else if( !strcmp( name, "CodecPrivateData" ) )
                            ql->CodecPrivateData = strdup( value );
                        else if( !strcmp( name, "WaveFormatEx" ) )
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
                        else if( !strcmp( name, "MaxWidth" ) || !strcmp( name, "Width" ) )
                            ql->MaxWidth = strtoul( value, NULL, 10 );
                        else if( !strcmp( name, "MaxHeight" ) || !strcmp( name, "Height" ) )
                            ql->MaxHeight = strtoul( value, NULL, 10 );
                        else if( !strcmp( name, "Channels" ) )
                            ql->Channels = strtoul( value, NULL, 10 );
                        else if( !strcmp( name, "SamplingRate" ) )
                            ql->SamplingRate = strtoul( value, NULL, 10 );
                        else if( !strcmp( name, "BitsPerSample" ) )
                            ql->BitsPerSample = strtoul( value, NULL, 10 );
                    }

                    ARRAY_APPEND( sms->qlevels, ql );
                }
                else if ( !strcmp( node, "Content" ) && sms && !sms->url_template )
                {
                    /* empty(@Url) && ./Content == manifest embedded content */
                    sms_Free( sms );
                    sms = NULL;
                }
                else if( !strcmp( node, "c" ) )
                {
                    if ( !sms )
                        break;
                    loop_count++;
                    start_time = duration = -1;
                    while( (name = xml_ReaderNextAttr( vlc_reader, &value )) )
                    {
                        if( !strcmp( name, "t" ) )
                            start_time = strtoll( value, NULL, 10 );
                        if( !strcmp( name, "d" ) )
                            duration = strtoll( value, NULL, 10 );
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

                    if( unlikely( chunk_AppendNew( sms, computed_duration,
                                        computed_start_time ) == NULL ) )
                    {
                        ret = VLC_ENOMEM;
                        goto cleanup;
                    }
                    if( b_weird && start_time != -1 )
                        computed_start_time = start_time;
                }
                break;

            case XML_READER_ENDELEM:
                if ( !strcmp( node, "CustomAttributes" ) )
                {
                    if ( cp )
                    {
                        ARRAY_APPEND(ql->custom_attrs, cp);
                        cp = NULL;
                    }
                }
                else if ( !strcmp( node, "Attribute" ) )
                {
                    if( !cp->psz_key || !cp->psz_value )
                    {
                        cleanup_attributes( &cp );
                    }
                }
                else if( strcmp( node, "StreamIndex" ) )
                    break;
                else if ( sms )
                {
                    ARRAY_APPEND( p_sys->sms, sms );

                    computed_start_time = 0;
                    computed_duration = 0;
                    loop_count = 0;
                    if( b_weird && !chunk_AppendNew( sms, computed_duration, computed_start_time ) )
                    {
                        ret = VLC_ENOMEM;
                        goto cleanup;
                    }

                    b_weird = false;

                    if( sms->qlevel_nb == 0 )
                        sms->qlevel_nb = sms->qlevels.i_size;

                    sms = NULL;
                }
                break;

            case XML_READER_TEXT:
                break;
            default:
                ret = VLC_EGENERIC;
                goto cleanup;
        }
    }
#undef TIMESCALE

cleanup:
    cleanup_attributes( &cp );
    sms_Free( sms );
    xml_ReaderDelete( vlc_reader );
    xml_Delete( vlc_xml );

    return ret;
}

static void SysCleanup( stream_sys_t *p_sys )
{
    if ( p_sys->sms.i_size )
    {
        FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms );
            sms_Free( sms );
        FOREACH_END();
        ARRAY_RESET( p_sys->sms );
    }
    ARRAY_RESET( p_sys->sms_selected );
    if ( p_sys->playback.init.p_datachunk )
        chunk_Free( p_sys->playback.init.p_datachunk );
    free( p_sys->download.base_url );
}

static sms_stream_t *next_playback_stream( stream_sys_t *p_sys )
{
    sms_stream_t *p_candidate = NULL;
    FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms_selected );
    if ( !sms->p_playback )
        continue;
    if ( p_candidate == NULL ||
         sms->p_playback->start_time < p_candidate->p_playback->start_time )
        p_candidate = sms;
    FOREACH_END();
    return p_candidate;
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
    p_sys->download.base_url = uri;

    ARRAY_INIT( p_sys->sms );
    ARRAY_INIT( p_sys->sms_selected );

    /* Parse SMS ismc content. */
    if( parse_Manifest( s ) != VLC_SUCCESS )
    {
        SysCleanup( p_sys );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( !p_sys->vod_duration )
       p_sys->b_live = true;

    /* Choose first video / audio / subtitle stream available */
    sms_stream_t *selected = NULL;
    FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms );
    selected = SMS_GET_SELECTED_ST( sms->type );
    if( !selected )
        ARRAY_APPEND( p_sys->sms_selected, sms );
    FOREACH_END();

    /* Choose lowest quality for the first chunks */
    FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms );
    quality_level_t *wanted = NULL;
    if ( sms->qlevels.i_size )
    {
        wanted = sms->qlevels.p_elems[0];
        for( int i=1; i < sms->qlevels.i_size; i++ )
        {
            if( sms->qlevels.p_elems[i]->Bitrate < wanted->Bitrate )
                wanted = sms->qlevels.p_elems[i];
        }
        sms->current_qlvl = wanted;
    }
    FOREACH_END();

    /* Init our playback queue */
    p_sys->p_current_stream = next_playback_stream( p_sys );
    if ( !p_sys->p_current_stream )
    {
        SysCleanup( p_sys );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->playback.toffset = p_sys->p_current_stream->p_playback->start_time;
    p_sys->playback.next_chunk_offset = CHUNK_OFFSET_UNSET;
    p_sys->i_probe_length = SMS_PROBE_LENGTH;

    vlc_mutex_init( &p_sys->lock );
    vlc_cond_init( &p_sys->download.wait );
    vlc_cond_init( &p_sys->playback.wait );
    vlc_mutex_init( &p_sys->playback.lock );

    /* */
    s->pf_read = Read;
    s->pf_peek = Peek;
    s->pf_control = Control;

    vlc_mutex_lock( &p_sys->playback.lock );

    if( vlc_clone( &p_sys->download.thread, sms_Thread, s, VLC_THREAD_PRIORITY_INPUT ) )
    {
        vlc_mutex_unlock( &p_sys->playback.lock );
        SysCleanup( p_sys );
        vlc_mutex_destroy( &p_sys->lock );
        vlc_cond_destroy( &p_sys->download.wait );
        vlc_mutex_destroy( &p_sys->playback.lock );
        vlc_cond_destroy( &p_sys->playback.wait );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* avoid race condition where the first init chunk isn't there yet
       and a non waiting get_chunk() is done (would return empty stream)*/
    vlc_cond_wait( &p_sys->playback.wait, &p_sys->playback.lock );
    vlc_mutex_unlock( &p_sys->playback.lock );

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys = s->p_sys;

    p_sys->b_close = true;
    vlc_cond_signal(&p_sys->download.wait);

    vlc_mutex_lock( &p_sys->playback.lock );
    p_sys->playback.toffset = 0;
    vlc_mutex_unlock( &p_sys->playback.lock );

    vlc_join( p_sys->download.thread, NULL );
    vlc_mutex_destroy( &p_sys->lock );
    vlc_cond_destroy( &p_sys->download.wait );
    vlc_mutex_destroy( &p_sys->playback.lock );
    vlc_cond_destroy( &p_sys->playback.wait );

    SysCleanup( p_sys );
    free( p_sys );
}

static chunk_t * gotoNextChunk( stream_sys_t *p_sys )
{
    assert(p_sys->p_current_stream);
    chunk_t *p_prev = p_sys->p_current_stream->p_playback;
    if ( p_prev )
    {
        if ( p_sys->b_live )
        {
            /* Discard chunk and update stream pointers */
            assert( p_sys->p_current_stream->p_chunks ==  p_sys->p_current_stream->p_playback );
            p_sys->p_current_stream->p_playback = p_sys->p_current_stream->p_playback->p_next;
            p_sys->p_current_stream->p_chunks = p_sys->p_current_stream->p_chunks->p_next;
            if ( p_sys->p_current_stream->p_lastchunk == p_prev )
                p_sys->p_current_stream->p_lastchunk = NULL;
            chunk_Free( p_prev );
        }
        else
        {
            /* Just cleanup chunk for reuse on seek */
            p_sys->p_current_stream->p_playback = p_sys->p_current_stream->p_playback->p_next;
            FREENULL(p_prev->data);
            p_prev->read_pos = 0;
            p_prev->offset = CHUNK_OFFSET_UNSET;
            p_prev->size = 0;
        }
    }

    /* Select new current pointer among streams playback heads */
    p_sys->p_current_stream = next_playback_stream( p_sys );
    if ( !p_sys->p_current_stream )
        return NULL;

    return p_sys->p_current_stream->p_playback;
}

static void setChunkOffset( stream_sys_t *p_sys, chunk_t *p_chunk )
{
    if ( p_chunk->offset == CHUNK_OFFSET_UNSET )
    {
        p_chunk->offset = CHUNK_OFFSET_0 + p_sys->playback.next_chunk_offset;
        p_sys->playback.next_chunk_offset += p_chunk->size;
    }
}

static chunk_t *get_chunk( stream_t *s, const bool wait, bool *pb_isinit )
{
    stream_sys_t *p_sys = s->p_sys;

    bool b_foo;
    if ( !pb_isinit )
        pb_isinit = &b_foo;

    *pb_isinit = false;

    vlc_mutex_lock( &p_sys->lock );

    assert(p_sys->p_current_stream);
    chunk_t *p_chunk = p_sys->p_current_stream->p_playback;

    /* init chunk handling, overrides the current one */
    vlc_mutex_lock( &p_sys->playback.lock );
    if ( p_sys->playback.init.p_datachunk )
    {
        if ( p_sys->playback.init.p_startchunk == NULL ||
             p_sys->playback.init.p_startchunk == p_chunk )
        {
            /* Override current chunk and pass init chunk instead */
            p_chunk = p_sys->playback.init.p_datachunk;
            *pb_isinit = true;
            setChunkOffset( p_sys, p_chunk );
#ifndef NDEBUG
            if ( !p_chunk->read_pos )
                msg_Dbg( s, "Sending init chunk from offset %"PRId64, p_chunk->offset );
#endif
            vlc_mutex_unlock( &p_sys->playback.lock );
            vlc_mutex_unlock( &p_sys->lock );
            return p_chunk;
        }
    }
    vlc_mutex_unlock( &p_sys->playback.lock );
    /* !init chunk handling */

    vlc_mutex_lock( &p_sys->p_current_stream->chunks_lock );
    vlc_mutex_unlock( &p_sys->lock );

    while( p_chunk && !p_chunk->data )
    {
        /* Yes I know, checking for p_sys->b_die is not reliable,
         * that's why vlc_object_alive() has been deprecated. But if I
         * understood well, there is no good solution with a stream_filter
         * module anyaway. */
        if( !wait || p_sys->b_error )
        {
            msg_Warn( s, "get_chunk failed! (starttime %"PRId64")", p_chunk->start_time );
            vlc_mutex_unlock( &p_sys->p_current_stream->chunks_lock );
            return NULL;
        }

        if( no_more_chunks( p_sys ) )
        {
            msg_Info( s, "No more chunks, end of the VOD" );
            vlc_mutex_unlock( &p_sys->p_current_stream->chunks_lock );
            return NULL;
        }

        msg_Dbg( s, "get_chunk is waiting for chunk %"PRId64"!!!", p_chunk->start_time );
        vlc_mutex_lock( &p_sys->playback.lock );
        p_sys->playback.b_underrun = true;
        vlc_mutex_unlock( &p_sys->playback.lock );
        vlc_cond_timedwait( &p_sys->playback.wait,
                &p_sys->p_current_stream->chunks_lock, mdate() + CLOCK_FREQ/2 );
    }

    if ( p_chunk )
    {
        setChunkOffset( p_sys, p_chunk );
#ifndef NDEBUG
        if ( !p_chunk->read_pos )
            msg_Dbg( s, "Sending chunk %"PRId64" from offset %"PRId64, p_chunk->start_time, p_chunk->offset );
#endif
    }
    vlc_mutex_unlock( &p_sys->p_current_stream->chunks_lock );

    return p_chunk;
}

static unsigned int sms_Read( stream_t *s, uint8_t *p_read, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    unsigned int copied = 0;
    chunk_t *chunk = NULL;

    do
    {
        bool b_isinitchunk = false;
        chunk = get_chunk( s, true, &b_isinitchunk );
        /* chunk here won't be processed further */
//        msg_Dbg( s, "chunk %"PRIu64" init %d", (uint64_t) chunk, b_isinitchunk );
        if( !chunk )
            return copied;

        if( chunk->read_pos >= chunk->size )
        {
            if( chunk->type == VIDEO_ES ||
                ( !SMS_GET_SELECTED_ST( VIDEO_ES ) && chunk->type == AUDIO_ES ) )
            {
                p_sys->playback.toffset += chunk->duration;
                vlc_cond_signal( &p_sys->download.wait );
            }

            if ( b_isinitchunk )
            {
                assert( chunk->read_pos == chunk->size );
                vlc_mutex_lock( &p_sys->playback.lock );
                p_sys->playback.init.p_startchunk = NULL;
                p_sys->playback.init.p_datachunk = NULL;
                chunk_Free( chunk );
                vlc_mutex_unlock( &p_sys->playback.lock );
            }
            else
            {
                vlc_mutex_lock( &p_sys->lock );
                if( gotoNextChunk( p_sys ) == NULL )
                {
                    vlc_mutex_unlock( &p_sys->lock );
                    return 0;
                }
                vlc_mutex_unlock( &p_sys->lock );
            }

            continue;
        }

        if( chunk->read_pos == 0 )
        {
            const char *verb = p_read == NULL ? "skipping" : "reading";
            msg_Dbg( s, "%s chunk time %"PRIu64" (%u bytes), type %i",
                        verb, chunk->start_time, i_read, chunk->type );
        }

        uint64_t len = 0;
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
            p_sys->playback.boffset += len;
        }

    } while ( i_read > 0 );

    return copied;
}

static int Read( stream_t *s, void *buffer, unsigned i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    int length = 0;
    i_read = __MIN(INT_MAX, i_read);

    if( p_sys->b_error )
        return 0;

    length = sms_Read( s, (uint8_t*) buffer, i_read );
    if( length == 0 )
        return 0;

    /* This call to sms_Read will increment p_sys->playback.index
     * in case the last chunk we read into is entirely read */
    sms_Read( s, NULL, 0 );

    if( length < (int)i_read )
        msg_Warn( s, "could not read %u bytes, only %u !", i_read, length );

    return length;
}

/* The MP4 demux should never have to to peek outside the current chunk */
static int Peek( stream_t *s, const uint8_t **pp_peek, unsigned i_peek )
{
    chunk_t *chunk = get_chunk( s, true, NULL );
    if( !chunk || !chunk->data )
    {
        msg_Err( s, "peek %"PRIu64" or no data", (uint64_t) chunk );
        return 0;
    }

    int bytes = chunk->size - chunk->read_pos;
    assert( bytes > 0 );

    if( (unsigned)bytes < i_peek )
    {
        msg_Err( s, "could not peek %u bytes, only %i!", i_peek, bytes );
    }
    msg_Dbg( s, "peeking at chunk %"PRIu64, chunk->start_time );
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

    chunk_t *chunk = get_chunk( s, false, NULL );
    if( chunk == NULL )
        return VLC_EGENERIC;

    assert( chunk->offset != CHUNK_OFFSET_UNSET );
    uint64_t i_chunkspos = CHUNK_OFFSET_0 + pos;

    bool inside_chunk = i_chunkspos >= chunk->offset &&
                        i_chunkspos < (chunk->offset + chunk->size);

    if( inside_chunk )
    {
        chunk->read_pos = i_chunkspos - chunk->offset;
        p_sys->playback.boffset = pos;
        return VLC_SUCCESS;
    }
    else
    {
        if( p_sys->b_live )
        {
            msg_Err( s, "Cannot seek to %"PRIu64" outside the current chunk for a live stream at %"PRIu64, pos, p_sys->playback.boffset );
            return VLC_EGENERIC;
        }

        msg_Info( s, "Seeking outside the current chunk (%"PRIu64"->%"PRIu64") to %"PRIu64, chunk->offset, chunk->offset+chunk->size, pos );
        assert( pos <= FAKE_STREAM_SIZE );

        vlc_mutex_lock( &p_sys->lock );
        resetChunksState( p_sys );
        p_sys->playback.toffset = p_sys->time_pos;
        p_sys->playback.next_chunk_offset = 0;
        p_sys->playback.boffset = 0;
        p_sys->time_pos = p_sys->vod_duration * pos / FAKE_STREAM_SIZE;

        /* set queues heads */
        FOREACH_ARRAY( sms_stream_t *sms, p_sys->sms_selected );
        vlc_mutex_lock( &sms->chunks_lock );
        chunk_t *p_chunk = sms->p_chunks;
        while ( p_chunk )
        {
            if ( p_chunk->start_time > p_sys->time_pos )
                break;
            if ( p_chunk->start_time <= p_sys->time_pos &&
                 p_chunk->start_time + p_chunk->duration >= p_sys->time_pos )
            {
                sms->p_playback = p_chunk;
                sms->p_nextdownload = p_chunk;
                break;
            }
            p_chunk = p_chunk->p_next;
        }
        vlc_mutex_unlock( &sms->chunks_lock );
        FOREACH_END();
        /* !set queues heads */

        vlc_cond_signal( &p_sys->download.wait);
        vlc_mutex_unlock( &p_sys->lock );

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
            *(va_arg( args, bool * )) = false;
            break;
        case STREAM_CAN_PAUSE: /* TODO */
        case STREAM_CAN_CONTROL_PACE:
            *(va_arg( args, bool * )) = true;
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
        case STREAM_GET_PTS_DELAY:
            *va_arg (args, int64_t *) = INT64_C(1000) *
                var_InheritInteger(s, "network-caching");
             break;
        case STREAM_SET_PAUSE_STATE:
            return (p_sys->b_live) ? VLC_EGENERIC : VLC_SUCCESS;
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
