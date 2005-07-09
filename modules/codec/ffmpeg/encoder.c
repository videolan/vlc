/*****************************************************************************
 * encoder.c: video and audio encoder using the ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc/sout.h>
#include <vlc/decoder.h>

/* ffmpeg header */
#define HAVE_MMX
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#if LIBAVCODEC_BUILD < 4704
#   define AV_NOPTS_VALUE 0
#endif
#if LIBAVCODEC_BUILD < 4684
#    define FF_QP2LAMBDA 118
#endif

#include "ffmpeg.h"

#define AVCODEC_MAX_VIDEO_FRAME_SIZE (3*1024*1024)
#define HURRY_UP_GUARD1 (450000)
#define HURRY_UP_GUARD2 (300000)
#define HURRY_UP_GUARD3 (100000)

#define MAX_FRAME_DELAY (FF_MAX_B_FRAMES + 2)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  E_(OpenEncoder) ( vlc_object_t * );
void E_(CloseEncoder)( vlc_object_t * );

static block_t *EncodeVideo( encoder_t *, picture_t * );
static block_t *EncodeAudio( encoder_t *, aout_buffer_t * );

struct thread_context_t;
static int FfmpegThread( struct thread_context_t *p_context );
static int FfmpegExecute( AVCodecContext *s,
                          int (*pf_func)(AVCodecContext *c2, void *arg2),
                          void **arg, int *ret, int count );

/*****************************************************************************
 * thread_context_t : for multithreaded encoding
 *****************************************************************************/
#if LIBAVCODEC_BUILD >= 4702
struct thread_context_t
{
    VLC_COMMON_MEMBERS

    AVCodecContext  *p_context;
    int             (* pf_func)(AVCodecContext *c, void *arg);
    void            *arg;
    int             i_ret;

    vlc_mutex_t     lock;
    vlc_cond_t      cond;
    vlc_bool_t      b_work, b_done;
};
#endif

/*****************************************************************************
 * encoder_sys_t : ffmpeg encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    /*
     * Ffmpeg properties
     */
    AVCodec         *p_codec;
    AVCodecContext  *p_context;

    /*
     * Common properties
     */
    char *p_buffer;
    char *p_buffer_out;

    /*
     * Video properties
     */
    mtime_t i_last_ref_pts;
    mtime_t i_buggy_pts_detect;
    mtime_t i_last_pts;
    vlc_bool_t b_inited;

    /*
     * Audio properties
     */
    int i_frame_size;
    int i_samples_delay;
    mtime_t i_pts;

    /* Encoding settings */
    int        i_key_int;
    int        i_b_frames;
    int        i_vtolerance;
    int        i_qmin;
    int        i_qmax;
    int        i_hq;
    vlc_bool_t b_strict_rc;
    int        i_rc_buffer_size;
    float      f_rc_buffer_aggressivity;
    vlc_bool_t b_pre_me;
    vlc_bool_t b_hurry_up;
    vlc_bool_t b_interlace;
    float      f_i_quant_factor;
    int        i_noise_reduction;
    vlc_bool_t b_mpeg4_matrix;
    vlc_bool_t b_trellis;
    int        i_quality; /* for VBR */
    float      f_lumi_masking, f_dark_masking, f_p_masking, f_border_masking;
    int        i_luma_elim, i_chroma_elim;

    /* Used to work around stupid timestamping behaviour in libavcodec */
    uint64_t i_framenum;
    mtime_t  pi_delay_pts[MAX_FRAME_DELAY];
};

static const char *ppsz_enc_options[] = {
    "keyint", "bframes", "vt", "qmin", "qmax", "hq", "strict-rc",
    "rc-buffer-size", "rc-buffer-aggressivity", "pre-me", "hurry-up",
    "interlace", "i-quant-factor", "noise-reduction", "mpeg4-matrix",
    "trellis", "qscale", "strict", "lumi-masking", "dark-masking",
    "p-masking", "border-masking", "luma-elim-threshold",
    "chroma-elim-threshold", NULL
};

/*****************************************************************************
 * OpenEncoder: probe the encoder
 *****************************************************************************/
extern int16_t IMPORT_SYMBOL ff_mpeg4_default_intra_matrix[];
extern int16_t IMPORT_SYMBOL ff_mpeg4_default_non_intra_matrix[];

