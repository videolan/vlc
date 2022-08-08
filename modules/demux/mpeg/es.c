/*****************************************************************************
 * es.c : Generic audio ES input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
 *               2022 VideoLabs
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
#include "../../packetizer/mpegaudio.h"
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
    add_float( "es-fps", 25, FPS_TEXT, FPS_LONGTEXT )

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

/* Mpga specific */
#define XING_FIELD_STREAMFRAMES    (1 << 0)
#define XING_FIELD_STREAMBYTES     (1 << 1)
#define XING_FIELD_TOC             (1 << 2)
#define XING_FIELD_QUALITY         (1 << 3)
#define XING_MIN_TAG_SIZE           42
#define XING_TOC_COUNTBYTES         100
#define XING_MAX_TAG_SIZE          (XING_MIN_TAG_SIZE + 4 + 4 + XING_TOC_COUNTBYTES + 4 + 2)
#define XING_SUB_ETE_NOK_OTAE       XING_MIN_TAG_SIZE

struct xing_info_s
{
    uint32_t i_frames;
    uint32_t i_bytes;
    uint32_t i_quality;
    vlc_fourcc_t infotag;
    vlc_fourcc_t encoder;
    uint8_t  rgi_toc[XING_TOC_COUNTBYTES];
    float f_peak_signal;
    float f_radio_replay_gain;
    float f_audiophile_replay_gain;
    enum
    {
        XING_MODE_UNKNOWN   = 0,
        XING_MODE_CBR       = 1,
        XING_MODE_ABR       = 2,
        XING_MODE_VBR1      = 3,
        XING_MODE_VBR2      = 4,
        XING_MODE_VBR3      = 5,
        XING_MODE_VBR4      = 6,
        XING_MODE_CBR_2PASS = 8,
        XING_MODE_ABR_2PASS = 9,
    } brmode;
    uint8_t  bitrate_avg;
    uint16_t i_delay_samples;
    uint16_t i_padding_samples;
    uint32_t i_music_length;
};

static int ParseXing( const uint8_t *p_buf, size_t i_buf, struct xing_info_s *xing )
{
    if( i_buf < XING_MIN_TAG_SIZE )
        return VLC_EGENERIC;

    vlc_fourcc_t infotag = VLC_FOURCC(p_buf[0], p_buf[1], p_buf[2], p_buf[3]);

    /* L3Enc VBR info */
    if( infotag == VLC_FOURCC('V','B','R','I') )
    {
        if( GetWBE( &p_buf[4] ) != 0x0001 )
            return VLC_EGENERIC;
        xing->i_bytes = GetDWBE( &p_buf[10] );
        xing->i_frames = GetDWBE( &p_buf[14] );
        xing->infotag = infotag;
        return VLC_SUCCESS;
    }
    /* Xing VBR/CBR tags */
    else if( infotag != VLC_FOURCC('X','i','n','g') &&
             infotag != VLC_FOURCC('I','n','f','o') )
    {
        return VLC_EGENERIC;
    }

    xing->infotag = infotag;

    const uint32_t i_flags = GetDWBE( &p_buf[4] );
    /* compute our variable struct size for early checks */
    const unsigned varsz[4] = { ((i_flags & 0x01) ? 4 : 0),
                                ((i_flags & 0x02) ? 4 : 0),
                                ((i_flags & 0x04) ? XING_TOC_COUNTBYTES : 0),
                                ((i_flags & 0x08) ? 4 : 0) };
    const unsigned i_varallsz = varsz[0] + varsz[1] + varsz[2] + varsz[3];
    const unsigned i_tag_total = XING_MIN_TAG_SIZE + i_varallsz;

    if( i_buf < i_tag_total )
        return VLC_EGENERIC;

    if( i_flags & XING_FIELD_STREAMFRAMES )
        xing->i_frames = GetDWBE( &p_buf[8] );
    if( i_flags & XING_FIELD_STREAMBYTES )
        xing->i_bytes = GetDWBE( &p_buf[8 + varsz[0]] );
    if( i_flags & XING_FIELD_TOC )
        memcpy( xing->rgi_toc, &p_buf[8 + varsz[0] + varsz[1]], XING_TOC_COUNTBYTES );
    if( i_flags & XING_FIELD_QUALITY )
        xing->i_quality = GetDWBE( &p_buf[8 + varsz[0] + varsz[1] + varsz[2]] );

    /* pointer past optional members */
    const uint8_t *p_fixed = &p_buf[8 + i_varallsz];

    /* Original Xing encoder header stops here */

    xing->encoder = VLC_FOURCC(p_fixed[0], p_fixed[1], p_fixed[2], p_fixed[3]); /* char version[9] start */

    if( xing->encoder != VLC_FOURCC('L','A','M','E') &&
        xing->encoder != VLC_FOURCC('L','a','v','c') &&
        xing->encoder != VLC_FOURCC('L','a','v','f') )
        return VLC_SUCCESS;

    xing->brmode  = p_fixed[8] & 0x0f; /* version upper / mode lower */
    uint32_t peak_signal  = GetDWBE( &p_fixed[11] );
    xing->f_peak_signal = peak_signal / 8388608.0; /* pow(2, 23) */
    uint16_t gain = GetWBE( &p_fixed[15] );
    xing->f_radio_replay_gain = (gain & 0x1FF) / /* 9bits val stored x10 */
                                ((gain & 0x200) ? -10.0 : 10.0); /* -sign bit on bit 6 */
    gain = GetWBE( &p_fixed[17] );
    xing->f_radio_replay_gain = (gain & 0x1FF) / /* 9bits val stored x10 */
                                ((gain & 0x200) ? -10.0 : 10.0); /* -sign bit on bit 6 */
    /* flags @19 */
    xing->bitrate_avg = (p_fixed[20] != 0xFF) ? p_fixed[20] : 0; /* clipped to 255, so it's unknown from there */
    xing->i_delay_samples = (p_fixed[21] << 4) | (p_fixed[22] >> 4); /* upper 12bits */
    xing->i_padding_samples = ((p_fixed[22] & 0x0F) << 8) | p_fixed[23]; /* lower 12bits */
    xing->i_music_length  = GetDWBE( &p_fixed[28] );

    return VLC_SUCCESS;
}

