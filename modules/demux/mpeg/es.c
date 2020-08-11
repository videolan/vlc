/*****************************************************************************
 * es.c : Generic audio ES input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codec.h>
#include <vlc_codecs.h>
#include <vlc_input.h>

#include "../../packetizer/a52.h"
#include "../../packetizer/dts_header.h"
#include "../../meta_engine/ID3Tag.h"
#include "../../meta_engine/ID3Text.h"
#include "../../meta_engine/ID3Meta.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenAudio( vlc_object_t * );
static int  OpenVideo( vlc_object_t * );
static void Close    ( vlc_object_t * );

#define FPS_TEXT N_("Frames per Second")
#define FPS_LONGTEXT N_("This is the frame rate used as a fallback when " \
    "playing MPEG video elementary streams.")

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("MPEG-I/II/4 / A52 / DTS / MLP audio" ) )
    set_shortname( N_("Audio ES") )
    set_capability( "demux", 155 )
    set_callbacks( OpenAudio, Close )

    add_shortcut( "mpga", "mp3",
                  "m4a", "mp4a", "aac",
                  "ac3", "a52",
                  "eac3",
                  "dts",
                  "mlp", "thd" )

    add_submodule()
    set_description( N_("MPEG-4 video" ) )
    set_capability( "demux", 5 )
    set_callbacks( OpenVideo, Close )
    add_float( "es-fps", 25, FPS_TEXT, FPS_LONGTEXT, false )

    add_shortcut( "m4v" )
    add_shortcut( "mp4v" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

#define WAV_PROBE_SIZE (512*1024)
#define BASE_PROBE_SIZE (8000)
#define WAV_EXTRA_PROBE_SIZE (44000/2*2*2)

typedef struct
{
    vlc_fourcc_t i_codec;
    bool       b_use_word;
    const char *psz_name;
    int  (*pf_probe)( demux_t *p_demux, uint64_t *pi_offset );
    int  (*pf_init)( demux_t *p_demux );
} codec_t;

typedef struct
{
    char  psz_version[10];
    int   i_lowpass;
} lame_extra_t;

typedef struct
{
    vlc_tick_t i_time;
    uint64_t i_pos;
    bs_t br;
} sync_table_ctx_t;

typedef struct
{
    uint16_t i_frames_btw_refs;
    uint32_t i_bytes_btw_refs;
    uint32_t i_ms_btw_refs;
    uint8_t i_bits_per_bytes_dev;
    uint8_t i_bits_per_ms_dev;
    uint8_t *p_bits;
    size_t i_bits;
    sync_table_ctx_t current;
} sync_table_t;

typedef struct
{
    uint32_t i_offset;
    seekpoint_t *p_seekpoint;
} chap_entry_t;

typedef struct
{
    codec_t codec;
    vlc_fourcc_t i_original;

    es_out_id_t *p_es;

    bool  b_start;
    decoder_t   *p_packetizer;
    block_t     *p_packetized_data;

    vlc_tick_t  i_pts;
    vlc_tick_t  i_time_offset;
    int64_t     i_bytes;

    bool        b_big_endian;
    bool        b_estimate_bitrate;
    int         i_bitrate_avg;  /* extracted from Xing header */

    bool b_initial_sync_failed;

    int i_packet_size;

    uint64_t i_stream_offset;
    unsigned i_demux_flags;

    float   f_fps;

    /* Mpga specific */
    struct
    {
        int i_frames;
        int i_bytes;
        int i_bitrate_avg;
        int i_frame_samples;
        lame_extra_t lame;
        bool b_lame;
    } xing;

    float rgf_replay_gain[AUDIO_REPLAY_GAIN_MAX];
    float rgf_replay_peak[AUDIO_REPLAY_GAIN_MAX];

    sync_table_t mllt;
    struct
    {
        size_t i_count;
        size_t i_current;
        chap_entry_t *p_entry;
    } chapters;
} demux_sys_t;

static int MpgaProbe( demux_t *p_demux, uint64_t *pi_offset );
static int MpgaInit( demux_t *p_demux );

static int AacProbe( demux_t *p_demux, uint64_t *pi_offset );
static int AacInit( demux_t *p_demux );

static int EA52Probe( demux_t *p_demux, uint64_t *pi_offset );
static int A52Probe( demux_t *p_demux, uint64_t *pi_offset );
static int A52Init( demux_t *p_demux );

static int DtsProbe( demux_t *p_demux, uint64_t *pi_offset );
static int DtsInit( demux_t *p_demux );

static int MlpProbe( demux_t *p_demux, uint64_t *pi_offset );
static int ThdProbe( demux_t *p_demux, uint64_t *pi_offset );
static int MlpInit( demux_t *p_demux );

static bool Parse( demux_t *p_demux, block_t **pp_output );
static uint64_t SeekByMlltTable( demux_t *p_demux, vlc_tick_t *pi_time );

static const codec_t p_codecs[] = {
    { VLC_CODEC_MP4A, false, "mp4 audio",  AacProbe,  AacInit },
    { VLC_CODEC_MPGA, false, "mpeg audio", MpgaProbe, MpgaInit },
    { VLC_CODEC_A52, true,  "a52 audio",  A52Probe,  A52Init },
    { VLC_CODEC_EAC3, true,  "eac3 audio", EA52Probe, A52Init },
    { VLC_CODEC_DTS, false, "dts audio",  DtsProbe,  DtsInit },
    { VLC_CODEC_MLP, false, "mlp audio",  MlpProbe,  MlpInit },
    { VLC_CODEC_TRUEHD, false, "TrueHD audio",  ThdProbe,  MlpInit },

    { 0, false, NULL, NULL, NULL }
};

static int VideoInit( demux_t *p_demux );

static const codec_t codec_m4v = {
    VLC_CODEC_MP4V, false, "mp4 video", NULL,  VideoInit
};

/*****************************************************************************
 * OpenCommon: initializes demux structures
 *****************************************************************************/
