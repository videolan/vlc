/*****************************************************************************
 * ogg.c : ogg stream demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Andre Pang <Andre.Pang@csiro.au> (Annodex support)
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_LIBVORBIS
  #include <vorbis/codec.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_meta.h>
#include <vlc_input.h>

#include <ogg/ogg.h>

#include <vlc_codecs.h>
#include <vlc_bits.h>
#include "xiph.h"
#include "xiph_metadata.h"
#include "ogg.h"
#include "oggseek.h"
#include "ogg_granule.h"
#include "opus.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname ( "OGG" )
    set_description( N_("OGG demuxer" ) )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 50 )
    set_callbacks( Open, Close )
    add_shortcut( "ogg" )
vlc_module_end ()


/*****************************************************************************
 * Definitions of structures and functions used by this plugins
 *****************************************************************************/

/* OggDS headers for the new header format (used in ogm files) */
typedef struct
{
    ogg_int32_t width;
    ogg_int32_t height;
} stream_header_video_t;

typedef struct
{
    ogg_int16_t channels;
    ogg_int16_t padding;
    ogg_int16_t blockalign;
    ogg_int32_t avgbytespersec;
} stream_header_audio_t;

typedef struct
{
    char        streamtype[8];
    char        subtype[4];

    ogg_int32_t size;                               /* size of the structure */

    ogg_int64_t time_unit;                              /* in reference time */
    ogg_int64_t samples_per_unit;
    ogg_int32_t default_len;                                /* in media time */

    ogg_int32_t buffersize;
    ogg_int16_t bits_per_sample;
    ogg_int16_t padding;

    union
    {
        /* Video specific */
        stream_header_video_t video;
        /* Audio specific */
        stream_header_audio_t audio;
    } sh;
} stream_header_t;

#define VORBIS_HEADER_IDENTIFICATION 1
#define VORBIS_HEADER_COMMENT        2
#define VORBIS_HEADER_SETUP          3
#define VORBIS_HEADER_TO_FLAG(i)     (1 << (i - 1))
#define VORBIS_HEADERS_VALID(p_stream) \
    ((p_stream->special.vorbis.i_headers_flags & 0x07) == 0x07) // 0b111

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux  ( demux_t * );
static int  Control( demux_t *, int, va_list );

/* Bitstream manipulation */
static int  Ogg_ReadPage     ( demux_t *, ogg_page * );
static void Ogg_DecodePacket ( demux_t *, logical_stream_t *, ogg_packet * );
static unsigned Ogg_OpusPacketDuration( ogg_packet * );
static void Ogg_QueueBlocks( demux_t *, logical_stream_t *, block_t * );
static void Ogg_SendQueuedBlock( demux_t *, logical_stream_t * );

static inline bool Ogg_HasQueuedBlocks( const logical_stream_t *p_stream )
{
    return ( p_stream->queue.p_blocks != NULL );
}

static void Ogg_CreateES( demux_t *p_demux, bool );
static int Ogg_BeginningOfStream( demux_t *p_demux );
static int Ogg_FindLogicalStreams( demux_t *p_demux );
static void Ogg_EndOfStream( demux_t *p_demux );

/* */
static void Ogg_LogicalStreamInit( logical_stream_t *p_stream );
static void Ogg_LogicalStreamDelete( demux_t *p_demux, logical_stream_t *p_stream );
static bool Ogg_LogicalStreamResetEsFormat( demux_t *p_demux, logical_stream_t *p_stream );
static void Ogg_ResetStream( logical_stream_t *p_stream );

/* */
static void Ogg_ExtractMeta( demux_t *p_demux, es_format_t *p_fmt, const uint8_t *p_headers, int i_headers );

/* Logical bitstream headers */
static bool Ogg_ReadDaalaHeader( logical_stream_t *, ogg_packet * );
static bool Ogg_ReadTheoraHeader( logical_stream_t *, ogg_packet * );
static bool Ogg_ReadVorbisHeader( logical_stream_t *, ogg_packet * );
static bool Ogg_ReadSpeexHeader( logical_stream_t *, ogg_packet * );
static void Ogg_ReadOpusHeader( logical_stream_t *, ogg_packet * );
static bool Ogg_ReadKateHeader( logical_stream_t *, ogg_packet * );
static bool Ogg_ReadFlacStreamInfo( demux_t *, logical_stream_t *, ogg_packet * );
static void Ogg_ReadAnnodexHeader( demux_t *, logical_stream_t *, ogg_packet * );
static bool Ogg_ReadDiracHeader( logical_stream_t *, ogg_packet * );
static bool Ogg_ReadVP8Header( demux_t *, logical_stream_t *, ogg_packet * );
static void Ogg_ReadSkeletonHeader( demux_t *, logical_stream_t *, ogg_packet * );
static bool Ogg_ReadOggSpotsHeader( logical_stream_t *, ogg_packet * );

/* Skeleton */
static void Ogg_ReadSkeletonBones( demux_t *, ogg_packet * );
static void Ogg_ReadSkeletonIndex( demux_t *, ogg_packet * );
static void Ogg_FreeSkeleton( ogg_skeleton_t * );
static void Ogg_ApplySkeleton( logical_stream_t * );

/* Special decoding */
static void Ogg_CleanSpecificData( logical_stream_t * );
#ifdef HAVE_LIBVORBIS
static void Ogg_DecodeVorbisHeader( logical_stream_t *, ogg_packet *, int );
#endif

static void fill_channels_info(audio_format_t *audio)
{
    static const int pi_channels_map[9] =
    {
        0,
        AOUT_CHAN_CENTER,
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
        AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
            | AOUT_CHAN_REARRIGHT,
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
            | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
            | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
            | AOUT_CHAN_REARCENTER | AOUT_CHAN_MIDDLELEFT
            | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE,
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
            | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
            | AOUT_CHAN_LFE,
    };

    unsigned chans = audio->i_channels;
    if (chans < sizeof(pi_channels_map) / sizeof(pi_channels_map[0]))
        audio->i_physical_channels = pi_channels_map[chans];
}

/*****************************************************************************
 * Open: initializes ogg demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t    *p_sys;
    const uint8_t  *p_peek;

    /* Check if we are dealing with an ogg stream */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 4 ) < 4 ) return VLC_EGENERIC;
    if( !p_demux->obj.force && memcmp( p_peek, "OggS", 4 ) )
    {
        char *psz_mime = stream_ContentType( p_demux->s );
        if( !psz_mime )
        {
            return VLC_EGENERIC;
        }
        else if ( strcmp( psz_mime, "application/ogg" ) &&
                  strcmp( psz_mime, "video/ogg" ) &&
                  strcmp( psz_mime, "audio/ogg" ) )
        {
            free( psz_mime );
            return VLC_EGENERIC;
        }
        free( psz_mime );
    }

    /* */
    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_length = -1;
    p_sys->b_preparsing_done = false;

    /* Set exported functions */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    /* Initialize the Ogg physical bitstream parser */
    ogg_sync_init( &p_sys->oy );

    /* */
    TAB_INIT( p_sys->i_seekpoints, p_sys->pp_seekpoints );

    /* Enforce exclusive mode, only one track can be selected at once. */
    es_out_Control( p_demux->out, ES_OUT_SET_ES_CAT_POLICY, AUDIO_ES,
                    ES_OUT_ES_POLICY_EXCLUSIVE );

    while ( !p_sys->b_preparsing_done && p_demux->pf_demux( p_demux ) > 0 )
    {}
    if ( p_sys->b_preparsing_done && p_demux->b_preparsing )
        Ogg_CreateES( p_demux, true );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys  ;

    /* Cleanup the bitstream parser */
    ogg_sync_clear( &p_sys->oy );

    Ogg_EndOfStream( p_demux );

    if( p_sys->p_old_stream )
        Ogg_LogicalStreamDelete( p_demux, p_sys->p_old_stream );

    free( p_sys );
}

static vlc_tick_t Ogg_GetLastDTS( demux_t * p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    vlc_tick_t i_dts = VLC_TICK_INVALID;
    for( int i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        logical_stream_t *p_stream = p_sys->pp_stream[i_stream];
        if ( p_stream->b_initializing )
            continue;
        if( p_stream->i_pcr > i_dts )
            i_dts = p_stream->i_pcr;
    }

    return i_dts;
}

static vlc_tick_t Ogg_GeneratePCR( demux_t * p_demux, bool b_drain )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    /* We will consider the lowest PCR among tracks, because the audio core badly
     * handles PCR rewind (mute)
     */
    vlc_tick_t i_pcr_candidate = VLC_TICK_INVALID;
    for( int i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        logical_stream_t *p_stream = p_sys->pp_stream[i_stream];
        if( p_stream->fmt.i_cat == SPU_ES )
            continue;
        if( p_stream->fmt.i_codec == VLC_CODEC_OGGSPOTS )
            continue;
        if( p_stream->i_pcr == VLC_TICK_INVALID )
            continue;
        if ( (!b_drain && p_stream->b_finished) || p_stream->b_initializing )
            continue;
        if( i_pcr_candidate == VLC_TICK_INVALID ||
            p_stream->i_pcr < i_pcr_candidate )
        {
            i_pcr_candidate = p_stream->i_pcr;
        }
    }

    return i_pcr_candidate;
}

static void Ogg_OutputQueues( demux_t *p_demux, bool b_drain )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_tick_t i_pcr;

    /* Generate First PCR */
    if( p_sys->i_pcr == VLC_TICK_INVALID )
    {
        i_pcr = Ogg_GeneratePCR( p_demux, b_drain );
        if( i_pcr != VLC_TICK_INVALID && i_pcr != p_sys->i_pcr )
        {
            p_sys->i_pcr = i_pcr;
            if( likely( !p_sys->b_slave ) )
                es_out_SetPCR( p_demux->out, p_sys->i_pcr );
        }
    }

    if( p_sys->i_pcr != VLC_TICK_INVALID )
    {
        bool b_continue;
        do
        {
            b_continue = false;
            for( int i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
            {
                logical_stream_t *p_stream = p_sys->pp_stream[i_stream];
                if( Ogg_HasQueuedBlocks( p_stream ) )
                    Ogg_SendQueuedBlock( p_demux, p_stream );
                b_continue |= Ogg_HasQueuedBlocks( p_stream );
            }

            /* Generate Current PCR */
            i_pcr = Ogg_GeneratePCR( p_demux, b_drain );
            if( i_pcr != VLC_TICK_INVALID && i_pcr != p_sys->i_pcr )
            {
                p_sys->i_pcr = i_pcr;
                if( likely( !p_sys->b_slave ) )
                    es_out_SetPCR( p_demux->out, p_sys->i_pcr );
            }
        } while ( b_continue );
    }
}


/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t * p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ogg_packet  oggpacket;
    int         i_stream;
    bool b_canseek;

    int i_active_streams = p_sys->i_streams;
    for ( int i=0; i < p_sys->i_streams; i++ )
    {
        if ( p_sys->pp_stream[i]->b_finished )
            i_active_streams--;
    }

    if ( i_active_streams == 0 )
    {
        if ( p_sys->i_streams ) /* All finished */
        {
            msg_Dbg( p_demux, "end of a group of %d logical streams", p_sys->i_streams );

            Ogg_OutputQueues( p_demux, true );

            vlc_tick_t i_lastdts = Ogg_GetLastDTS( p_demux );

            /* We keep the ES to try reusing it in Ogg_BeginningOfStream
             * only 1 ES is supported (common case for ogg web radio) */
            if( p_sys->i_streams == 1 && p_sys->pp_stream[0]->p_es )
            {
                if( p_sys->p_old_stream ) /* if no setupEs has reused */
                    Ogg_LogicalStreamDelete( p_demux, p_sys->p_old_stream );
                p_sys->p_old_stream = p_sys->pp_stream[0];
                TAB_CLEAN( p_sys->i_streams, p_sys->pp_stream );
            }

            Ogg_EndOfStream( p_demux );
            p_sys->b_chained_boundary = true;

            if( i_lastdts != VLC_TICK_INVALID )
            {
                p_sys->i_nzpcr_offset = i_lastdts - VLC_TICK_0;
                if( likely( !p_sys->b_slave ) )
                    es_out_SetPCR( p_demux->out, i_lastdts );
            }
            p_sys->i_pcr = VLC_TICK_INVALID;
        }

        if( Ogg_BeginningOfStream( p_demux ) != VLC_SUCCESS )
            return VLC_DEMUXER_EOF;

        msg_Dbg( p_demux, "beginning of a group of logical streams" );

        if ( !p_sys->b_chained_boundary )
        {
            /* Find the real duration */
            vlc_stream_Control( p_demux->s, STREAM_CAN_SEEK, &b_canseek );
            if ( b_canseek )
                Oggseek_ProbeEnd( p_demux );
        }
        else
        {
            p_sys->b_chained_boundary = false;
        }
    }

    if ( p_sys->b_preparsing_done && !p_sys->b_es_created )
        Ogg_CreateES( p_demux, false );

    /*
     * The first data page of a physical stream is stored in the relevant logical stream
     * in Ogg_FindLogicalStreams. Therefore, we must not read a page and only update the
     * stream it belongs to if we haven't processed this first page yet. If we do, we
     * will only process that first page whenever we find the second page for this stream.
     * While this is fine for Vorbis and Theora, which are continuous codecs, which means
     * the second page will arrive real quick, this is not fine for Kate, whose second
     * data page will typically arrive much later.
     * This means it is now possible to seek right at the start of a stream where the last
     * logical stream is Kate, without having to wait for the second data page to unblock
     * the first one, which is the one that triggers the 'no more headers to backup' code.
     * And, as we all know, seeking without having backed up all headers is bad, since the
     * codec will fail to initialize if it's missing its headers.
     */
    if( !p_sys->b_page_waiting)
    {
        /*
         * Demux an ogg page from the stream
         */
        if( Ogg_ReadPage( p_demux, &p_sys->current_page ) != VLC_SUCCESS )
            return VLC_DEMUXER_EOF; /* EOF */
        /* Test for End of Stream */
        if( ogg_page_eos( &p_sys->current_page ) )
        {
            /* If we delayed restarting encoders/SET_ES_FMT for more
             * skeleton provided configuration */
            if ( p_sys->p_skelstream )
            {
                if ( p_sys->p_skelstream->i_serial_no == ogg_page_serialno(&p_sys->current_page) )
                {
                    msg_Dbg( p_demux, "End of Skeleton" );
                    p_sys->b_preparsing_done = true;
                    for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
                    {
                        logical_stream_t *p_stream = p_sys->pp_stream[i_stream];
                        Ogg_ApplySkeleton( p_stream );
                    }
                }
            }

            for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
            {
                if ( p_sys->pp_stream[i_stream]->i_serial_no == ogg_page_serialno( &p_sys->current_page ) )
                {
                    p_sys->pp_stream[i_stream]->b_finished = true;
                    break;
                }
            }
        }
    }

    for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        logical_stream_t *p_stream = p_sys->pp_stream[i_stream];

        /* if we've just pulled page, look for the right logical stream */
        if( !p_sys->b_page_waiting )
        {
            if( p_sys->i_streams == 1 &&
                ogg_page_serialno( &p_sys->current_page ) != p_stream->os.serialno )
            {
                msg_Err( p_demux, "Broken Ogg stream (serialno) mismatch" );
                Ogg_ResetStream( p_stream );
                if( p_stream->i_pcr != VLC_TICK_INVALID )
                    p_sys->i_nzpcr_offset = p_stream->i_pcr - VLC_TICK_0;
                ogg_stream_reset_serialno( &p_stream->os, ogg_page_serialno( &p_sys->current_page ) );
            }

            /* Does fail if serialno differs */
            if( ogg_stream_pagein( &p_stream->os, &p_sys->current_page ) != 0 )
            {
                continue;
            }
        }

        /* clear the finished flag if pages after eos (ex: after a seek) */
        if ( ! ogg_page_eos( &p_sys->current_page ) && p_sys->p_skelstream != p_stream )
            p_stream->b_finished = false;

        DemuxDebug(
            if ( p_stream->fmt.i_cat == VIDEO_ES )
                msg_Dbg(p_demux, "DEMUX READ pageno %ld g%"PRId64" (%d packets) cont %d %ld bytes",
                    ogg_page_pageno( &p_sys->current_page ),
                    ogg_page_granulepos( &p_sys->current_page ),
                    ogg_page_packets( &p_sys->current_page ),
                    ogg_page_continued(&p_sys->current_page),
                    p_sys->current_page.body_len )
        );

        while( ogg_stream_packetout( &p_stream->os, &oggpacket ) > 0 )
        {
            /* Read info from any secondary header packets, if there are any */
            if( p_stream->i_secondary_header_packets > 0 )
            {
                if( p_stream->fmt.i_codec == VLC_CODEC_THEORA &&
                        oggpacket.bytes >= 7 &&
                        ! memcmp( oggpacket.packet, "\x80theora", 7 ) )
                {
                    Ogg_ReadTheoraHeader( p_stream, &oggpacket );
                    p_stream->i_secondary_header_packets = 0;
                }
                else if( p_stream->fmt.i_codec == VLC_CODEC_DAALA &&
                        oggpacket.bytes >= 6 &&
                        ! memcmp( oggpacket.packet, "\x80""daala", 6 ) )
                {
                    Ogg_ReadDaalaHeader( p_stream, &oggpacket );
                    p_stream->i_secondary_header_packets = 0;
                }
                else if( p_stream->fmt.i_codec == VLC_CODEC_VORBIS &&
                        oggpacket.bytes >= 7 &&
                        ! memcmp( oggpacket.packet, "\x01vorbis", 7 ) )
                {
                    es_format_Change( &p_stream->fmt, AUDIO_ES, VLC_CODEC_VORBIS );
                    Ogg_ReadVorbisHeader( p_stream, &oggpacket );
                    p_stream->i_secondary_header_packets = 0;
                }

                /* update start of data pointer */
                p_stream->i_data_start = vlc_stream_Tell( p_demux->s );
            }

            if( p_stream->b_reinit )
            {
                p_stream->b_reinit = false;
                if( p_stream->fmt.i_codec == VLC_CODEC_OPUS )
                {
                    p_stream->i_skip_frames = p_stream->i_pre_skip;
                }
            }

            Ogg_DecodePacket( p_demux, p_stream, &oggpacket );
        }


        if( !p_sys->b_page_waiting )
            break;
    }

    /* if a page was waiting, it's now processed */
    p_sys->b_page_waiting = false;

    if ( p_sys->p_skelstream && !p_sys->p_skelstream->b_finished )
        p_sys->b_preparsing_done = false;
    else
        p_sys->b_preparsing_done = true;

    if( p_sys->b_preparsing_done )
    {
        for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
        {
            logical_stream_t *p_stream = p_sys->pp_stream[i_stream];
            if ( p_stream->b_initializing )
            {
                /* We have 1 or more streams needing more than 1 page for preparsing */
                p_sys->b_preparsing_done = false;
                break;
            }
        }
    }

    if( p_sys->b_preparsing_done )
        Ogg_OutputQueues( p_demux, false );

    return VLC_DEMUXER_SUCCESS;
}

