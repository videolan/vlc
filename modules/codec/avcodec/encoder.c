/*****************************************************************************
 * encoder.c: video and audio encoder using the libavcodec library
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 * Part of the file Copyright (C) FFmpeg Project Developers
 * (mpeg4_default matrixes)
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

#include <math.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_sout.h>
#include <vlc_codec.h>
#include <vlc_dialog.h>
#include <vlc_avcodec.h>
#include <vlc_cpu.h>

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>

#include "avcodec.h"
#include "avcommon.h"

#include <libavutil/channel_layout.h>

#define HURRY_UP_GUARD1 VLC_TICK_FROM_MS(450)
#define HURRY_UP_GUARD2 VLC_TICK_FROM_MS(300)
#define HURRY_UP_GUARD3 VLC_TICK_FROM_MS(100)

#define MAX_FRAME_DELAY (FF_MAX_B_FRAMES + 2)

#define RAW_AUDIO_FRAME_SIZE (2048)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *EncodeVideo( encoder_t *, picture_t * );
static block_t *EncodeAudio( encoder_t *, block_t * );

struct thread_context_t;

/*****************************************************************************
 * thread_context_t : for multithreaded encoding
 *****************************************************************************/
struct thread_context_t
{
    struct vlc_object_t obj;

    AVCodecContext  *p_context;
    int             (* pf_func)(AVCodecContext *c, void *arg);
    void            *arg;
    int             i_ret;

    vlc_mutex_t     lock;
    vlc_cond_t      cond;
    bool            b_work, b_done;
};

/*****************************************************************************
 * encoder_sys_t : libavcodec encoder descriptor
 *****************************************************************************/
typedef struct
{
    /*
     * libavcodec properties
     */
    AVCodec         *p_codec;
    AVCodecContext  *p_context;

    /*
     * Common buffer mainly for audio as frame size in there needs usually be constant
     */
    uint8_t *p_buffer;
    size_t i_buffer_out;
    uint8_t *p_interleave_buf;

    /*
     * Video properties
     */
    vlc_tick_t i_last_ref_pts;
    vlc_tick_t i_buggy_pts_detect;
    vlc_tick_t i_last_pts;
    bool    b_inited;

    /*
     * Audio properties
     */
    size_t i_sample_bytes;
    size_t i_frame_size;
    size_t i_samples_delay; //How much samples in delay buffer
    bool b_planar;
    bool b_variable;    //Encoder can be fed with any size frames not just frame_size
    vlc_tick_t i_pts;
    date_t  buffer_date;

    /* Multichannel (>2) channel reordering */
    uint8_t    i_channels_to_reorder;
    uint8_t    pi_reorder_layout[AOUT_CHAN_MAX];

    /* Encoding settings */
    int        i_key_int;
    int        i_b_frames;
    int        i_vtolerance;
    int        i_qmin;
    int        i_qmax;
    int        i_hq;
    int        i_rc_buffer_size;
    float      f_rc_buffer_aggressivity;
    bool       b_pre_me;
    bool       b_hurry_up;
    bool       b_interlace, b_interlace_me;
    float      f_i_quant_factor;
    bool       b_mpeg4_matrix;
    bool       b_trellis;
    int        i_quality; /* for VBR */
    float      f_lumi_masking, f_dark_masking, f_p_masking, f_border_masking;
    int        i_aac_profile; /* AAC profile to use.*/

    AVFrame    *frame;
} encoder_sys_t;


/* Taken from audio.c*/
static const uint64_t pi_channels_map[][2] =
{
    { AV_CH_FRONT_LEFT,        AOUT_CHAN_LEFT },
    { AV_CH_FRONT_RIGHT,       AOUT_CHAN_RIGHT },
    { AV_CH_FRONT_CENTER,      AOUT_CHAN_CENTER },
    { AV_CH_LOW_FREQUENCY,     AOUT_CHAN_LFE },
    { AV_CH_BACK_LEFT,         AOUT_CHAN_REARLEFT },
    { AV_CH_BACK_RIGHT,        AOUT_CHAN_REARRIGHT },
    { AV_CH_FRONT_LEFT_OF_CENTER, 0 },
    { AV_CH_FRONT_RIGHT_OF_CENTER, 0 },
    { AV_CH_BACK_CENTER,       AOUT_CHAN_REARCENTER },
    { AV_CH_SIDE_LEFT,         AOUT_CHAN_MIDDLELEFT },
    { AV_CH_SIDE_RIGHT,        AOUT_CHAN_MIDDLERIGHT },
    { AV_CH_TOP_CENTER,        0 },
    { AV_CH_TOP_FRONT_LEFT,    0 },
    { AV_CH_TOP_FRONT_CENTER,  0 },
    { AV_CH_TOP_FRONT_RIGHT,   0 },
    { AV_CH_TOP_BACK_LEFT,     0 },
    { AV_CH_TOP_BACK_CENTER,   0 },
    { AV_CH_TOP_BACK_RIGHT,    0 },
    { AV_CH_STEREO_LEFT,       0 },
    { AV_CH_STEREO_RIGHT,      0 },
};

static const uint32_t channel_mask[][2] = {
    {0,0},
    {AOUT_CHAN_CENTER, AV_CH_LAYOUT_MONO},
    {AOUT_CHANS_STEREO, AV_CH_LAYOUT_STEREO},
    {AOUT_CHANS_2_1, AV_CH_LAYOUT_2POINT1},
    {AOUT_CHANS_4_0, AV_CH_LAYOUT_4POINT0},
    {AOUT_CHANS_4_1, AV_CH_LAYOUT_4POINT1},
    {AOUT_CHANS_5_1, AV_CH_LAYOUT_5POINT1_BACK},
    {AOUT_CHANS_7_0, AV_CH_LAYOUT_7POINT0},
    {AOUT_CHANS_7_1, AV_CH_LAYOUT_7POINT1},
    {AOUT_CHANS_8_1, AV_CH_LAYOUT_OCTAGONAL},
};

static const char *const ppsz_enc_options[] = {
    "keyint", "bframes", "vt", "qmin", "qmax", "codec", "hq",
    "rc-buffer-size", "rc-buffer-aggressivity", "pre-me", "hurry-up",
    "interlace", "interlace-me", "i-quant-factor", "noise-reduction", "mpeg4-matrix",
    "trellis", "qscale", "strict", "lumi-masking", "dark-masking",
    "p-masking", "border-masking",
    "aac-profile", "options",
    NULL
};

static const uint16_t mpa_bitrate_tab[2][15] =
{
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
};

static const uint16_t mpa_freq_tab[6] =
{ 44100, 48000, 32000, 22050, 24000, 16000 };

static const uint16_t mpeg4_default_intra_matrix[64] = {
  8, 17, 18, 19, 21, 23, 25, 27,
 17, 18, 19, 21, 23, 25, 27, 28,
 20, 21, 22, 23, 24, 26, 28, 30,
 21, 22, 23, 24, 26, 28, 30, 32,
 22, 23, 24, 26, 28, 30, 32, 35,
 23, 24, 26, 28, 30, 32, 35, 38,
 25, 26, 28, 30, 32, 35, 38, 41,
 27, 28, 30, 32, 35, 38, 41, 45,
};

static const uint16_t mpeg4_default_non_intra_matrix[64] = {
 16, 17, 18, 19, 20, 21, 22, 23,
 17, 18, 19, 20, 21, 22, 23, 24,
 18, 19, 20, 21, 22, 23, 24, 25,
 19, 20, 21, 22, 23, 24, 26, 27,
 20, 21, 22, 23, 25, 26, 27, 28,
 21, 22, 23, 24, 26, 27, 28, 30,
 22, 23, 24, 26, 27, 28, 30, 31,
 23, 24, 25, 27, 28, 30, 31, 33,
};