int E_(OpenEncoder)( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;
    AVCodecContext *p_context;
    AVCodec *p_codec;
    int i_codec_id, i_cat;
    char *psz_namecodec;
    vlc_value_t val;

    if( !E_(GetFfmpegCodec)( p_enc->fmt_out.i_codec, &i_cat, &i_codec_id,
                             &psz_namecodec ) )
    {
        if( E_(GetFfmpegChroma)( p_enc->fmt_out.i_codec ) < 0 )
        {
            /* handed chroma output */
            return VLC_EGENERIC;
        }
        i_cat      = VIDEO_ES;
        i_codec_id = CODEC_ID_RAWVIDEO;
        psz_namecodec = "Raw video";
    }


    if( p_enc->fmt_out.i_cat == VIDEO_ES && i_cat != VIDEO_ES )
    {
        msg_Err( p_enc, "\"%s\" is not a video encoder", psz_namecodec );
        return VLC_EGENERIC;
    }

    if( p_enc->fmt_out.i_cat == AUDIO_ES && i_cat != AUDIO_ES )
    {
        msg_Err( p_enc, "\"%s\" is not an audio encoder", psz_namecodec );
        return VLC_EGENERIC;
    }

    /* Initialization must be done before avcodec_find_decoder() */
    E_(InitLibavcodec)(p_this);

    p_codec = avcodec_find_encoder( i_codec_id );
    if( !p_codec )
    {
        msg_Err( p_enc, "cannot find encoder %s", psz_namecodec );
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
    {
        msg_Err( p_enc, "out of memory" );
        return VLC_EGENERIC;
    }
    memset( p_sys, 0, sizeof(encoder_sys_t) );
    p_enc->p_sys = p_sys;
    p_sys->p_codec = p_codec;

    p_enc->pf_encode_video = EncodeVideo;
    p_enc->pf_encode_audio = EncodeAudio;

    p_sys->p_buffer_out = NULL;
    p_sys->p_buffer = NULL;

    p_sys->p_context = p_context = avcodec_alloc_context();

    /* Set CPU capabilities */
    p_context->dsp_mask = 0;
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_MMX) )
    {
        p_context->dsp_mask |= FF_MM_MMX;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT) )
    {
        p_context->dsp_mask |= FF_MM_MMXEXT;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_3DNOW) )
    {
        p_context->dsp_mask |= FF_MM_3DNOW;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_SSE) )
    {
        p_context->dsp_mask |= FF_MM_SSE;
        p_context->dsp_mask |= FF_MM_SSE2;
    }

    sout_CfgParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    var_Get( p_enc, ENC_CFG_PREFIX "keyint", &val );
    p_sys->i_key_int = val.i_int;

    var_Get( p_enc, ENC_CFG_PREFIX "bframes", &val );
    p_sys->i_b_frames = val.i_int;

    var_Get( p_enc, ENC_CFG_PREFIX "vt", &val );
    p_sys->i_vtolerance = val.i_int;

    var_Get( p_enc, ENC_CFG_PREFIX "interlace", &val );
    p_sys->b_interlace = val.b_bool;

    var_Get( p_enc, ENC_CFG_PREFIX "pre-me", &val );
    p_sys->b_pre_me = val.b_bool;

    var_Get( p_enc, ENC_CFG_PREFIX "hurry-up", &val );
    p_sys->b_hurry_up = val.b_bool;
    if( p_sys->b_hurry_up )
    {
        /* hurry up mode needs noise reduction, even small */
        p_sys->i_noise_reduction = 1;
    }

    var_Get( p_enc, ENC_CFG_PREFIX "strict-rc", &val );
    p_sys->b_strict_rc = val.b_bool;
    var_Get( p_enc, ENC_CFG_PREFIX "rc-buffer-size", &val );
    p_sys->i_rc_buffer_size = val.i_int;
    var_Get( p_enc, ENC_CFG_PREFIX "rc-buffer-aggressivity", &val );
    p_sys->f_rc_buffer_aggressivity = val.f_float;

    var_Get( p_enc, ENC_CFG_PREFIX "i-quant-factor", &val );
    p_sys->f_i_quant_factor = val.f_float;

    var_Get( p_enc, ENC_CFG_PREFIX "noise-reduction", &val );
    p_sys->i_noise_reduction = val.i_int;

    var_Get( p_enc, ENC_CFG_PREFIX "mpeg4-matrix", &val );
    p_sys->b_mpeg4_matrix = val.b_bool;

    var_Get( p_enc, ENC_CFG_PREFIX "qscale", &val );
    if( val.f_float < 0.01 || val.f_float > 255.0 ) val.f_float = 0;
    p_sys->i_quality = (int)(FF_QP2LAMBDA * val.f_float + 0.5);

    var_Get( p_enc, ENC_CFG_PREFIX "hq", &val );
    if( val.psz_string && *val.psz_string )
    {
        if( !strcmp( val.psz_string, "rd" ) )
            p_sys->i_hq = FF_MB_DECISION_RD;
        else if( !strcmp( val.psz_string, "bits" ) )
            p_sys->i_hq = FF_MB_DECISION_BITS;
        else if( !strcmp( val.psz_string, "simple" ) )
            p_sys->i_hq = FF_MB_DECISION_SIMPLE;
        else
            p_sys->i_hq = FF_MB_DECISION_RD;
    }
    if( val.psz_string ) free( val.psz_string );

    var_Get( p_enc, ENC_CFG_PREFIX "qmin", &val );
    p_sys->i_qmin = val.i_int;
    var_Get( p_enc, ENC_CFG_PREFIX "qmax", &val );
    p_sys->i_qmax = val.i_int;
    var_Get( p_enc, ENC_CFG_PREFIX "trellis", &val );
    p_sys->b_trellis = val.b_bool;

    var_Get( p_enc, ENC_CFG_PREFIX "strict", &val );
    if( val.i_int < - 1 || val.i_int > 1 ) val.i_int = 0;
    p_context->strict_std_compliance = val.i_int;

    var_Get( p_enc, ENC_CFG_PREFIX "lumi-masking", &val );
    p_sys->f_lumi_masking = val.f_float;
    var_Get( p_enc, ENC_CFG_PREFIX "dark-masking", &val );
    p_sys->f_dark_masking = val.f_float;
    var_Get( p_enc, ENC_CFG_PREFIX "p-masking", &val );
    p_sys->f_p_masking = val.f_float;
    var_Get( p_enc, ENC_CFG_PREFIX "border-masking", &val );
    p_sys->f_border_masking = val.f_float;
    var_Get( p_enc, ENC_CFG_PREFIX "luma-elim-threshold", &val );
    p_sys->i_luma_elim = val.i_int;
    var_Get( p_enc, ENC_CFG_PREFIX "chroma-elim-threshold", &val );
    p_sys->i_chroma_elim = val.i_int;

    if( p_enc->fmt_in.i_cat == VIDEO_ES )
    {
        int i_aspect_num, i_aspect_den;

        if( !p_enc->fmt_in.video.i_width || !p_enc->fmt_in.video.i_height )
        {
            msg_Warn( p_enc, "invalid size %ix%i", p_enc->fmt_in.video.i_width,
                      p_enc->fmt_in.video.i_height );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_context->width = p_enc->fmt_in.video.i_width;
        p_context->height = p_enc->fmt_in.video.i_height;

#if LIBAVCODEC_BUILD >= 4754
        p_context->time_base.num = p_enc->fmt_in.video.i_frame_rate_base;
        p_context->time_base.den = p_enc->fmt_in.video.i_frame_rate;
#else
        p_context->frame_rate = p_enc->fmt_in.video.i_frame_rate;
        p_context->frame_rate_base= p_enc->fmt_in.video.i_frame_rate_base;
#endif

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
#if LIBAVCODEC_BUILD >= 4741
        p_context->border_masking = p_sys->f_border_masking;
#endif
        p_context->luma_elim_threshold = p_sys->i_luma_elim;
        p_context->chroma_elim_threshold = p_sys->i_chroma_elim;

        if( p_sys->i_key_int > 0 )
            p_context->gop_size = p_sys->i_key_int;
        p_context->max_b_frames =
            __MAX( __MIN( p_sys->i_b_frames, FF_MAX_B_FRAMES ), 0 );
        p_context->b_frame_strategy = 0;

#if LIBAVCODEC_BUILD >= 4687
        av_reduce( &i_aspect_num, &i_aspect_den,
                   p_enc->fmt_in.video.i_aspect,
                   VOUT_ASPECT_FACTOR, 1 << 30 /* something big */ );
        av_reduce( &p_context->sample_aspect_ratio.num,
                   &p_context->sample_aspect_ratio.den,
                   i_aspect_num * (int64_t)p_context->height,
                   i_aspect_den * (int64_t)p_context->width, 1 << 30 );
#else
        p_context->aspect_ratio = ((float)p_enc->fmt_in.video.i_aspect) /
            VOUT_ASPECT_FACTOR;
#endif

        p_sys->p_buffer_out = malloc( AVCODEC_MAX_VIDEO_FRAME_SIZE );

        p_enc->fmt_in.i_codec = VLC_FOURCC('I','4','2','0');
        p_context->pix_fmt = E_(GetFfmpegChroma)( p_enc->fmt_in.i_codec );
#if LIBAVCODEC_BUILD >= 4714
        if( p_codec->pix_fmts )
        {
            const enum PixelFormat *p = p_codec->pix_fmts;
            for( ; *p != -1; p++ )
            {
                if( *p == p_context->pix_fmt ) break;
            }
            if( *p == -1 ) p_context->pix_fmt = p_codec->pix_fmts[0];
            p_enc->fmt_in.i_codec = E_(GetVlcChroma)( p_context->pix_fmt );
        }
#else
        p_enc->fmt_in.i_codec = E_(GetVlcChroma)( p_context->pix_fmt );
#endif

        if ( p_sys->b_strict_rc )
        {
            p_context->rc_max_rate = p_enc->fmt_out.i_bitrate;
            p_context->rc_buffer_size = p_sys->i_rc_buffer_size;
            /* This is from ffmpeg's ffmpeg.c : */
            p_context->rc_initial_buffer_occupancy
                = p_sys->i_rc_buffer_size * 3/4;
            p_context->rc_buffer_aggressivity = p_sys->f_rc_buffer_aggressivity;
        }

        if ( p_sys->f_i_quant_factor != 0.0 )
            p_context->i_quant_factor = p_sys->f_i_quant_factor;

#if LIBAVCODEC_BUILD >= 4690
        p_context->noise_reduction = p_sys->i_noise_reduction;
#endif

        if ( p_sys->b_mpeg4_matrix )
        {
            p_context->intra_matrix = ff_mpeg4_default_intra_matrix;
            p_context->inter_matrix = ff_mpeg4_default_non_intra_matrix;
        }

        if ( p_sys->b_pre_me )
        {
            p_context->pre_me = 1;
            p_context->me_pre_cmp = FF_CMP_CHROMA;
        }

        if ( p_sys->b_interlace )
        {
            p_context->flags |= CODEC_FLAG_INTERLACED_DCT;
#if LIBAVCODEC_BUILD >= 4698
            p_context->flags |= CODEC_FLAG_INTERLACED_ME;
#endif
        }

        if ( p_sys->b_trellis )
            p_context->flags |= CODEC_FLAG_TRELLIS_QUANT;

        if ( p_sys->i_qmin > 0 && p_sys->i_qmin == p_sys->i_qmax )
            p_context->flags |= CODEC_FLAG_QSCALE;

#if LIBAVCODEC_BUILD >= 4702
        if ( p_enc->i_threads >= 1 )
            p_context->thread_count = p_enc->i_threads;
#endif

        if( p_sys->i_vtolerance > 0 )
            p_context->bit_rate_tolerance = p_sys->i_vtolerance;

        if( p_sys->i_qmin > 0 )
            p_context->mb_qmin = p_context->qmin = p_sys->i_qmin;
        if( p_sys->i_qmax > 0 )
            p_context->mb_qmax = p_context->qmax = p_sys->i_qmax;
        p_context->max_qdiff = 3;

        p_context->mb_decision = p_sys->i_hq;

        if( p_sys->i_quality )
        {
            p_context->flags |= CODEC_FLAG_QSCALE;
#if LIBAVCODEC_BUILD >= 4668
            p_context->global_quality = p_sys->i_quality;
#endif
        }
    }
    else if( p_enc->fmt_in.i_cat == AUDIO_ES )
    {
        /* work around bug in libmp3lame encoding */
        if( i_codec_id == CODEC_ID_MP3 && p_enc->fmt_in.audio.i_channels > 2 )
            p_enc->fmt_in.audio.i_channels = 2;

        p_enc->fmt_in.i_codec  = AOUT_FMT_S16_NE;
        p_context->sample_rate = p_enc->fmt_in.audio.i_rate;
        p_context->channels    = p_enc->fmt_in.audio.i_channels;
    }

    /* Misc parameters */
    p_context->bit_rate = p_enc->fmt_out.i_bitrate;

    if( i_codec_id == CODEC_ID_RAWVIDEO )
    {
        /* XXX: hack: Force same codec (will be handled by transcode) */
        p_enc->fmt_in.i_codec = p_enc->fmt_out.i_codec;
        p_context->pix_fmt = E_(GetFfmpegChroma)( p_enc->fmt_in.i_codec );
    }

    /* Make sure we get extradata filled by the encoder */
    p_context->extradata_size = 0;
    p_context->extradata = NULL;
    p_context->flags |= CODEC_FLAG_GLOBAL_HEADER;

    if( avcodec_open( p_context, p_codec ) )
    {
        if( p_enc->fmt_in.i_cat == AUDIO_ES && p_context->channels > 2 )
        {
            p_context->channels = 2;
            p_enc->fmt_in.audio.i_channels = 2; // FIXME
            if( avcodec_open( p_context, p_codec ) )
            {
                msg_Err( p_enc, "cannot open encoder" );
                free( p_sys );
                return VLC_EGENERIC;
            }
            msg_Warn( p_enc, "stereo mode selected (codec limitation)" );
        }
        else
        {
            msg_Err( p_enc, "cannot open encoder" );
            free( p_sys );
            return VLC_EGENERIC;
        }
    }

    p_enc->fmt_out.i_extra = p_context->extradata_size;
    p_enc->fmt_out.p_extra = p_context->extradata;
    p_context->flags &= ~CODEC_FLAG_GLOBAL_HEADER;

    if( p_enc->fmt_in.i_cat == AUDIO_ES )
    {
        p_sys->p_buffer_out = malloc( 2 * AVCODEC_MAX_AUDIO_FRAME_SIZE );
        p_sys->i_frame_size = p_context->frame_size * 2 * p_context->channels;
        p_sys->p_buffer = malloc( p_sys->i_frame_size );
    }

    msg_Dbg( p_enc, "found encoder %s", psz_namecodec );

    return VLC_SUCCESS;
}