static int OpenCommon( demux_t *p_demux,
                       int i_cat, const codec_t *p_codec, uint64_t i_bs_offset )
{
    demux_sys_t *p_sys;

    es_format_t fmt;

    DEMUX_INIT_COMMON(); p_sys = p_demux->p_sys;
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    p_sys->codec = *p_codec;
    p_sys->p_es = NULL;
    p_sys->b_start = true;
    p_sys->i_stream_offset = i_bs_offset;
    p_sys->b_estimate_bitrate = true;
    p_sys->i_bitrate_avg = 0;
    p_sys->b_big_endian = false;
    p_sys->f_fps = var_InheritFloat( p_demux, "es-fps" );
    p_sys->p_packetized_data = NULL;
    p_sys->chapters.i_current = 0;
    TAB_INIT(p_sys->chapters.i_count, p_sys->chapters.p_entry);

    if( vlc_stream_Seek( p_demux->s, p_sys->i_stream_offset ) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_sys->codec.pf_init( p_demux ) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "detected format %4.4s", (const char*)&p_sys->codec.i_codec );

    /* Load the audio packetizer */
    es_format_Init( &fmt, i_cat, p_sys->codec.i_codec );
    fmt.i_original_fourcc = p_sys->i_original;
    p_sys->p_packetizer = demux_PacketizerNew( p_demux, &fmt, p_sys->codec.psz_name );
    if( !p_sys->p_packetizer )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    es_format_t *p_fmt = &p_sys->p_packetizer->fmt_out;
    for( int i = 0; i < AUDIO_REPLAY_GAIN_MAX; i++ )
    {
        if ( p_sys->rgf_replay_gain[i] != 0.0 )
        {
            p_fmt->audio_replay_gain.pb_gain[i] = true;
            p_fmt->audio_replay_gain.pf_gain[i] = p_sys->rgf_replay_gain[i];
        }
        if ( p_sys->rgf_replay_peak[i] != 0.0 )
        {
            p_fmt->audio_replay_gain.pb_peak[i] = true;
            p_fmt->audio_replay_gain.pf_peak[i] = p_sys->rgf_replay_peak[i];
        }
    }

    for( ;; )
    {
        if( Parse( p_demux, &p_sys->p_packetized_data ) )
            break;
        if( p_sys->p_packetized_data )
            break;
    }

    return VLC_SUCCESS;
}
static int OpenAudio( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    for( int i = 0; p_codecs[i].i_codec != 0; i++ )
    {
        uint64_t i_offset;
        if( !p_codecs[i].pf_probe( p_demux, &i_offset ) )
            return OpenCommon( p_demux, AUDIO_ES, &p_codecs[i], i_offset );
    }
    return VLC_EGENERIC;
}
static int OpenVideo( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;

    /* Only m4v is supported for the moment */
    bool b_m4v_ext    = demux_IsPathExtension( p_demux, ".m4v" );
    bool b_re4_ext    = !b_m4v_ext && demux_IsPathExtension( p_demux, ".re4" );
    bool b_m4v_forced = demux_IsForced( p_demux, "m4v" ) ||
                        demux_IsForced( p_demux, "mp4v" );

    if( !b_m4v_ext && !b_m4v_forced && !b_re4_ext )
        return VLC_EGENERIC;

    ssize_t i_off = b_re4_ext ? 220 : 0;
    const uint8_t *p_peek;
    if( vlc_stream_Peek( p_demux->s, &p_peek, i_off + 4 ) < i_off + 4 )
        return VLC_EGENERIC;
    if( p_peek[i_off + 0] != 0x00 || p_peek[i_off + 1] != 0x00 || p_peek[i_off + 2] != 0x01 )
    {
        if( !b_m4v_forced)
            return VLC_EGENERIC;
        msg_Warn( p_demux,
                  "this doesn't look like an MPEG ES stream, continuing anyway" );
    }
    return OpenCommon( p_demux, VIDEO_ES, &codec_m4v, i_off );
}

static void IncreaseChapter( demux_t *p_demux, vlc_tick_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    while( p_sys->chapters.i_current + 1 < p_sys->chapters.i_count )
    {
        const chap_entry_t *p = &p_sys->chapters.p_entry[p_sys->chapters.i_current + 1];
        if( (p->i_offset != UINT32_MAX && vlc_stream_Tell( p_demux->s ) < p->i_offset) ||
            (i_time == VLC_TICK_INVALID || p->p_seekpoint->i_time_offset + VLC_TICK_0 > i_time) )
            break;
        ++p_sys->chapters.i_current;
        p_sys->i_demux_flags |= INPUT_UPDATE_SEEKPOINT;
    }
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    int ret = 1;
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t *p_block_out = p_sys->p_packetized_data;
    if( p_block_out )
        p_sys->p_packetized_data = NULL;
    else
        ret = Parse( p_demux, &p_block_out ) ? 0 : 1;

    /* Update chapter if any */
    IncreaseChapter( p_demux,
                     p_block_out ? p_sys->i_time_offset + p_block_out->i_dts
                                 : VLC_TICK_INVALID );

    while( p_block_out )
    {
        block_t *p_next = p_block_out->p_next;

        /* Correct timestamp */
        if( p_sys->p_packetizer->fmt_out.i_cat == VIDEO_ES )
        {
            if( p_block_out->i_pts == VLC_TICK_INVALID &&
                p_block_out->i_dts == VLC_TICK_INVALID )
                p_block_out->i_dts = VLC_TICK_0 + p_sys->i_pts + VLC_TICK_FROM_SEC(1) / p_sys->f_fps;
            if( p_block_out->i_dts != VLC_TICK_INVALID )
                p_sys->i_pts = p_block_out->i_dts - VLC_TICK_0;
        }
        else
        {
            p_sys->i_pts = p_block_out->i_pts - VLC_TICK_0;
        }

        if( p_block_out->i_pts != VLC_TICK_INVALID )
        {
            p_block_out->i_pts += p_sys->i_time_offset;
        }
        if( p_block_out->i_dts != VLC_TICK_INVALID )
        {
            p_block_out->i_dts += p_sys->i_time_offset;
            es_out_SetPCR( p_demux->out, p_block_out->i_dts );
        }
        /* Re-estimate bitrate */
        if( p_sys->b_estimate_bitrate && p_sys->i_pts > VLC_TICK_FROM_MS(500) )
            p_sys->i_bitrate_avg = 8 * CLOCK_FREQ * p_sys->i_bytes
                                   / (p_sys->i_pts - 1);
        p_sys->i_bytes += p_block_out->i_buffer;


        p_block_out->p_next = NULL;
        es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

        p_block_out = p_next;
    }
    return ret;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_packetized_data )
        block_ChainRelease( p_sys->p_packetized_data );
    for( size_t i=0; i< p_sys->chapters.i_count; i++ )
        vlc_seekpoint_Delete( p_sys->chapters.p_entry[i].p_seekpoint );
    TAB_CLEAN( p_sys->chapters.i_count, p_sys->chapters.p_entry );
    if( p_sys->mllt.p_bits )
        free( p_sys->mllt.p_bits );
    demux_PacketizerDestroy( p_sys->p_packetizer );
    free( p_sys );
}