static void Ogg_ResetStream( logical_stream_t *p_stream )
{
#ifdef HAVE_LIBVORBIS
    if ( p_stream->fmt.i_codec == VLC_CODEC_VORBIS )
    {
        p_stream->special.vorbis.i_prev_blocksize = 0;
    }
#endif
    /* we'll trash all the data until we find the next pcr */
    p_stream->b_reinit = true;
    p_stream->i_pcr = VLC_TICK_INVALID;
    p_stream->i_next_block_flags = 0;
    p_stream->b_interpolation_failed = false;
    date_Set( &p_stream->dts, VLC_TICK_INVALID );
    ogg_stream_reset( &p_stream->os );
    block_ChainRelease( p_stream->queue.p_blocks );
    p_stream->queue.p_blocks = NULL;
    p_stream->queue.pp_append = &p_stream->queue.p_blocks;
}

static void Ogg_PreparePostSeek( demux_sys_t *p_sys )
{
    for( int i = 0; i < p_sys->i_streams; i++ )
    {
        Ogg_ResetStream( p_sys->pp_stream[i] );
        p_sys->pp_stream[i]->i_next_block_flags = BLOCK_FLAG_DISCONTINUITY;
    }

    ogg_sync_reset( &p_sys->oy );
    p_sys->i_pcr = VLC_TICK_INVALID;
}

static logical_stream_t * Ogg_GetSelectedStream( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    logical_stream_t *p_stream = NULL;
    for( int i=0; i<p_sys->i_streams; i++ )
    {
        logical_stream_t *p_candidate = p_sys->pp_stream[i];
        if ( !p_candidate->p_es ) continue;

        bool b_selected = false;
        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                        p_candidate->p_es, &b_selected );
        if ( !b_selected ) continue;

        if ( !p_stream && p_candidate->fmt.i_cat == AUDIO_ES )
        {
            p_stream = p_candidate;
            continue; /* Try to find video anyway */
        }

        if ( p_candidate->fmt.i_cat == VIDEO_ES )
        {
            p_stream = p_candidate;
            break;
        }
    }
    return p_stream;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    vlc_meta_t *p_meta;
    vlc_tick_t i64;
    double f;
    bool *pb_bool, b, acc;
    logical_stream_t *p_stream;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            return vlc_stream_vaControl( p_demux->s, i_query, args );

        case DEMUX_GET_META:
            p_meta = va_arg( args, vlc_meta_t * );
            if( p_sys->p_meta )
                vlc_meta_Merge( p_meta, p_sys->p_meta );
            return VLC_SUCCESS;

        case DEMUX_HAS_UNSUPPORTED_META:
            pb_bool = va_arg( args, bool* );
            *pb_bool = true;
            return VLC_SUCCESS;

        case DEMUX_SET_NEXT_DEMUX_TIME:
            p_sys->b_slave = true;
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
            if( p_sys->i_pcr != VLC_TICK_INVALID || p_sys->b_slave )
            {
                *va_arg( args, vlc_tick_t * ) = p_sys->i_pcr;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
        {
            i64 = va_arg( args, vlc_tick_t );
            acc = va_arg( args, int );
            p_stream = Ogg_GetSelectedStream( p_demux );
            if ( !p_stream )
            {
                msg_Err( p_demux, "No selected seekable stream found" );
                return VLC_EGENERIC;
            }
            vlc_stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &b );
            if ( Oggseek_BlindSeektoAbsoluteTime( p_demux, p_stream, VLC_TICK_0 + i64, b ) )
            {
                Ogg_PreparePostSeek( p_sys );
                if( acc )
                    es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                                    VLC_TICK_0 + i64 );
                return VLC_SUCCESS;
            }
            else
                return VLC_EGENERIC;
        }

        case DEMUX_GET_ATTACHMENTS:
        {
            input_attachment_t ***ppp_attach =
                va_arg( args, input_attachment_t *** );
            int *pi_int = va_arg( args, int * );

            if( p_sys->i_attachments <= 0 )
                return VLC_EGENERIC;

            *ppp_attach = vlc_alloc( p_sys->i_attachments, sizeof(input_attachment_t*) );
            if (!**ppp_attach)
                return VLC_ENOMEM;
            *pi_int = p_sys->i_attachments;
            for( int i = 0; i < p_sys->i_attachments; i++ )
                (*ppp_attach)[i] = vlc_input_attachment_Duplicate( p_sys->attachments[i] );
            return VLC_SUCCESS;
        }

        case DEMUX_GET_POSITION: {
            double pos = 0.;
            uint64_t size;

            if( p_sys->i_length > 0 && p_sys->i_pcr != VLC_TICK_INVALID )
            {
                vlc_tick_t duration = vlc_tick_from_sec( p_sys->i_length );
                pos = (double) p_sys->i_pcr / (double) duration;
            }
            else if( vlc_stream_GetSize( p_demux->s, &size ) == 0 && size > 0 )
            {
                uint64_t offset = vlc_stream_Tell( p_demux->s );
                pos = (double) offset / (double) size;
            }

            *va_arg( args, double * ) = pos;
            return VLC_SUCCESS;
        }

        case DEMUX_SET_POSITION:
            /* forbid seeking if we haven't initialized all logical bitstreams yet;
               if we allowed, some headers would not get backed up and decoder init
               would fail, making that logical stream unusable */
            for ( int i=0; i< p_sys->i_streams; i++ )
            {
                if ( p_sys->pp_stream[i]->b_initializing )
                    return VLC_EGENERIC;
            }

            p_stream = Ogg_GetSelectedStream( p_demux );
            if ( !p_stream )
            {
                msg_Err( p_demux, "No selected seekable stream found" );
                return VLC_EGENERIC;
            }

            vlc_stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &b );

            f = va_arg( args, double );
            acc = va_arg( args, int );
            if ( p_sys->i_length <= 0 || !b /* || ! STREAM_CAN_FASTSEEK */ )
            {
                Ogg_PreparePostSeek( p_sys );
                return Oggseek_BlindSeektoPosition( p_demux, p_stream, f, b );
            }

            assert( p_sys->i_length > 0 );
            i64 = vlc_tick_from_sec( f * p_sys->i_length );
            Ogg_PreparePostSeek( p_sys );
            if ( Oggseek_SeektoAbsolutetime( p_demux, p_stream, VLC_TICK_0 + i64 ) >= 0 )
            {
                if( acc )
                    es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                                    VLC_TICK_0 + i64 );
                return VLC_SUCCESS;
            }

            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            if ( p_sys->i_length < 0 )
                return demux_vaControlHelper( p_demux->s, 0, -1, p_sys->i_bitrate,
                                              1, i_query, args );
            *va_arg( args, vlc_tick_t * ) = vlc_tick_from_sec(p_sys->i_length);
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***ppp_title = va_arg( args, input_title_t *** );
            int *pi_int = va_arg( args, int* );
            int *pi_title_offset = va_arg( args, int* );
            int *pi_seekpoint_offset = va_arg( args, int* );

            if( p_sys->i_seekpoints > 0 )
            {
                *pi_int = 1;
                *ppp_title = malloc( sizeof( input_title_t* ) );
                input_title_t *p_title = (*ppp_title)[0] = vlc_input_title_New();
                for( int i = 0; i < p_sys->i_seekpoints; i++ )
                {
                    seekpoint_t *p_seekpoint_copy = vlc_seekpoint_Duplicate( p_sys->pp_seekpoints[i] );
                    if ( likely( p_seekpoint_copy ) )
                        TAB_APPEND( p_title->i_seekpoint, p_title->seekpoint, p_seekpoint_copy );
                }
                *pi_title_offset = 0;
                *pi_seekpoint_offset = 0;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }
        case DEMUX_SET_TITLE:
        {
            const int i_title = va_arg( args, int );
            if( i_title > 1 )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_SEEKPOINT:
        {
            const int i_seekpoint = va_arg( args, int );
            if( i_seekpoint > p_sys->i_seekpoints )
                return VLC_EGENERIC;

            for ( int i=0; i< p_sys->i_streams; i++ )
            {
                if ( p_sys->pp_stream[i]->b_initializing )
                    return VLC_EGENERIC;
            }

            i64 = p_sys->pp_seekpoints[i_seekpoint]->i_time_offset;

            p_stream = Ogg_GetSelectedStream( p_demux );
            if ( !p_stream )
            {
                msg_Err( p_demux, "No selected seekable stream found" );
                return VLC_EGENERIC;
            }

            vlc_stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &b );
            if ( Oggseek_BlindSeektoAbsoluteTime( p_demux, p_stream, VLC_TICK_0 + i64, b ) )
            {
                Ogg_PreparePostSeek( p_sys );
                es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                                VLC_TICK_0 + i64 );
                p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
                p_sys->cur_seekpoint = i_seekpoint;
                return VLC_SUCCESS;
            }
            else
                return VLC_EGENERIC;
        }
        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict flags = va_arg( args, unsigned * );
            *flags &= p_sys->updates;
            p_sys->updates &= ~*flags;
            return VLC_SUCCESS;
        }
        case DEMUX_GET_TITLE:
            *va_arg( args, int * ) = 0;
            return VLC_SUCCESS;
        case DEMUX_GET_SEEKPOINT:
            *va_arg( args, int * ) = p_sys->cur_seekpoint;
            return VLC_SUCCESS;

        default:
            return demux_vaControlHelper( p_demux->s, 0, -1, p_sys->i_bitrate,
                                           1, i_query, args );
    }
}

/****************************************************************************
 * Ogg_ReadPage: Read a full Ogg page from the physical bitstream.
 ****************************************************************************
 * Returns VLC_SUCCESS if a page has been read. An error might happen if we
 * are at the end of stream.
 ****************************************************************************/
static int Ogg_ReadPage( demux_t *p_demux, ogg_page *p_oggpage )
{
    demux_sys_t *p_ogg = p_demux->p_sys  ;
    int i_read = 0;
    char *p_buffer;

    while( ogg_sync_pageout( &p_ogg->oy, p_oggpage ) != 1 )
    {
        p_buffer = ogg_sync_buffer( &p_ogg->oy, OGGSEEK_BYTES_TO_READ );

        i_read = vlc_stream_Read( p_demux->s, p_buffer, OGGSEEK_BYTES_TO_READ );
        if( i_read <= 0 )
            return VLC_EGENERIC;

        ogg_sync_wrote( &p_ogg->oy, i_read );
    }

    return VLC_SUCCESS;
}

static void Ogg_SetNextFrame( demux_t *p_demux, logical_stream_t *p_stream,
                              ogg_packet *p_oggpacket )
{
    VLC_UNUSED(p_demux);
    ogg_int64_t i_granule = p_oggpacket->granulepos;

    if( Ogg_GranuleIsValid( p_stream, i_granule ) )
    {
        vlc_tick_t i_endtime = Ogg_GranuleToTime( p_stream, i_granule, false, false );
        assert( !p_stream->b_contiguous || i_endtime != VLC_TICK_INVALID );
        if( i_endtime != VLC_TICK_INVALID )
        {
            date_Set( &p_stream->dts, i_endtime );
            return;
        }
    }

    /* Do Interpolation if can't compute directly from granule */
    if( date_Get( &p_stream->dts ) != VLC_TICK_INVALID )
    {
        if( p_stream->fmt.i_cat == VIDEO_ES )
        {
            date_Increment( &p_stream->dts, 1 );
        }
        else if( p_stream->fmt.i_cat == AUDIO_ES )
        {
            int64_t i_samples = 0;
            switch( p_stream->fmt.i_codec )
            {
                case VLC_CODEC_OPUS:
                    i_samples = Ogg_OpusPacketDuration( p_oggpacket );
                    break;
                case VLC_CODEC_SPEEX:
                    i_samples = p_stream->special.speex.i_framesize *
                                p_stream->special.speex.i_framesperpacket;
                    break;
#ifdef HAVE_LIBVORBIS
                case VLC_CODEC_VORBIS:
                    if( p_stream->special.vorbis.p_info &&
                        VORBIS_HEADERS_VALID(p_stream) )
                    {
                        long i_blocksize = vorbis_packet_blocksize(
                                    p_stream->special.vorbis.p_info, p_oggpacket );
                        /* duration in samples per channel */
                        if ( p_stream->special.vorbis.i_prev_blocksize )
                            i_samples = ( i_blocksize + p_stream->special.vorbis.i_prev_blocksize ) / 4;
                        else
                            i_samples = i_blocksize / 2;
                        p_stream->special.vorbis.i_prev_blocksize = i_blocksize;
                    }
                    break;
#endif
                default:
                    if( p_stream->fmt.i_bitrate )
                    {
                        i_samples = 8 * p_oggpacket->bytes * p_stream->dts.i_divider_num;
                        i_samples /= p_stream->fmt.i_bitrate / p_stream->dts.i_divider_den;
                    }
                    break;
            }
            if( i_samples == 0 )
                p_stream->b_interpolation_failed = true;
            else
                date_Increment( &p_stream->dts, i_samples );
        }
    }
}