/****************************************************************************
 * Ffmpeg threading system
 ****************************************************************************/
#if LIBAVCODEC_BUILD >= 4702
static int FfmpegThread( struct thread_context_t *p_context )
{
    while ( !p_context->b_die && !p_context->b_error )
    {
        vlc_mutex_lock( &p_context->lock );
        while ( !p_context->b_work && !p_context->b_die && !p_context->b_error )
        {
            vlc_cond_wait( &p_context->cond, &p_context->lock );
        }
        p_context->b_work = 0;
        vlc_mutex_unlock( &p_context->lock );
        if ( p_context->b_die || p_context->b_error )
            break;

        if ( p_context->pf_func )
        {
            p_context->i_ret = p_context->pf_func( p_context->p_context,
                                                   p_context->arg );
        }

        vlc_mutex_lock( &p_context->lock );
        p_context->b_done = 1;
        vlc_cond_signal( &p_context->cond );
        vlc_mutex_unlock( &p_context->lock );
    }

    return 0;
}

static int FfmpegExecute( AVCodecContext *s,
                          int (*pf_func)(AVCodecContext *c2, void *arg2),
                          void **arg, int *ret, int count )
{
    struct thread_context_t ** pp_contexts =
                         (struct thread_context_t **)s->thread_opaque;
    int i;

    /* Note, we can be certain that this is not called with the same
     * AVCodecContext by different threads at the same time */
    for ( i = 0; i < count; i++ )
    {
        vlc_mutex_lock( &pp_contexts[i]->lock );
        pp_contexts[i]->arg = arg[i];
        pp_contexts[i]->pf_func = pf_func;
        pp_contexts[i]->i_ret = 12345;
        pp_contexts[i]->b_work = 1;
        vlc_cond_signal( &pp_contexts[i]->cond );
        vlc_mutex_unlock( &pp_contexts[i]->lock );
    }
    for ( i = 0; i < count; i++ )
    {
        vlc_mutex_lock( &pp_contexts[i]->lock );
        while ( !pp_contexts[i]->b_done )
        {
            vlc_cond_wait( &pp_contexts[i]->cond, &pp_contexts[i]->lock );
        }
        pp_contexts[i]->b_done = 0;
        pp_contexts[i]->pf_func = NULL;
        vlc_mutex_unlock( &pp_contexts[i]->lock );

        if ( ret )
        {
            ret[i] = pp_contexts[i]->i_ret;
        }
    }

    return 0;
}
#endif