static int MpgaGetCBRSeekpoint( const struct mpga_frameheader_s *mpgah,
                                const struct xing_info_s *xing,
                                uint64_t i_seekablesize,
                                double f_pos, vlc_tick_t *pi_time, uint64_t *pi_offset )
{
    if( xing->infotag != 0 &&
        xing->infotag != VLC_FOURCC('I','n','f','o') &&
        xing->brmode != XING_MODE_CBR &&
        xing->brmode != XING_MODE_CBR_2PASS )
        return -1;

    if( !mpgah->i_sample_rate || !mpgah->i_samples_per_frame || !mpgah->i_frame_size )
        return -1;

    uint32_t i_total_frames = xing->i_frames ? xing->i_frames
                                             : i_seekablesize / mpgah->i_frame_size;

    /* xing must be optional here */
    uint32_t i_frame = i_total_frames * f_pos;
    *pi_offset = i_frame * mpgah->i_frame_size;
    *pi_time = vlc_tick_from_samples( i_frame * mpgah->i_samples_per_frame,
                                      mpgah->i_sample_rate ) + VLC_TICK_0;

    return 0;
}

static int SeekByPosUsingXingTOC( const struct mpga_frameheader_s *mpgah,
                                  const struct xing_info_s *xing,double pos,
                                  vlc_tick_t *pi_time, uint64_t *pi_offset )
{
    if( !xing->rgi_toc[XING_TOC_COUNTBYTES - 1] ||
        !mpgah->i_sample_rate || !mpgah->i_samples_per_frame )
        return -1;

    unsigned syncentry = pos * XING_TOC_COUNTBYTES;
    syncentry = VLC_CLIP(syncentry, 0, XING_TOC_COUNTBYTES - 1);
    unsigned syncframe = syncentry * xing->i_frames / XING_TOC_COUNTBYTES;

    *pi_time = vlc_tick_from_samples( syncframe * mpgah->i_samples_per_frame,
                                      mpgah->i_sample_rate ) + VLC_TICK_0;
    *pi_offset = xing->rgi_toc[syncentry] * xing->i_bytes / 256;

    return 0;
}