/*****************************************************************************
 * Time seek:
 *****************************************************************************/
static int MovetoTimePos( demux_t *p_demux, vlc_tick_t i_time, uint64_t i_pos )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    int i_ret = vlc_stream_Seek( p_demux->s, p_sys->i_stream_offset + i_pos );
    if( i_ret != VLC_SUCCESS )
        return i_ret;
    p_sys->i_time_offset = i_time - p_sys->i_pts;
    /* And reset buffered data */
    if( p_sys->p_packetized_data )
        block_ChainRelease( p_sys->p_packetized_data );
    p_sys->p_packetized_data = NULL;
    p_sys->chapters.i_current = 0;
    p_sys->i_demux_flags |= INPUT_UPDATE_SEEKPOINT;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    bool *pb_bool;

    switch( i_query )
    {
        case DEMUX_HAS_UNSUPPORTED_META:
            pb_bool = va_arg( args, bool * );
            *pb_bool = true;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_pts + p_sys->i_time_offset;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
        {
            va_list ap;
            int i_ret;

            va_copy ( ap, args );
            i_ret = demux_vaControlHelper( p_demux->s, p_sys->i_stream_offset,
                                    -1, p_sys->i_bitrate_avg, 1, i_query, ap );
            va_end( ap );

            /* No bitrate, we can't have it precisely, but we can compute
             * a raw approximation with time/position */
            if( i_ret && !p_sys->i_bitrate_avg )
            {
                float f_pos = (double)(uint64_t)( vlc_stream_Tell( p_demux->s ) ) /
                              (double)(uint64_t)( stream_Size( p_demux->s ) );
                /* The first few seconds are guaranteed to be very whacky,
                 * don't bother trying ... Too bad */
                if( f_pos < 0.01f ||
                    (p_sys->i_pts + p_sys->i_time_offset) < VLC_TICK_FROM_SEC(8) )
                {
                    return VLC_EGENERIC;
                }

                *va_arg( args, vlc_tick_t * ) =
                    (p_sys->i_pts + p_sys->i_time_offset) / f_pos;
                return VLC_SUCCESS;
            }
            return i_ret;
        }

        case DEMUX_SET_TIME:
            if( p_sys->mllt.p_bits )
            {
                vlc_tick_t i_time = va_arg(args, vlc_tick_t);
                uint64_t i_pos = SeekByMlltTable( p_demux, &i_time );
                return MovetoTimePos( p_demux, i_time, i_pos );
            }
            /* FIXME TODO: implement a high precision seek (with mp3 parsing)
             * needed for multi-input */
            break;

        case DEMUX_GET_TITLE_INFO:
        {
            if( p_sys->chapters.i_count == 0 )
                return VLC_EGENERIC;
            input_title_t **pp_title = malloc( sizeof(*pp_title) );
            if( !pp_title )
                return VLC_EGENERIC;
            *pp_title = vlc_input_title_New();
            if( !*pp_title )
            {
                free( pp_title );
                return VLC_EGENERIC;
            }
            (*pp_title)->seekpoint = vlc_alloc( p_sys->chapters.i_count, sizeof(seekpoint_t) );
            if( !(*pp_title)->seekpoint )
            {
                free( *pp_title );
                free( pp_title );
                return VLC_EGENERIC;
            }
            for( size_t i=0; i<p_sys->chapters.i_count; i++ )
            {
                seekpoint_t *s = vlc_seekpoint_Duplicate( p_sys->chapters.p_entry[i].p_seekpoint );
                if( s )
                    (*pp_title)->seekpoint[(*pp_title)->i_seekpoint++] = s;
            }
            *(va_arg( args, input_title_t *** )) = pp_title;
            *(va_arg( args, int* )) = 1;
            *(va_arg( args, int* )) = 0;
            *(va_arg( args, int* )) = 0;
            return VLC_SUCCESS;
        }
        break;

        case DEMUX_GET_TITLE:
            if( p_sys->chapters.i_count == 0 )
                return VLC_EGENERIC;
            *(va_arg( args, int* )) = 0;
            return VLC_SUCCESS;

        case DEMUX_GET_SEEKPOINT:
            if( p_sys->chapters.i_count == 0 )
                return VLC_EGENERIC;
            *(va_arg( args, int* )) = p_sys->chapters.i_current;
            return VLC_SUCCESS;

        case DEMUX_SET_TITLE:
            return va_arg( args, int) == 0 ? VLC_SUCCESS : VLC_EGENERIC;

        case DEMUX_SET_SEEKPOINT:
        {
            int i = va_arg( args, int );
            if( (size_t)i>=p_sys->chapters.i_count )
                return VLC_EGENERIC;
            const chap_entry_t *p = &p_sys->chapters.p_entry[i];
            if( p_sys->chapters.p_entry[i].i_offset == UINT32_MAX )
                return demux_Control( p_demux, DEMUX_SET_TIME, p->p_seekpoint->i_time_offset );
            int i_ret= MovetoTimePos( p_demux, p->p_seekpoint->i_time_offset, p->i_offset );
            if( i_ret == VLC_SUCCESS )
                p_sys->chapters.i_current = i;
            return i_ret;
        }

        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict flags = va_arg( args, unsigned * );
            *flags &= p_sys->i_demux_flags;
            p_sys->i_demux_flags &= ~*flags;
            return VLC_SUCCESS;
        }
    }

    int ret = demux_vaControlHelper( p_demux->s, p_sys->i_stream_offset, -1,
                                       p_sys->i_bitrate_avg, 1, i_query, args );
    if( ret != VLC_SUCCESS )
        return ret;

    if( i_query == DEMUX_SET_POSITION || i_query == DEMUX_SET_TIME )
    {
        if( p_sys->i_bitrate_avg > 0 )
        {
            int64_t i_time = INT64_C(8000000)
                * ( vlc_stream_Tell(p_demux->s) - p_sys->i_stream_offset )
                / p_sys->i_bitrate_avg;

            /* Fix time_offset */
            if( i_time >= 0 )
                p_sys->i_time_offset = i_time - p_sys->i_pts;
            /* And reset buffered data */
            if( p_sys->p_packetized_data )
                block_ChainRelease( p_sys->p_packetized_data );
            p_sys->p_packetized_data = NULL;
        }

        /* Reset chapter if any */
        p_sys->chapters.i_current = 0;
        p_sys->i_demux_flags |= INPUT_UPDATE_SEEKPOINT;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Makes a link list of buffer of parsed data
 * Returns true if EOF
 *****************************************************************************/
static bool Parse( demux_t *p_demux, block_t **pp_output )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block_in, *p_block_out;

    *pp_output = NULL;

    if( p_sys->codec.b_use_word )
    {
        /* Make sure we are word aligned */
        int64_t i_pos = vlc_stream_Tell( p_demux->s );
        if( (i_pos & 1) && vlc_stream_Read( p_demux->s, NULL, 1 ) != 1 )
            return true;
    }

    p_block_in = vlc_stream_Block( p_demux->s, p_sys->i_packet_size );
    bool b_eof = p_block_in == NULL;

    if( p_block_in )
    {
        if( p_sys->codec.b_use_word && !p_sys->b_big_endian && p_block_in->i_buffer > 0 )
        {
            /* Convert to big endian */
            block_t *old = p_block_in;
            p_block_in = block_Alloc( p_block_in->i_buffer );
            if( p_block_in )
            {
                block_CopyProperties( p_block_in, old );
                swab( old->p_buffer, p_block_in->p_buffer, old->i_buffer );
            }
            block_Release( old );
        }

        if( p_block_in )
        {
            p_block_in->i_pts =
            p_block_in->i_dts = (p_sys->b_start || p_sys->b_initial_sync_failed) ?
                                 VLC_TICK_0 : VLC_TICK_INVALID;
        }
    }
    p_sys->b_initial_sync_failed = p_sys->b_start; /* Only try to resync once */

    while( ( p_block_out = p_sys->p_packetizer->pf_packetize( p_sys->p_packetizer, p_block_in ? &p_block_in : NULL ) ) )
    {
        p_sys->b_initial_sync_failed = false;
        while( p_block_out )
        {
            if( !p_sys->p_es )
            {
                p_sys->p_packetizer->fmt_out.b_packetized = true;
                p_sys->p_packetizer->fmt_out.i_id = 0;
                p_sys->p_es = es_out_Add( p_demux->out,
                                          &p_sys->p_packetizer->fmt_out);


                /* Try the xing header */
                if( p_sys->xing.i_bytes && p_sys->xing.i_frames &&
                    p_sys->xing.i_frame_samples )
                {
                    p_sys->i_bitrate_avg = p_sys->xing.i_bytes * INT64_C(8) *
                        p_sys->p_packetizer->fmt_out.audio.i_rate /
                        p_sys->xing.i_frames / p_sys->xing.i_frame_samples;

                    if( p_sys->i_bitrate_avg > 0 )
                        p_sys->b_estimate_bitrate = false;
                }
                /* Use the bitrate as initual value */
                if( p_sys->b_estimate_bitrate )
                    p_sys->i_bitrate_avg = p_sys->p_packetizer->fmt_out.i_bitrate;
            }

            block_t *p_next = p_block_out->p_next;
            p_block_out->p_next = NULL;

            block_ChainLastAppend( &pp_output, p_block_out );

            p_block_out = p_next;
        }
    }

    if( p_sys->b_initial_sync_failed )
        msg_Dbg( p_demux, "did not sync on first block" );
    p_sys->b_start = false;

    return b_eof;
}