/****************************************************************************
 * EncodeVideo: the whole thing
 ****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    AVFrame frame;
    int i_out, i_plane;

#if LIBAVCODEC_BUILD >= 4702
    if ( !p_sys->b_inited && p_enc->i_threads >= 1 )
    {
        struct thread_context_t ** pp_contexts;
        int i;

        p_sys->b_inited = 1;
        pp_contexts = malloc( sizeof(struct thread_context_t *)
                                 * p_enc->i_threads );
        p_sys->p_context->thread_opaque = (void *)pp_contexts;

        for ( i = 0; i < p_enc->i_threads; i++ )
        {
            pp_contexts[i] = vlc_object_create( p_enc,
                                     sizeof(struct thread_context_t) );
            pp_contexts[i]->p_context = p_sys->p_context;
            vlc_mutex_init( p_enc, &pp_contexts[i]->lock );
            vlc_cond_init( p_enc, &pp_contexts[i]->cond );
            pp_contexts[i]->b_work = 0;
            pp_contexts[i]->b_done = 0;
            if ( vlc_thread_create( pp_contexts[i], "encoder", FfmpegThread,
                                    VLC_THREAD_PRIORITY_VIDEO, VLC_FALSE ) )
            {
                msg_Err( p_enc, "cannot spawn encoder thread, expect to die soon" );
                return NULL;
            }
        }

        p_sys->p_context->execute = FfmpegExecute;
    }
#endif

    memset( &frame, 0, sizeof( AVFrame ) );
    for( i_plane = 0; i_plane < p_pict->i_planes; i_plane++ )
    {
        frame.data[i_plane] = p_pict->p[i_plane].p_pixels;
        frame.linesize[i_plane] = p_pict->p[i_plane].i_pitch;
    }

    /* Let ffmpeg select the frame type */
    frame.pict_type = 0;

    frame.repeat_pict = 2 - p_pict->i_nb_fields;