static int SeekByTimeUsingXingTOC( const struct mpga_frameheader_s *mpgah,
                                   const struct xing_info_s *xing,
                                   vlc_tick_t *pi_time, uint64_t *pi_offset )
{
    if( !mpgah->i_sample_rate || !mpgah->i_samples_per_frame || !xing->i_frames ||
        *pi_time < VLC_TICK_0 )
        return -1;

    uint32_t sample = samples_from_vlc_tick( *pi_time - VLC_TICK_0, mpgah->i_sample_rate );
    uint64_t totalsamples = mpgah->i_samples_per_frame * (uint64_t) xing->i_frames;

    return SeekByPosUsingXingTOC( mpgah, xing, (double)sample / totalsamples,
                                  pi_time, pi_offset );
}

static int MpgaSeek( const struct mpga_frameheader_s *mpgah,
                     const struct xing_info_s *xing,
                     uint64_t i_seekablesize, double f_pos,
                     vlc_tick_t *pi_time, uint64_t *pi_offset )
{
    if( f_pos >= 0 ) /* Set Pos */
    {
        if( !MpgaGetCBRSeekpoint( mpgah, xing, i_seekablesize,
                                  f_pos, pi_time, pi_offset ) )
        {
             return 0;
        }
    }

    if( *pi_time != VLC_TICK_INVALID &&
        !SeekByTimeUsingXingTOC( mpgah, xing, pi_time, pi_offset ) )
        return 0;

    return SeekByPosUsingXingTOC( mpgah, xing, f_pos, pi_time, pi_offset );
}

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
    uint64_t    i_bytes;

    bool        b_big_endian;
    bool        b_estimate_bitrate;
    unsigned    i_bitrate;  /* extracted from Xing header */
    vlc_tick_t  i_duration;

    bool b_initial_sync_failed;

    unsigned i_packet_size;

    uint64_t i_stream_offset;
    unsigned i_demux_flags;

    float   f_fps;

    /* Mpga specific */
    struct mpga_frameheader_s mpgah;
    struct xing_info_s xing;

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
static int SeekByMlltTable( sync_table_t *, vlc_tick_t *, uint64_t * );

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
    p_sys->i_bitrate = 0;
    p_sys->i_duration = 0;
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

    if( vlc_stream_Seek( p_demux->s, p_sys->i_stream_offset ) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "detected format %4.4s", (const char*)&p_sys->codec.i_codec );

    /* Load the audio packetizer */
    es_format_Init( &fmt, i_cat, p_sys->codec.i_codec );
    fmt.i_original_fourcc = p_sys->i_original;
    p_sys->p_packetizer = demux_PacketizerNew( VLC_OBJECT(p_demux), &fmt, p_sys->codec.psz_name );
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
            p_sys->i_bitrate = 8 * CLOCK_FREQ * p_sys->i_bytes
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
static void PostSeekCleanup( demux_sys_t *p_sys, vlc_tick_t i_time )
{
    /* Fix time_offset */
    if( i_time >= 0 )
        p_sys->i_time_offset = i_time - p_sys->i_pts;
    /* And reset buffered data */
    if( p_sys->p_packetized_data )
        block_ChainRelease( p_sys->p_packetized_data );
    p_sys->p_packetized_data = NULL;
    /* Reset chapter if any */
    p_sys->chapters.i_current = 0;
    p_sys->i_demux_flags |= INPUT_UPDATE_SEEKPOINT;
}