/* Check to apply to WAVE fmt header */
static int GenericFormatCheck( int i_format, const uint8_t *p_head )
{
    if ( i_format == WAVE_FORMAT_PCM )
    {
        if( GetWLE( p_head /* nChannels */ ) != 2 )
            return VLC_EGENERIC;
        if( GetDWLE( p_head + 2 /* nSamplesPerSec */ ) != 44100 )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Wav header skipper
 *****************************************************************************/
static int WavSkipHeader( demux_t *p_demux, uint64_t *pi_skip,
                          const uint16_t rgi_twocc[],
                          int (*pf_format_check)( int, const uint8_t * ) )
{
    const uint8_t *p_peek;
    size_t i_peek = 0;
    uint32_t i_len;

    /* */
    *pi_skip = 0;

    /* Check if we are dealing with a WAV file */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 12+8 ) != 12 + 8 )
        return VLC_SUCCESS;

    if( memcmp( p_peek, "RIFF", 4 ) || memcmp( &p_peek[8], "WAVE", 4 ) )
        return VLC_SUCCESS;

    /* Find the wave format header */
    i_peek = 12 + 8;
    while( memcmp( p_peek + i_peek - 8, "fmt ", 4 ) )
    {
        i_len = GetDWLE( p_peek + i_peek - 4 );
        if( i_len > WAV_PROBE_SIZE || i_peek + i_len > WAV_PROBE_SIZE )
            return VLC_EGENERIC;

        i_peek += i_len + 8;
        if( vlc_stream_Peek( p_demux->s, &p_peek, i_peek ) != (ssize_t) i_peek )
            return VLC_EGENERIC;
    }

    /* Sanity check the wave format header */
    i_len = GetDWLE( p_peek + i_peek - 4 );
    if( i_len > WAV_PROBE_SIZE )
        return VLC_EGENERIC;

    i_peek += i_len + 8;
    if( vlc_stream_Peek( p_demux->s, &p_peek, i_peek ) != (ssize_t) i_peek )
        return VLC_EGENERIC;
    const uint16_t i_twocc = GetWLE( p_peek + i_peek - i_len - 8 /* wFormatTag */ );
    int i_format_idx;
    for( i_format_idx = 0; rgi_twocc[i_format_idx] != WAVE_FORMAT_UNKNOWN; i_format_idx++ )
    {
        if( i_twocc == rgi_twocc[i_format_idx] )
            break;
    }
    if( rgi_twocc[i_format_idx] == WAVE_FORMAT_UNKNOWN )
        return VLC_EGENERIC;

    if( pf_format_check &&
        pf_format_check( i_twocc, p_peek + i_peek - i_len - 6 ) != VLC_SUCCESS )
            return VLC_EGENERIC;

    /* Skip the wave header */
    while( memcmp( p_peek + i_peek - 8, "data", 4 ) )
    {
        i_len = GetDWLE( p_peek + i_peek - 4 );
        if( i_len > WAV_PROBE_SIZE || i_peek + i_len > WAV_PROBE_SIZE )
            return VLC_EGENERIC;

        i_peek += i_len + 8;
        if( vlc_stream_Peek( p_demux->s, &p_peek, i_peek ) != (ssize_t) i_peek )
            return VLC_EGENERIC;
    }
    *pi_skip = i_peek;
    return VLC_SUCCESS;
}