#if LIBAVCODEC_BUILD >= 4685
    frame.interlaced_frame = !p_pict->b_progressive;
    frame.top_field_first = !!p_pict->b_top_field_first;
#endif

#if LIBAVCODEC_BUILD < 4702
    /* Set the pts of the frame being encoded (segfaults with mpeg4!)*/
    if( p_enc->fmt_out.i_codec == VLC_FOURCC( 'm', 'p', 'g', 'v' ) ||
        p_enc->fmt_out.i_codec == VLC_FOURCC( 'm', 'p', '1', 'v' ) ||
        p_enc->fmt_out.i_codec == VLC_FOURCC( 'm', 'p', '2', 'v' ) )
#else
    if( 1 )
#endif
    {
        frame.pts = p_pict->date ? p_pict->date : AV_NOPTS_VALUE;

        if ( p_sys->b_hurry_up && frame.pts != AV_NOPTS_VALUE )
        {
            mtime_t current_date = mdate();

            if ( current_date + HURRY_UP_GUARD3 > frame.pts )
            {
                p_sys->p_context->mb_decision = FF_MB_DECISION_SIMPLE;
                p_sys->p_context->flags &= ~CODEC_FLAG_TRELLIS_QUANT;
                msg_Dbg( p_enc, "hurry up mode 3" );
            }
            else
            {
                p_sys->p_context->mb_decision = p_sys->i_hq;

                if ( current_date + HURRY_UP_GUARD2 > frame.pts )
                {
                    p_sys->p_context->flags &= ~CODEC_FLAG_TRELLIS_QUANT;
#if LIBAVCODEC_BUILD >= 4690
                    p_sys->p_context->noise_reduction = p_sys->i_noise_reduction
                         + (HURRY_UP_GUARD2 + current_date - frame.pts) / 500;
#endif
                    msg_Dbg( p_enc, "hurry up mode 2" );
                }
                else
                {
                    if ( p_sys->b_trellis )
                        p_sys->p_context->flags |= CODEC_FLAG_TRELLIS_QUANT;
#if LIBAVCODEC_BUILD >= 4690
                    p_sys->p_context->noise_reduction =
                        p_sys->i_noise_reduction;
#endif
                }
            }

            if ( current_date + HURRY_UP_GUARD1 > frame.pts )
            {
                frame.pict_type = FF_P_TYPE;
                /* msg_Dbg( p_enc, "hurry up mode 1 %lld", current_date + HURRY_UP_GUARD1 - frame.pts ); */
            }
        }
    }
    else
    {
        frame.pts = AV_NOPTS_VALUE;
    }

    if ( frame.pts != AV_NOPTS_VALUE && frame.pts != 0 )
    {
        if ( p_sys->i_last_pts == frame.pts )
        {
            msg_Warn( p_enc, "almost fed libavcodec with two frames with the "
                      "same PTS (" I64Fd ")", frame.pts );
            return NULL;
        }
        else if ( p_sys->i_last_pts > frame.pts )
        {
            msg_Warn( p_enc, "almost fed libavcodec with a frame in the "
                      "past (current: " I64Fd ", last: "I64Fd")",
                      frame.pts, p_sys->i_last_pts );
            return NULL;
        }
        else
        {
            p_sys->i_last_pts = frame.pts;
        }
    }

    frame.quality = p_sys->i_quality;

    /* Ugly work-around for stupid libavcodec behaviour */