static const int DEFAULT_ALIGN = 0;


/*****************************************************************************
 * InitVideoEnc: probe the encoder
 *****************************************************************************/
static void probe_video_frame_rate( encoder_t *p_enc, AVCodecContext *p_context, AVCodec *p_codec )
{
    /* if we don't have i_frame_rate_base, we are probing and just checking if we can find codec
     * so set fps to requested fps if asked by user or input fps is availabled */
    p_context->time_base.num = p_enc->fmt_in.video.i_frame_rate_base ? p_enc->fmt_in.video.i_frame_rate_base : 1;

    // MP4V doesn't like CLOCK_FREQ denominator in time_base, so use 1/25 as default for that
    p_context->time_base.den = p_enc->fmt_in.video.i_frame_rate_base ? p_enc->fmt_in.video.i_frame_rate :
                                  ( p_enc->fmt_out.i_codec == VLC_CODEC_MP4V ? 25 : CLOCK_FREQ );

    msg_Dbg( p_enc, "Time base for probing set to %d/%d", p_context->time_base.num, p_context->time_base.den );
    if( p_codec->supported_framerates )
    {
        /* We are finding fps values so 1/time_base */
        AVRational target = {
            .num = p_context->time_base.den,
            .den = p_context->time_base.num
        };
        int idx = av_find_nearest_q_idx(target, p_codec->supported_framerates);

        p_context->time_base.num = p_codec->supported_framerates[idx].den ?
                                    p_codec->supported_framerates[idx].den : 1;
        p_context->time_base.den = p_codec->supported_framerates[idx].den ?
                                    p_codec->supported_framerates[idx].num : CLOCK_FREQ;

        /* If we have something reasonable on supported framerates, use that*/
        if( p_context->time_base.den && p_context->time_base.den < CLOCK_FREQ )
        {
            p_enc->fmt_out.video.i_frame_rate_base =
                p_enc->fmt_in.video.i_frame_rate_base =
                p_context->time_base.num;
            p_enc->fmt_out.video.i_frame_rate =
                p_enc->fmt_in.video.i_frame_rate =
                p_context->time_base.den;
        }
    }
    msg_Dbg( p_enc, "Time base set to %d/%d", p_context->time_base.num, p_context->time_base.den );
}

static void add_av_option_int( encoder_t *p_enc, AVDictionary** pp_dict, const char* psz_name, int i_value )
{
    char buff[32];
    if ( snprintf( buff, sizeof(buff), "%d", i_value ) < 0 )
        return;
    if( av_dict_set( pp_dict, psz_name, buff, 0 ) < 0 )
        msg_Warn( p_enc, "Failed to set encoder option %s", psz_name );
}

static void add_av_option_float( encoder_t *p_enc, AVDictionary** pp_dict, const char* psz_name, float f_value )
{
    char buff[128];
    if ( snprintf( buff, sizeof(buff), "%f", f_value ) < 0 )
        return;
    if( av_dict_set( pp_dict, psz_name, buff, 0 ) < 0 )
        msg_Warn( p_enc, "Failed to set encoder option %s", psz_name );
}