static int GenericProbe( demux_t *p_demux, uint64_t *pi_offset,
                         const char * ppsz_name[],
                         int (*pf_check)( const uint8_t *, unsigned * ),
                         unsigned i_check_size,
                         unsigned i_base_probing,
                         unsigned i_wav_extra_probing,
                         bool b_use_word,
                         const uint16_t pi_twocc[],
                         int (*pf_format_check)( int, const uint8_t * ) )
{
    bool   b_forced_demux;

    uint64_t i_offset;
    const uint8_t *p_peek;
    uint64_t i_skip;

    b_forced_demux = false;
    for( size_t i = 0; ppsz_name[i] != NULL; i++ )
    {
        b_forced_demux |= demux_IsForced( p_demux, ppsz_name[i] );
    }

    i_offset = vlc_stream_Tell( p_demux->s );

    if( WavSkipHeader( p_demux, &i_skip, pi_twocc, pf_format_check ) )
    {
        if( !b_forced_demux )
            return VLC_EGENERIC;
    }
    const bool b_wav = i_skip > 0;

    /* peek the begining
     * It is common that wav files have some sort of garbage at the begining
     * We will accept probing 0.5s of data in this case.
     */
    const size_t i_probe = i_skip + i_check_size + i_base_probing + ( b_wav ? i_wav_extra_probing : 0);
    const ssize_t i_peek = vlc_stream_Peek( p_demux->s, &p_peek, i_probe );

    if( i_peek < 0 || (size_t)i_peek < i_skip + i_check_size )
        return VLC_EGENERIC;

    for( ;; )
    {
        if( i_skip + i_check_size > i_peek )
        {
            if( !b_forced_demux )
                return VLC_EGENERIC;
            break;
        }
        unsigned i_samples = 0;
        int i_size = pf_check( &p_peek[i_skip], &i_samples );
        if( i_size >= 0 )
        {
            if( i_size == 0 || /* 0 sized frame ?? */
                i_skip == 0 /* exact match from start, we're not WAVE either, so skip multiple checks (would break if padding) */ )
                break;

            /* If we have the frame size, check the next frame for
             * extra robustness
             * The second test is because some .wav have paddings
             */
            bool b_ok = false;
            for( int t = 0; t < 1 + !!b_wav; t++ )
            {
                if( t == 1 )
                {
                    if(!i_samples)
                        break;
                    i_size = i_samples * 2 * 2;
                }

                if( i_skip + i_check_size + i_size <= i_peek )
                {
                    b_ok = pf_check( &p_peek[i_skip+i_size], NULL ) >= 0;
                    if( b_ok )
                        break;
                }
            }
            if( b_ok )
                break;
        }
        if( b_use_word )
            i_skip += ((i_offset + i_skip) % 2 == 0) ? 2 : 1;
        else
            i_skip++;
        if( !b_wav && !b_forced_demux )
            return VLC_EGENERIC;
    }

    *pi_offset = i_offset + i_skip;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Mpeg I/II Audio
 *****************************************************************************/
static int MpgaCheckSync( const uint8_t *p_peek )
{
    uint32_t h = GetDWBE( p_peek );

    if( ((( h >> 21 )&0x07FF) != 0x07FF )   /* header sync */
        || (((h >> 19)&0x03) == 1 )         /* valid version ID ? */
        || (((h >> 17)&0x03) == 0 )         /* valid layer ?*/
        || (((h >> 12)&0x0F) == 0x0F )      /* valid bitrate ?*/
        || (((h >> 10) & 0x03) == 0x03 )    /* valide sampling freq ? */
        || ((h & 0x03) == 0x02 ))           /* valid emphasis ? */
    {
        return false;
    }
    return true;
}

#define MPGA_VERSION( h )   ( 1 - (((h)>>19)&0x01) )
#define MPGA_MODE(h)        (((h)>> 6)&0x03)

static int MpgaGetFrameSamples( uint32_t h )
{
    const int i_layer = 3 - (((h)>>17)&0x03);
    switch( i_layer )
    {
    case 0:
        return 384;
    case 1:
        return 1152;
    case 2:
        return MPGA_VERSION(h) ? 576 : 1152;
    default:
        return 0;
    }
}

static int MpgaProbe( demux_t *p_demux, uint64_t *pi_offset )
{
    const uint16_t rgi_twocc[] = { WAVE_FORMAT_MPEG, WAVE_FORMAT_MPEGLAYER3, WAVE_FORMAT_UNKNOWN };
    bool   b_forced;
    bool   b_forced_demux;
    uint64_t i_offset;

    const uint8_t *p_peek;
    uint64_t i_skip;
    ssize_t i_peek;

    b_forced = demux_IsPathExtension( p_demux, ".mp3" );
    b_forced_demux = demux_IsForced( p_demux, "mp3" ) ||
                     demux_IsForced( p_demux, "mpga" );

    i_offset = vlc_stream_Tell( p_demux->s );

    if( WavSkipHeader( p_demux, &i_skip, rgi_twocc, NULL ) )
    {
        if( !b_forced_demux )
            return VLC_EGENERIC;

        return VLC_EGENERIC;
    }

    i_peek = vlc_stream_Peek( p_demux->s, &p_peek, i_skip + 4 );
    if( i_peek <= 0 || (uint64_t) i_peek < i_skip + 4 )
        return VLC_EGENERIC;

    if( !MpgaCheckSync( &p_peek[i_skip] ) )
    {
        bool b_ok = false;

        if( !b_forced_demux && !b_forced )
            return VLC_EGENERIC;

        i_peek = vlc_stream_Peek( p_demux->s, &p_peek, i_skip + 8096 );
        while( i_peek > 0 && i_skip + 4 < (uint64_t) i_peek )
        {
            if( MpgaCheckSync( &p_peek[i_skip] ) )
            {
                b_ok = true;
                break;
            }
            i_skip++;
        }
        if( !b_ok && !b_forced_demux )
            return VLC_EGENERIC;
    }
    *pi_offset = i_offset + i_skip;
    return VLC_SUCCESS;
}

static void MpgaXingSkip( const uint8_t **pp_xing, int *pi_xing, int i_count )
{
    if(i_count > *pi_xing )
        i_count = *pi_xing;

    (*pp_xing) += i_count;
    (*pi_xing) -= i_count;
}

static uint32_t MpgaXingGetDWBE( const uint8_t **pp_xing, int *pi_xing, uint32_t i_default )
{
    if( *pi_xing < 4 )
        return i_default;

    uint32_t v = GetDWBE( *pp_xing );

    MpgaXingSkip( pp_xing, pi_xing, 4 );

    return v;
}

static uint16_t MpgaXingGetWBE( const uint8_t **pp_xing, int *pi_xing, uint16_t i_default )
{
    if( *pi_xing < 2 )
        return i_default;

    uint16_t v = GetWBE( *pp_xing );

    MpgaXingSkip( pp_xing, pi_xing, 2 );

    return v;
}

static double MpgaXingLameConvertGain( uint16_t x )
{
    double gain = (x & 0x1FF) / 10.0;

    return x & 0x200 ? -gain : gain;
}

static double MpgaXingLameConvertPeak( uint32_t x )
{
    return x / 8388608.0; /* pow(2, 23) */
}

static uint64_t SeekByMlltTable( demux_t *p_demux, vlc_tick_t *pi_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    sync_table_ctx_t *p_cur = &p_sys->mllt.current;

    /* reset or init context */
    if( *pi_time < p_cur->i_time || !p_cur->br.p )
    {
        p_cur->i_time = 0;
        p_cur->i_pos = 0;
        bs_init(&p_cur->br, p_sys->mllt.p_bits, p_sys->mllt.i_bits);
    }

    while(!bs_eof(&p_cur->br))
    {
        const uint32_t i_bytesdev = bs_read(&p_cur->br, p_sys->mllt.i_bits_per_bytes_dev);
        const uint32_t i_msdev = bs_read(&p_cur->br, p_sys->mllt.i_bits_per_ms_dev);
        if(bs_error(&p_cur->br))
            break;
        const vlc_tick_t i_deltatime = VLC_TICK_FROM_MS(p_sys->mllt.i_ms_btw_refs + i_msdev);
        if( p_cur->i_time + i_deltatime > *pi_time )
            break;
        p_cur->i_time += i_deltatime;
        p_cur->i_pos += p_sys->mllt.i_bytes_btw_refs + i_bytesdev;
    }
    *pi_time = p_cur->i_time;
    return p_cur->i_pos;
}

static int ID3TAG_Parse_Handler( uint32_t i_tag, const uint8_t *p_payload, size_t i_payload, void *p_priv )
{
    demux_t *p_demux = (demux_t *) p_priv;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_tag == VLC_FOURCC('M', 'L', 'L', 'T') )
    {
        if( i_payload > 20 )
        {
            p_sys->mllt.i_frames_btw_refs = GetWBE(p_payload);
            p_sys->mllt.i_bytes_btw_refs = GetDWBE(&p_payload[1]) & 0x00FFFFFF;
            p_sys->mllt.i_ms_btw_refs = GetDWBE(&p_payload[4]) & 0x00FFFFFF;
            if( !p_sys->mllt.i_frames_btw_refs || !p_sys->mllt.i_bytes_btw_refs ||
                    !p_sys->mllt.i_ms_btw_refs ||
                    p_payload[8] > 31 || p_payload[9] > 31 || /* bits length sanity check */
                    ((p_payload[8] + p_payload[9]) % 4) || p_payload[8] + p_payload[9] < 4 )
                return VLC_EGENERIC;
            p_sys->mllt.i_bits_per_bytes_dev = p_payload[8];
            p_sys->mllt.i_bits_per_ms_dev = p_payload[9];
            p_sys->mllt.p_bits = malloc(i_payload - 10);
            if( likely(p_sys->mllt.p_bits) )
            {
                p_sys->mllt.i_bits = i_payload - 10;
                memcpy(p_sys->mllt.p_bits, &p_payload[10], p_sys->mllt.i_bits);
                msg_Dbg(p_demux, "read MLLT sync table with %zu entries",
                        (p_sys->mllt.i_bits * 8) / (p_sys->mllt.i_bits_per_bytes_dev + p_sys->mllt.i_bits_per_ms_dev) );
            }
        }
        return VLC_EGENERIC;
    }
    else if( i_tag == VLC_FOURCC('T', 'X', 'X', 'X') )
    {
        vlc_meta_t *p_meta = vlc_meta_New();
        if( p_meta )
        {
            bool b_updated;
            if( ID3HandleTag( p_payload, i_payload, i_tag, p_meta, &b_updated ) )
            {
                char ** ppsz_keys = vlc_meta_CopyExtraNames( p_meta );
                if( ppsz_keys )
                {
                    for( size_t i = 0; ppsz_keys[i]; ++i )
                    {
                        float *pf = NULL;
                        if(     !strcasecmp( ppsz_keys[i], "REPLAYGAIN_TRACK_GAIN" ) )
                            pf = &p_sys->rgf_replay_gain[AUDIO_REPLAY_GAIN_TRACK];
                        else if( !strcasecmp( ppsz_keys[i], "REPLAYGAIN_TRACK_PEAK" ) )
                            pf = &p_sys->rgf_replay_peak[AUDIO_REPLAY_GAIN_TRACK];
                        else if( !strcasecmp( ppsz_keys[i], "REPLAYGAIN_ALBUM_GAIN" ) )
                            pf = &p_sys->rgf_replay_gain[AUDIO_REPLAY_GAIN_ALBUM];
                        else if( !strcasecmp( ppsz_keys[i], "REPLAYGAIN_ALBUM_PEAK" ) )
                            pf = &p_sys->rgf_replay_peak[AUDIO_REPLAY_GAIN_ALBUM];
                        if( pf )
                        {
                            const char *psz_val = vlc_meta_GetExtra( p_meta, ppsz_keys[i] );
                            if( psz_val )
                                *pf = us_atof( psz_val );
                        }
                        free( ppsz_keys[i] );
                    }
                    free( ppsz_keys );
                }
            }
            vlc_meta_Delete( p_meta );
        }
    }
    else if ( i_tag == VLC_FOURCC('C', 'H', 'A', 'P') && i_payload >= 17 )
    {
        char *psz_title = strndup( (const char *)p_payload, i_payload - 16 );
        size_t i_offset = psz_title ? strlen( psz_title ) : 0;
        if( p_payload[i_offset] != 0 )
        {
            free( psz_title );
            return VLC_EGENERIC;
        }
        chap_entry_t e;
        e.p_seekpoint = vlc_seekpoint_New();
        if( e.p_seekpoint )
        {
            e.p_seekpoint->psz_name = psz_title;
            e.p_seekpoint->i_time_offset = VLC_TICK_FROM_MS(GetDWBE(&p_payload[1 + i_offset]));
            e.i_offset = GetDWBE(&p_payload[1 + i_offset + 8]);
            p_payload += i_offset + 1 + 16;
            i_payload -= i_offset + 1 + 16;
            if( 12 < i_payload && !memcmp("TIT2", p_payload, 4) )
            {
                psz_title = NULL; /* optional alloc */
                const char *psz = ID3TextConvert(&p_payload[10], i_payload-12, &psz_title);
                if( psz ) /* replace with TIT2 */
                {
                    free( e.p_seekpoint->psz_name );
                    e.p_seekpoint->psz_name = (psz_title) ? psz_title : strdup( psz );
                }
            }
            TAB_APPEND(p_sys->chapters.i_count, p_sys->chapters.p_entry, e);
        } else free( psz_title );
    }

    return VLC_SUCCESS;
}