#if LIBAVCODEC_BUILD >= 4722
    p_sys->i_framenum++;
    p_sys->pi_delay_pts[p_sys->i_framenum % MAX_FRAME_DELAY] = frame.pts;
    frame.pts = p_sys->i_framenum * AV_TIME_BASE *
        p_enc->fmt_in.video.i_frame_rate_base;
    frame.pts += p_enc->fmt_in.video.i_frame_rate - 1;
    frame.pts /= p_enc->fmt_in.video.i_frame_rate;
#endif
    /* End work-around */

    i_out = avcodec_encode_video( p_sys->p_context, p_sys->p_buffer_out,
                                  AVCODEC_MAX_VIDEO_FRAME_SIZE, &frame );

    if( i_out > 0 )
    {
        block_t *p_block = block_New( p_enc, i_out );
        memcpy( p_block->p_buffer, p_sys->p_buffer_out, i_out );

        /* FIXME, 3-2 pulldown is not handled correctly */
        p_block->i_length = I64C(1000000) *
            p_enc->fmt_in.video.i_frame_rate_base /
                p_enc->fmt_in.video.i_frame_rate;

        if( !p_sys->p_context->max_b_frames || !p_sys->p_context->delay )
        {
            /* No delay -> output pts == input pts */
            p_block->i_pts = p_block->i_dts = p_pict->date;
        }
        else if( p_sys->p_context->coded_frame->pts != AV_NOPTS_VALUE &&
            p_sys->p_context->coded_frame->pts != 0 &&
            p_sys->i_buggy_pts_detect != p_sys->p_context->coded_frame->pts )
        {
            p_sys->i_buggy_pts_detect = p_sys->p_context->coded_frame->pts;
            p_block->i_pts = p_sys->p_context->coded_frame->pts;

            /* Ugly work-around for stupid libavcodec behaviour */
#if LIBAVCODEC_BUILD >= 4722
            {
            int64_t i_framenum = p_block->i_pts *
                p_enc->fmt_in.video.i_frame_rate /
                p_enc->fmt_in.video.i_frame_rate_base / AV_TIME_BASE;

            p_block->i_pts = p_sys->pi_delay_pts[i_framenum % MAX_FRAME_DELAY];
            }
#endif
            /* End work-around */

            if( p_sys->p_context->coded_frame->pict_type != FF_I_TYPE &&
                p_sys->p_context->coded_frame->pict_type != FF_P_TYPE )
            {
                p_block->i_dts = p_block->i_pts;
            }
            else
            {
                if( p_sys->i_last_ref_pts )
                {
                    p_block->i_dts = p_sys->i_last_ref_pts;
                }
                else
                {
                    /* Let's put something sensible */
                    p_block->i_dts = p_block->i_pts;
                }

                p_sys->i_last_ref_pts = p_block->i_pts;
            }
        }
        else
        {
            /* Buggy libavcodec which doesn't update coded_frame->pts
             * correctly */
            p_block->i_dts = p_block->i_pts = p_pict->date;
        }

        switch ( p_sys->p_context->coded_frame->pict_type )
        {
        case FF_I_TYPE:
            p_block->i_flags |= BLOCK_FLAG_TYPE_I;
            break;
        case FF_P_TYPE:
            p_block->i_flags |= BLOCK_FLAG_TYPE_P;
            break;
        case FF_B_TYPE:
            p_block->i_flags |= BLOCK_FLAG_TYPE_B;
            break;
        }

        return p_block;
    }

    return NULL;
}