int InitVideoEnc( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    AVCodecContext *p_context;
    AVCodec *p_codec = NULL;
    unsigned i_codec_id;
    const char *psz_namecodec;
    float f_val;
    char *psz_val;

    msg_Dbg( p_this, "using %s %s", AVPROVIDER(LIBAVCODEC), LIBAVCODEC_IDENT );

    /* Initialization must be done before avcodec_find_encoder() */
    vlc_init_avcodec(p_this);

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    switch( p_enc->fmt_out.i_cat )
    {
        case VIDEO_ES:
            if( p_enc->fmt_out.i_codec == VLC_CODEC_MP1V )
            {
                i_codec_id = AV_CODEC_ID_MPEG1VIDEO;
                psz_namecodec = "MPEG-1 video";
                break;
            }
            if( GetFfmpegCodec( VIDEO_ES, p_enc->fmt_out.i_codec, &i_codec_id,
                                &psz_namecodec ) )
                break;
            if( FindFfmpegChroma( p_enc->fmt_out.i_codec ) != AV_PIX_FMT_NONE )
            {
                i_codec_id = AV_CODEC_ID_RAWVIDEO;
                psz_namecodec = "Raw video";
                break;
            }
            return VLC_EGENERIC;

        case AUDIO_ES:
            if( GetFfmpegCodec( AUDIO_ES, p_enc->fmt_out.i_codec, &i_codec_id,
                                &psz_namecodec ) )
                break;
            /* fall through */
        default:
            /* We don't support subtitle encoding */
            return VLC_EGENERIC;
    }

    char *psz_encoder = var_GetString( p_this, ENC_CFG_PREFIX "codec" );
    if( psz_encoder && *psz_encoder )
    {
        p_codec = avcodec_find_encoder_by_name( psz_encoder );
        if( !p_codec )
        {
            msg_Err( p_this, "Encoder `%s' not found", psz_encoder );
            free( psz_encoder );
            return VLC_EGENERIC;
        }
        else if( p_codec->id != i_codec_id )
        {
            msg_Err( p_this, "Encoder `%s' can't handle %4.4s",
                    psz_encoder, (char*)&p_enc->fmt_out.i_codec );
            free( psz_encoder );
            return VLC_EGENERIC;
        }
    }
    free( psz_encoder );
    if( !p_codec )
        p_codec = avcodec_find_encoder( i_codec_id );
    if( !p_codec )
    {
        msg_Err( p_enc, "cannot find encoder %s\n"
"*** Your Libav/FFmpeg installation is crippled.   ***\n"
"*** Please check with your Libav/FFmpeg packager. ***\n"
"*** This is NOT a VLC media player issue.   ***", psz_namecodec );

#if !defined(_WIN32)
        vlc_dialog_display_error( p_enc, _("Streaming / Transcoding failed"), _(
/* I have had enough of all these MPEG-3 transcoding bug reports.
 * Downstream packager, you had better not patch this out, or I will be really
 * annoyed. Think about it - you don't want to fork the VLC translation files,
 * do you? -- Courmisch, 2008-10-22 */
"It seems your Libav/FFmpeg (libavcodec) installation lacks the following encoder:\n"
"%s.\n"
"If you don't know how to fix this, ask for support from your distribution.\n"
"\n"
"This is not an error inside VLC media player.\n"
"Do not contact the VideoLAN project about this issue.\n"),
            psz_namecodec );
#endif

        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the encoder's structure */
    if( ( p_sys = calloc( 1, sizeof(encoder_sys_t) ) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;
    p_sys->i_samples_delay = 0;
    p_sys->p_codec = p_codec;
    p_sys->b_planar = false;

    p_sys->p_buffer = NULL;
    p_sys->p_interleave_buf = NULL;
    p_sys->i_buffer_out = 0;

    p_context = avcodec_alloc_context3(p_codec);
    if( unlikely(p_context == NULL) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_sys->p_context = p_context;
    p_sys->p_context->codec_id = p_sys->p_codec->id;
    p_context->thread_type = 0;
    p_context->debug = var_InheritInteger( p_enc, "avcodec-debug" );
    p_context->opaque = (void *)p_this;

    p_sys->i_key_int = var_GetInteger( p_enc, ENC_CFG_PREFIX "keyint" );
    p_sys->i_b_frames = var_GetInteger( p_enc, ENC_CFG_PREFIX "bframes" );
    p_sys->i_vtolerance = var_GetInteger( p_enc, ENC_CFG_PREFIX "vt" ) * 1000;
    p_sys->b_interlace = var_GetBool( p_enc, ENC_CFG_PREFIX "interlace" );
    p_sys->b_interlace_me = var_GetBool( p_enc, ENC_CFG_PREFIX "interlace-me" );
    p_sys->b_pre_me = var_GetBool( p_enc, ENC_CFG_PREFIX "pre-me" );
    p_sys->b_hurry_up = var_GetBool( p_enc, ENC_CFG_PREFIX "hurry-up" );

    p_sys->i_rc_buffer_size = var_GetInteger( p_enc, ENC_CFG_PREFIX "rc-buffer-size" );
    p_sys->f_rc_buffer_aggressivity = var_GetFloat( p_enc, ENC_CFG_PREFIX "rc-buffer-aggressivity" );
    p_sys->f_i_quant_factor = var_GetFloat( p_enc, ENC_CFG_PREFIX "i-quant-factor" );
    p_sys->b_mpeg4_matrix = var_GetBool( p_enc, ENC_CFG_PREFIX "mpeg4-matrix" );

    f_val = var_GetFloat( p_enc, ENC_CFG_PREFIX "qscale" );

    p_sys->i_quality = 0;
    if( f_val < .01f || f_val > 255.f )
        f_val = 0.f;
    else
        p_sys->i_quality = lroundf(FF_QP2LAMBDA * f_val);

    psz_val = var_GetString( p_enc, ENC_CFG_PREFIX "hq" );
    p_sys->i_hq = FF_MB_DECISION_RD;
    if( psz_val && *psz_val )
    {
        if( !strcmp( psz_val, "rd" ) )
            p_sys->i_hq = FF_MB_DECISION_RD;
        else if( !strcmp( psz_val, "bits" ) )
            p_sys->i_hq = FF_MB_DECISION_BITS;
        else if( !strcmp( psz_val, "simple" ) )
            p_sys->i_hq = FF_MB_DECISION_SIMPLE;
        else
            p_sys->i_hq = FF_MB_DECISION_RD;
    }
    else
        p_sys->i_hq = FF_MB_DECISION_RD;
    free( psz_val );

    p_sys->i_qmin = var_GetInteger( p_enc, ENC_CFG_PREFIX "qmin" );
    p_sys->i_qmax = var_GetInteger( p_enc, ENC_CFG_PREFIX "qmax" );
    p_sys->b_trellis = var_GetBool( p_enc, ENC_CFG_PREFIX "trellis" );

    p_context->strict_std_compliance = var_GetInteger( p_enc, ENC_CFG_PREFIX "strict" );

    p_sys->f_lumi_masking = var_GetFloat( p_enc, ENC_CFG_PREFIX "lumi-masking" );
    p_sys->f_dark_masking = var_GetFloat( p_enc, ENC_CFG_PREFIX "dark-masking" );
    p_sys->f_p_masking = var_GetFloat( p_enc, ENC_CFG_PREFIX "p-masking" );
    p_sys->f_border_masking = var_GetFloat( p_enc, ENC_CFG_PREFIX "border-masking" );

    psz_val = var_GetString( p_enc, ENC_CFG_PREFIX "aac-profile" );
    /* libavcodec uses faac encoder atm, and it has issues with
     * other than low-complexity profile, so default to that */
    p_sys->i_aac_profile = FF_PROFILE_AAC_LOW;
    if( psz_val && *psz_val )
    {
        if( !strncmp( psz_val, "main", 4 ) )
            p_sys->i_aac_profile = FF_PROFILE_AAC_MAIN;
        else if( !strncmp( psz_val, "low", 3 ) )
            p_sys->i_aac_profile = FF_PROFILE_AAC_LOW;
        else if( !strncmp( psz_val, "ssr", 3 ) )
            p_sys->i_aac_profile = FF_PROFILE_AAC_SSR;
        else if( !strncmp( psz_val, "ltp", 3 ) )
            p_sys->i_aac_profile = FF_PROFILE_AAC_LTP;
/* These require libavcodec with libfdk-aac */
        else if( !strncmp( psz_val, "hev2", 4 ) )
            p_sys->i_aac_profile = FF_PROFILE_AAC_HE_V2;
        else if( !strncmp( psz_val, "hev1", 4 ) )
            p_sys->i_aac_profile = FF_PROFILE_AAC_HE;
        else if( !strncmp( psz_val, "ld", 2 ) )
            p_sys->i_aac_profile = FF_PROFILE_AAC_LD;
        else if( !strncmp( psz_val, "eld", 3 ) )
            p_sys->i_aac_profile = FF_PROFILE_AAC_ELD;
        else
        {
            msg_Warn( p_enc, "unknown AAC profile requested, setting it to low" );
            p_sys->i_aac_profile = FF_PROFILE_AAC_LOW;
        }
    }
    free( psz_val );
    AVDictionary *options = NULL;

    if( p_enc->fmt_in.i_cat == VIDEO_ES )
    {
        if( !p_enc->fmt_in.video.i_visible_width || !p_enc->fmt_in.video.i_visible_height )
        {
            msg_Warn( p_enc, "invalid size %ix%i", p_enc->fmt_in.video.i_visible_width,
                      p_enc->fmt_in.video.i_visible_height );
            avcodec_free_context( &p_context );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_context->codec_type = AVMEDIA_TYPE_VIDEO;

        p_context->coded_width = p_enc->fmt_in.video.i_width;
        p_context->coded_height = p_enc->fmt_in.video.i_height;
        p_context->width = p_enc->fmt_in.video.i_visible_width;
        p_context->height = p_enc->fmt_in.video.i_visible_height;

        probe_video_frame_rate( p_enc, p_context, p_codec );
        set_video_color_settings( &p_enc->fmt_in.video, p_context );

        /* Defaults from ffmpeg.c */
        p_context->qblur = 0.5;
        p_context->qcompress = 0.5;
        p_context->b_quant_offset = 1.25;
        p_context->b_quant_factor = 1.25;
        p_context->i_quant_offset = 0.0;
        p_context->i_quant_factor = -0.8;

        p_context->lumi_masking = p_sys->f_lumi_masking;
        p_context->dark_masking = p_sys->f_dark_masking;
        p_context->p_masking = p_sys->f_p_masking;
        add_av_option_float( p_enc, &options, "border_mask", p_sys->f_border_masking );

        if( p_sys->i_key_int > 0 )
            p_context->gop_size = p_sys->i_key_int;
        p_context->max_b_frames =
            VLC_CLIP( p_sys->i_b_frames, 0, FF_MAX_B_FRAMES );
        if( !p_context->max_b_frames  &&
            (  p_enc->fmt_out.i_codec == VLC_CODEC_MPGV ||
               p_enc->fmt_out.i_codec == VLC_CODEC_MP2V ) )
            p_context->flags |= AV_CODEC_FLAG_LOW_DELAY;

        av_reduce( &p_context->sample_aspect_ratio.num,
                   &p_context->sample_aspect_ratio.den,
                   p_enc->fmt_in.video.i_sar_num,
                   p_enc->fmt_in.video.i_sar_den, 1 << 30 );


        p_enc->fmt_in.i_codec = VLC_CODEC_I420;

        /* Very few application support YUV in TIFF, not even VLC */
        if( p_enc->fmt_out.i_codec == VLC_CODEC_TIFF )
            p_enc->fmt_in.i_codec = VLC_CODEC_RGB24;

        p_enc->fmt_in.video.i_chroma = p_enc->fmt_in.i_codec;
        GetFfmpegChroma( &p_context->pix_fmt, &p_enc->fmt_in.video );

        if( p_codec->pix_fmts )
        {
            static const enum AVPixelFormat vlc_pix_fmts[] = {
                AV_PIX_FMT_YUV420P,
                AV_PIX_FMT_NV12,
                AV_PIX_FMT_RGB24,
            };
            bool found = false;
            const enum PixelFormat *p = p_codec->pix_fmts;
            for( ; !found && *p != -1; p++ )
            {
                for( size_t i = 0; i < ARRAY_SIZE(vlc_pix_fmts); ++i )
                {
                    if( *p == vlc_pix_fmts[i] )
                    {
                        found = true;
                        p_context->pix_fmt = *p;
                        break;
                    }
                }
            }
            if (!found) p_context->pix_fmt = p_codec->pix_fmts[0];
            GetVlcChroma( &p_enc->fmt_in.video, p_context->pix_fmt );
            p_enc->fmt_in.i_codec = p_enc->fmt_in.video.i_chroma;
        }


        if ( p_sys->f_i_quant_factor != 0.f )
            p_context->i_quant_factor = p_sys->f_i_quant_factor;

        int nr = var_GetInteger( p_enc, ENC_CFG_PREFIX "noise-reduction" );
        add_av_option_int( p_enc, &options, "noise_reduction", nr );

        if ( p_sys->b_mpeg4_matrix )
        {
            p_context->intra_matrix = av_malloc( sizeof(mpeg4_default_intra_matrix) );
            if ( p_context->intra_matrix )
                memcpy( p_context->intra_matrix, mpeg4_default_intra_matrix,
                        sizeof(mpeg4_default_intra_matrix));
            p_context->inter_matrix = av_malloc( sizeof(mpeg4_default_non_intra_matrix) );
            if ( p_context->inter_matrix )
                memcpy( p_context->inter_matrix, mpeg4_default_non_intra_matrix,
                        sizeof(mpeg4_default_non_intra_matrix));
        }

        if ( p_sys->b_pre_me )
        {
            add_av_option_int( p_enc, &options, "mepre", 1 );
            p_context->me_pre_cmp = FF_CMP_CHROMA;
        }

        if ( p_sys->b_interlace )
        {
            if ( p_context->height <= 280 )
            {
                if ( p_context->height != 16 || p_context->width != 16 )
                    msg_Warn( p_enc,
                        "disabling interlaced video because height=%d <= 280",
                        p_context->height );
            }
            else
            {
                p_context->flags |= AV_CODEC_FLAG_INTERLACED_DCT;
                if ( p_sys->b_interlace_me )
                    p_context->flags |= AV_CODEC_FLAG_INTERLACED_ME;
            }
        }

        p_context->trellis = p_sys->b_trellis;

        if ( p_sys->i_qmin > 0 && p_sys->i_qmin == p_sys->i_qmax )
            p_context->flags |= AV_CODEC_FLAG_QSCALE;
        /* These codecs cause libavcodec to exit if thread_count is > 1.
           See libavcodec/mpegvideo_enc.c:MPV_encode_init and
           libavcodec/svq3.c , WMV2 calls MPV_encode_init also.
         */
        if ( i_codec_id == AV_CODEC_ID_FLV1 ||
             i_codec_id == AV_CODEC_ID_H261 ||
             i_codec_id == AV_CODEC_ID_LJPEG ||
             i_codec_id == AV_CODEC_ID_MJPEG ||
             i_codec_id == AV_CODEC_ID_H263 ||
             i_codec_id == AV_CODEC_ID_H263P ||
             i_codec_id == AV_CODEC_ID_MSMPEG4V1 ||
             i_codec_id == AV_CODEC_ID_MSMPEG4V2 ||
             i_codec_id == AV_CODEC_ID_MSMPEG4V3 ||
             i_codec_id == AV_CODEC_ID_WMV1 ||
             i_codec_id == AV_CODEC_ID_WMV2 ||
             i_codec_id == AV_CODEC_ID_RV10 ||
             i_codec_id == AV_CODEC_ID_RV20 ||
             i_codec_id == AV_CODEC_ID_SVQ3 )
            p_enc->i_threads = 1;

        if( p_sys->i_vtolerance > 0 )
            p_context->bit_rate_tolerance = p_sys->i_vtolerance;

        /* usually if someone sets bitrate, he likes more to get that bitrate
         * over quality should help 'normal' user to get asked bitrate
         */
        if( p_enc->fmt_out.i_bitrate > 0 && p_sys->i_qmax == 0 && p_sys->i_qmin == 0 )
        {
            p_sys->i_qmax = 51;
            p_sys->i_qmin = 3;
        }

        if( p_sys->i_qmin > 0 )
        {
            p_context->qmin = p_sys->i_qmin;
            p_context->mb_lmin = p_sys->i_qmin * FF_QP2LAMBDA;
            add_av_option_int( p_enc, &options, "lmin", p_context->mb_lmin);
        }
        if( p_sys->i_qmax > 0 )
        {
            p_context->qmax = p_sys->i_qmax;
            p_context->mb_lmax = p_sys->i_qmax * FF_QP2LAMBDA;
            add_av_option_int( p_enc, &options, "lmax", p_context->mb_lmax);
        }
        p_context->max_qdiff = 3;

        p_context->mb_decision = p_sys->i_hq;

        if( p_sys->i_quality && !p_enc->fmt_out.i_bitrate )
        {
            p_context->flags |= AV_CODEC_FLAG_QSCALE;
            p_context->global_quality = p_sys->i_quality;
        }
        else
        {
            av_dict_set(&options, "qsquish", "1.0", 0);
            /* Default to 1/2 second buffer for given bitrate unless defined otherwise*/
            if( !p_sys->i_rc_buffer_size )
            {
                p_sys->i_rc_buffer_size = p_enc->fmt_out.i_bitrate * 8 / 2;
            }
            msg_Dbg( p_enc, "rc buffer size %d bits", p_sys->i_rc_buffer_size );
            /* Set maxrate/minrate to bitrate to try to get CBR */
            p_context->rc_max_rate = p_enc->fmt_out.i_bitrate;
            p_context->rc_min_rate = p_enc->fmt_out.i_bitrate;
            p_context->rc_buffer_size = p_sys->i_rc_buffer_size;
            /* This is from ffmpeg's ffmpeg.c : */
            p_context->rc_initial_buffer_occupancy
                = p_sys->i_rc_buffer_size * 3/4;
            add_av_option_float( p_enc, &options, "rc_buffer_aggressivity", p_sys->f_rc_buffer_aggressivity );
        }
    }
    else if( p_enc->fmt_in.i_cat == AUDIO_ES )
    {
        p_context->codec_type  = AVMEDIA_TYPE_AUDIO;
        p_context->sample_fmt  = p_codec->sample_fmts ?
                                    p_codec->sample_fmts[0] :
                                    AV_SAMPLE_FMT_S16;

        /* Try to match avcodec input format to vlc format so we could avoid one
           format conversion */
        if( GetVlcAudioFormat( p_context->sample_fmt ) != p_enc->fmt_in.i_codec
            && p_codec->sample_fmts )
        {
            msg_Dbg( p_enc, "Trying to find more suitable sample format instead of %s", av_get_sample_fmt_name( p_context->sample_fmt ) );
            for( unsigned int i=0; p_codec->sample_fmts[i] != -1; i++ )
            {
                if( GetVlcAudioFormat( p_codec->sample_fmts[i] ) == p_enc->fmt_in.i_codec )
                {
                    p_context->sample_fmt = p_codec->sample_fmts[i];
                    msg_Dbg( p_enc, "Using %s as new sample format", av_get_sample_fmt_name( p_context->sample_fmt ) );
                    break;
                }
            }
        }
        p_sys->b_planar = av_sample_fmt_is_planar( p_context->sample_fmt );
        // Try if we can use interleaved format for codec input as VLC doesn't really do planar audio yet
        // FIXME: Remove when planar/interleaved audio in vlc is equally supported
        if( p_sys->b_planar && p_codec->sample_fmts )
        {
            msg_Dbg( p_enc, "Trying to find packet sample format instead of planar %s", av_get_sample_fmt_name( p_context->sample_fmt ) );
            for( unsigned int i=0; p_codec->sample_fmts[i] != -1; i++ )
            {
                if( !av_sample_fmt_is_planar( p_codec->sample_fmts[i] ) )
                {
                    p_context->sample_fmt = p_codec->sample_fmts[i];
                    msg_Dbg( p_enc, "Changing to packet format %s as new sample format", av_get_sample_fmt_name( p_context->sample_fmt ) );
                    break;
                }
            }
        }
        msg_Dbg( p_enc, "Ended up using %s as sample format", av_get_sample_fmt_name( p_context->sample_fmt ) );
        p_enc->fmt_in.i_codec  = GetVlcAudioFormat( p_context->sample_fmt );
        p_sys->b_planar = av_sample_fmt_is_planar( p_context->sample_fmt );

        p_context->sample_rate = p_enc->fmt_out.audio.i_rate;
        date_Init( &p_sys->buffer_date, p_enc->fmt_out.audio.i_rate, 1 );
        p_context->time_base.num = 1;
        p_context->time_base.den = p_context->sample_rate;
        p_context->channels      = p_enc->fmt_out.audio.i_channels;
        p_context->channel_layout = channel_mask[p_context->channels][1];

        /* Setup Channel ordering for multichannel audio
         * as VLC channel order isn't same as libavcodec expects
         */

        p_sys->i_channels_to_reorder = 0;

        /* Specified order
         * Copied from audio.c
         */
        const unsigned i_order_max = 8 * sizeof(p_context->channel_layout);
        uint32_t pi_order_dst[AOUT_CHAN_MAX] = { 0 };
        uint32_t order_mask = 0;
        int i_channels_src = 0;

        if( p_context->channel_layout )
        {
            msg_Dbg( p_enc, "Creating channel order for reordering");
            for( unsigned i = 0; i < sizeof(pi_channels_map)/sizeof(*pi_channels_map); i++ )
            {
                if( p_context->channel_layout & pi_channels_map[i][0] )
                {
                    msg_Dbg( p_enc, "%d %"PRIx64" mapped to %"PRIx64"", i_channels_src, pi_channels_map[i][0], pi_channels_map[i][1]);
                    pi_order_dst[i_channels_src++] = pi_channels_map[i][1];
                    order_mask |= pi_channels_map[i][1];
                }
            }
        }
        else
        {
            msg_Dbg( p_enc, "Creating default channel order for reordering");
            /* Create default order  */
            for( unsigned int i = 0; i < __MIN( i_order_max, (unsigned)p_sys->p_context->channels ); i++ )
            {
                if( i < sizeof(pi_channels_map)/sizeof(*pi_channels_map) )
                {
                    msg_Dbg( p_enc, "%d channel is %"PRIx64"", i_channels_src, pi_channels_map[i][1]);
                    pi_order_dst[i_channels_src++] = pi_channels_map[i][1];
                    order_mask |= pi_channels_map[i][1];
                }
            }
        }
        if( i_channels_src != p_context->channels )
            msg_Err( p_enc, "Channel layout not understood" );

        p_sys->i_channels_to_reorder =
            aout_CheckChannelReorder( NULL, pi_order_dst, order_mask,
                                      p_sys->pi_reorder_layout );

        if ( p_enc->fmt_out.i_codec == VLC_CODEC_MP4A )
        {
            /* XXX: FAAC does resample only when setting the INPUT samplerate
             * to the desired value (-R option of the faac frontend)
            p_enc->fmt_in.audio.i_rate = p_context->sample_rate;*/
            /* vlc should default to low-complexity profile, faac encoder
             * has bug and aac audio has issues otherwise atm */
            p_context->profile = p_sys->i_aac_profile;
        }
    }

    /* Misc parameters */
    p_context->bit_rate = p_enc->fmt_out.i_bitrate;

    /* Set reasonable defaults to VP8, based on
       libvpx-720p preset from libvpx ffmpeg-patch */
    if( i_codec_id == AV_CODEC_ID_VP8 )
    {
        /* Lets give bitrate tolerance */
        p_context->bit_rate_tolerance = __MAX(2 * (int)p_enc->fmt_out.i_bitrate, p_sys->i_vtolerance );
        /* default to 120 frames between keyframe */
        if( !var_GetInteger( p_enc, ENC_CFG_PREFIX "keyint" ) )
            p_context->gop_size = 120;
        /* Don't set rc-values atm, they were from time before
           libvpx was officially in FFmpeg */
        //p_context->rc_max_rate = 24 * 1000 * 1000; //24M
        //p_context->rc_min_rate = 40 * 1000; // 40k
        /* seems that FFmpeg presets have 720p as divider for buffers */
        if( p_enc->fmt_out.video.i_visible_height >= 720 )
        {
            /* Check that we don't overrun users qmin/qmax values */
            if( !var_GetInteger( p_enc, ENC_CFG_PREFIX "qmin" ) )
            {
                p_context->qmin = 10;
                p_context->mb_lmin = 10 * FF_QP2LAMBDA;
                add_av_option_int( p_enc, &options, "lmin", p_context->mb_lmin );
            }

            if( !var_GetInteger( p_enc, ENC_CFG_PREFIX "qmax" ) )
            {
                p_context->qmax = 42;
                p_context->mb_lmax = 42 * FF_QP2LAMBDA;
                add_av_option_int( p_enc, &options, "lmax", p_context->mb_lmax );
            }

        } else if( !var_GetInteger( p_enc, ENC_CFG_PREFIX "qmin" ) )
        {
                p_context->qmin = 1;
                p_context->mb_lmin = FF_QP2LAMBDA;
                add_av_option_int( p_enc, &options, "lmin", p_context->mb_lmin );
        }


#if 0 /* enable when/if vp8 encoder is accepted in libavcodec */
        p_context->lag = 16;
        p_context->level = 216;
        p_context->profile = 0;
        p_context->rc_buffer_aggressivity = 0.95;
        p_context->token_partitions = 4;
        p_context->mb_static_threshold = 0;
#endif
    }

    if( i_codec_id == AV_CODEC_ID_RAWVIDEO )
    {
        /* XXX: hack: Force same codec (will be handled by transcode) */
        p_enc->fmt_in.video.i_chroma = p_enc->fmt_in.i_codec = p_enc->fmt_out.i_codec;
        GetFfmpegChroma( &p_context->pix_fmt, &p_enc->fmt_in.video );
    }

    /* Make sure we get extradata filled by the encoder */
    p_context->extradata_size = 0;
    p_context->extradata = NULL;
    p_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if( p_enc->i_threads >= 1)
        p_context->thread_count = p_enc->i_threads;
    else
        p_context->thread_count = vlc_GetCPUCount();

    int ret;
    char *psz_opts = var_InheritString(p_enc, ENC_CFG_PREFIX "options");
    if (psz_opts) {
        vlc_av_get_options(psz_opts, &options);
        free(psz_opts);
    }

    vlc_avcodec_lock();
    ret = avcodec_open2( p_context, p_codec, options ? &options : NULL );
    vlc_avcodec_unlock();

    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(options, "", t, AV_DICT_IGNORE_SUFFIX))) {
        msg_Err(p_enc, "Unknown option \"%s\"", t->key);
    }

    if( ret )
    {
        if( p_enc->fmt_in.i_cat != AUDIO_ES ||
                (p_context->channels <= 2 && i_codec_id != AV_CODEC_ID_MP2
                 && i_codec_id != AV_CODEC_ID_MP3) )
errmsg:
        {
            static const char types[][12] = {
                [UNKNOWN_ES] = N_("unknown"), [VIDEO_ES] = N_("video"),
                [AUDIO_ES] = N_("audio"), [SPU_ES] = N_("subpicture"),
            };
            const char *type = types[0];
            union
            {
                vlc_fourcc_t value;
                char txt[4];
            } fcc = { .value = p_enc->fmt_out.i_codec };

            if (likely((unsigned)p_enc->fmt_in.i_cat < sizeof (types) / sizeof (types[0])))
                type = types[p_enc->fmt_in.i_cat];
            msg_Err( p_enc, "cannot open %4.4s %s encoder", fcc.txt, type );
            vlc_dialog_display_error( p_enc, _("Streaming / Transcoding failed"),
                _("VLC could not open the %4.4s %s encoder."),
                fcc.txt, vlc_gettext(type) );
            av_dict_free(&options);
            goto error;
        }

        if( p_context->channels > 2 )
        {
            p_context->channels = 2;
            p_context->channel_layout = channel_mask[p_context->channels][1];

            /* Change fmt_in in order to ask for a channels conversion */
            p_enc->fmt_in.audio.i_channels =
            p_enc->fmt_out.audio.i_channels = 2;
            p_enc->fmt_in.audio.i_physical_channels =
            p_enc->fmt_out.audio.i_physical_channels = AOUT_CHANS_STEREO;
            p_sys->i_channels_to_reorder = 0;
            msg_Warn( p_enc, "stereo mode selected (codec limitation)" );
        }

        if( i_codec_id == AV_CODEC_ID_MP2 || i_codec_id == AV_CODEC_ID_MP3 )
        {
            int i_frequency, i;
            es_format_t *fmt = &p_enc->fmt_out;

            for ( i_frequency = 0; i_frequency < 6; i_frequency++ )
                if ( fmt->audio.i_rate == mpa_freq_tab[i_frequency] )
                    break;

            if ( i_frequency == 6 )
            {
                msg_Warn( p_enc, "MPEG audio doesn't support frequency=%d",
                        fmt->audio.i_rate );
                /* Fallback to 44100 hz */
                p_context->sample_rate = p_enc->fmt_in.audio.i_rate =
                p_enc->fmt_out.audio.i_rate = 44100;
            }

            for ( i = 1; i < 14; i++ )
                if (fmt->i_bitrate/1000 <= mpa_bitrate_tab[i_frequency / 3][i])
                    break;

            if (fmt->i_bitrate / 1000 != mpa_bitrate_tab[i_frequency / 3][i])
            {
                msg_Warn( p_enc,
                        "MPEG audio doesn't support bitrate=%d, using %d",
                        fmt->i_bitrate,
                        mpa_bitrate_tab[i_frequency / 3][i] * 1000 );
                fmt->i_bitrate = mpa_bitrate_tab[i_frequency / 3][i] * 1000;
                p_context->bit_rate = fmt->i_bitrate;
            }
        }

        p_context->codec = NULL;
        vlc_avcodec_lock();
        ret = avcodec_open2( p_context, p_codec, options ? &options : NULL );
        vlc_avcodec_unlock();
        if( ret )
            goto errmsg;
    }

    av_dict_free(&options);

    if( i_codec_id == AV_CODEC_ID_FLAC )
    {
        p_enc->fmt_out.i_extra = 4 + 1 + 3 + p_context->extradata_size;
        p_enc->fmt_out.p_extra = malloc( p_enc->fmt_out.i_extra );
        if( p_enc->fmt_out.p_extra )
        {
            uint8_t *p = p_enc->fmt_out.p_extra;
            p[0] = 0x66;    /* f */
            p[1] = 0x4C;    /* L */
            p[2] = 0x61;    /* a */
            p[3] = 0x43;    /* C */
            p[4] = 0x80;    /* streaminfo block, last block before audio */
            p[5] = ( p_context->extradata_size >> 16 ) & 0xff;
            p[6] = ( p_context->extradata_size >>  8 ) & 0xff;
            p[7] = ( p_context->extradata_size       ) & 0xff;
            memcpy( &p[8], p_context->extradata, p_context->extradata_size );
        }
        else
        {
            p_enc->fmt_out.i_extra = 0;
        }
    }
    else
    {
        p_enc->fmt_out.i_extra = p_context->extradata_size;
        if( p_enc->fmt_out.i_extra )
        {
            p_enc->fmt_out.p_extra = malloc( p_enc->fmt_out.i_extra );
            if ( p_enc->fmt_out.p_extra == NULL )
            {
                goto error;
            }
            memcpy( p_enc->fmt_out.p_extra, p_context->extradata,
                    p_enc->fmt_out.i_extra );
        }
    }

    p_context->flags &= ~AV_CODEC_FLAG_GLOBAL_HEADER;

    if( p_enc->fmt_in.i_cat == AUDIO_ES )
    {
        p_enc->fmt_in.i_codec = GetVlcAudioFormat( p_sys->p_context->sample_fmt );
        p_enc->fmt_in.audio.i_bitspersample = aout_BitsPerSample( p_enc->fmt_in.i_codec );

        p_sys->i_sample_bytes = (p_enc->fmt_in.audio.i_bitspersample / 8);
        p_sys->i_frame_size = p_context->frame_size > 1 ?
                                    p_context->frame_size :
                                    AV_INPUT_BUFFER_MIN_SIZE;
        p_sys->i_buffer_out = av_samples_get_buffer_size(NULL,
                p_sys->p_context->channels, p_sys->i_frame_size,
                p_sys->p_context->sample_fmt, DEFAULT_ALIGN);
        p_sys->p_buffer = av_malloc( p_sys->i_buffer_out );
        if ( unlikely( p_sys->p_buffer == NULL ) )
        {
            goto error;
        }
        p_enc->fmt_out.audio.i_frame_length = p_context->frame_size;
        p_enc->fmt_out.audio.i_blockalign = p_context->block_align;
        p_enc->fmt_out.audio.i_bitspersample = aout_BitsPerSample( p_enc->fmt_out.i_codec );
        //b_variable tells if we can feed any size frames to encoder
        p_sys->b_variable = p_context->frame_size ? false : true;


        if( p_sys->b_planar )
        {
            p_sys->p_interleave_buf = av_malloc( p_sys->i_buffer_out );
            if( unlikely( p_sys->p_interleave_buf == NULL ) )
                goto error;
        }
    }

    p_sys->frame = av_frame_alloc();
    if( !p_sys->frame )
    {
        goto error;
    }
    msg_Dbg( p_enc, "found encoder %s", psz_namecodec );

    p_enc->pf_encode_video = EncodeVideo;
    p_enc->pf_encode_audio = EncodeAudio;


    return VLC_SUCCESS;
error:
    free( p_enc->fmt_out.p_extra );
    av_free( p_sys->p_buffer );
    av_free( p_sys->p_interleave_buf );
    avcodec_free_context( &p_context );
    free( p_sys );
    return VLC_ENOMEM;
}

typedef struct
{
    block_t self;
    AVPacket packet;
} vlc_av_packet_t;

static void vlc_av_packet_Release(block_t *block)
{
    vlc_av_packet_t *b = (void *) block;

    av_packet_unref(&b->packet);
    free(b);
}

static const struct vlc_block_callbacks vlc_av_packet_cbs =
{
    vlc_av_packet_Release,
};

static block_t *vlc_av_packet_Wrap(AVPacket *packet, vlc_tick_t i_length, AVCodecContext *context )
{
    if ( packet->data == NULL &&
         packet->flags == 0 &&
         packet->pts == AV_NOPTS_VALUE &&
         packet->dts == AV_NOPTS_VALUE )
        return NULL; /* totally empty AVPacket */

    vlc_av_packet_t *b = malloc( sizeof( *b ) );
    if( unlikely(b == NULL) )
        return NULL;

    block_t *p_block = &b->self;

    block_Init( p_block, &vlc_av_packet_cbs, packet->data, packet->size );
    p_block->i_nb_samples = 0;
    p_block->cbs = &vlc_av_packet_cbs;
    b->packet = *packet;

    p_block->i_length = i_length;
    p_block->i_pts = FROM_AV_TS(packet->pts);
    p_block->i_dts = FROM_AV_TS(packet->dts);
    if( unlikely( packet->flags & AV_PKT_FLAG_CORRUPT ) )
        p_block->i_flags |= BLOCK_FLAG_CORRUPTED;
    if( packet->flags & AV_PKT_FLAG_KEY )
        p_block->i_flags |= BLOCK_FLAG_TYPE_I;
    p_block->i_pts = p_block->i_pts * CLOCK_FREQ * context->time_base.num / context->time_base.den;
    p_block->i_dts = p_block->i_dts * CLOCK_FREQ * context->time_base.num / context->time_base.den;

    return p_block;
}

static void check_hurry_up( encoder_sys_t *p_sys, AVFrame *frame, encoder_t *p_enc )
{
    vlc_tick_t current_date = vlc_tick_now();

    if ( current_date + HURRY_UP_GUARD3 > FROM_AV_TS(frame->pts) )
    {
        p_sys->p_context->mb_decision = FF_MB_DECISION_SIMPLE;
        p_sys->p_context->trellis = 0;
        msg_Dbg( p_enc, "hurry up mode 3" );
    }
    else
    {
        p_sys->p_context->mb_decision = p_sys->i_hq;

        if ( current_date + HURRY_UP_GUARD2 > FROM_AV_TS(frame->pts) )
        {
            p_sys->p_context->trellis = 0;
            msg_Dbg( p_enc, "hurry up mode 2" );
        }
        else
        {
            p_sys->p_context->trellis = p_sys->b_trellis;
        }
    }

    if ( current_date + HURRY_UP_GUARD1 > FROM_AV_TS(frame->pts) )
    {
        frame->pict_type = AV_PICTURE_TYPE_P;
        /* msg_Dbg( p_enc, "hurry up mode 1 %lld", current_date + HURRY_UP_GUARD1 - frame.pts ); */
    }
}

static block_t *encode_avframe( encoder_t *p_enc, encoder_sys_t *p_sys, AVFrame *frame )
{
    AVPacket av_pkt;
    av_pkt.data = NULL;
    av_pkt.size = 0;

    av_init_packet( &av_pkt );

    int ret = avcodec_send_frame( p_sys->p_context, frame );
    if( frame && ret != 0 && ret != AVERROR(EAGAIN) )
    {
        msg_Warn( p_enc, "cannot send one frame to encoder %d", ret );
        return NULL;
    }
    ret = avcodec_receive_packet( p_sys->p_context, &av_pkt );
    if( ret != 0 && ret != AVERROR(EAGAIN) )
    {
        msg_Warn( p_enc, "cannot encode one frame" );
        return NULL;
    }

    block_t *p_block = vlc_av_packet_Wrap( &av_pkt,
            av_pkt.duration / p_sys->p_context->time_base.den, p_sys->p_context );
    if( unlikely(p_block == NULL) )
    {
        av_packet_unref( &av_pkt );
        return NULL;
    }
    return p_block;
}

/****************************************************************************
 * EncodeVideo: the whole thing
 ****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    int i_plane;

    AVFrame *frame = NULL;
    if( likely(p_pict) ) {
        frame = p_sys->frame;
        av_frame_unref( frame );

        for( i_plane = 0; i_plane < p_pict->i_planes; i_plane++ )
        {
            p_sys->frame->data[i_plane] = p_pict->p[i_plane].p_pixels;
            p_sys->frame->linesize[i_plane] = p_pict->p[i_plane].i_pitch;
        }

        /* Let libavcodec select the frame type */
        frame->pict_type = 0;

        frame->repeat_pict = p_pict->i_nb_fields - 2;
        frame->interlaced_frame = !p_pict->b_progressive;
        frame->top_field_first = !!p_pict->b_top_field_first;

        frame->format = p_sys->p_context->pix_fmt;
        frame->width = p_sys->p_context->width;
        frame->height = p_sys->p_context->height;

        /* Set the pts of the frame being encoded
         * avcodec likes pts to be in time_base units
         * frame number */
        if( likely( p_pict->date != VLC_TICK_INVALID ) )
            frame->pts = p_pict->date * p_sys->p_context->time_base.den /
                          CLOCK_FREQ / p_sys->p_context->time_base.num;
        else
            frame->pts = AV_NOPTS_VALUE;

        if ( p_sys->b_hurry_up && frame->pts != AV_NOPTS_VALUE )
            check_hurry_up( p_sys, frame, p_enc );

        if ( ( frame->pts != AV_NOPTS_VALUE ) && ( frame->pts != VLC_TICK_INVALID ) )
        {
            if ( p_sys->i_last_pts == FROM_AV_TS(frame->pts) )
            {
                msg_Warn( p_enc, "almost fed libavcodec with two frames with "
                          "the same PTS (%"PRId64 ")", frame->pts );
                return NULL;
            }
            else if ( p_sys->i_last_pts > FROM_AV_TS(frame->pts) )
            {
                msg_Warn( p_enc, "almost fed libavcodec with a frame in the "
                         "past (current: %"PRId64 ", last: %"PRId64")",
                         frame->pts, p_sys->i_last_pts );
                return NULL;
            }
            else
                p_sys->i_last_pts = FROM_AV_TS(frame->pts);
        }

        frame->quality = p_sys->i_quality;
    }

    block_t *p_block = encode_avframe( p_enc, p_sys, frame );

    if( p_block )
    {
       switch ( p_sys->p_context->coded_frame->pict_type )
       {
       case AV_PICTURE_TYPE_I:
       case AV_PICTURE_TYPE_SI:
           p_block->i_flags |= BLOCK_FLAG_TYPE_I;
           break;
       case AV_PICTURE_TYPE_P:
       case AV_PICTURE_TYPE_SP:
           p_block->i_flags |= BLOCK_FLAG_TYPE_P;
           break;
       case AV_PICTURE_TYPE_B:
       case AV_PICTURE_TYPE_BI:
           p_block->i_flags |= BLOCK_FLAG_TYPE_B;
           break;
       default:
           p_block->i_flags |= BLOCK_FLAG_TYPE_PB;
       }
    }

    return p_block;
}

static block_t *handle_delay_buffer( encoder_t *p_enc, encoder_sys_t *p_sys, unsigned int buffer_delay,
                                     block_t *p_aout_buf, size_t leftover_samples )
{
    block_t *p_block = NULL;
    //How much we need to copy from new packet
    const size_t leftover = leftover_samples * p_sys->p_context->channels * p_sys->i_sample_bytes;

    av_frame_unref( p_sys->frame );
    p_sys->frame->format     = p_sys->p_context->sample_fmt;
    p_sys->frame->nb_samples = leftover_samples + p_sys->i_samples_delay;

    if( likely( date_Get( &p_sys->buffer_date ) != VLC_TICK_INVALID) )
    {
        /* Convert to AV timing */
        p_sys->frame->pts = date_Get( &p_sys->buffer_date ) - VLC_TICK_0;
        p_sys->frame->pts = p_sys->frame->pts * p_sys->p_context->time_base.den /
                            CLOCK_FREQ / p_sys->p_context->time_base.num;
        date_Increment( &p_sys->buffer_date, p_sys->frame->nb_samples );
    }
    else p_sys->frame->pts = AV_NOPTS_VALUE;

    if( likely( p_aout_buf ) )
    {

        p_aout_buf->i_nb_samples -= leftover_samples;
        memcpy( p_sys->p_buffer+buffer_delay, p_aout_buf->p_buffer, leftover );

        // We need to deinterleave from p_aout_buf to p_buffer the leftover bytes
        if( p_sys->b_planar )
            aout_Deinterleave( p_sys->p_interleave_buf, p_sys->p_buffer,
                p_sys->i_frame_size, p_sys->p_context->channels, p_enc->fmt_in.i_codec );
        else
            memcpy( p_sys->p_buffer + buffer_delay, p_aout_buf->p_buffer, leftover);

        p_aout_buf->p_buffer     += leftover;
        p_aout_buf->i_buffer     -= leftover;
        p_aout_buf->i_pts         = date_Get( &p_sys->buffer_date );
    }

    if(unlikely( ( (leftover + buffer_delay) < p_sys->i_buffer_out ) &&
                 !(p_sys->p_codec->capabilities & AV_CODEC_CAP_SMALL_LAST_FRAME )))
    {
        msg_Dbg( p_enc, "No small last frame support, padding");
        size_t padding_size = p_sys->i_buffer_out - (leftover+buffer_delay);
        memset( p_sys->p_buffer + (leftover+buffer_delay), 0, padding_size );
        buffer_delay += padding_size;
    }
    if( avcodec_fill_audio_frame( p_sys->frame, p_sys->p_context->channels,
            p_sys->p_context->sample_fmt, p_sys->b_planar ? p_sys->p_interleave_buf : p_sys->p_buffer,
            p_sys->i_buffer_out,
            DEFAULT_ALIGN) < 0 )
    {
        msg_Err( p_enc, "filling error on fillup" );
        p_sys->frame->nb_samples = 0;
    }


    p_sys->i_samples_delay = 0;

    p_block = encode_avframe( p_enc, p_sys, p_sys->frame );

    return p_block;
}

/****************************************************************************
 * EncodeAudio: the whole thing
 ****************************************************************************/
static block_t *EncodeAudio( encoder_t *p_enc, block_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    block_t *p_block, *p_chain = NULL;
    size_t buffer_delay = 0, i_samples_left = 0;


    //i_bytes_left is amount of bytes we get
    i_samples_left = p_aout_buf ? p_aout_buf->i_nb_samples : 0;
    buffer_delay = p_sys->i_samples_delay * p_sys->i_sample_bytes * p_sys->p_context->channels;

    //p_sys->i_buffer_out = p_sys->i_frame_size * chan * p_sys->i_sample_bytes
    //Calculate how many bytes we would need from current buffer to fill frame
    size_t leftover_samples = __MAX(0,__MIN((ssize_t)i_samples_left, (ssize_t)(p_sys->i_frame_size - p_sys->i_samples_delay)));

    if( p_aout_buf && ( p_aout_buf->i_pts != VLC_TICK_INVALID ) )
    {
        date_Set( &p_sys->buffer_date, p_aout_buf->i_pts );
        /* take back amount we have leftover from previous buffer*/
        if( p_sys->i_samples_delay > 0 )
            date_Decrement( &p_sys->buffer_date, p_sys->i_samples_delay );
    }
    /* Handle reordering here so we have p_sys->p_buffer always in correct
     * order already */
    if( p_aout_buf && p_sys->i_channels_to_reorder > 0 )
    {
        aout_ChannelReorder( p_aout_buf->p_buffer, p_aout_buf->i_buffer,
             p_sys->i_channels_to_reorder, p_sys->pi_reorder_layout,
             p_enc->fmt_in.i_codec );
    }

    // Check if we have enough samples in delay_buffer and current p_aout_buf to fill frame
    // Or if we are cleaning up
    if( ( buffer_delay > 0 ) &&
            ( ( p_aout_buf && ( leftover_samples <= p_aout_buf->i_nb_samples ) &&
               ( (leftover_samples + p_sys->i_samples_delay ) >= p_sys->i_frame_size )
              ) ||
             ( !p_aout_buf )
            )
         )
    {
        p_chain = handle_delay_buffer( p_enc, p_sys, buffer_delay, p_aout_buf, leftover_samples );
        buffer_delay = 0;
        if( unlikely( !p_chain ) )
            return NULL;
    }

    if( unlikely( !p_aout_buf ) )
    {
        msg_Dbg(p_enc,"Flushing..");
        do {
            p_block = encode_avframe( p_enc, p_sys, NULL );
            if( likely( p_block ) )
            {
                block_ChainAppend( &p_chain, p_block );
            }
        } while( p_block );
        return p_chain;
    }


    while( ( p_aout_buf->i_nb_samples >= p_sys->i_frame_size ) ||
           ( p_sys->b_variable && p_aout_buf->i_nb_samples ) )
    {
        av_frame_unref( p_sys->frame );

        if( p_sys->b_variable )
            p_sys->frame->nb_samples = p_aout_buf->i_nb_samples;
        else
            p_sys->frame->nb_samples = p_sys->i_frame_size;
        p_sys->frame->format     = p_sys->p_context->sample_fmt;
        if( likely(date_Get( &p_sys->buffer_date ) != VLC_TICK_INVALID) )
        {
            /* Convert to AV timing */
            p_sys->frame->pts = date_Get( &p_sys->buffer_date ) - VLC_TICK_0;
            p_sys->frame->pts = p_sys->frame->pts * p_sys->p_context->time_base.den /
                                CLOCK_FREQ / p_sys->p_context->time_base.num;
        }
        else p_sys->frame->pts = AV_NOPTS_VALUE;

        const int in_bytes = p_sys->frame->nb_samples *
            p_sys->p_context->channels * p_sys->i_sample_bytes;

        if( p_sys->b_planar )
        {
            aout_Deinterleave( p_sys->p_buffer, p_aout_buf->p_buffer,
                               p_sys->frame->nb_samples, p_sys->p_context->channels, p_enc->fmt_in.i_codec );

        }
        else
        {
            memcpy(p_sys->p_buffer, p_aout_buf->p_buffer, in_bytes);
        }

        if( avcodec_fill_audio_frame( p_sys->frame, p_sys->p_context->channels,
                                    p_sys->p_context->sample_fmt,
                                    p_sys->p_buffer,
                                    p_sys->i_buffer_out,
                                    DEFAULT_ALIGN) < 0 )
        {
                 msg_Err( p_enc, "filling error on encode" );
                 p_sys->frame->nb_samples = 0;
        }

        p_aout_buf->p_buffer     += in_bytes;
        p_aout_buf->i_buffer     -= in_bytes;
        p_aout_buf->i_nb_samples -= p_sys->frame->nb_samples;
        if( likely(date_Get( &p_sys->buffer_date ) != VLC_TICK_INVALID) )
            date_Increment( &p_sys->buffer_date, p_sys->frame->nb_samples );

        p_block = encode_avframe( p_enc, p_sys, p_sys->frame );
        if( likely( p_block ) )
            block_ChainAppend( &p_chain, p_block );
    }

    // We have leftover samples that don't fill frame_size, and libavcodec doesn't seem to like
    // that frame has more data than p_sys->i_frame_size most of the cases currently.
    if( p_aout_buf->i_nb_samples > 0 )
    {
       memcpy( p_sys->p_buffer + buffer_delay, p_aout_buf->p_buffer,
               p_aout_buf->i_nb_samples * p_sys->i_sample_bytes * p_sys->p_context->channels);
       p_sys->i_samples_delay += p_aout_buf->i_nb_samples;
    }

    return p_chain;
}

/*****************************************************************************
 * EndVideoEnc: libavcodec encoder destruction
 *****************************************************************************/
void EndVideoEnc( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    av_frame_free( &p_sys->frame );

    vlc_avcodec_lock();
    avcodec_close( p_sys->p_context );
    vlc_avcodec_unlock();
    avcodec_free_context( &p_sys->p_context );


    av_free( p_sys->p_interleave_buf );
    av_free( p_sys->p_buffer );

    free( p_sys );
}