static int ID3Parse( demux_t *p_demux,
                     int (*pf_callback)(uint32_t, const uint8_t *, size_t, void *) )
{
    const block_t *p_tags = NULL;

    if( vlc_stream_Control( p_demux->s, STREAM_GET_TAGS, &p_tags ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    for( ; p_tags; p_tags = p_tags->p_next )
        ID3TAG_Parse( p_tags->p_buffer, p_tags->i_buffer, pf_callback, (void *) p_demux );

    return VLC_SUCCESS;
}

static int MpgaInit( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    const uint8_t *p_peek;
    int i_peek;

    /* */
    p_sys->i_packet_size = 1024;

    ID3Parse( p_demux, ID3TAG_Parse_Handler );

    /* Load a potential xing header */
    i_peek = vlc_stream_Peek( p_demux->s, &p_peek, 4 + 1024 );
    if( i_peek < 4 + 21 )
        return VLC_SUCCESS;

    const uint32_t header = GetDWBE( p_peek );
    if( !MpgaCheckSync( p_peek ) )
        return VLC_SUCCESS;

    /* Xing header */
    const uint8_t *p_xing = p_peek;
    int i_xing = i_peek;
    int i_skip;

    if( MPGA_VERSION( header ) == 0 )
        i_skip = MPGA_MODE( header ) != 3 ? 36 : 21;
    else
        i_skip = MPGA_MODE( header ) != 3 ? 21 : 13;

    if( i_skip + 8 >= i_xing || memcmp( &p_xing[i_skip], "Xing", 4 ) )
        return VLC_SUCCESS;

    const uint32_t i_flags = GetDWBE( &p_xing[i_skip+4] );

    MpgaXingSkip( &p_xing, &i_xing, i_skip + 8 );

    if( i_flags&0x01 )
        p_sys->xing.i_frames = MpgaXingGetDWBE( &p_xing, &i_xing, 0 );
    if( i_flags&0x02 )
        p_sys->xing.i_bytes = MpgaXingGetDWBE( &p_xing, &i_xing, 0 );
    if( i_flags&0x04 ) /* TODO Support XING TOC to improve seeking accuracy */
        MpgaXingSkip( &p_xing, &i_xing, 100 );
    if( i_flags&0x08 )
    {
        /* FIXME: doesn't return the right bitrage average, at least
           with some MP3's */
        p_sys->xing.i_bitrate_avg = MpgaXingGetDWBE( &p_xing, &i_xing, 0 );
        msg_Dbg( p_demux, "xing vbr value present (%d)",
                 p_sys->xing.i_bitrate_avg );
    }

    if( p_sys->xing.i_frames > 0 && p_sys->xing.i_bytes > 0 )
    {
        p_sys->xing.i_frame_samples = MpgaGetFrameSamples( header );
        msg_Dbg( p_demux, "xing frames&bytes value present "
                 "(%d bytes, %d frames, %d samples/frame)",
                 p_sys->xing.i_bytes, p_sys->xing.i_frames,
                 p_sys->xing.i_frame_samples );
    }

    if( i_xing >= 20 && memcmp( p_xing, "LAME", 4 ) == 0)
    {
        p_sys->xing.b_lame = true;
        lame_extra_t *p_lame = &p_sys->xing.lame;

        memcpy( p_lame->psz_version, p_xing, 9 );
        p_lame->psz_version[9] = '\0';

        MpgaXingSkip( &p_xing, &i_xing, 9 );
        MpgaXingSkip( &p_xing, &i_xing, 1 ); /* rev_method */

        p_lame->i_lowpass = (*p_xing) * 100;
        MpgaXingSkip( &p_xing, &i_xing, 1 );

        uint32_t peak  = MpgaXingGetDWBE( &p_xing, &i_xing, 0 );
        uint16_t track = MpgaXingGetWBE( &p_xing, &i_xing, 0 );
        uint16_t album = MpgaXingGetWBE( &p_xing, &i_xing, 0 );

        p_sys->rgf_replay_peak[AUDIO_REPLAY_GAIN_TRACK] = (float) MpgaXingLameConvertPeak( peak );
        p_sys->rgf_replay_gain[AUDIO_REPLAY_GAIN_TRACK] = (float) MpgaXingLameConvertGain( track );
        p_sys->rgf_replay_gain[AUDIO_REPLAY_GAIN_ALBUM] = (float) MpgaXingLameConvertGain( album );

        MpgaXingSkip( &p_xing, &i_xing, 1 ); /* flags */
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * AAC
 *****************************************************************************/
static int AacProbe( demux_t *p_demux, uint64_t *pi_offset )
{
    bool   b_forced;
    bool   b_forced_demux;

    uint64_t i_offset;
    const uint8_t *p_peek;

    b_forced = demux_IsPathExtension( p_demux, ".aac" ) ||
               demux_IsPathExtension( p_demux, ".aacp" );
    b_forced_demux = demux_IsForced( p_demux, "m4a" ) ||
                     demux_IsForced( p_demux, "aac" ) ||
                     demux_IsForced( p_demux, "mp4a" );

    if( !b_forced_demux && !b_forced )
        return VLC_EGENERIC;

    i_offset = vlc_stream_Tell( p_demux->s );

    /* peek the begining (10 is for adts header) */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 10 ) < 10 )
        return VLC_EGENERIC;

    if( !strncmp( (char *)p_peek, "ADIF", 4 ) )
    {
        msg_Err( p_demux, "ADIF file. Not yet supported. (Please report)" );
        return VLC_EGENERIC;
    }

    *pi_offset = i_offset;
    return VLC_SUCCESS;
}
static int AacInit( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_packet_size = 4096;
    p_sys->i_original = VLC_FOURCC('H','E','A','D');

    return VLC_SUCCESS;
}


/*****************************************************************************
 * A52
 *****************************************************************************/
static int A52CheckSync( const uint8_t *p_peek, bool *p_big_endian, unsigned *pi_samples, bool b_eac3 )
{
    vlc_a52_header_t header;
    uint8_t p_tmp[VLC_A52_MIN_HEADER_SIZE];

    *p_big_endian =  p_peek[0] == 0x0b && p_peek[1] == 0x77;
    if( !*p_big_endian )
    {
        swab( p_peek, p_tmp, VLC_A52_MIN_HEADER_SIZE );
        p_peek = p_tmp;
    }

    if( vlc_a52_header_Parse( &header, p_peek, VLC_A52_MIN_HEADER_SIZE ) )
        return VLC_EGENERIC;

    if( !header.b_eac3 != !b_eac3 )
        return VLC_EGENERIC;
    if( pi_samples )
        *pi_samples = header.i_samples;
    return header.i_size;
}
static int EA52CheckSyncProbe( const uint8_t *p_peek, unsigned *pi_samples )
{
    bool b_dummy;
    return A52CheckSync( p_peek, &b_dummy, pi_samples, true );
}

static int EA52Probe( demux_t *p_demux, uint64_t *pi_offset )
{
    const char *ppsz_name[] = { "eac3", NULL };
    const uint16_t rgi_twocc[] = { WAVE_FORMAT_PCM, WAVE_FORMAT_A52, WAVE_FORMAT_UNKNOWN };

    return GenericProbe( p_demux, pi_offset, ppsz_name, EA52CheckSyncProbe,
                         VLC_A52_MIN_HEADER_SIZE,
                         1920 + VLC_A52_MIN_HEADER_SIZE + 1,
                         WAV_EXTRA_PROBE_SIZE,
                         true, rgi_twocc, GenericFormatCheck );
}

static int A52CheckSyncProbe( const uint8_t *p_peek, unsigned *pi_samples )
{
    bool b_dummy;
    return A52CheckSync( p_peek, &b_dummy, pi_samples, false );
}

static int A52Probe( demux_t *p_demux, uint64_t *pi_offset )
{
    const char *ppsz_name[] = { "a52", "ac3", NULL };
    const uint16_t rgi_twocc[] = { WAVE_FORMAT_PCM, WAVE_FORMAT_A52, WAVE_FORMAT_UNKNOWN };

    return GenericProbe( p_demux, pi_offset, ppsz_name, A52CheckSyncProbe,
                         VLC_A52_MIN_HEADER_SIZE,
                         1920 + VLC_A52_MIN_HEADER_SIZE + 1,
                         WAV_EXTRA_PROBE_SIZE,
                         true, rgi_twocc, GenericFormatCheck );
}

static int A52Init( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->b_big_endian = false;
    p_sys->i_packet_size = 1024;

    const uint8_t *p_peek;

    /* peek the begining */
    if( vlc_stream_Peek( p_demux->s, &p_peek, VLC_A52_MIN_HEADER_SIZE ) >= VLC_A52_MIN_HEADER_SIZE )
    {
        A52CheckSync( p_peek, &p_sys->b_big_endian, NULL, true );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DTS
 *****************************************************************************/
static int DtsCheckSync( const uint8_t *p_peek, unsigned *pi_samples )
{
    VLC_UNUSED(pi_samples);

    vlc_dts_header_t dts;
    if( vlc_dts_header_Parse( &dts, p_peek, VLC_DTS_HEADER_SIZE ) == VLC_SUCCESS
     && dts.i_frame_size > 0 && dts.i_frame_size <= 8192 )
    {
        if( pi_samples )
            *pi_samples = dts.i_frame_length;
        return dts.i_frame_size;
    }
    else
        return VLC_EGENERIC;
}

static int DtsProbe( demux_t *p_demux, uint64_t *pi_offset )
{
    const char *ppsz_name[] = { "dts", NULL };
    const uint16_t rgi_twocc[] = { WAVE_FORMAT_PCM, WAVE_FORMAT_DTS, WAVE_FORMAT_UNKNOWN };

    return GenericProbe( p_demux, pi_offset, ppsz_name, DtsCheckSync,
                         VLC_DTS_HEADER_SIZE,
                         16384 + VLC_DTS_HEADER_SIZE + 1,
                         WAV_EXTRA_PROBE_SIZE,
                         false, rgi_twocc, NULL );
}
static int DtsInit( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_packet_size = 16384;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * MLP
 *****************************************************************************/
static int MlpCheckSync( const uint8_t *p_peek, unsigned *pi_samples )
{
    if( p_peek[4+0] != 0xf8 || p_peek[4+1] != 0x72 || p_peek[4+2] != 0x6f )
        return -1;

    if( p_peek[4+3] != 0xbb )
        return -1;

    /* TODO checksum and real size for robustness */
    VLC_UNUSED(pi_samples);
    return 0;
}
static int ThdCheckSync( const uint8_t *p_peek, unsigned *pi_samples )
{
    if( p_peek[4+0] != 0xf8 || p_peek[4+1] != 0x72 || p_peek[4+2] != 0x6f )
        return -1;

    if( p_peek[4+3] != 0xba )
        return -1;

    /* TODO checksum and real size for robustness */
    VLC_UNUSED(pi_samples);
    return 0;
}
static int MlpProbe( demux_t *p_demux, uint64_t *pi_offset )
{
    const char *ppsz_name[] = { "mlp", NULL };
    const uint16_t rgi_twocc[] = { WAVE_FORMAT_PCM, WAVE_FORMAT_UNKNOWN };

    return GenericProbe( p_demux, pi_offset, ppsz_name, MlpCheckSync,
                         4+28+16*4, BASE_PROBE_SIZE, WAV_EXTRA_PROBE_SIZE,
                         false, rgi_twocc, GenericFormatCheck );
}
static int ThdProbe( demux_t *p_demux, uint64_t *pi_offset )
{
    const char *ppsz_name[] = { "thd", NULL };
    const uint16_t rgi_twocc[] = { WAVE_FORMAT_PCM, WAVE_FORMAT_UNKNOWN };

    return GenericProbe( p_demux, pi_offset, ppsz_name, ThdCheckSync,
                         4+28+16*4, BASE_PROBE_SIZE, WAV_EXTRA_PROBE_SIZE,
                         false, rgi_twocc, GenericFormatCheck );
}
static int MlpInit( demux_t *p_demux )

{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_packet_size = 4096;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Video
 *****************************************************************************/
static int VideoInit( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_packet_size = 4096;

    return VLC_SUCCESS;
}