/****************************************************************************
 * EncodeAudio: the whole thing
 ****************************************************************************/
static block_t *EncodeAudio( encoder_t *p_enc, aout_buffer_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_block, *p_chain = NULL;

    char *p_buffer = p_aout_buf->p_buffer;
    int i_samples = p_aout_buf->i_nb_samples;
    int i_samples_delay = p_sys->i_samples_delay;

    p_sys->i_pts = p_aout_buf->start_date -
                (mtime_t)1000000 * (mtime_t)p_sys->i_samples_delay /
                (mtime_t)p_enc->fmt_in.audio.i_rate;

    p_sys->i_samples_delay += i_samples;

    while( p_sys->i_samples_delay >= p_sys->p_context->frame_size )
    {
        int16_t *p_samples;
        int i_out;

        if( i_samples_delay )
        {
            /* Take care of the left-over from last time */
            int i_delay_size = i_samples_delay * 2 *
                                 p_sys->p_context->channels;
            int i_size = p_sys->i_frame_size - i_delay_size;

            p_samples = (int16_t *)p_sys->p_buffer;
            memcpy( p_sys->p_buffer + i_delay_size, p_buffer, i_size );
            p_buffer -= i_delay_size;
            i_samples += i_samples_delay;
            i_samples_delay = 0;
        }
        else
        {
            p_samples = (int16_t *)p_buffer;
        }

        i_out = avcodec_encode_audio( p_sys->p_context, p_sys->p_buffer_out,
                                      2 * AVCODEC_MAX_AUDIO_FRAME_SIZE,
                                      p_samples );

#if 0
        msg_Warn( p_enc, "avcodec_encode_audio: %d", i_out );
#endif
        if( i_out < 0 ) break;

        p_buffer += p_sys->i_frame_size;
        p_sys->i_samples_delay -= p_sys->p_context->frame_size;
        i_samples -= p_sys->p_context->frame_size;

        if( i_out == 0 ) continue;

        p_block = block_New( p_enc, i_out );
        memcpy( p_block->p_buffer, p_sys->p_buffer_out, i_out );

        p_block->i_length = (mtime_t)1000000 *
            (mtime_t)p_sys->p_context->frame_size /
            (mtime_t)p_sys->p_context->sample_rate;

        p_block->i_dts = p_block->i_pts = p_sys->i_pts;

        /* Update pts */
        p_sys->i_pts += p_block->i_length;
        block_ChainAppend( &p_chain, p_block );
    }

    /* Backup the remaining raw samples */
    if( i_samples )
    {
        memcpy( p_sys->p_buffer + i_samples_delay * 2 *
                p_sys->p_context->channels, p_buffer,
                i_samples * 2 * p_sys->p_context->channels );
    }

    return p_chain;
}

/*****************************************************************************
 * CloseEncoder: ffmpeg encoder destruction
 *****************************************************************************/
void E_(CloseEncoder)( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

#if LIBAVCODEC_BUILD >= 4702
    if ( p_sys->b_inited && p_enc->i_threads >= 1 )
    {
        int i;
        struct thread_context_t ** pp_contexts =
                (struct thread_context_t **)p_sys->p_context->thread_opaque;
        for ( i = 0; i < p_enc->i_threads; i++ )
        {
            pp_contexts[i]->b_die = 1;
            vlc_cond_signal( &pp_contexts[i]->cond );
            vlc_thread_join( pp_contexts[i] );
            vlc_mutex_destroy( &pp_contexts[i]->lock );
            vlc_cond_destroy( &pp_contexts[i]->cond );
            vlc_object_destroy( pp_contexts[i] );
        }

        free( pp_contexts );
    }
#endif

    avcodec_close( p_sys->p_context );
    av_free( p_sys->p_context );

    if( p_sys->p_buffer ) free( p_sys->p_buffer );
    if( p_sys->p_buffer_out ) free( p_sys->p_buffer_out );

    free( p_sys );
}