static int MovetoTimePos( demux_t *p_demux, vlc_tick_t i_time, uint64_t i_pos )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    int i_ret = vlc_stream_Seek( p_demux->s, p_sys->i_stream_offset + i_pos );
    if( i_ret != VLC_SUCCESS )
        return i_ret;
    PostSeekCleanup( p_sys, i_time );
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
            if( p_sys->i_duration > 0 )
            {
                *va_arg( args, vlc_tick_t * ) = p_sys->i_duration;
                return VLC_SUCCESS;
            }
            /* compute length from bitrate */
            va_list ap;
            int i_ret;

            va_copy ( ap, args );
            i_ret = demux_vaControlHelper( p_demux->s, p_sys->i_stream_offset,
                                    -1, p_sys->i_bitrate, 1, i_query, ap );
            va_end( ap );

            /* No bitrate, we can't have it precisely, but we can compute
             * a raw approximation with time/position */
            if( i_ret && !p_sys->i_bitrate )
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
        case DEMUX_SET_POSITION:
        {
            vlc_tick_t i_time;
            double f_pos;
            uint64_t i_offset;

            va_list ap;
            va_copy ( ap, args ); /* don't break args for helper fallback */
            if( i_query == DEMUX_SET_TIME )
            {
                i_time = va_arg(ap, vlc_tick_t);
                f_pos = p_sys->i_duration ? i_time / (double) p_sys->i_duration : -1.0;
                if( f_pos > 1.0 )
                    f_pos = 1.0;
            }
            else
            {
                f_pos = va_arg(ap, double);
                i_time = p_sys->i_duration ? p_sys->i_duration * f_pos : VLC_TICK_INVALID;
            }
            va_end( ap );

            /* Try to use ID3 table */
            if( !SeekByMlltTable( &p_sys->mllt, &i_time, &i_offset ) )
                return MovetoTimePos( p_demux, i_time, i_offset );

            if( p_sys->codec.i_codec == VLC_CODEC_MPGA )
            {
                uint64_t streamsize;
                if( !vlc_stream_GetSize( p_demux->s, &streamsize ) &&
                    streamsize > p_sys->i_stream_offset )
                    streamsize -= p_sys->i_stream_offset;
                else
                    streamsize = 0;

                if( !MpgaSeek( &p_sys->mpgah, &p_sys->xing,
                           streamsize, f_pos, &i_time, &i_offset ) )
                {
                    return MovetoTimePos( p_demux, i_time, i_offset );
                }
            }

            /* fallback on bitrate / file position seeking */
            i_time = VLC_TICK_INVALID;
            int ret = demux_vaControlHelper( p_demux->s, p_sys->i_stream_offset, -1,
                                             p_sys->i_bitrate, 1, i_query, args );
            if( ret != VLC_SUCCESS )
                return ret; /* not much we can do */

            if( p_sys->i_bitrate > 0 )
            {
                i_offset = vlc_stream_Tell( p_demux->s ); /* new pos */
                i_time = VLC_TICK_0;
                if( likely(i_offset > p_sys->i_stream_offset) )
                {
                    i_offset -= p_sys->i_stream_offset;
                    i_time += vlc_tick_from_samples( i_offset * 8, p_sys->i_bitrate );
                }
            }
            PostSeekCleanup( p_sys, i_time );

            /* FIXME TODO: implement a high precision seek (with mp3 parsing)
             * needed for multi-input */
            return VLC_SUCCESS;
        }

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

    return demux_vaControlHelper( p_demux->s, p_sys->i_stream_offset, -1,
                                  p_sys->i_bitrate, 1, i_query, args );
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
                    p_sys->mpgah.i_samples_per_frame )
                {
                    p_sys->i_bitrate = p_sys->xing.i_bytes * INT64_C(8) *
                        p_sys->p_packetizer->fmt_out.audio.i_rate /
                        p_sys->xing.i_frames / p_sys->mpgah.i_samples_per_frame;

                    if( p_sys->i_bitrate > 0 )
                        p_sys->b_estimate_bitrate = false;
                }
                /* Use the bitrate as initual value */
                if( p_sys->b_estimate_bitrate )
                    p_sys->i_bitrate = p_sys->p_packetizer->fmt_out.i_bitrate;
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

    /* peek the beginning
     * It is common that wav files have some sort of garbage at the beginning
     * We will accept probing 0.5s of data in this case.
     */
    const size_t i_probe = i_skip + i_check_size + i_base_probing + ( b_wav ? i_wav_extra_probing : 0);
    const ssize_t i_peek = vlc_stream_Peek( p_demux->s, &p_peek, i_probe );

    if( i_peek < 0 || (size_t)i_peek < i_skip + i_check_size )
        return VLC_EGENERIC;

    for( ;; )
    {
        /* i_peek > 0 so we can cast into size_t. */
        if( i_skip + i_check_size > (size_t)i_peek )
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

                if( i_skip + i_check_size + i_size <= (size_t)i_peek )
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
        || (((h >> 10) & 0x03) == 0x03 )    /* valid sampling freq ? */
        || ((h & 0x03) == 0x02 ))           /* valid emphasis ? */
    {
        return false;
    }
    return true;
}