static vlc_tick_t Ogg_FixupOutputQueue( demux_t *p_demux, logical_stream_t *p_stream )
{
    vlc_tick_t i_enddts = VLC_TICK_INVALID;

#ifdef HAVE_LIBVORBIS
    long i_prev_blocksize = 0;
#else
    VLC_UNUSED(p_demux);
#endif
    // PASS 1, set number of samples
    unsigned i_total_samples = 0;
    for( block_t *p_block = p_stream->queue.p_blocks; p_block; p_block = p_block->p_next )
    {
        if( p_block->i_dts != VLC_TICK_INVALID )
        {
            i_enddts = p_block->i_dts;
            break;
        }

        if( p_block->i_flags & BLOCK_FLAG_HEADER )
            continue;

        ogg_packet dumb_packet;
        dumb_packet.bytes = p_block->i_buffer;
        dumb_packet.packet = p_block->p_buffer;

        switch( p_stream->fmt.i_codec )
        {
            case VLC_CODEC_SPEEX:
                p_block->i_nb_samples = p_stream->special.speex.i_framesize *
                                        p_stream->special.speex.i_framesperpacket;
                break;
            case VLC_CODEC_OPUS:
                p_block->i_nb_samples = Ogg_OpusPacketDuration( &dumb_packet );
                break;
#ifdef HAVE_LIBVORBIS
            case VLC_CODEC_VORBIS:
            {
                if( !VORBIS_HEADERS_VALID(p_stream) )
                {
                    msg_Err( p_demux, "missing vorbis headers, can't compute block size" );
                    break;
                }
                long i_blocksize = vorbis_packet_blocksize( p_stream->special.vorbis.p_info,
                                                            &dumb_packet );
                if ( i_prev_blocksize )
                    p_block->i_nb_samples = ( i_blocksize + i_prev_blocksize ) / 4;
                else
                    p_block->i_nb_samples = i_blocksize / 2;
                i_prev_blocksize = i_blocksize;
                break;
            }
#endif
            default:
                if( p_stream->fmt.i_cat == VIDEO_ES )
                    p_block->i_nb_samples = 1;
                break;
        }
        i_total_samples += p_block->i_nb_samples;
    }

    // PASS 2
    if( i_enddts != VLC_TICK_INVALID )
    {
        date_t d = p_stream->dts;
        date_Set( &d, i_enddts );
        i_enddts = date_Decrement( &d, i_total_samples );
        for( block_t *p_block = p_stream->queue.p_blocks; p_block; p_block = p_block->p_next )
        {
            if( p_block->i_dts != VLC_TICK_INVALID )
                break;
            if( p_block->i_flags & BLOCK_FLAG_HEADER )
                continue;
            p_block->i_dts = date_Get( &d );
            date_Increment( &d, p_block->i_nb_samples );
        }
    } /* else can't do anything, no timestamped blocks in stream */

    return i_enddts;
}

static void Ogg_QueueBlocks( demux_t *p_demux, logical_stream_t *p_stream, block_t *p_block )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    VLC_UNUSED(p_sys);

    if( p_block == NULL )
    {
        assert( p_block != NULL );
        return;
    }

    block_ChainLastAppend( &p_stream->queue.pp_append, p_block );

    if( p_stream->i_pcr == VLC_TICK_INVALID && p_block->i_dts != VLC_TICK_INVALID )
    {
        /* fixup queue */
        p_stream->i_pcr = Ogg_FixupOutputQueue( p_demux, p_stream );
    }

    DemuxDebug( msg_Dbg( p_demux, "%4.4s block queued > dts %"PRId64" spcr %"PRId64" pcr %"PRId64,
                         (char*)&p_stream->fmt.i_codec, p_block->i_dts, p_stream->i_pcr, p_sys->i_pcr ); )
}

static void Ogg_SendQueuedBlock( demux_t *p_demux, logical_stream_t *p_stream )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( Ogg_HasQueuedBlocks( p_stream ) )
    {
        block_t *p_queued = p_stream->queue.p_blocks;
        p_stream->queue.p_blocks = p_queued->p_next;
        p_queued->p_next = NULL;

        if( p_queued->i_dts == VLC_TICK_INVALID )
            p_queued->i_dts = p_queued->i_pts;

        if( p_queued->i_flags & BLOCK_FLAG_HEADER )
        {
            if( p_sys->i_nzpcr_offset > 0 || /* Don't send metadata from chained streams */
                p_stream->fmt.i_extra > 0 )  /* Don't send metadata if configured by extradata */
            {
                block_Release( p_queued );
                goto end;
            }
            p_queued->i_flags &= ~BLOCK_FLAG_HEADER;
        }

        unsigned i_toskip = 0;
        if( p_stream->i_skip_frames > 0 )
        {
            if( p_sys->i_nzpcr_offset > 0 )
            {
                /* not preskip handling on chained streams */
                p_stream->i_skip_frames = 0;
            }
            else
            {
                i_toskip = __MIN( p_stream->i_skip_frames, p_queued->i_nb_samples );
                p_stream->i_skip_frames -= i_toskip;
                p_queued->i_nb_samples -= i_toskip;
                if( p_queued->i_nb_samples == 0 )
                    p_queued->i_flags |= BLOCK_FLAG_PREROLL;
            }
        }

        p_queued->i_flags |= p_stream->i_next_block_flags;
        p_stream->i_next_block_flags = 0;
        p_stream->i_pcr = p_queued->i_dts;

        DemuxDebug( msg_Dbg( p_demux, "%4.4s block sent > dts %"PRId64" pts %"PRId64" spcr %"PRId64" pcr %"PRId64
                             " samples (%d/%d)",
                             (char*)&p_stream->fmt.i_codec, p_queued->i_dts,
                             p_queued->i_pts, p_stream->i_pcr, p_sys->i_pcr,
                             p_queued->i_nb_samples, i_toskip ); );

        assert( p_sys->i_pcr != VLC_TICK_INVALID );

        if( p_stream->p_es )
            es_out_Send( p_demux->out, p_stream->p_es, p_queued );
        else
            block_Release( p_queued );
    }

end:
    if( p_stream->queue.p_blocks == NULL )
        p_stream->queue.pp_append = &p_stream->queue.p_blocks;
}

static bool Ogg_IsHeaderPacket( const logical_stream_t *p_stream,
                                const ogg_packet *p_oggpacket )
{
    if ( p_stream->b_oggds )
    {
        return p_oggpacket->bytes > 0 &&
               (p_oggpacket->packet[0] & PACKET_TYPE_HEADER);
    }
    else return ( p_oggpacket->granulepos == 0 && p_stream->i_first_frame_index > 0 );
}

/****************************************************************************
 * Ogg_DecodePacket: Decode an Ogg packet.
 ****************************************************************************/
static void Ogg_DecodePacket( demux_t *p_demux,
                              logical_stream_t *p_stream,
                              ogg_packet *p_oggpacket )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block;
    bool b_selected;
    long i_header_len = 0;

    if( p_oggpacket->bytes >= 7 &&
        ! memcmp ( p_oggpacket->packet, "Annodex", 7 ) )
    {
        /* it's an Annodex packet -- skip it (do nothing) */
        return;
    }
    else if( p_oggpacket->bytes >= 7 &&
        ! memcmp ( p_oggpacket->packet, "AnxData", 7 ) )
    {
        /* it's an AnxData packet -- skip it (do nothing) */
        return;
    }
    else if( p_oggpacket->bytes >= 8 &&
        ! memcmp ( p_oggpacket->packet, "fisbone", 8 ) )
    {
        Ogg_ReadSkeletonBones( p_demux, p_oggpacket );
        return;
    }
    else if( p_oggpacket->bytes >= 6 &&
        ! memcmp ( p_oggpacket->packet, "index", 6 ) )
    {
        Ogg_ReadSkeletonIndex( p_demux, p_oggpacket );
        return;
    }
    else if( p_stream->fmt.i_codec == VLC_CODEC_VP8 &&
             p_oggpacket->bytes >= 7 &&
             !memcmp( p_oggpacket->packet, "OVP80\x02\x20", 7 ) )
    {
        Ogg_ReadVP8Header( p_demux, p_stream, p_oggpacket );
        return;
    }

    if( p_stream->fmt.i_codec == VLC_CODEC_SUBT && p_oggpacket->bytes > 0 &&
        p_oggpacket->packet[0] & PACKET_TYPE_BITS ) return;

    /* Check the ES is selected */
    if ( !p_stream->p_es )
        b_selected = true;
    else
        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                        p_stream->p_es, &b_selected );

    if( p_stream->b_force_backup )
    {
        bool b_xiph;
        p_stream->i_packets_backup++;
        switch( p_stream->fmt.i_codec )
        {
        case VLC_CODEC_VORBIS:
#ifdef HAVE_LIBVORBIS
            Ogg_DecodeVorbisHeader( p_stream, p_oggpacket, p_stream->i_packets_backup );
#endif
            /* fallthrough */
        case VLC_CODEC_THEORA:
            if( p_stream->i_packets_backup == 3 )
                p_stream->b_force_backup = false;
            b_xiph = true;
            break;

        case VLC_CODEC_DAALA:
            if( p_stream->i_packets_backup == 3 )
                p_stream->b_force_backup = false;
            b_xiph = true;
            break;

        case VLC_CODEC_SPEEX:
            if( p_stream->i_packets_backup == 2 + p_stream->i_extra_headers_packets )
                p_stream->b_force_backup = false;
            b_xiph = true;
            break;

        case VLC_CODEC_OPUS:
            if( p_stream->i_packets_backup == 2 )
                p_stream->b_force_backup = false;
            b_xiph = true;
            break;

        case VLC_CODEC_FLAC:
            if( p_stream->i_packets_backup == 1 + p_stream->i_extra_headers_packets )
            {
                p_stream->b_force_backup = false;
            }
            if( p_stream->special.flac.b_old )
            {
                Ogg_ReadFlacStreamInfo( p_demux, p_stream, p_oggpacket );
            }
            else if( p_stream->i_packets_backup == 1 )
            {
                if( p_oggpacket->bytes >= 9 ) /* Point to Flac for extradata */
                {
                    p_oggpacket->packet += 9;
                    p_oggpacket->bytes -= 9;
                }
            }
            b_xiph = false;
            break;

        case VLC_CODEC_KATE:
            if( p_stream->i_packets_backup == p_stream->special.kate.i_num_headers )
                p_stream->b_force_backup = false;
            b_xiph = true;
            break;

        default:
            p_stream->b_force_backup = false;
            b_xiph = false;
            break;
        }

        /* Backup the ogg packet (likely an header packet) */
        if( !b_xiph )
        {
            uint8_t *p_realloc = realloc( p_stream->p_headers, p_stream->i_headers + p_oggpacket->bytes );
            if( p_realloc )
            {
                memcpy( &p_realloc[p_stream->i_headers], p_oggpacket->packet, p_oggpacket->bytes );
                p_stream->i_headers += p_oggpacket->bytes;
                p_stream->p_headers = p_realloc;
            }
            else
            {
                free( p_stream->p_headers );
                p_stream->i_headers = 0;
                p_stream->p_headers = NULL;
            }
        }
        else if( xiph_AppendHeaders( &p_stream->i_headers, &p_stream->p_headers,
                                     p_oggpacket->bytes, p_oggpacket->packet ) )
        {
            p_stream->i_headers = 0;
            p_stream->p_headers = NULL;
        }
        if( p_stream->i_headers > 0 )
        {
            if( !p_stream->b_force_backup )
            {
                /* Last header received, commit changes */
                free( p_stream->fmt.p_extra );

                p_stream->fmt.i_extra = p_stream->i_headers;
                p_stream->fmt.p_extra = malloc( p_stream->i_headers );
                if( p_stream->fmt.p_extra )
                    memcpy( p_stream->fmt.p_extra, p_stream->p_headers,
                            p_stream->i_headers );
                else
                    p_stream->fmt.i_extra = 0;

                if( p_stream->i_headers > 0 )
                    Ogg_ExtractMeta( p_demux, & p_stream->fmt,
                                     p_stream->p_headers, p_stream->i_headers );

                /* we're not at BOS anymore for this logical stream */
                p_stream->b_initializing = false;
            }
        }

        b_selected = false; /* Discard the header packet */
    }
    else
    {
        p_stream->b_initializing = false;
    }

    vlc_tick_t i_dts = Ogg_GranuleToTime( p_stream, p_oggpacket->granulepos, true, false );
    vlc_tick_t i_expected_dts = p_stream->b_interpolation_failed ? VLC_TICK_INVALID :
                                date_Get( &p_stream->dts ); /* Interpolated or previous end time */
    if( i_dts == VLC_TICK_INVALID )
        i_dts = i_expected_dts;
    else
        date_Set( &p_stream->dts, i_dts );

    /* Write end granule as next start, or do interpolation */
    bool b_header = Ogg_IsHeaderPacket( p_stream, p_oggpacket );
    if( !b_header )
        Ogg_SetNextFrame( p_demux, p_stream, p_oggpacket );

    if( !b_selected )
    {
        /* This stream isn't currently selected so we don't need to decode it,
         * but we did need to store its pcr as it might be selected later on */
        if( !b_header && !p_stream->b_initializing )
        {
            vlc_tick_t i_pcr = date_Get( &p_stream->dts );
            if( i_pcr != VLC_TICK_INVALID )
                p_stream->i_pcr = p_sys->i_nzpcr_offset + i_pcr;
        }
        return;
    }

    if( !( p_block = block_Alloc( p_oggpacket->bytes ) ) )
        return;

    /* Set effective timestamp */
    if( i_dts != VLC_TICK_INVALID )
        p_block->i_dts = p_sys->i_nzpcr_offset + i_dts;

    /* Vorbis and Opus can trim the end of a stream using granule positions. */
    if( p_oggpacket->e_o_s )
    {
        vlc_tick_t i_endtime = Ogg_GranuleToTime( p_stream, p_oggpacket->granulepos, false, false );
        if( i_endtime != VLC_TICK_INVALID && i_expected_dts != VLC_TICK_INVALID )
        {
                p_block->i_length = i_endtime - i_expected_dts;
                p_block->i_flags |= BLOCK_FLAG_END_OF_SEQUENCE;
        }
    }

    if( p_stream->fmt.i_codec == VLC_CODEC_OPUS ) /* also required for trimming */
        p_block->i_nb_samples = Ogg_OpusPacketDuration( p_oggpacket );

    DemuxDebug( msg_Dbg(p_demux, "%4.4s block set from granule %"PRId64" to pts/pcr %"PRId64" skip %d",
                        (char *) &p_stream->fmt.i_codec, p_oggpacket->granulepos,
                        p_block->i_dts, p_stream->i_skip_frames); )

    /* may need to preroll after a seek or in case of preskip */

    /* Conditional block fixes */
    if ( p_stream->fmt.i_cat == VIDEO_ES )
    {
        if( Ogg_IsKeyFrame( p_stream, p_oggpacket ) )
            p_block->i_flags |= BLOCK_FLAG_TYPE_I;

        if( p_stream->fmt.i_codec == VLC_CODEC_DIRAC )
        {
            if( p_oggpacket->granulepos > 0 )
                p_block->i_pts = Ogg_GranuleToTime( p_stream, p_oggpacket->granulepos, true, true );
        }
        else if( p_stream->fmt.i_codec == VLC_CODEC_THEORA )
        {
            p_block->i_pts = p_block->i_dts;
        }
    }
    else if( p_stream->fmt.i_cat == AUDIO_ES )
    {
        if( p_stream->b_interpolation_failed && p_oggpacket->granulepos < 0 )
            p_block->i_pts = VLC_TICK_INVALID;
        else
            p_block->i_pts = p_block->i_dts;
    }
    else if( p_stream->fmt.i_cat == SPU_ES )
    {
        p_block->i_length = 0;
        p_block->i_pts = p_block->i_dts;
    }

    p_stream->b_interpolation_failed = false;

    if( p_stream->b_oggds )
    {
        if( p_oggpacket->bytes <= 0 )
        {
            msg_Dbg( p_demux, "discarding 0 sized packet" );
            block_Release( p_block );
            return;
        }
        /* We remove the header from the packet */
        i_header_len = (*p_oggpacket->packet & PACKET_LEN_BITS01) >> 6;
        i_header_len |= (*p_oggpacket->packet & PACKET_LEN_BITS2) << 1;

        if( i_header_len >= p_oggpacket->bytes )
        {
            msg_Dbg( p_demux, "discarding invalid packet" );
            block_Release( p_block );
            return;
        }

        if( p_stream->fmt.i_codec == VLC_CODEC_SUBT)
        {
            /* But with subtitles we need to retrieve the duration first */
            int i, lenbytes = 0;

            if( i_header_len > 0 && p_oggpacket->bytes >= i_header_len + 1 )
            {
                for( i = 0, lenbytes = 0; i < i_header_len; i++ )
                {
                    lenbytes = lenbytes << 8;
                    lenbytes += *(p_oggpacket->packet + i_header_len - i);
                }
            }
            if( p_oggpacket->bytes - 1 - i_header_len > 2 ||
                ( p_oggpacket->packet[i_header_len + 1] != ' ' &&
                  p_oggpacket->packet[i_header_len + 1] != 0 &&
                  p_oggpacket->packet[i_header_len + 1] != '\n' &&
                  p_oggpacket->packet[i_header_len + 1] != '\r' ) )
            {
                p_block->i_length = (vlc_tick_t)lenbytes * 1000;
            }
        }

        i_header_len++;
        if( p_block->i_buffer >= (unsigned int)i_header_len )
            p_block->i_buffer -= i_header_len;
        else
            p_block->i_buffer = 0;
    }

    if( b_header )
        p_block->i_flags |= BLOCK_FLAG_HEADER;

    memcpy( p_block->p_buffer, p_oggpacket->packet + i_header_len,
            p_oggpacket->bytes - i_header_len );

    Ogg_QueueBlocks( p_demux, p_stream, p_block );
}