#define MPGA_VERSION( h )   ( 1 - (((h)>>19)&0x01) )
#define MPGA_MODE(h)        (((h)>> 6)&0x03)

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

static int SeekByMlltTable( sync_table_t *mllt, vlc_tick_t *pi_time, uint64_t *pi_offset )
{
    if( !mllt->p_bits )
        return -1;

    sync_table_ctx_t *p_cur = &mllt->current;

    /* reset or init context */
    if( *pi_time < p_cur->i_time || !p_cur->br.p )
    {
        p_cur->i_time = 0;
        p_cur->i_pos = 0;
        bs_init(&p_cur->br, mllt->p_bits, mllt->i_bits);
    }

    while(!bs_eof(&p_cur->br))
    {
        const uint32_t i_bytesdev = bs_read(&p_cur->br, mllt->i_bits_per_bytes_dev);
        const uint32_t i_msdev = bs_read(&p_cur->br, mllt->i_bits_per_ms_dev);
        if(bs_error(&p_cur->br))
            break;
        const vlc_tick_t i_deltatime = VLC_TICK_FROM_MS(mllt->i_ms_btw_refs + i_msdev);
        if( p_cur->i_time + i_deltatime > *pi_time )
            break;
        p_cur->i_time += i_deltatime;
        p_cur->i_pos += mllt->i_bytes_btw_refs + i_bytesdev;
    }
    *pi_time = p_cur->i_time;
    *pi_offset = p_cur->i_pos;

    return 0;
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
                                *pf = vlc_atof_c( psz_val );
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
    if( !MpgaCheckSync( p_peek ) || mpga_decode_frameheader( header, &p_sys->mpgah ) )
        return VLC_SUCCESS;

    /* Xing header */
    int i_skip;

    if( MPGA_VERSION( header ) == 0 )
        i_skip = MPGA_MODE( header ) != 3 ? 36 : 21;
    else
        i_skip = MPGA_MODE( header ) != 3 ? 21 : 13;

    if( i_skip >= i_peek )
        return VLC_SUCCESS;

    struct xing_info_s *xing = &p_sys->xing;
    if( ParseXing( &p_peek[i_skip], i_peek - i_skip, xing ) == VLC_SUCCESS )
    {
        const uint64_t i_total_samples = xing->i_frames * (uint64_t) p_sys->mpgah.i_samples_per_frame;
        const uint64_t i_dropped_samples = xing->i_delay_samples + xing->i_padding_samples;

        if( p_sys->mpgah.i_sample_rate && i_dropped_samples < i_total_samples )
        {
            uint64_t i_stream_size;
            /* We only set duration if we can't check (then compute it using bitrate/size)
               or if we verified the file isn't truncated */
            if( xing->i_bytes == 0 ||
                vlc_stream_GetSize( p_demux->s, &i_stream_size ) ||
                i_stream_size >= xing->i_bytes + p_sys->mpgah.i_frame_size )
            {
                p_sys->i_duration = vlc_tick_from_samples( i_total_samples - i_dropped_samples,
                                                           p_sys->mpgah.i_sample_rate );
            }
        }

        p_sys->rgf_replay_peak[AUDIO_REPLAY_GAIN_TRACK] = xing->f_peak_signal;
        p_sys->rgf_replay_gain[AUDIO_REPLAY_GAIN_TRACK] = xing->f_radio_replay_gain;
        p_sys->rgf_replay_gain[AUDIO_REPLAY_GAIN_ALBUM] = xing->f_audiophile_replay_gain;

        msg_Dbg( p_demux, "Using '%4.4s' infotag"
                          "(%"PRIu32" bytes, %"PRIu32" frames, %u samples/frame)",
                          (char *) &xing->infotag,
                          xing->i_bytes, xing->i_frames,
                          p_sys->mpgah.i_samples_per_frame );

        /* We'll need to skip that part for playback
         * and avoid using it as container frame could be different rate/size */
        p_sys->i_stream_offset += p_sys->mpgah.i_frame_size;
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

    /* peek the beginning (10 is for adts header) */
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

    /* peek the beginning */
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
    const uint16_t rgi_twocc[] = { WAVE_FORMAT_PCM, WAVE_FORMAT_DTSINC_DTS, WAVE_FORMAT_UNKNOWN };

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