static unsigned Ogg_OpusPacketDuration( ogg_packet *p_oggpacket )
{
    return opus_frame_duration(p_oggpacket->packet, p_oggpacket->bytes);
}

/****************************************************************************
 * Ogg_FindLogicalStreams: Find the logical streams embedded in the physical
 *                         stream and fill p_ogg.
 *****************************************************************************
 * The initial page of a logical stream is marked as a 'bos' page.
 * Furthermore, the Ogg specification mandates that grouped bitstreams begin
 * together and all of the initial pages must appear before any data pages.
 *
 * On success this function returns VLC_SUCCESS.
 ****************************************************************************/
static int Ogg_FindLogicalStreams( demux_t *p_demux )
{
    demux_sys_t *p_ogg = p_demux->p_sys;
    ogg_packet oggpacket;

    p_ogg->i_total_length = stream_Size ( p_demux->s );
    msg_Dbg( p_demux, "File length is %"PRId64" bytes", p_ogg->i_total_length );


    while( Ogg_ReadPage( p_demux, &p_ogg->current_page ) == VLC_SUCCESS )
    {

        if( ogg_page_bos( &p_ogg->current_page ) )
        {

            /* All is wonderful in our fine fine little world.
             * We found the beginning of our first logical stream. */
            while( ogg_page_bos( &p_ogg->current_page ) )
            {
                logical_stream_t *p_stream = malloc( sizeof(logical_stream_t) );
                if( unlikely( !p_stream ) )
                    return VLC_ENOMEM;

                Ogg_LogicalStreamInit( p_stream );

                /* Setup the logical stream */
                p_stream->i_serial_no = ogg_page_serialno( &p_ogg->current_page );
                ogg_stream_init( &p_stream->os, p_stream->i_serial_no );

                TAB_APPEND( p_ogg->i_streams, p_ogg->pp_stream, p_stream );

                /* Extract the initial header from the first page and verify
                 * the codec type of this Ogg bitstream */
                if( ogg_stream_pagein( &p_stream->os, &p_ogg->current_page ) < 0 )
                {
                    /* error. stream version mismatch perhaps */
                    msg_Err( p_demux, "error reading first page of "
                             "Ogg bitstream data" );
                    return VLC_EGENERIC;
                }

                if ( ogg_stream_packetpeek( &p_stream->os, &oggpacket ) != 1 )
                {
                    msg_Err( p_demux, "error in ogg_stream_packetpeek" );
                    return VLC_EGENERIC;
                }

                /* Check for Vorbis header */
                if( oggpacket.bytes >= 7 &&
                    ! memcmp( oggpacket.packet, "\x01vorbis", 7 ) )
                {
                    es_format_Change( &p_stream->fmt, AUDIO_ES, VLC_CODEC_VORBIS);
                    if ( Ogg_ReadVorbisHeader( p_stream, &oggpacket ) )
                        msg_Dbg( p_demux, "found vorbis header" );
                    else
                    {
                        msg_Dbg( p_demux, "found invalid vorbis header" );
                        Ogg_LogicalStreamDelete( p_demux, p_stream );
                        p_stream = NULL;
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                /* Check for Speex header */
                else if( oggpacket.bytes >= 5 &&
                    ! memcmp( oggpacket.packet, "Speex", 5 ) )
                {
                    es_format_Change( &p_stream->fmt, AUDIO_ES, VLC_CODEC_SPEEX );
                    if ( Ogg_ReadSpeexHeader( p_stream, &oggpacket ) )
                        msg_Dbg( p_demux, "found speex header, channels: %i, "
                                "rate: %"PRIu32"/%"PRIu32",  bitrate: %i, frames: %i group %i",
                                p_stream->fmt.audio.i_channels,
                                p_stream->dts.i_divider_num, p_stream->dts.i_divider_den,
                                p_stream->fmt.i_bitrate,
                                p_stream->special.speex.i_framesize,
                                p_stream->special.speex.i_framesperpacket );
                    else
                    {
                        msg_Dbg( p_demux, "found invalid Speex header" );
                        Ogg_LogicalStreamDelete( p_demux, p_stream );
                        p_stream = NULL;
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                /* Check for Opus header */
                else if( oggpacket.bytes >= 8 &&
                    ! memcmp( oggpacket.packet, "OpusHead", 8 ) )
                {
                    es_format_Change( &p_stream->fmt, AUDIO_ES, VLC_CODEC_OPUS );
                    Ogg_ReadOpusHeader( p_stream, &oggpacket );
                    msg_Dbg( p_demux, "found opus header, channels: %i, "
                             "pre-skip: %i",
                             p_stream->fmt.audio.i_channels,
                             (int)p_stream->i_pre_skip);
                    p_stream->i_skip_frames = p_stream->i_pre_skip;
                }
                /* Check for OLD Flac header */
                else if( oggpacket.bytes >= 4 &&
                    ! memcmp( oggpacket.packet, "fLaC", 4 ) )
                {
                    msg_Dbg( p_demux, "found FLAC header" );

                    /* Grrrr!!!! Did they really have to put all the
                     * important info in the second header packet!!!
                     * (STREAMINFO metadata is in the following packet) */
                    p_stream->b_force_backup = true;
                    p_stream->i_extra_headers_packets = 1;
                    p_stream->special.flac.b_old = true;
                    date_Init( &p_stream->dts, 48000, 1 ); /* better be safe since that's delayed */
                    es_format_Change( &p_stream->fmt, AUDIO_ES, VLC_CODEC_FLAC );
                }
                /* Check for Flac header (>= version 1.0.0) */
                else if( oggpacket.bytes >= 13 && oggpacket.packet[0] ==0x7F &&
                    ! memcmp( &oggpacket.packet[1], "FLAC", 4 ) &&
                    ! memcmp( &oggpacket.packet[9], "fLaC", 4 ) )
                {
                    int i_packets = ((int)oggpacket.packet[7]) << 8 |
                        oggpacket.packet[8];
                    msg_Dbg( p_demux, "found FLAC header version %i.%i "
                             "(%i header packets)",
                             oggpacket.packet[5], oggpacket.packet[6],
                             i_packets );
                    /* STREAMINFO is in current packet, and then
                       followed by 0 or more metadata, blockheader prefixed, and first being a vorbis comment */
                    p_stream->b_force_backup = true;
                    p_stream->i_extra_headers_packets = i_packets;
                    p_stream->special.flac.b_old = false;

                    es_format_Change( &p_stream->fmt, AUDIO_ES, VLC_CODEC_FLAC );
                    oggpacket.packet += 13; oggpacket.bytes -= 13; /* Point to the streaminfo */
                    if ( !Ogg_ReadFlacStreamInfo( p_demux, p_stream, &oggpacket ) )
                    {
                        msg_Dbg( p_demux, "found invalid Flac header" );
                        Ogg_LogicalStreamDelete( p_demux, p_stream );
                        p_stream = NULL;
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                    p_stream->fmt.b_packetized = false;
                }
                /* Check for Theora header */
                else if( oggpacket.bytes >= 7 &&
                         ! memcmp( oggpacket.packet, "\x80theora", 7 ) )
                {
                    es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_CODEC_THEORA );
                    if ( Ogg_ReadTheoraHeader( p_stream, &oggpacket ) )
                        msg_Dbg( p_demux,
                                 "found theora header, bitrate: %i, rate: %"PRIu32"/%"PRIu32,
                                 p_stream->fmt.i_bitrate,
                                 p_stream->dts.i_divider_num, p_stream->dts.i_divider_den );
                    else
                    {
                        msg_Dbg( p_demux, "found invalid Theora header" );
                        Ogg_LogicalStreamDelete( p_demux, p_stream );
                        p_stream = NULL;
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                /* Check for Daala header */
                else if( oggpacket.bytes >= 6 &&
                         ! memcmp( oggpacket.packet, "\x80""daala", 6 ) )
                {
                    es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_CODEC_DAALA );
                    if ( Ogg_ReadDaalaHeader( p_stream, &oggpacket ) )
                        msg_Dbg( p_demux,
                                 "found daala header, bitrate: %i, rate: %"PRIu32"/%"PRIu32,
                                 p_stream->fmt.i_bitrate,
                                 p_stream->dts.i_divider_num, p_stream->dts.i_divider_den );
                    else
                    {
                        msg_Dbg( p_demux, "found invalid Daala header" );
                        Ogg_LogicalStreamDelete( p_demux, p_stream );
                        p_stream = NULL;
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                /* Check for Dirac header */
                else if( ( oggpacket.bytes >= 5 &&
                           ! memcmp( oggpacket.packet, "BBCD\x00", 5 ) ) ||
                         ( oggpacket.bytes >= 9 &&
                           ! memcmp( oggpacket.packet, "KW-DIRAC\x00", 9 ) ) )
                {
                    es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_CODEC_DIRAC );
                    if( Ogg_ReadDiracHeader( p_stream, &oggpacket ) )
                        msg_Dbg( p_demux, "found dirac header" );
                    else
                    {
                        msg_Warn( p_demux, "found dirac header isn't decodable" );
                        Ogg_LogicalStreamDelete( p_demux, p_stream );
                        p_stream = NULL;
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                /* Check for VP8 header */
                else if( oggpacket.bytes >= 26 &&
                         ! memcmp( oggpacket.packet, "OVP80", 5 ) )
                {
                    es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_CODEC_VP8 );
                    if ( Ogg_ReadVP8Header( p_demux, p_stream, &oggpacket ) )
                        msg_Dbg( p_demux, "found VP8 header "
                             "fps: %"PRIu32"/%"PRIu32", width:%i; height:%i",
                             p_stream->dts.i_divider_num, p_stream->dts.i_divider_den,
                             p_stream->fmt.video.i_width,
                             p_stream->fmt.video.i_height );
                    else
                    {
                        msg_Dbg( p_demux, "invalid VP8 header found");
                        Ogg_LogicalStreamDelete( p_demux, p_stream );
                        p_stream = NULL;
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                /* Check for Annodex header */
                else if( oggpacket.bytes >= 7 &&
                         ! memcmp( oggpacket.packet, "Annodex", 7 ) )
                {
                    Ogg_ReadAnnodexHeader( p_demux, p_stream, &oggpacket );
                    /* kill annodex track */
                    FREENULL( p_stream );
                    TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                               p_ogg->i_streams - 1 );
                }
                /* Check for Annodex header */
                else if( oggpacket.bytes >= 7 &&
                         ! memcmp( oggpacket.packet, "AnxData", 7 ) )
                {
                    Ogg_ReadAnnodexHeader( p_demux, p_stream, &oggpacket );
                }
                /* Check for Kate header */
                else if( oggpacket.bytes >= 8 &&
                    ! memcmp( &oggpacket.packet[1], "kate\0\0\0", 7 ) )
                {
                    es_format_Change( &p_stream->fmt, SPU_ES, VLC_CODEC_KATE );
                    if ( Ogg_ReadKateHeader( p_stream, &oggpacket ) )
                        msg_Dbg( p_demux, "found kate header" );
                    else
                    {
                        msg_Dbg( p_demux, "invalid kate header found");
                        Ogg_LogicalStreamDelete( p_demux, p_stream );
                        p_stream = NULL;
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                /* Check for OggDS */
                else if( oggpacket.bytes >= 142 &&
                         !memcmp( &oggpacket.packet[1],
                                   "Direct Show Samples embedded in Ogg", 35 ))
                {
                    /* Old header type */
                    p_stream->b_oggds = true;
                    p_stream->b_contiguous = false;
                    /* Check for video header (old format) */
                    if( GetDWLE((oggpacket.packet+96)) == 0x05589f80 &&
                        oggpacket.bytes >= 184 )
                    {
                        es_format_Change( &p_stream->fmt, VIDEO_ES,
                                          VLC_FOURCC( oggpacket.packet[68],
                                                      oggpacket.packet[69],
                                                      oggpacket.packet[70],
                                                      oggpacket.packet[71] ) );
                        msg_Dbg( p_demux, "found video header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );

                        unsigned num = OGGDS_RESOLUTION;
                        unsigned den = GetQWLE(oggpacket.packet+164);
                        vlc_ureduce( &num, &den, num, den > 0 ? den : 1, OGGDS_RESOLUTION );
                        p_stream->fmt.video.i_frame_rate = num;
                        p_stream->fmt.video.i_frame_rate_base = den;
                        date_Init( &p_stream->dts, num, den );
                        p_stream->fmt.video.i_bits_per_pixel =
                            GetWLE((oggpacket.packet+182));
                        if( !p_stream->fmt.video.i_bits_per_pixel )
                            /* hack, FIXME */
                            p_stream->fmt.video.i_bits_per_pixel = 24;
                        p_stream->fmt.video.i_width =
                            GetDWLE((oggpacket.packet+176));
                        p_stream->fmt.video.i_height =
                            GetDWLE((oggpacket.packet+180));
                        p_stream->fmt.video.i_visible_width =
                            p_stream->fmt.video.i_width;
                        p_stream->fmt.video.i_visible_height =
                            p_stream->fmt.video.i_height;

                        msg_Dbg( p_demux,
                                 "fps: %u/%u, width:%i; height:%i, bitcount:%i",
                                 p_stream->fmt.video.i_frame_rate,
                                 p_stream->fmt.video.i_frame_rate_base,
                                 p_stream->fmt.video.i_width,
                                 p_stream->fmt.video.i_height,
                                 p_stream->fmt.video.i_bits_per_pixel);

                        if ( !p_stream->fmt.video.i_frame_rate ||
                             !p_stream->fmt.video.i_frame_rate_base )
                        {
                            Ogg_LogicalStreamDelete( p_demux, p_stream );
                            p_stream = NULL;
                            TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                       p_ogg->i_streams - 1 );
                        }
                    }
                    /* Check for audio header (old format) */
                    else if( GetDWLE((oggpacket.packet+96)) == 0x05589F81 )
                    {
                        int i_extra_size;
                        unsigned int i_format_tag;

                        es_format_Change( &p_stream->fmt, AUDIO_ES, 0 );

                        i_extra_size = GetWLE((oggpacket.packet+140));
                        if( i_extra_size > 0 && i_extra_size < oggpacket.bytes - 142 )
                        {
                            p_stream->fmt.i_extra = i_extra_size;
                            p_stream->fmt.p_extra = malloc( i_extra_size );
                            if( p_stream->fmt.p_extra )
                                memcpy( p_stream->fmt.p_extra,
                                        oggpacket.packet + 142, i_extra_size );
                            else
                                p_stream->fmt.i_extra = 0;
                        }

                        i_format_tag = GetWLE((oggpacket.packet+124));
                        p_stream->fmt.audio.i_channels =
                            GetWLE((oggpacket.packet+126));
                        fill_channels_info(&p_stream->fmt.audio);
                        p_stream->fmt.audio.i_rate =
                            GetDWLE((oggpacket.packet+128));
                        p_stream->fmt.i_bitrate =
                            GetDWLE((oggpacket.packet+132)) * 8;
                        p_stream->fmt.audio.i_blockalign =
                            GetWLE((oggpacket.packet+136));
                        p_stream->fmt.audio.i_bitspersample =
                            GetWLE((oggpacket.packet+138));

                        date_Init( &p_stream->dts, p_stream->fmt.audio.i_rate, 1 );

                        wf_tag_to_fourcc( i_format_tag,
                                          &p_stream->fmt.i_codec, 0 );

                        if( p_stream->fmt.i_codec == VLC_CODEC_UNKNOWN )
                        {
                            p_stream->fmt.i_codec = VLC_FOURCC( 'm', 's',
                                ( i_format_tag >> 8 ) & 0xff,
                                i_format_tag & 0xff );
                        }

                        msg_Dbg( p_demux, "found audio header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );
                        msg_Dbg( p_demux, "audio:0x%4.4x channels:%d %dHz "
                                 "%dbits/sample %dkb/s",
                                 i_format_tag,
                                 p_stream->fmt.audio.i_channels,
                                 p_stream->fmt.audio.i_rate,
                                 p_stream->fmt.audio.i_bitspersample,
                                 p_stream->fmt.i_bitrate / 1024 );

                        if ( p_stream->fmt.audio.i_rate == 0 )
                        {
                            msg_Dbg( p_demux, "invalid oggds audio header" );
                            Ogg_LogicalStreamDelete( p_demux, p_stream );
                            p_stream = NULL;
                            TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                       p_ogg->i_streams - 1 );
                        }
                    }
                    else
                    {
                        msg_Dbg( p_demux, "stream %d has an old header "
                            "but is of an unknown type", p_ogg->i_streams-1 );
                        FREENULL( p_stream );
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                /* Check for OggDS */
                else if( oggpacket.bytes >= 44+1 &&
                         (*oggpacket.packet & PACKET_TYPE_BITS ) == PACKET_TYPE_HEADER )
                {
                    stream_header_t tmp;
                    stream_header_t *st = &tmp;

                    p_stream->b_oggds = true;
                    p_stream->b_contiguous = false;

                    memcpy( st->streamtype, &oggpacket.packet[1+0], 8 );
                    memcpy( st->subtype, &oggpacket.packet[1+8], 4 );
                    st->size = GetDWLE( &oggpacket.packet[1+12] );
                    st->time_unit = GetQWLE( &oggpacket.packet[1+16] );
                    st->samples_per_unit = GetQWLE( &oggpacket.packet[1+24] );
                    st->default_len = GetDWLE( &oggpacket.packet[1+32] );
                    st->buffersize = GetDWLE( &oggpacket.packet[1+36] );
                    st->bits_per_sample = GetWLE( &oggpacket.packet[1+40] ); // (padding 2)

                    /* Check for video header (new format) */
                    if( !strncmp( st->streamtype, "video", 5 ) &&
                        oggpacket.bytes >= 52+1 )
                    {
                        st->sh.video.width = GetDWLE( &oggpacket.packet[1+44] );
                        st->sh.video.height = GetDWLE( &oggpacket.packet[1+48] );

                        es_format_Change( &p_stream->fmt, VIDEO_ES, 0 );

                        /* We need to get rid of the header packet */
                        ogg_stream_packetout( &p_stream->os, &oggpacket );

                        p_stream->fmt.i_codec =
                            VLC_FOURCC( st->subtype[0], st->subtype[1],
                                        st->subtype[2], st->subtype[3] );
                        msg_Dbg( p_demux, "found video header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );

                        /* FIXME: no clue where it's from */
                        if( st->time_unit <= 0 )
                            st->time_unit = 400000;
                        unsigned num, den;
                        vlc_ureduce( &num, &den,
                                     st->samples_per_unit * OGGDS_RESOLUTION,
                                     st->time_unit > 0 ? st->time_unit : OGGDS_RESOLUTION,
                                     OGGDS_RESOLUTION );
                        date_Init( &p_stream->dts, num, den );
                        p_stream->fmt.video.i_frame_rate = num;
                        p_stream->fmt.video.i_frame_rate_base = den;
                        p_stream->fmt.video.i_bits_per_pixel = st->bits_per_sample;
                        p_stream->fmt.video.i_width = st->sh.video.width;
                        p_stream->fmt.video.i_height = st->sh.video.height;
                        p_stream->fmt.video.i_visible_width =
                            p_stream->fmt.video.i_width;
                        p_stream->fmt.video.i_visible_height =
                            p_stream->fmt.video.i_height;

                        msg_Dbg( p_demux,
                                 "fps: %u/%u, width:%i; height:%i, bitcount:%i",
                                 p_stream->fmt.video.i_frame_rate,
                                 p_stream->fmt.video.i_frame_rate_base,
                                 p_stream->fmt.video.i_width,
                                 p_stream->fmt.video.i_height,
                                 p_stream->fmt.video.i_bits_per_pixel );
                    }
                    /* Check for audio header (new format) */
                    else if( !strncmp( st->streamtype, "audio", 5 ) &&
                             oggpacket.bytes >= 56+1 )
                    {
                        char p_buffer[5];
                        int i_extra_size;
                        int i_format_tag;

                        st->sh.audio.channels = GetWLE( &oggpacket.packet[1+44] );
                        st->sh.audio.blockalign = GetWLE( &oggpacket.packet[1+48] );
                        st->sh.audio.avgbytespersec = GetDWLE( &oggpacket.packet[1+52] );

                        es_format_Change( &p_stream->fmt, AUDIO_ES, 0 );

                        /* We need to get rid of the header packet */
                        ogg_stream_packetout( &p_stream->os, &oggpacket );

                        i_extra_size = st->size - 56;

                        if( i_extra_size > 0 &&
                            i_extra_size < oggpacket.bytes - 1 - 56 )
                        {
                            p_stream->fmt.i_extra = i_extra_size;
                            p_stream->fmt.p_extra = malloc( p_stream->fmt.i_extra );
                            if( p_stream->fmt.p_extra )
                                memcpy( p_stream->fmt.p_extra, oggpacket.packet + 57,
                                        p_stream->fmt.i_extra );
                            else
                                p_stream->fmt.i_extra = 0;
                        }

                        memcpy( p_buffer, st->subtype, 4 );
                        p_buffer[4] = '\0';
                        i_format_tag = strtol(p_buffer,NULL,16);
                        p_stream->fmt.audio.i_channels = st->sh.audio.channels;
                        fill_channels_info(&p_stream->fmt.audio);

                        unsigned num,den;
                        vlc_ureduce( &num, &den,
                                     st->samples_per_unit * OGGDS_RESOLUTION,
                                     st->time_unit > 0 ? st->time_unit : OGGDS_RESOLUTION,
                                     OGGDS_RESOLUTION );
                        date_Init( &p_stream->dts, num, den );
                        p_stream->fmt.audio.i_rate = num / den;
                        p_stream->fmt.i_bitrate = st->sh.audio.avgbytespersec * 8;
                        p_stream->fmt.audio.i_blockalign = st->sh.audio.blockalign;
                        p_stream->fmt.audio.i_bitspersample = st->bits_per_sample;

                        wf_tag_to_fourcc( i_format_tag,
                                          &p_stream->fmt.i_codec, 0 );

                        if( p_stream->fmt.i_codec == VLC_CODEC_UNKNOWN )
                        {
                            p_stream->fmt.i_codec = VLC_FOURCC( 'm', 's',
                                ( i_format_tag >> 8 ) & 0xff,
                                i_format_tag & 0xff );
                        }

                        msg_Dbg( p_demux, "found audio header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );
                        msg_Dbg( p_demux, "audio:0x%4.4x channels:%d %dHz "
                                 "%dbits/sample %dkb/s",
                                 i_format_tag,
                                 p_stream->fmt.audio.i_channels,
                                 p_stream->fmt.audio.i_rate,
                                 p_stream->fmt.audio.i_bitspersample,
                                 p_stream->fmt.i_bitrate / 1024 );
                        if ( p_stream->fmt.audio.i_rate == 0 )
                        {
                            msg_Dbg( p_demux, "invalid oggds audio header" );
                            Ogg_LogicalStreamDelete( p_demux, p_stream );
                            p_stream = NULL;
                            TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                       p_ogg->i_streams - 1 );
                        }
                    }
                    /* Check for text (subtitles) header */
                    else if( !strncmp(st->streamtype, "text", 4) )
                    {
                        /* We need to get rid of the header packet */
                        ogg_stream_packetout( &p_stream->os, &oggpacket );

                        msg_Dbg( p_demux, "found text subtitle header" );
                        es_format_Change( &p_stream->fmt, SPU_ES, VLC_CODEC_SUBT );
                        date_Init( &p_stream->dts, 1000, 1 ); /* granulepos is in millisec */
                    }
                    else
                    {
                        msg_Dbg( p_demux, "stream %d has a header marker "
                            "but is of an unknown type", p_ogg->i_streams-1 );
                        FREENULL( p_stream );
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                else if( oggpacket.bytes >= 8 &&
                             ! memcmp( oggpacket.packet, "fishead\0", 8 ) )

                {
                    /* Skeleton */
                    msg_Dbg( p_demux, "stream %d is a skeleton",
                                p_ogg->i_streams-1 );
                    Ogg_ReadSkeletonHeader( p_demux, p_stream, &oggpacket );
                }
                /* Check for OggSpots header */
                else if( oggpacket.bytes >= 8 &&
                         ! memcmp( oggpacket.packet, "SPOTS\0\0", 8 ) )
                {
                    if ( Ogg_ReadOggSpotsHeader( p_stream, &oggpacket ) )
                        msg_Dbg( p_demux,
                                 "found OggSpots header, time resolution: %u/%u",
                                 p_stream->fmt.video.i_frame_rate,
                                 p_stream->fmt.video.i_frame_rate_base );
                    else
                    {
                        msg_Err( p_demux, "found invalid OggSpots header" );
                        Ogg_LogicalStreamDelete( p_demux, p_stream );
                        p_stream = NULL;
                        TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                                   p_ogg->i_streams - 1 );
                    }
                }
                else
                {
                    Ogg_LogicalStreamDelete( p_demux, p_stream );
                    p_stream = NULL;
                    TAB_ERASE( p_ogg->i_streams, p_ogg->pp_stream,
                               p_ogg->i_streams - 1 );
                    msg_Dbg( p_demux, "stream %d is of unknown type",
                             p_ogg->i_streams );
                }

                /* we'll need to get all headers */
                if ( p_stream )
                    p_stream->b_initializing &= p_stream->b_force_backup;

                if( Ogg_ReadPage( p_demux, &p_ogg->current_page ) != VLC_SUCCESS )
                    return VLC_EGENERIC;
            }

            /* This is the first data page, which means we are now finished
             * with the initial pages. We just need to store it in the relevant
             * bitstream. */
            for( int i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
            {
                if( ogg_stream_pagein( &p_ogg->pp_stream[i_stream]->os,
                                       &p_ogg->current_page ) == 0 )
                {
                    p_ogg->b_page_waiting = true;
                    break;
                }
            }

            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}

/****************************************************************************
 * Ogg_CreateES: Creates all Elementary streams once headers are parsed
 ****************************************************************************/
static void Ogg_CreateES( demux_t *p_demux, bool stable_id )
{
    demux_sys_t *p_ogg = p_demux->p_sys;
    logical_stream_t *p_old_stream = p_ogg->p_old_stream;
    int i_stream;

    for( i_stream = 0 ; i_stream < p_ogg->i_streams; i_stream++ )
    {
        logical_stream_t *p_stream = p_ogg->pp_stream[i_stream];

        if ( p_stream->p_es == NULL && !p_stream->b_finished )
        {
            /* Better be safe than sorry when possible with ogm */
            if( p_stream->fmt.i_codec == VLC_CODEC_MPGA ||
                p_stream->fmt.i_codec == VLC_CODEC_A52 )
                p_stream->fmt.b_packetized = false;

            /* Try first to reuse an old ES */
            if( p_old_stream &&
                p_old_stream->fmt.i_cat == p_stream->fmt.i_cat &&
                p_old_stream->fmt.i_codec == p_stream->fmt.i_codec &&
                p_old_stream->p_es != NULL && p_stream->p_es != NULL )
            {
                msg_Dbg( p_demux, "will reuse old stream to avoid glitch" );

                p_stream->p_es = p_old_stream->p_es;
                p_stream->b_finished = false;
                p_stream->b_reinit = false;
                p_stream->b_initializing = false;
                p_stream->i_pre_skip = 0;
                es_format_Clean( &p_stream->fmt_old );
                es_format_Copy( &p_stream->fmt_old, &p_old_stream->fmt );
                bool b_resetdecoder = Ogg_LogicalStreamResetEsFormat( p_demux, p_stream );

                p_old_stream->p_es = NULL;
                p_old_stream = NULL;
                if ( b_resetdecoder )
                {
                    es_out_Control( p_demux->out, ES_OUT_SET_ES_FMT,
                                    p_stream->p_es, &p_stream->fmt );
                }
            }
            else
            {
                if( stable_id )
                {
                    /* IDs are stable when ES tracks are created from the Open
                     * function. Don't specify ids when tracks are added
                     * midstream */
                    p_stream->fmt.i_id = i_stream;
                }
                p_stream->p_es = es_out_Add( p_demux->out, &p_stream->fmt );
            }
        }
    }

    if( p_ogg->p_old_stream )
    {
        if( p_ogg->p_old_stream->p_es )
            msg_Dbg( p_demux, "old stream not reused" );
        Ogg_LogicalStreamDelete( p_demux, p_ogg->p_old_stream );
        p_ogg->p_old_stream = NULL;
    }
    p_ogg->b_es_created = true;
}

/****************************************************************************
 * Ogg_BeginningOfStream: Look for Beginning of Stream ogg pages and add
 *                        Elementary streams.
 ****************************************************************************/
static int Ogg_BeginningOfStream( demux_t *p_demux )
{
    demux_sys_t *p_ogg = p_demux->p_sys  ;
    int i_stream;

    /* Find the logical streams embedded in the physical stream and
     * initialize our p_ogg structure. */
    if( Ogg_FindLogicalStreams( p_demux ) != VLC_SUCCESS )
    {
        msg_Warn( p_demux, "couldn't find any ogg logical stream" );
        return VLC_EGENERIC;
    }

    p_ogg->i_bitrate = 0;

    for( i_stream = 0 ; i_stream < p_ogg->i_streams; i_stream++ )
    {
        logical_stream_t *p_stream = p_ogg->pp_stream[i_stream];

        p_stream->p_es = NULL;

        /* initialise kframe index */
        p_stream->idx=NULL;

        if ( p_stream->fmt.i_bitrate == 0  &&
             ( p_stream->fmt.i_cat == VIDEO_ES ||
               p_stream->fmt.i_cat == AUDIO_ES ) )
            p_ogg->b_partial_bitrate = true;
        else
            p_ogg->i_bitrate += p_stream->fmt.i_bitrate;

        p_stream->i_pcr = VLC_TICK_INVALID;
        p_stream->b_reinit = false;
    }

    /* get total frame count for video stream; we will need this for seeking */
    p_ogg->i_total_frames = 0;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Ogg_EndOfStream: clean up the ES when an End of Stream is detected.
 ****************************************************************************/
static void Ogg_EndOfStream( demux_t *p_demux )
{
    demux_sys_t *p_ogg = p_demux->p_sys  ;
    int i_stream;

    for( i_stream = 0 ; i_stream < p_ogg->i_streams; i_stream++ )
        Ogg_LogicalStreamDelete( p_demux, p_ogg->pp_stream[i_stream] );
    free( p_ogg->pp_stream );

    /* Reinit p_ogg */
    p_ogg->i_bitrate = 0;
    p_ogg->i_streams = 0;
    p_ogg->pp_stream = NULL;
    p_ogg->skeleton.major = 0;
    p_ogg->skeleton.minor = 0;
    p_ogg->b_preparsing_done = false;
    p_ogg->b_es_created = false;

    /* */
    if( p_ogg->p_meta )
        vlc_meta_Delete( p_ogg->p_meta );
    p_ogg->p_meta = NULL;

    for(int i=0; i<p_ogg->i_attachments; i++)
        vlc_input_attachment_Delete( p_ogg->attachments[i] );
    TAB_CLEAN(p_ogg->i_attachments, p_ogg->attachments);

    for ( int i=0; i < p_ogg->i_seekpoints; i++ )
    {
        if ( p_ogg->pp_seekpoints[i] )
            vlc_seekpoint_Delete( p_ogg->pp_seekpoints[i] );
    }
    TAB_CLEAN( p_ogg->i_seekpoints, p_ogg->pp_seekpoints );
}

static void Ogg_CleanSpecificData( logical_stream_t *p_stream )
{
#ifdef HAVE_LIBVORBIS
    if ( p_stream->fmt.i_codec == VLC_CODEC_VORBIS )
    {
        if( p_stream->special.vorbis.p_info )
            vorbis_info_clear( p_stream->special.vorbis.p_info );
        FREENULL( p_stream->special.vorbis.p_info );
        if( p_stream->special.vorbis.p_comment )
            vorbis_comment_clear( p_stream->special.vorbis.p_comment );
        FREENULL( p_stream->special.vorbis.p_comment );
        p_stream->special.vorbis.i_headers_flags = 0;
    }
#else
    VLC_UNUSED( p_stream );
#endif
}

static void Ogg_LogicalStreamInit( logical_stream_t *p_stream )
{
    memset( p_stream, 0, sizeof(logical_stream_t) );
    es_format_Init( &p_stream->fmt, UNKNOWN_ES, 0 );
    es_format_Init( &p_stream->fmt_old, UNKNOWN_ES, 0 );
    p_stream->i_pcr = VLC_TICK_INVALID;
    p_stream->i_first_frame_index = 1;
    date_Set( &p_stream->dts, VLC_TICK_INVALID );
    p_stream->b_initializing = true;
    p_stream->b_contiguous = true; /* default */
    p_stream->queue.pp_append = &p_stream->queue.p_blocks;
}

/**
 * This function delete and release all data associated to a logical_stream_t
 */
static void Ogg_LogicalStreamDelete( demux_t *p_demux, logical_stream_t *p_stream )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_stream->p_es )
        es_out_Del( p_demux->out, p_stream->p_es );

    ogg_stream_clear( &p_stream->os );
    free( p_stream->p_headers );

    Ogg_CleanSpecificData( p_stream );

    es_format_Clean( &p_stream->fmt_old );
    es_format_Clean( &p_stream->fmt );

    if ( p_stream->idx != NULL)
    {
        oggseek_index_entries_free( p_stream->idx );
    }

    Ogg_FreeSkeleton( p_stream->p_skel );
    p_stream->p_skel = NULL;
    if( p_sys->p_skelstream == p_stream )
        p_sys->p_skelstream = NULL;

    /* Shouldn't happen */
    block_ChainRelease( p_stream->queue.p_blocks );

    free( p_stream );
}
/**
 * This function check if a we need to reset a decoder in case we are
 * reusing an old ES
 */
static bool Ogg_IsVorbisFormatCompatible( const es_format_t *p_new, const es_format_t *p_old )
{
    unsigned pi_new_size[XIPH_MAX_HEADER_COUNT];
    const void *pp_new_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_new_count;

    if( xiph_SplitHeaders(pi_new_size, pp_new_data, &i_new_count, p_new->i_extra, p_new->p_extra ) )
        i_new_count = 0;

    unsigned pi_old_size[XIPH_MAX_HEADER_COUNT];
    const void *pp_old_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_old_count;

    if( xiph_SplitHeaders(pi_old_size, pp_old_data, &i_old_count, p_old->i_extra, p_old->p_extra ) )
        i_old_count = 0;

    bool b_match = i_new_count == i_old_count;
    for( unsigned i = 0; i < i_new_count && b_match; i++ )
    {
        /* Ignore vorbis comment */
        if( i == 1 )
            continue;
        if( pi_new_size[i] != pi_old_size[i] ||
            memcmp( pp_new_data[i], pp_old_data[i], pi_new_size[i] ) )
            b_match = false;
    }

    return b_match;
}

static bool Ogg_IsOpusFormatCompatible( const es_format_t *p_new,
                                        const es_format_t *p_old )
{
    unsigned pi_new_size[XIPH_MAX_HEADER_COUNT];
    const void *pp_new_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_new_count;

    if( xiph_SplitHeaders(pi_new_size, pp_new_data, &i_new_count, p_new->i_extra, p_new->p_extra ) )
        i_new_count = 0;

    unsigned pi_old_size[XIPH_MAX_HEADER_COUNT];
    const void *pp_old_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_old_count;

    if( xiph_SplitHeaders(pi_old_size, pp_old_data, &i_old_count, p_old->i_extra, p_old->p_extra ) )
        i_old_count = 0;
    bool b_match = false;
    if( i_new_count == i_old_count && i_new_count > 0 )
    {
        static const unsigned char default_map[2] = { 0, 1 };
        const unsigned char *p_old_head;
        unsigned char *p_new_head;
        const unsigned char *p_old_map;
        const unsigned char *p_new_map;
        int i_old_channel_count;
        int i_new_channel_count;
        int i_old_stream_count;
        int i_new_stream_count;
        int i_old_coupled_count;
        int i_new_coupled_count;
        p_old_head = pp_old_data[0];
        i_old_channel_count = i_old_stream_count = i_old_coupled_count = 0;
        p_old_map = default_map;
        if( pi_old_size[0] >= 19 && p_old_head[8] <= 15 )
        {
            i_old_channel_count = p_old_head[9];
            switch( p_old_head[18] )
            {
                case 0:
                    i_old_stream_count = 1;
                    i_old_coupled_count = i_old_channel_count - 1;
                    break;
                case 1:
                    if( pi_old_size[0] >= 21U + i_old_channel_count )
                    {
                        i_old_stream_count = p_old_head[19];
                        i_old_coupled_count = p_old_head[20];
                        p_old_map = p_old_head + 21;
                    }
                    break;
            }
        }
        p_new_head = (unsigned char *)pp_new_data[0];
        i_new_channel_count = i_new_stream_count = i_new_coupled_count = 0;
        p_new_map = default_map;
        if( pi_new_size[0] >= 19 && p_new_head[8] <= 15 )
        {
            i_new_channel_count = p_new_head[9];
            switch( p_new_head[18] )
            {
                case 0:
                    i_new_stream_count = 1;
                    i_new_coupled_count = i_new_channel_count - 1;
                    break;
                case 1:
                    if( pi_new_size[0] >= 21U + i_new_channel_count )
                    {
                        i_new_stream_count = p_new_head[19];
                        i_new_coupled_count = p_new_head[20];
                        p_new_map = p_new_head+21;
                    }
                    break;
            }
        }
        b_match = i_old_channel_count == i_new_channel_count &&
                  i_old_stream_count == i_new_stream_count &&
                  i_old_coupled_count == i_new_coupled_count &&
                  memcmp(p_old_map, p_new_map,
                      i_new_channel_count*sizeof(*p_new_map)) == 0;
    }

    return b_match;
}

static bool Ogg_LogicalStreamResetEsFormat( demux_t *p_demux, logical_stream_t *p_stream )
{
    bool b_compatible = false;
    if( !p_stream->fmt_old.i_cat || !p_stream->fmt_old.i_codec )
        return true;

    /* Only Vorbis and Opus are supported. */
    if( p_stream->fmt.i_codec == VLC_CODEC_VORBIS )
        b_compatible = Ogg_IsVorbisFormatCompatible( &p_stream->fmt, &p_stream->fmt_old );
    else if( p_stream->fmt.i_codec == VLC_CODEC_OPUS )
        b_compatible = Ogg_IsOpusFormatCompatible( &p_stream->fmt, &p_stream->fmt_old );
    else if( p_stream->fmt.i_codec == VLC_CODEC_FLAC )
        b_compatible = !p_stream->fmt.b_packetized;

    if( !b_compatible )
        msg_Warn( p_demux, "cannot reuse old stream, resetting the decoder" );

    return !b_compatible;
}

static void Ogg_ExtractComments( demux_t *p_demux, es_format_t *p_fmt,
                                 const void *p_headers, unsigned i_headers )
{
    demux_sys_t *p_ogg = p_demux->p_sys;
    int i_cover_score = 0;
    int i_cover_idx = 0;
    float pf_replay_gain[AUDIO_REPLAY_GAIN_MAX];
    float pf_replay_peak[AUDIO_REPLAY_GAIN_MAX];
    for(int i=0; i< AUDIO_REPLAY_GAIN_MAX; i++ )
    {
        pf_replay_gain[i] = 0;
        pf_replay_peak[i] = 0;
    }
    vorbis_ParseComment( p_fmt, &p_ogg->p_meta, p_headers, i_headers,
                         &p_ogg->i_attachments, &p_ogg->attachments,
                         &i_cover_score, &i_cover_idx,
                         &p_ogg->i_seekpoints, &p_ogg->pp_seekpoints,
                         &pf_replay_gain, &pf_replay_peak );
    if( p_ogg->p_meta != NULL && i_cover_idx < p_ogg->i_attachments )
    {
        char psz_url[128];
        snprintf( psz_url, sizeof(psz_url), "attachment://%s",
                  p_ogg->attachments[i_cover_idx]->psz_name );
        vlc_meta_Set( p_ogg->p_meta, vlc_meta_ArtworkURL, psz_url );
    }

    for ( int i=0; i<AUDIO_REPLAY_GAIN_MAX;i++ )
    {
        if ( pf_replay_gain[i] != 0 )
        {
            p_fmt->audio_replay_gain.pb_gain[i] = true;
            p_fmt->audio_replay_gain.pf_gain[i] = pf_replay_gain[i];
            msg_Dbg( p_demux, "setting replay gain %d to %f", i, pf_replay_gain[i] );
        }
        if ( pf_replay_peak[i] != 0 )
        {
            p_fmt->audio_replay_gain.pb_peak[i] = true;
            p_fmt->audio_replay_gain.pf_peak[i] = pf_replay_peak[i];
            msg_Dbg( p_demux, "setting replay peak %d to %f", i, pf_replay_gain[i] );
        }
    }

    if( p_ogg->i_seekpoints > 1 )
    {
        p_ogg->updates |= INPUT_UPDATE_TITLE_LIST;
    }
}

static inline uint32_t GetDW24BE( const uint8_t *p )
{
    uint32_t i = ( p[0] << 16 ) + ( p[1] << 8 ) + ( p[2] );
#ifdef WORDS_BIGENDIAN
    i = vlc_bswap32(i);
#endif
    return i;
}

static void Ogg_ExtractFlacComments( demux_t *p_demux, es_format_t *p_fmt,
                                     const uint8_t *p_headers, unsigned i_headers )
{
    /* Skip Streaminfo 42 bytes / 1st page */
    if(i_headers <= 46)
        return;
    p_headers += 42; i_headers -= 42;
    /* Block Header 1 + 3 bytes */
    uint32_t blocksize = GetDW24BE(&p_headers[1]);
    if(p_headers[0] == 0x84 && blocksize <= i_headers - 4)
    {
        Ogg_ExtractComments( p_demux, p_fmt, &p_headers[4], i_headers - 4 );
    }
}

static void Ogg_ExtractXiphMeta( demux_t *p_demux, es_format_t *p_fmt,
                                 const void *p_headers, unsigned i_headers, unsigned i_skip )
{
    unsigned pi_size[XIPH_MAX_HEADER_COUNT];
    const void *pp_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_count;

    if( xiph_SplitHeaders( pi_size, pp_data, &i_count, i_headers, p_headers ) )
        return;
    /* TODO how to handle multiple comments properly ? */
    if( i_count >= 2 && pi_size[1] > i_skip )
    {
        Ogg_ExtractComments( p_demux, p_fmt,
                             (const uint8_t *)pp_data[1] + i_skip,
                             pi_size[1] - i_skip );
    }
}

static void Ogg_ExtractMeta( demux_t *p_demux, es_format_t *p_fmt, const uint8_t *p_headers, int i_headers )
{
    demux_sys_t *p_ogg = p_demux->p_sys;

    switch( p_fmt->i_codec )
    {
    /* 3 headers with the 2° one being the comments */
    case VLC_CODEC_VORBIS:
    case VLC_CODEC_THEORA:
    case VLC_CODEC_DAALA:
        Ogg_ExtractXiphMeta( p_demux, p_fmt, p_headers, i_headers, 1+6 );
        break;
    case VLC_CODEC_OPUS:
        Ogg_ExtractXiphMeta( p_demux, p_fmt, p_headers, i_headers, 8 );
        break;
    case VLC_CODEC_SPEEX:
        Ogg_ExtractXiphMeta( p_demux, p_fmt, p_headers, i_headers, 0 );
        break;
    case VLC_CODEC_VP8:
        Ogg_ExtractComments( p_demux, p_fmt, p_headers, i_headers );
        break;
    /* N headers with the 2° one being the comments */
    case VLC_CODEC_KATE:
        /* 1 byte for header type, 7 bytes for magic, 1 reserved zero byte */
        Ogg_ExtractXiphMeta( p_demux, p_fmt, p_headers, i_headers, 1+7+1 );
        break;

    /* TODO */
    case VLC_CODEC_FLAC:
        Ogg_ExtractFlacComments( p_demux, p_fmt, p_headers, i_headers );
        break;

    /* No meta data */
    case VLC_CODEC_DIRAC:
    default:
        break;
    }
    if( p_ogg->p_meta )
        p_ogg->updates |= INPUT_UPDATE_META;
}

static bool Ogg_ReadTheoraHeader( logical_stream_t *p_stream,
                                  ogg_packet *p_oggpacket )
{
    bs_t bitstream;
    unsigned int i_fps_numerator;
    unsigned int i_fps_denominator;
    int i_keyframe_frequency_force;
    int i_major;
    int i_minor;
    int i_subminor;
    int i_version;

    /* Signal that we want to keep a backup of the theora
     * stream headers. They will be used when switching between
     * audio streams. */
    p_stream->b_force_backup = true;

    /* Cheat and get additionnal info ;) */
    bs_init( &bitstream, p_oggpacket->packet, p_oggpacket->bytes );
    bs_skip( &bitstream, 56 );

    i_major = bs_read( &bitstream, 8 ); /* major version num */
    i_minor = bs_read( &bitstream, 8 ); /* minor version num */
    i_subminor = bs_read( &bitstream, 8 ); /* subminor version num */

    bs_read( &bitstream, 16 ) /*<< 4*/; /* width */
    bs_read( &bitstream, 16 ) /*<< 4*/; /* height */
    bs_read( &bitstream, 24 ); /* frame width */
    bs_read( &bitstream, 24 ); /* frame height */
    bs_read( &bitstream, 8 ); /* x offset */
    bs_read( &bitstream, 8 ); /* y offset */

    i_fps_numerator = bs_read( &bitstream, 32 );
    i_fps_denominator = bs_read( &bitstream, 32 );
    i_fps_denominator = __MAX( i_fps_denominator, 1 );
    bs_read( &bitstream, 24 ); /* aspect_numerator */
    bs_read( &bitstream, 24 ); /* aspect_denominator */

    p_stream->fmt.video.i_frame_rate = i_fps_numerator;
    p_stream->fmt.video.i_frame_rate_base = i_fps_denominator;

    bs_read( &bitstream, 8 ); /* colorspace */
    p_stream->fmt.i_bitrate = bs_read( &bitstream, 24 );
    bs_read( &bitstream, 6 ); /* quality */

    i_keyframe_frequency_force = 1 << bs_read( &bitstream, 5 );

    /* granule_shift = i_log( frequency_force -1 ) */
    p_stream->i_granule_shift = 0;
    i_keyframe_frequency_force--;
    while( i_keyframe_frequency_force )
    {
        p_stream->i_granule_shift++;
        i_keyframe_frequency_force >>= 1;
    }

    i_version = i_major * 1000000 + i_minor * 1000 + i_subminor;
    p_stream->i_first_frame_index = (i_version >= 3002001) ? 1 : 0;
    if ( !i_fps_denominator || !i_fps_numerator )
        return false;
    date_Init( &p_stream->dts, i_fps_numerator, i_fps_denominator );

    return true;
}

static bool Ogg_ReadDaalaHeader( logical_stream_t *p_stream,
                                 ogg_packet *p_oggpacket )
{
    oggpack_buffer opb;
    uint32_t i_timebase_numerator;
    uint32_t i_timebase_denominator;
    int keyframe_granule_shift;
    unsigned int i_keyframe_frequency_force;
    uint8_t i_major;
    uint8_t i_minor;
    uint8_t i_subminor;
    int i_version;

    /* Signal that we want to keep a backup of the daala
     * stream headers. They will be used when switching between
     * audio streams. */
    p_stream->b_force_backup = true;

    /* Cheat and get additionnal info ;) */
    oggpack_readinit( &opb, p_oggpacket->packet, p_oggpacket->bytes );
    oggpack_adv( &opb, 48 );

    i_major = oggpack_read( &opb, 8 ); /* major version num */
    i_minor = oggpack_read( &opb, 8 ); /* minor version num */
    i_subminor = oggpack_read( &opb, 8 ); /* subminor version num */

    oggpack_adv( &opb, 32 ); /* width */
    oggpack_adv( &opb, 32 ); /* height */

    oggpack_adv( &opb, 32 ); /* aspect numerator */
    oggpack_adv( &opb, 32 ); /* aspect denominator */
    i_timebase_numerator = oggpack_read( &opb, 32 );

    i_timebase_denominator = oggpack_read( &opb, 32 );
    i_timebase_denominator = __MAX( i_timebase_denominator, 1 );

    p_stream->fmt.video.i_frame_rate = i_timebase_numerator;
    p_stream->fmt.video.i_frame_rate_base = i_timebase_denominator;

    oggpack_adv( &opb, 32 ); /* frame duration */

    keyframe_granule_shift = oggpack_read( &opb, 8 );
    keyframe_granule_shift = __MIN(keyframe_granule_shift, 31);
    i_keyframe_frequency_force = 1u << keyframe_granule_shift;

    /* granule_shift = i_log( frequency_force -1 ) */
    p_stream->i_granule_shift = 0;
    i_keyframe_frequency_force--;
    while( i_keyframe_frequency_force )
    {
        p_stream->i_granule_shift++;
        i_keyframe_frequency_force >>= 1;
    }

    i_version = i_major * 1000000 + i_minor * 1000 + i_subminor;
    VLC_UNUSED(i_version);
    p_stream->i_first_frame_index = 0;
    if ( !i_timebase_numerator || !i_timebase_denominator )
        return false;
    date_Init( &p_stream->dts, i_timebase_numerator, i_timebase_denominator );

    return true;
}

static bool Ogg_ReadVorbisHeader( logical_stream_t *p_stream,
                                  ogg_packet *p_oggpacket )
{
    oggpack_buffer opb;

    /* Signal that we want to keep a backup of the vorbis
     * stream headers. They will be used when switching between
     * audio streams. */
    p_stream->b_force_backup = true;

    /* Cheat and get additionnal info ;) */
    oggpack_readinit( &opb, p_oggpacket->packet, p_oggpacket->bytes);
    oggpack_adv( &opb, 88 );
    p_stream->fmt.audio.i_channels = oggpack_read( &opb, 8 );
    fill_channels_info(&p_stream->fmt.audio);
    p_stream->fmt.audio.i_rate = oggpack_read( &opb, 32 );
    if( p_stream->fmt.audio.i_rate == 0 )
        return false;
    date_Init( &p_stream->dts, p_stream->fmt.audio.i_rate, 1 );

    oggpack_adv( &opb, 32 );
    p_stream->fmt.i_bitrate = oggpack_read( &opb, 32 ); /* is signed 32 */
    if( p_stream->fmt.i_bitrate > INT32_MAX ) p_stream->fmt.i_bitrate = 0;
    return true;
}
#ifdef HAVE_LIBVORBIS
static void Ogg_DecodeVorbisHeader( logical_stream_t *p_stream,
                                    ogg_packet *p_oggpacket, int i_number )
{
    switch( i_number )
    {
    case VORBIS_HEADER_IDENTIFICATION:
        p_stream->special.vorbis.p_info = calloc( 1, sizeof(vorbis_info) );
        p_stream->special.vorbis.p_comment = malloc( sizeof(vorbis_comment) );
        if ( !p_stream->special.vorbis.p_info || !p_stream->special.vorbis.p_comment )
        {
            FREENULL( p_stream->special.vorbis.p_info );
            FREENULL( p_stream->special.vorbis.p_comment );
            break;
        }
        vorbis_info_init( p_stream->special.vorbis.p_info );
        vorbis_comment_init( p_stream->special.vorbis.p_comment );
        /* fallthrough */

    case VORBIS_HEADER_COMMENT:
    case VORBIS_HEADER_SETUP:
        if ( !p_stream->special.vorbis.p_info ||
             vorbis_synthesis_headerin(
                 p_stream->special.vorbis.p_info,
                 p_stream->special.vorbis.p_comment, p_oggpacket ) )
            break;

        p_stream->special.vorbis.i_headers_flags |= VORBIS_HEADER_TO_FLAG(i_number);
        /* fallthrough */

    default:
        break;
    }
}
#endif

static bool Ogg_ReadSpeexHeader( logical_stream_t *p_stream,
                                 ogg_packet *p_oggpacket )
{
    oggpack_buffer opb;

    /* Signal that we want to keep a backup of the speex
     * stream headers. They will be used when switching between
     * audio streams. */
    p_stream->b_force_backup = true;

    /* Cheat and get additionnal info ;) */
    oggpack_readinit( &opb, p_oggpacket->packet, p_oggpacket->bytes);
    oggpack_adv( &opb, 224 );
    oggpack_adv( &opb, 32 ); /* speex_version_id */
    oggpack_adv( &opb, 32 ); /* header_size */
    p_stream->fmt.audio.i_rate = oggpack_read( &opb, 32 );
    if ( !p_stream->fmt.audio.i_rate )
        return false;
    date_Init( &p_stream->dts, p_stream->fmt.audio.i_rate, 1 );
    oggpack_adv( &opb, 32 ); /* mode */
    oggpack_adv( &opb, 32 ); /* mode_bitstream_version */
    p_stream->fmt.audio.i_channels = oggpack_read( &opb, 32 );
    fill_channels_info(&p_stream->fmt.audio);
    p_stream->fmt.i_bitrate = oggpack_read( &opb, 32 );
    p_stream->special.speex.i_framesize =
            oggpack_read( &opb, 32 ); /* frame_size */
    oggpack_adv( &opb, 32 ); /* vbr */
    p_stream->special.speex.i_framesperpacket =
            oggpack_read( &opb, 32 ); /* frames_per_packet */
    p_stream->i_extra_headers_packets = oggpack_read( &opb, 32 ); /* extra_headers */
    return true;
}

static void Ogg_ReadOpusHeader( logical_stream_t *p_stream,
                                ogg_packet *p_oggpacket )
{
    oggpack_buffer opb;

    /* Signal that we want to keep a backup of the opus
     * stream headers. They will be used when switching between
     * audio streams. */
    p_stream->b_force_backup = true;

    /* All OggOpus streams are timestamped at 48kHz and
     * can be played at 48kHz. */
    p_stream->fmt.audio.i_rate = 48000;
    date_Init( &p_stream->dts, p_stream->fmt.audio.i_rate, 1 );
    p_stream->fmt.i_bitrate = 0;

    /* Cheat and get additional info ;) */
    oggpack_readinit( &opb, p_oggpacket->packet, p_oggpacket->bytes);
    oggpack_adv( &opb, 64 );
    oggpack_adv( &opb, 8 ); /* version_id */
    p_stream->fmt.audio.i_channels = oggpack_read( &opb, 8 );
    fill_channels_info(&p_stream->fmt.audio);
    p_stream->i_pre_skip = oggpack_read( &opb, 16 );
    /* For Opus, trash the first 80 ms of decoded output as
           well, to avoid blowing out speakers if we get unlucky.
           Opus predicts content from prior frames, which can go
           badly if we seek right where the stream goes from very
           quiet to very loud. It will converge after a bit. */
    p_stream->i_pre_skip = __MAX( 80*48, p_stream->i_pre_skip );
}

static bool Ogg_ReadFlacStreamInfo( demux_t *p_demux, logical_stream_t *p_stream,
                                ogg_packet *p_oggpacket )
{
    /* Parse the STREAMINFO metadata */
    bs_t s;

    bs_init( &s, p_oggpacket->packet, p_oggpacket->bytes );

    bs_read( &s, 1 );
    if( p_oggpacket->bytes > 0 && bs_read( &s, 7 ) != 0 )
    {
        msg_Dbg( p_demux, "Invalid FLAC STREAMINFO metadata" );
        return false;
    }

    if( bs_read( &s, 24 ) >= 34 /*size STREAMINFO*/ )
    {
        bs_skip( &s, 80 );
        p_stream->fmt.audio.i_rate = bs_read( &s, 20 );
        p_stream->fmt.audio.i_channels = bs_read( &s, 3 ) + 1;
        fill_channels_info(&p_stream->fmt.audio);

        msg_Dbg( p_demux, "FLAC header, channels: %"PRIu8", rate: %u",
                 p_stream->fmt.audio.i_channels, p_stream->fmt.audio.i_rate );
        if ( p_stream->fmt.audio.i_rate == 0 )
            return false;
        date_Init( &p_stream->dts, p_stream->fmt.audio.i_rate, 1 );
    }
    else
    {
        msg_Dbg( p_demux, "FLAC STREAMINFO metadata too short" );
    }

    /* Fake this as the last metadata block */
    *((uint8_t*)p_oggpacket->packet) |= 0x80;
    return true;
}

static bool Ogg_ReadKateHeader( logical_stream_t *p_stream,
                                ogg_packet *p_oggpacket )
{
    oggpack_buffer opb;
    uint32_t gnum;
    uint32_t gden;
    int n;
    char *psz_desc;

    /* Signal that we want to keep a backup of the kate
     * stream headers. They will be used when switching between
     * kate streams. */
    p_stream->b_force_backup = true;

    /* Cheat and get additionnal info ;) */
    oggpack_readinit( &opb, p_oggpacket->packet, p_oggpacket->bytes);
    oggpack_adv( &opb, 11*8 ); /* packet type, kate magic, version */
    p_stream->special.kate.i_num_headers = oggpack_read( &opb, 8 );
    oggpack_adv( &opb, 3*8 );
    p_stream->i_granule_shift = oggpack_read( &opb, 8 );
    oggpack_adv( &opb, 8*8 ); /* reserved */
    gnum = oggpack_read( &opb, 32 );
    gden = oggpack_read( &opb, 32 );
    gden = __MAX( gden, 1 );
    if ( !gnum || !gden )
        return false;
    date_Init( &p_stream->dts, gnum, gden );

    p_stream->fmt.psz_language = malloc(16);
    if( p_stream->fmt.psz_language )
    {
        for( n = 0; n < 16; n++ )
            p_stream->fmt.psz_language[n] = oggpack_read(&opb,8);
        p_stream->fmt.psz_language[15] = 0; /* just in case */
    }
    else
    {
        for( n = 0; n < 16; n++ )
            oggpack_read(&opb,8);
    }
    p_stream->fmt.psz_description = malloc(16);
    if( p_stream->fmt.psz_description )
    {
        for( n = 0; n < 16; n++ )
            p_stream->fmt.psz_description[n] = oggpack_read(&opb,8);
        p_stream->fmt.psz_description[15] = 0; /* just in case */

        /* Now find a localized user readable description for this category */
        psz_desc = strdup(FindKateCategoryName(p_stream->fmt.psz_description));
        if( psz_desc )
        {
            free( p_stream->fmt.psz_description );
            p_stream->fmt.psz_description = psz_desc;
        }
    }
    else
    {
        for( n = 0; n < 16; n++ )
            oggpack_read(&opb,8);
    }

    return true;
}

static bool Ogg_ReadVP8Header( demux_t *p_demux, logical_stream_t *p_stream,
                               ogg_packet *p_oggpacket )
{
    switch( p_oggpacket->packet[5] )
    {
    /* STREAMINFO */
    case 0x01:
        /* Mapping version */
        if ( p_oggpacket->packet[6] != 0x01 || p_oggpacket->packet[7] != 0x00 )
            return false;
        p_stream->i_granule_shift = 32;
        p_stream->fmt.video.i_width = GetWBE( &p_oggpacket->packet[8] );
        p_stream->fmt.video.i_height = GetWBE( &p_oggpacket->packet[10] );
        p_stream->fmt.video.i_visible_width = p_stream->fmt.video.i_width;
        p_stream->fmt.video.i_visible_height = p_stream->fmt.video.i_height;
        p_stream->fmt.video.i_sar_num = GetDWBE( &p_oggpacket->packet[12 - 1] ) & 0x0FFF;
        p_stream->fmt.video.i_sar_den = GetDWBE( &p_oggpacket->packet[15 - 1] ) & 0x0FFF;
        p_stream->fmt.video.i_frame_rate = GetDWBE( &p_oggpacket->packet[18] );
        p_stream->fmt.video.i_frame_rate_base = GetDWBE( &p_oggpacket->packet[22] );
        if ( !p_stream->fmt.video.i_frame_rate || !p_stream->fmt.video.i_frame_rate_base )
            return false;
        date_Init( &p_stream->dts, p_stream->fmt.video.i_frame_rate,
                                   p_stream->fmt.video.i_frame_rate_base );
        return true;
    /* METADATA */
    case 0x02:
        Ogg_ExtractMeta( p_demux, & p_stream->fmt,
                         p_oggpacket->packet + 7, p_oggpacket->bytes - 7 );
        return true;
    default:
        return false;
    }
}

static void Ogg_ApplyContentType( logical_stream_t *p_stream, const char* psz_value,
                                  bool *b_force_backup )
{
    if( p_stream->fmt.i_cat != UNKNOWN_ES )
        return;

    if( !strncmp(psz_value, "audio/x-wav", 11) )
    {
        /* n.b. WAVs are unsupported right now */
        es_format_Change( &p_stream->fmt, UNKNOWN_ES, 0 );
        free( p_stream->fmt.psz_description );
        p_stream->fmt.psz_description = strdup("WAV Audio (Unsupported)");
    }
    else if( !strncmp(psz_value, "audio/x-vorbis", 14) ||
             !strncmp(psz_value, "audio/vorbis", 12) )
    {
        es_format_Change( &p_stream->fmt, AUDIO_ES, VLC_CODEC_VORBIS );

        *b_force_backup = true;
    }
    else if( !strncmp(psz_value, "audio/x-speex", 13) ||
             !strncmp(psz_value, "audio/speex", 11) )
    {
        es_format_Change( &p_stream->fmt, AUDIO_ES, VLC_CODEC_SPEEX );

        *b_force_backup = true;
    }
    else if( !strncmp(psz_value, "audio/flac", 10) )
    {
        es_format_Change( &p_stream->fmt, AUDIO_ES, VLC_CODEC_FLAC );

        *b_force_backup = true;
    }
    else if( !strncmp(psz_value, "video/x-theora", 14) ||
             !strncmp(psz_value, "video/theora", 12) )
    {
        es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_CODEC_THEORA );

        *b_force_backup = true;
    }
    else if( !strncmp(psz_value, "video/x-daala", 13) ||
             !strncmp(psz_value, "video/daala", 11) )
    {
        es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_CODEC_DAALA );

        *b_force_backup = true;
    }
    else if( !strncmp(psz_value, "video/x-xvid", 12) )
    {
        es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_FOURCC( 'x','v','i','d' ) );

        *b_force_backup = true;
    }
    else if( !strncmp(psz_value, "video/mpeg", 10) )
    {
        /* n.b. MPEG streams are unsupported right now */
        es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_CODEC_MPGV );
    }
    else if( !strncmp(psz_value, "application/kate", 16) )
    {
        /* ??? */
        es_format_Change( &p_stream->fmt, UNKNOWN_ES, 0 );
        p_stream->fmt.psz_description = strdup("OGG Kate Overlay (Unsupported)");
    }
    else if( !strncmp(psz_value, "video/x-vp8", 11) )
    {
        es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_CODEC_VP8 );
    }
}

static void Ogg_ReadAnnodexHeader( demux_t *p_demux,
                                   logical_stream_t *p_stream,
                                   ogg_packet *p_oggpacket )
{
    if( p_oggpacket->bytes >= 28 &&
        !memcmp( p_oggpacket->packet, "Annodex", 7 ) )
    {
        oggpack_buffer opb;

        uint16_t major_version;
        uint16_t minor_version;
        uint64_t timebase_numerator;
        uint64_t timebase_denominator;

        Ogg_ReadTheoraHeader( p_stream, p_oggpacket );

        oggpack_readinit( &opb, p_oggpacket->packet, p_oggpacket->bytes);
        oggpack_adv( &opb, 8*8 ); /* "Annodex\0" header */
        major_version = oggpack_read( &opb, 2*8 ); /* major version */
        minor_version = oggpack_read( &opb, 2*8 ); /* minor version */
        timebase_numerator = GetQWLE( &p_oggpacket->packet[16] );
        timebase_denominator = GetQWLE( &p_oggpacket->packet[24] );

        msg_Dbg( p_demux, "Annodex info: version %"PRIu16".%"PRIu16" "
                          "Timebase  %"PRId64" / %"PRId64,
                          major_version, minor_version,
                          timebase_numerator, timebase_denominator );
    }
    else if( p_oggpacket->bytes >= 42 &&
             !memcmp( p_oggpacket->packet, "AnxData", 7 ) )
    {
        uint64_t granule_rate_numerator;
        uint64_t granule_rate_denominator;
        char content_type_string[1024];

        /* Read in Annodex header fields */

        granule_rate_numerator = GetQWLE( &p_oggpacket->packet[8] );
        granule_rate_denominator = GetQWLE( &p_oggpacket->packet[16] );
        p_stream->i_secondary_header_packets =
            GetDWLE( &p_oggpacket->packet[24] );

        /* we are guaranteed that the first header field will be
         * the content-type (by the Annodex standard) */
        content_type_string[0] = '\0';
        if( !strncasecmp( (char*)(&p_oggpacket->packet[28]), "Content-Type: ", 14 ) )
        {
            uint8_t *p = memchr( &p_oggpacket->packet[42], '\r',
                                 p_oggpacket->bytes - 1 );
            if( p && p[0] == '\r' && p[1] == '\n' )
                sscanf( (char*)(&p_oggpacket->packet[42]), "%1023s\r\n",
                        content_type_string );
        }

        msg_Dbg( p_demux, "AnxData packet info: %"PRId64" / %"PRId64", %d, ``%s''",
                 granule_rate_numerator, granule_rate_denominator,
                 p_stream->i_secondary_header_packets, content_type_string );

        if( granule_rate_numerator && granule_rate_denominator )
            date_Init( &p_stream->dts, granule_rate_numerator, granule_rate_denominator );

        /* What type of file do we have?
         * strcmp is safe to use here because we've extracted
         * content_type_string from the stream manually */
        Ogg_ApplyContentType( p_stream, content_type_string,
                              &p_stream->b_force_backup );
    }
}

static void Ogg_ReadSkeletonHeader( demux_t *p_demux, logical_stream_t *p_stream,
                                    ogg_packet *p_oggpacket )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if( p_oggpacket->bytes < 12 )
        return;

    p_sys->p_skelstream = p_stream;
    /* There can be only 1 skeleton for streams */
    p_sys->skeleton.major = GetWLE( &p_oggpacket->packet[8] );
    p_sys->skeleton.minor = GetWLE( &p_oggpacket->packet[10] );
    if ( asprintf( & p_stream->fmt.psz_description,
                   "OGG Skeleton version %" PRIu16 ".%" PRIu16,
                   p_sys->skeleton.major, p_sys->skeleton.minor ) < 0 )
        p_stream->fmt.psz_description = NULL;
}

static void Ogg_ReadSkeletonBones( demux_t *p_demux, ogg_packet *p_oggpacket )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if ( p_sys->skeleton.major < 3 || p_oggpacket->bytes < 52 ) return;

    /* Find the matching stream for this skeleton data */
    ogg_int32_t i_serialno = GetDWLE( &p_oggpacket->packet[12] );
    logical_stream_t *p_target_stream = NULL;
    for ( int i=0; i< p_sys->i_streams; i++ )
    {
        if ( p_sys->pp_stream[i]->i_serial_no == i_serialno )
        {
            p_target_stream = p_sys->pp_stream[i];
            break;
        }
    }
    if ( !p_target_stream ) return;

    ogg_skeleton_t *p_skel = p_target_stream->p_skel;
    if ( !p_skel )
    {
        p_skel = malloc( sizeof( ogg_skeleton_t ) );
        if ( !p_skel ) return;
        TAB_INIT( p_skel->i_messages, p_skel->ppsz_messages );
        p_skel->p_index = NULL;
        p_target_stream->p_skel = p_skel;
    }

    const unsigned char *p_messages = p_oggpacket->packet + 8 + GetDWLE( &p_oggpacket->packet[8] );
    const unsigned char *p_boundary = p_oggpacket->packet + p_oggpacket->bytes;
    const unsigned char *p = p_messages;
    while ( p <= p_boundary - 1 && p > p_oggpacket->packet )
    {
        if ( *p == 0x0D && *(p+1) == 0x0A )
        {
            char *psz_message = strndup( (const char *) p_messages,
                                         p - p_messages );
            if ( psz_message )
            {
                msg_Dbg( p_demux, "stream %" PRId32 " [%s]", i_serialno, psz_message );
                TAB_APPEND( p_skel->i_messages, p_skel->ppsz_messages, psz_message );
            }
            if ( p < p_boundary - 1 ) p_messages = p + 2;
        }
        p++;
    }

}

/* Unpacks the 7bit variable encoding used in skeleton indexes */
unsigned const char * Read7BitsVariableLE( unsigned const char *p_begin,
                                           unsigned const char *p_end,
                                           uint64_t *pi_value )
{
    int i_shift = 0;
    int64_t i_read = 0;
    *pi_value = 0;

    while ( p_begin < p_end )
    {
        i_read = *p_begin & 0x7F; /* High bit is start of integer */
        *pi_value = *pi_value | ( i_read << i_shift );
        i_shift += 7;
        if ( (*p_begin++ & 0x80) == 0x80 ) break; /* see prev */
    }

    *pi_value = GetQWLE( pi_value );
    return p_begin;
}

static void Ogg_ReadSkeletonIndex( demux_t *p_demux, ogg_packet *p_oggpacket )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->skeleton.major < 4
         || p_oggpacket->bytes < 44 /* Need at least 1 index value (42+1+1) */
    ) return;

    /* Find the matching stream for this skeleton data */
    int32_t i_serialno = GetDWLE( &p_oggpacket->packet[6] );
    logical_stream_t *p_stream = NULL;
    for ( int i=0; i< p_sys->i_streams; i++ )
    {
        if( p_sys->pp_stream[i]->i_serial_no == i_serialno )
        {
            p_stream = p_sys->pp_stream[i];
            break;
        }
    }
    if ( !p_stream || !p_stream->p_skel ) return;
    uint64_t i_keypoints = GetQWLE( &p_oggpacket->packet[10] );
    msg_Dbg( p_demux, "%" PRIi64 " index data for %" PRIi32, i_keypoints, i_serialno );
    if ( !i_keypoints ) return;

    p_stream->p_skel->i_indexstampden = GetQWLE( &p_oggpacket->packet[18] );
    p_stream->p_skel->i_indexfirstnum = GetQWLE( &p_oggpacket->packet[24] );
    p_stream->p_skel->i_indexlastnum = GetQWLE( &p_oggpacket->packet[32] );
    unsigned const char *p_fwdbyte = &p_oggpacket->packet[42];
    unsigned const char *p_boundary = p_oggpacket->packet + p_oggpacket->bytes;
    uint64_t i_offset = 0;
    uint64_t i_time = 0;
    uint64_t i_keypoints_found = 0;

    while( p_fwdbyte < p_boundary && i_keypoints_found < i_keypoints )
    {
        uint64_t i_val;
        p_fwdbyte = Read7BitsVariableLE( p_fwdbyte, p_boundary, &i_val );
        i_offset += i_val;
        p_fwdbyte = Read7BitsVariableLE( p_fwdbyte, p_boundary, &i_val );
        i_time += i_val * p_stream->p_skel->i_indexstampden;
        i_keypoints_found++;
    }

    if ( i_keypoints_found != i_keypoints )
    {
        msg_Warn( p_demux, "Invalid Index: missing entries" );
        return;
    }

    p_stream->p_skel->p_index = malloc( p_oggpacket->bytes - 42 );
    if ( !p_stream->p_skel->p_index ) return;
    memcpy( p_stream->p_skel->p_index, &p_oggpacket->packet[42],
            p_oggpacket->bytes - 42 );
    p_stream->p_skel->i_index = i_keypoints_found;
    p_stream->p_skel->i_index_size = p_oggpacket->bytes - 42;
}

static void Ogg_FreeSkeleton( ogg_skeleton_t *p_skel )
{
    if ( !p_skel ) return;
    for ( int i=0; i< p_skel->i_messages; i++ )
        free( p_skel->ppsz_messages[i] );
    TAB_CLEAN( p_skel->i_messages, p_skel->ppsz_messages );
    free( p_skel->p_index );
    free( p_skel );
}

static void Ogg_ApplySkeleton( logical_stream_t *p_stream )
{
    if ( !p_stream->p_skel ) return;
    for ( int i=0; i< p_stream->p_skel->i_messages; i++ )
    {
        const char *psz_message = p_stream->p_skel->ppsz_messages[i];
        if ( ! strncmp( "Name: ", psz_message, 6 ) )
        {
            free( p_stream->fmt.psz_description );
            p_stream->fmt.psz_description = strdup( psz_message + 6 );
        }
        else if ( ! strncmp("Content-Type: ", psz_message, 14 ) )
        {
            bool b_foo;
            Ogg_ApplyContentType( p_stream, psz_message + 14, &b_foo );
        }
    }
}

/* Return true if there's a skeleton exact match */
bool Ogg_GetBoundsUsingSkeletonIndex( logical_stream_t *p_stream, vlc_tick_t i_time,
                                      int64_t *pi_lower, int64_t *pi_upper )
{
    if ( !p_stream || !p_stream->p_skel || !p_stream->p_skel->p_index ||
         i_time == VLC_TICK_INVALID )
        return false;

    i_time -= VLC_TICK_0;

    /* Validate range */
    if ( i_time < p_stream->p_skel->i_indexfirstnum
                * p_stream->p_skel->i_indexstampden ||
         i_time > p_stream->p_skel->i_indexlastnum
                * p_stream->p_skel->i_indexstampden ) return false;

    /* Then Lookup its index */
    unsigned const char *p_fwdbyte = p_stream->p_skel->p_index;
    struct
    {
        int64_t i_pos;
        vlc_tick_t i_time;
    } current = { 0, 0 }, prev = { -1, -1 };

    uint64_t i_keypoints_found = 0;

    while( p_fwdbyte < p_fwdbyte + p_stream->p_skel->i_index_size
           && i_keypoints_found < p_stream->p_skel->i_index )
    {
        uint64_t i_val;
        p_fwdbyte = Read7BitsVariableLE( p_fwdbyte,
                        p_fwdbyte + p_stream->p_skel->i_index_size, &i_val );
        current.i_pos += i_val;
        p_fwdbyte = Read7BitsVariableLE( p_fwdbyte,
                        p_fwdbyte + p_stream->p_skel->i_index_size, &i_val );
        current.i_time += i_val * p_stream->p_skel->i_indexstampden;
        if ( current.i_pos < 0 || current.i_time < 0 ) break;

        i_keypoints_found++;

        if ( i_time <= current.i_time )
        {
            *pi_lower = prev.i_pos;
            *pi_upper = current.i_pos;
            return ( i_time == current.i_time );
        }
        prev = current;
    }
    return false;
}

static uint32_t dirac_uint( bs_t *p_bs )
{
    uint32_t u_count = 0, u_value = 0;

    while( !bs_eof( p_bs ) && !bs_read( p_bs, 1 ) )
    {
        u_count++;
        u_value <<= 1;
        u_value |= bs_read( p_bs, 1 );
    }

    return (1<<u_count) - 1 + u_value;
}

static int dirac_bool( bs_t *p_bs )
{
    return bs_read( p_bs, 1 );
}

static bool Ogg_ReadDiracHeader( logical_stream_t *p_stream,
                                 ogg_packet *p_oggpacket )
{
    p_stream->special.dirac.b_old = (p_oggpacket->packet[0] == 'K');

    static const struct {
        uint32_t u_n /* numerator */, u_d /* denominator */;
    } p_dirac_frate_tbl[] = { /* table 10.3 */
        {1,1}, /* this first value is never used */
        {24000,1001}, {24,1}, {25,1}, {30000,1001}, {30,1},
        {50,1}, {60000,1001}, {60,1}, {15000,1001}, {25,2},
    };
    static const size_t u_dirac_frate_tbl = sizeof(p_dirac_frate_tbl)/sizeof(*p_dirac_frate_tbl);

    static const uint32_t pu_dirac_vidfmt_frate[] = { /* table C.1 */
        1, 9, 10, 9, 10, 9, 10, 4, 3, 7, 6, 4, 3, 7, 6, 2, 2, 7, 6, 7, 6,
    };
    static const size_t u_dirac_vidfmt_frate = sizeof(pu_dirac_vidfmt_frate)/sizeof(*pu_dirac_vidfmt_frate);

    bs_t bs;

    /* Backing up stream headers is not required -- seqhdrs are repeated
     * thoughout the stream at suitable decoding start points */
    p_stream->b_force_backup = false;

    /* read in useful bits from sequence header */
    bs_init( &bs, p_oggpacket->packet, p_oggpacket->bytes );
    bs_skip( &bs, 13*8); /* parse_info_header */
    dirac_uint( &bs ); /* major_version */
    dirac_uint( &bs ); /* minor_version */
    dirac_uint( &bs ); /* profile */
    dirac_uint( &bs ); /* level */

    uint32_t u_video_format = dirac_uint( &bs ); /* index */
    if( u_video_format >= u_dirac_vidfmt_frate )
    {
        /* don't know how to parse this ogg dirac stream */
        return false;
    }

    if( dirac_bool( &bs ) )
    {
        dirac_uint( &bs ); /* frame_width */
        dirac_uint( &bs ); /* frame_height */
    }

    if( dirac_bool( &bs ) )
    {
        dirac_uint( &bs ); /* chroma_format */
    }

    if( dirac_bool( &bs ) )
    {
        p_stream->special.dirac.b_interlaced = dirac_uint( &bs ); /* scan_format */
    }
    else
        p_stream->special.dirac.b_interlaced = false;

    uint32_t u_n = p_dirac_frate_tbl[pu_dirac_vidfmt_frate[u_video_format]].u_n;
    uint32_t u_d = p_dirac_frate_tbl[pu_dirac_vidfmt_frate[u_video_format]].u_d;
    u_d = __MAX( u_d, 1 );
    if( dirac_bool( &bs ) )
    {
        uint32_t u_frame_rate_index = dirac_uint( &bs );
        if( u_frame_rate_index >= u_dirac_frate_tbl )
        {
            /* something is wrong with this stream */
            return false;
        }
        u_n = p_dirac_frate_tbl[u_frame_rate_index].u_n;
        u_d = p_dirac_frate_tbl[u_frame_rate_index].u_d;
        if( u_frame_rate_index == 0 )
        {
            u_n = dirac_uint( &bs ); /* frame_rate_numerator */
            u_d = dirac_uint( &bs ); /* frame_rate_denominator */
        }
    }

    if( !u_n || !u_d )
        return false;

    /*
     * NB, OggDirac granulepos values are in units of 2*picturerate
     * When picture_coding_mode = 0 (progressive),
     * pt increments by two for each picture in display order.
     * When picture_coding_mode = 1 (interlace),
     * pt increments by one for each field in display order. */
    date_Init( &p_stream->dts, 2 * u_n, u_d );

    return true;
}

static bool Ogg_ReadOggSpotsHeader( logical_stream_t *p_stream,
                                    ogg_packet *p_oggpacket )
{
    uint64_t i_granulerate_numerator;
    uint64_t i_granulerate_denominator;
    int i_major;
    int i_minor;

    es_format_Change( &p_stream->fmt, VIDEO_ES, VLC_CODEC_OGGSPOTS );

    /* Signal that we want to keep a backup of the OggSpots
     * stream headers. They will be used when switching between
     * audio streams. */
    p_stream->b_force_backup = true;

    /* Cheat and get additionnal info ;) */
    if ( p_oggpacket->bytes != 52 )
    {
        /* The OggSpots header is always 52 bytes */
        return false;
    }

    i_major = GetWLE( &p_oggpacket->packet[ 8] ); /* major version num */
    i_minor = GetWLE( &p_oggpacket->packet[10] ); /* minor version num */
    if ( i_major != 0 || i_minor != 1 )
    {
        return false;
    }

    /* Granule rate */
    i_granulerate_numerator   = GetQWLE( &p_oggpacket->packet[12] );
    i_granulerate_denominator = GetQWLE( &p_oggpacket->packet[20] );
    if ( i_granulerate_numerator == 0 || i_granulerate_denominator == 0 )
    {
        return false;
    }

    /* The OggSpots spec contained an error and there are implementations out
     * there that used the wrong value. So we detect that case and switch
     * numerator and denominator in that case */
    if ( i_granulerate_numerator == 1 && i_granulerate_denominator == 30 )
    {
        i_granulerate_numerator   = 30;
        i_granulerate_denominator = 1;
    }

    if ( !i_granulerate_numerator || !i_granulerate_denominator )
        return false;

    /* Normalize granulerate */
    vlc_ureduce(&p_stream->fmt.video.i_frame_rate,
                &p_stream->fmt.video.i_frame_rate_base,
                i_granulerate_numerator, i_granulerate_denominator, 0);

    date_Init( &p_stream->dts, p_stream->fmt.video.i_frame_rate,
                               p_stream->fmt.video.i_frame_rate_base );

    p_stream->i_granule_shift = p_oggpacket->packet[28];

    return true;
}
