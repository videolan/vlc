/*****************************************************************************
 * transcode.c: transcoding stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#if LIBAVCODEC_BUILD < 4704
#   define AV_NOPTS_VALUE 0
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("Transcode stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "transcode" );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, block_t* );

static int  transcode_audio_ffmpeg_new    ( sout_stream_t *, sout_stream_id_t * );
static void transcode_audio_ffmpeg_close  ( sout_stream_t *, sout_stream_id_t * );
static int  transcode_audio_ffmpeg_process( sout_stream_t *, sout_stream_id_t *, block_t *, block_t ** );

static int  transcode_video_ffmpeg_new    ( sout_stream_t *, sout_stream_id_t * );
static void transcode_video_ffmpeg_close  ( sout_stream_t *, sout_stream_id_t * );
static int  transcode_video_ffmpeg_process( sout_stream_t *, sout_stream_id_t *, block_t *, block_t ** );

static int  transcode_video_ffmpeg_getframebuf( struct AVCodecContext *, AVFrame *);

static int  EncoderThread( struct sout_stream_sys_t * p_sys );

static int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,   AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
};

#define PICTURE_RING_SIZE 64

struct sout_stream_sys_t
{
    VLC_COMMON_MEMBERS

    sout_stream_t * p_out;
    sout_stream_id_t * id_video;
    block_t * p_buffers;
    vlc_mutex_t     lock_out;
    vlc_cond_t      cond;
    picture_t *     pp_pics[PICTURE_RING_SIZE];
    int             i_first_pic, i_last_pic;

    vlc_fourcc_t    i_acodec;   /* codec audio (0 if not transcode) */
    int             i_sample_rate;
    int             i_channels;
    int             i_abitrate;

    vlc_fourcc_t    i_vcodec;   /*    "   video  " "   "      " */
    int             i_vbitrate;
    int             i_vtolerance;
    double          f_scale;
    int             i_width;
    int             i_height;
    int             i_b_frames;
    int             i_key_int;
    int             i_qmin;
    int             i_qmax;
    vlc_bool_t      i_hq;
    vlc_bool_t      b_deinterlace;
    vlc_bool_t      b_interlace;
    vlc_bool_t      b_strict_rc;
    vlc_bool_t      b_pre_me;
    vlc_bool_t      b_hurry_up;
    int             i_rc_buffer_size;
    float           f_rc_buffer_aggressivity;
    float           f_i_quant_factor;
    int             i_noise_reduction;
    vlc_bool_t      b_mpeg4_matrix;
    int             i_threads;
    vlc_bool_t      b_trellis;

    int             i_crop_top;
    int             i_crop_bottom;
    int             i_crop_right;
    int             i_crop_left;

    mtime_t         i_input_dts;
    mtime_t         i_input_pts;
    vlc_bool_t      b_input_has_b_frames;

    mtime_t         i_output_pts;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char *codec;

    p_sys = vlc_object_create( p_this, sizeof( sout_stream_sys_t ) );
    p_sys->p_out = sout_stream_new( p_stream->p_sout, p_stream->psz_next );

    p_sys->f_scale      = 1;
    p_sys->i_vtolerance = -1;
    p_sys->i_key_int    = -1;
    p_sys->i_qmin       = 2;
    p_sys->i_qmax       = 31;
    p_sys->f_i_quant_factor = 0.0;
#if LIBAVCODEC_BUILD >= 4673
    p_sys->i_hq         = FF_MB_DECISION_SIMPLE;
#else
    p_sys->i_hq         = VLC_FALSE;
#endif
    p_sys->i_rc_buffer_size = 224*1024*8 * 3/2;
    p_sys->f_rc_buffer_aggressivity = 0.1;
    p_sys->i_threads = 0;
    p_sys->b_trellis = 0;
    p_sys->b_input_has_b_frames = VLC_FALSE;
    p_sys->i_output_pts = 0;

    if( ( codec = sout_cfg_find_value( p_stream->p_cfg, "acodec" ) ) )
    {
        char fcc[4] = "    ";
        char *val;

        memcpy( fcc, codec, __MIN( strlen( codec ), 4 ) );

        p_sys->i_acodec = VLC_FOURCC( fcc[0], fcc[1], fcc[2], fcc[3] );

        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "samplerate" ) ) )
        {
            p_sys->i_sample_rate = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "channels" ) ) )
        {
            p_sys->i_channels = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "ab" ) ) )
        {
            p_sys->i_abitrate = atoi( val );
            if( p_sys->i_abitrate < 4000 )
            {
                p_sys->i_abitrate *= 1000;
            }
        }

        msg_Dbg( p_stream, "codec audio=%4.4s %dHz %d channels %dKb/s", fcc,
                 p_sys->i_sample_rate, p_sys->i_channels,
                 p_sys->i_abitrate / 1000 );
    }

    if( ( codec = sout_cfg_find_value( p_stream->p_cfg, "vcodec" ) ) )
    {
        char fcc[4] = "    ";
        char *val;

        memcpy( fcc, codec, __MIN( strlen( codec ), 4 ) );

        p_sys->i_vcodec = VLC_FOURCC( fcc[0], fcc[1], fcc[2], fcc[3] );

        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "scale" ) ) )
        {
            p_sys->f_scale = atof( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "width" ) ) )
        {
            p_sys->i_width = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "height" ) ) )
        {
            p_sys->i_height = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "vb" ) ) )
        {
            p_sys->i_vbitrate = atoi( val );
            if( p_sys->i_vbitrate < 16000 )
            {
                p_sys->i_vbitrate *= 1000;
            }
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "vt" ) ) )
        {
            p_sys->i_vtolerance = atoi( val );
        }
        if( sout_cfg_find( p_stream->p_cfg, "deinterlace" ) )
        {
            p_sys->b_deinterlace = VLC_TRUE;
        }
        if( sout_cfg_find( p_stream->p_cfg, "interlace" ) )
        {
            p_sys->b_interlace = VLC_TRUE;
        }
        if( sout_cfg_find( p_stream->p_cfg, "strict_rc" ) )
        {
            p_sys->b_strict_rc = VLC_TRUE;
        }
        if( sout_cfg_find( p_stream->p_cfg, "pre_me" ) )
        {
            p_sys->b_pre_me = VLC_TRUE;
        }
        if( sout_cfg_find( p_stream->p_cfg, "hurry_up" ) )
        {
            p_sys->b_hurry_up = VLC_TRUE;
            /* hurry up mode needs noise reduction, even small */
            p_sys->i_noise_reduction = 1;
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "rc_buffer_size" ) ) )
        {
            p_sys->i_rc_buffer_size = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "rc_buffer_aggressivity" ) ) )
        {
            p_sys->f_rc_buffer_aggressivity = atof( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "i_quant_factor" ) ) )
        {
            p_sys->f_i_quant_factor = atof( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "noise_reduction" ) ) )
        {
            p_sys->i_noise_reduction = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "mpeg4_matrix" ) ) )
        {
            p_sys->b_mpeg4_matrix = VLC_TRUE;
        }
        /* crop */
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "croptop" ) ) )
        {
            p_sys->i_crop_top = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "cropbottom" ) ) )
        {
            p_sys->i_crop_bottom = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "cropleft" ) ) )
        {
            p_sys->i_crop_left = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "cropright" ) ) )
        {
            p_sys->i_crop_right = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "keyint" ) ) )
        {
            p_sys->i_key_int    = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "bframes" ) ) )
        {
            p_sys->i_b_frames   = atoi( val );
        }
#if LIBAVCODEC_BUILD >= 4673
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "hq" ) ) )
        {
            if( !strcmp( val, "rd" ) )
            {
                p_sys->i_hq = FF_MB_DECISION_RD;
            }
            else if( !strcmp( val, "bits" ) )
            {
                p_sys->i_hq = FF_MB_DECISION_BITS;
            }
            else if( !strcmp( val, "simple" ) )
            {
                p_sys->i_hq = FF_MB_DECISION_SIMPLE;
            }
            else
            {
                p_sys->i_hq = FF_MB_DECISION_RD;
            }
        }
#else
        if( sout_cfg_find( p_stream->p_cfg, "hq" ) )
        {
            p_sys->i_hq = VLC_TRUE;
        }
#endif
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "qmin" ) ) )
        {
            p_sys->i_qmin   = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "qmax" ) ) )
        {
            p_sys->i_qmax   = atoi( val );
        }
        if( ( val = sout_cfg_find_value( p_stream->p_cfg, "threads" ) ) )
        {
            p_sys->i_threads = atoi( val );
        }
        if( sout_cfg_find( p_stream->p_cfg, "trellis" ) )
        {
            p_sys->b_trellis = VLC_TRUE;
        }

        msg_Dbg( p_stream, "codec video=%4.4s %dx%d scaling: %f %dkb/s",
                 fcc, p_sys->i_width, p_sys->i_height, p_sys->f_scale,
                 p_sys->i_vbitrate / 1000 );
    }

    if( !p_sys->p_out )
    {
        msg_Err( p_stream, "cannot create chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;
    p_stream->p_sys     = p_sys;

    avcodec_init();
    avcodec_register_all();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    sout_stream_delete( p_sys->p_out );
    vlc_object_destroy( p_sys );
}

struct sout_stream_id_t
{
    vlc_fourcc_t  b_transcode;
    es_format_t f_src;        /* only if transcoding */
    es_format_t f_dst;        /*  "   "      " */
    unsigned int  i_inter_pixfmt; /* intermediary format when transcoding */

    /* id of the out stream */
    void *id;

    /* Encoder */
    encoder_t       *p_encoder;
    vlc_fourcc_t    b_enc_inited;

    /* ffmpeg part */
    AVCodec         *ff_dec;
    AVCodecContext  *ff_dec_c;

    mtime_t         i_dts;
    mtime_t         i_length;

    int             i_buffer;
    int             i_buffer_pos;
    uint8_t         *p_buffer;

    AVFrame         *p_ff_pic;
    AVFrame         *p_ff_pic_tmp0; /* to do deinterlace */
    AVFrame         *p_ff_pic_tmp1; /* to do pix conversion */
    AVFrame         *p_ff_pic_tmp2; /* to do resample */

    ImgReSampleContext *p_vresample;
};


static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;
    sout_stream_id_t    *id;

    id = malloc( sizeof( sout_stream_id_t ) );
    id->i_dts = 0;
    id->id = NULL;
    id->p_encoder = NULL;

    if( p_fmt->i_cat == AUDIO_ES && p_sys->i_acodec != 0 )
    {
        msg_Dbg( p_stream,
                 "creating audio transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
                 (char*)&p_fmt->i_codec,
                 (char*)&p_sys->i_acodec );

        /* src format */
        memcpy( &id->f_src, p_fmt, sizeof( es_format_t ) );

        /* create dst format */
        es_format_Init( &id->f_dst, AUDIO_ES, p_sys->i_acodec );
        id->f_dst.i_id    = id->f_src.i_id;
        id->f_dst.i_group = id->f_src.i_group;
        if( id->f_src.psz_language ) id->f_dst.psz_language = strdup( id->f_src.psz_language );
        id->f_dst.audio.i_rate = p_sys->i_sample_rate  > 0 ? p_sys->i_sample_rate : id->f_src.audio.i_rate;
        id->f_dst.audio.i_channels    = p_sys->i_channels > 0 ? p_sys->i_channels : id->f_src.audio.i_channels;
        id->f_dst.i_bitrate     = p_sys->i_abitrate > 0 ? p_sys->i_abitrate : 64000;
        id->f_dst.audio.i_blockalign = 0;
        id->f_dst.i_extra  = 0;
        id->f_dst.p_extra  = NULL;

        /* build decoder -> filter -> encoder */
        if( transcode_audio_ffmpeg_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create audio chain" );
            free( id );
            return NULL;
        }

        /* open output stream */
        id->id = p_sys->p_out->pf_add( p_sys->p_out, &id->f_dst );
        id->b_transcode = VLC_TRUE;

        if( id->id == NULL )
        {
            free( id );
            return NULL;
        }
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_sys->i_vcodec != 0 )
    {
        msg_Dbg( p_stream,
                 "creating video transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
                 (char*)&p_fmt->i_codec,
                 (char*)&p_sys->i_vcodec );

        memcpy( &id->f_src, p_fmt, sizeof( es_format_t ) );

        /* create dst format */
        es_format_Init( &id->f_dst, VIDEO_ES, p_sys->i_vcodec );
        id->f_dst.i_id    = id->f_src.i_id;
        id->f_dst.i_group = id->f_src.i_group;
        if( id->f_src.psz_language ) id->f_dst.psz_language = strdup( id->f_src.psz_language );
        id->f_dst.video.i_width = p_sys->i_width;
        id->f_dst.video.i_height= p_sys->i_height;
        id->f_dst.i_bitrate     = p_sys->i_vbitrate > 0 ? p_sys->i_vbitrate : 800*1000;
        id->f_dst.i_extra       = 0;
        id->f_dst.p_extra       = NULL;

        /* build decoder -> filter -> encoder */
        if( transcode_video_ffmpeg_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create video chain" );
            free( id );
            return NULL;
        }
#if 0
        /* open output stream */
        id->id = p_sys->p_out->pf_add( p_sys->p_out, &id->f_dst );
#endif
        id->b_transcode = VLC_TRUE;
    }
    else
    {
        msg_Dbg( p_stream, "not transcoding a stream (fcc=`%4.4s')", (char*)&p_fmt->i_codec );
        id->id = p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
        id->b_transcode = VLC_FALSE;

        if( id->id == NULL )
        {
            free( id );
            return NULL;
        }
    }

    return id;
}

static int     Del      ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if( id->b_transcode )
    {
        if( id->f_src.i_cat == AUDIO_ES )
        {
            transcode_audio_ffmpeg_close( p_stream, id );
        }
        else if( id->f_src.i_cat == VIDEO_ES )
        {
            transcode_video_ffmpeg_close( p_stream, id );
        }
    }

    if( id->id ) p_sys->p_out->pf_del( p_sys->p_out, id->id );
    free( id );

    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if( id->b_transcode )
    {
        block_t *p_buffer_out;

        /* Be sure to have padding */
        p_buffer = block_Realloc( p_buffer, 0, p_buffer->i_buffer + FF_INPUT_BUFFER_PADDING_SIZE );
        if( p_buffer == NULL )
        {
            return VLC_EGENERIC;
        }
        p_buffer->i_buffer -= FF_INPUT_BUFFER_PADDING_SIZE;
        memset( &p_buffer->p_buffer[p_buffer->i_buffer], 0, FF_INPUT_BUFFER_PADDING_SIZE );

        if( id->f_src.i_cat == AUDIO_ES )
        {
            transcode_audio_ffmpeg_process( p_stream, id, p_buffer,
                                            &p_buffer_out );
        }
        else if( id->f_src.i_cat == VIDEO_ES )
        {
            if( transcode_video_ffmpeg_process( p_stream, id, p_buffer,
                &p_buffer_out ) != VLC_SUCCESS )
            {
                block_Release( p_buffer );
                return VLC_EGENERIC;
            }
        }
        block_Release( p_buffer );

        if( p_buffer_out )
        {
            return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer_out );
        }
        return VLC_SUCCESS;
    }
    else if( id->id != NULL )
    {
        return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer );
    }
    else
    {
        block_Release( p_buffer );
        return VLC_EGENERIC;
    }
}

/****************************************************************************
 * ffmpeg decoder reencocdr part
 ****************************************************************************/

static struct
{
    vlc_fourcc_t i_fcc;
    int          i_ff_codec;
} fourcc_to_ff_code[] =
{
    /* audio */
    { VLC_FOURCC( 'm', 'p', 'g', 'a' ), CODEC_ID_MP2 },
    { VLC_FOURCC( 'm', 'p', '3', ' ' ), CODEC_ID_MP3LAME },
    { VLC_FOURCC( 'm', 'p', '4', 'a' ), CODEC_ID_AAC },
    { VLC_FOURCC( 'a', '5', '2', ' ' ), CODEC_ID_AC3 },
    { VLC_FOURCC( 'a', 'c', '3', ' ' ), CODEC_ID_AC3 },
    { VLC_FOURCC( 'w', 'm', 'a', '1' ), CODEC_ID_WMAV1 },
    { VLC_FOURCC( 'w', 'm', 'a', '2' ), CODEC_ID_WMAV2 },
    { VLC_FOURCC( 'v', 'o', 'r', 'b' ), CODEC_ID_VORBIS },
    { VLC_FOURCC( 'a', 'l', 'a', 'w' ), CODEC_ID_PCM_ALAW },

    /* video */
    { VLC_FOURCC( 'm', 'p', 'g', 'v' ), CODEC_ID_MPEG1VIDEO },
    { VLC_FOURCC( 'm', 'p', '1', 'v' ), CODEC_ID_MPEG1VIDEO },
#if LIBAVCODEC_BUILD >= 4676
    { VLC_FOURCC( 'm', 'p', '2', 'v' ), CODEC_ID_MPEG2VIDEO },
#endif
    { VLC_FOURCC( 'm', 'p', '4', 'v'),  CODEC_ID_MPEG4 },
    { VLC_FOURCC( 'D', 'I', 'V', '1' ), CODEC_ID_MSMPEG4V1 },
    { VLC_FOURCC( 'D', 'I', 'V', '2' ), CODEC_ID_MSMPEG4V2 },
    { VLC_FOURCC( 'D', 'I', 'V', '3' ), CODEC_ID_MSMPEG4V3 },
    { VLC_FOURCC( 'H', '2', '6', '3' ), CODEC_ID_H263 },
    { VLC_FOURCC( 'I', '2', '6', '3' ), CODEC_ID_H263I },
    { VLC_FOURCC( 'h', 'u', 'f', 'f' ), CODEC_ID_HUFFYUV },
    { VLC_FOURCC( 'W', 'M', 'V', '1' ), CODEC_ID_WMV1 },
    { VLC_FOURCC( 'W', 'M', 'V', '2' ), CODEC_ID_WMV2 },
    { VLC_FOURCC( 'M', 'J', 'P', 'G' ), CODEC_ID_MJPEG },
    { VLC_FOURCC( 'm', 'j', 'p', 'b' ), CODEC_ID_MJPEGB },
    { VLC_FOURCC( 'd', 'v', 's', 'l' ), CODEC_ID_DVVIDEO },
    { VLC_FOURCC( 'S', 'V', 'Q', '1' ), CODEC_ID_SVQ1 },
#if LIBAVCODEC_BUILD >= 4666
    { VLC_FOURCC( 'S', 'V', 'Q', '3' ), CODEC_ID_SVQ3 },
#endif

    /* raw video code, only used for 'encoding' */
    { VLC_FOURCC( 'I', '4', '2', '0' ), CODEC_ID_RAWVIDEO },
    { VLC_FOURCC( 'I', '4', '2', '2' ), CODEC_ID_RAWVIDEO },
    { VLC_FOURCC( 'I', '4', '4', '4' ), CODEC_ID_RAWVIDEO },
    { VLC_FOURCC( 'R', 'V', '1', '5' ), CODEC_ID_RAWVIDEO },
    { VLC_FOURCC( 'R', 'V', '1', '6' ), CODEC_ID_RAWVIDEO },
    { VLC_FOURCC( 'R', 'V', '2', '4' ), CODEC_ID_RAWVIDEO },
    { VLC_FOURCC( 'R', 'V', '3', '2' ), CODEC_ID_RAWVIDEO },
    { VLC_FOURCC( 'Y', 'U', 'Y', '2' ), CODEC_ID_RAWVIDEO },
    { VLC_FOURCC( 'Y', 'V', '1', '2' ), CODEC_ID_RAWVIDEO },
    { VLC_FOURCC( 'I', 'Y', 'U', 'V' ), CODEC_ID_RAWVIDEO },

    { VLC_FOURCC(   0,   0,   0,   0 ), 0 }
};

static inline int get_ff_codec( vlc_fourcc_t i_fcc )
{
    int i;

    for( i = 0; fourcc_to_ff_code[i].i_fcc != 0; i++ )
    {
        if( fourcc_to_ff_code[i].i_fcc == i_fcc )
        {
            return fourcc_to_ff_code[i].i_ff_codec;
        }
    }

    return 0;
}

static inline int get_ff_chroma( vlc_fourcc_t i_chroma )
{
    switch( i_chroma )
    {
        case VLC_FOURCC( 'Y', 'V', '1', '2' ):
        case VLC_FOURCC( 'I', 'Y', 'U', 'V' ):
        case VLC_FOURCC( 'I', '4', '2', '0' ):
            return PIX_FMT_YUV420P;
        case VLC_FOURCC( 'I', '4', '2', '2' ):
            return PIX_FMT_YUV422P;
        case VLC_FOURCC( 'I', '4', '4', '4' ):
            return PIX_FMT_YUV444P;
        case VLC_FOURCC( 'R', 'V', '1', '5' ):
            return PIX_FMT_RGB555;
        case VLC_FOURCC( 'R', 'V', '1', '6' ):
            return PIX_FMT_RGB565;
        case VLC_FOURCC( 'R', 'V', '2', '4' ):
            return PIX_FMT_BGR24;
        case VLC_FOURCC( 'R', 'V', '3', '2' ):
            return PIX_FMT_RGBA32;
        case VLC_FOURCC( 'G', 'R', 'E', 'Y' ):
            return PIX_FMT_GRAY8;
        case VLC_FOURCC( 'Y', 'U', 'Y', '2' ):
            return PIX_FMT_YUV422;
        default:
            return 0;
    }
}

static inline vlc_fourcc_t get_vlc_chroma( int i_pix_fmt )
{
    switch( i_pix_fmt )
    {
    case PIX_FMT_YUV420P:
        return VLC_FOURCC('I','4','2','0');
    case PIX_FMT_YUV422P:
        return VLC_FOURCC('I','4','2','2');
    case PIX_FMT_YUV444P:
        return VLC_FOURCC('I','4','4','4');

    case PIX_FMT_YUV422:
        return VLC_FOURCC('Y','U','Y','2');

    case PIX_FMT_RGB555:
        return VLC_FOURCC('R','V','1','5');
    case PIX_FMT_RGB565:
        return VLC_FOURCC('R','V','1','6');
    case PIX_FMT_RGB24:
        return VLC_FOURCC('R','V','2','4');
    case PIX_FMT_RGBA32:
        return VLC_FOURCC('R','V','3','2');
    case PIX_FMT_GRAY8:
        return VLC_FOURCC('G','R','E','Y');

    case PIX_FMT_YUV410P:
    case PIX_FMT_YUV411P:
    case PIX_FMT_BGR24:
    default:
        return 0;
    }
}

static int transcode_audio_ffmpeg_new( sout_stream_t *p_stream,
                                       sout_stream_id_t *id )
{
    int i_ff_codec;

    if( id->f_src.i_codec == VLC_FOURCC('s','1','6','l') ||
        id->f_src.i_codec == VLC_FOURCC('s','1','6','b') ||
        id->f_src.i_codec == VLC_FOURCC('s','8',' ',' ') ||
        id->f_src.i_codec == VLC_FOURCC('u','8',' ',' ') )
    {
        id->ff_dec = NULL;

        id->ff_dec_c = avcodec_alloc_context();
        id->ff_dec_c->sample_rate = id->f_src.audio.i_rate;
        id->ff_dec_c->channels    = id->f_src.audio.i_channels;
        id->ff_dec_c->block_align = id->f_src.audio.i_blockalign;
        id->ff_dec_c->bit_rate    = id->f_src.i_bitrate;
    }
    else
    {
        /* find decoder */
        i_ff_codec = get_ff_codec( id->f_src.i_codec );
        if( i_ff_codec == 0 )
        {
            msg_Err( p_stream, "cannot find decoder id" );
            return VLC_EGENERIC;
        }

        id->ff_dec = avcodec_find_decoder( i_ff_codec );
        if( !id->ff_dec )
        {
            msg_Err( p_stream, "cannot find decoder (avcodec)" );
            return VLC_EGENERIC;
        }

        id->ff_dec_c = avcodec_alloc_context();
        id->ff_dec_c->sample_rate = id->f_src.audio.i_rate;
        id->ff_dec_c->channels    = id->f_src.audio.i_channels;
        id->ff_dec_c->block_align = id->f_src.audio.i_blockalign;
        id->ff_dec_c->bit_rate    = id->f_src.i_bitrate;

        id->ff_dec_c->extradata_size = id->f_src.i_extra;
        id->ff_dec_c->extradata      = id->f_src.p_extra;
        if( avcodec_open( id->ff_dec_c, id->ff_dec ) )
        {
            msg_Err( p_stream, "cannot open decoder" );
            return VLC_EGENERIC;
        }
    }

    id->i_buffer     = 2 * AVCODEC_MAX_AUDIO_FRAME_SIZE;
    id->i_buffer_pos = 0;
    id->p_buffer     = malloc( id->i_buffer );

    /* Sanity check for audio channels */
    id->f_dst.audio.i_channels = __MIN( id->f_dst.audio.i_channels, id->f_src.audio.i_channels );

    /* find encoder */
    id->p_encoder = vlc_object_create( p_stream, VLC_OBJECT_ENCODER );

    /* Initialization of encoder format structures */
    es_format_Init( &id->p_encoder->fmt_in, AUDIO_ES, AOUT_FMT_S16_NE );
    id->p_encoder->fmt_in.audio.i_format = AOUT_FMT_S16_NE;
    id->p_encoder->fmt_in.audio.i_rate = id->f_dst.audio.i_rate;
    id->p_encoder->fmt_in.audio.i_physical_channels =
        id->p_encoder->fmt_in.audio.i_original_channels =
            pi_channels_maps[id->f_dst.audio.i_channels];
    id->p_encoder->fmt_in.audio.i_channels = id->f_dst.audio.i_channels;

    id->p_encoder->fmt_out = id->p_encoder->fmt_in;
    id->p_encoder->fmt_out.i_codec = id->f_dst.i_codec;
    id->p_encoder->fmt_out.i_bitrate = id->f_dst.i_bitrate;

    id->p_encoder->p_module =
        module_Need( id->p_encoder, "encoder", NULL, 0 );
    if( !id->p_encoder->p_module )
    {
        vlc_object_destroy( id->p_encoder );
        msg_Err( p_stream, "cannot open encoder" );
        return VLC_EGENERIC;
    }

    id->b_enc_inited = VLC_FALSE;

    id->f_dst.audio.i_channels = id->p_encoder->fmt_in.audio.i_channels;
    id->f_dst.audio.i_rate     = id->p_encoder->fmt_in.audio.i_rate;
    id->f_dst.i_extra = id->p_encoder->fmt_out.i_extra;
    id->f_dst.p_extra = id->p_encoder->fmt_out.p_extra;

    /* Hack for mp3 transcoding support */
    if( id->f_dst.i_codec == VLC_FOURCC( 'm','p','3',' ' ) )
    {
        id->f_dst.i_codec = VLC_FOURCC( 'm','p','g','a' );
    }

    return VLC_SUCCESS;
}

static void transcode_audio_ffmpeg_close( sout_stream_t *p_stream,
                                          sout_stream_id_t *id )
{
    if( id->ff_dec ) avcodec_close( id->ff_dec_c );
    av_free( id->ff_dec_c );

    module_Unneed( id->p_encoder, id->p_encoder->p_module );
    vlc_object_destroy( id->p_encoder );

    free( id->p_buffer );
}

static int transcode_audio_ffmpeg_process( sout_stream_t *p_stream,
                                           sout_stream_id_t *id,
                                           block_t *in,
                                           block_t **out )
{
    aout_buffer_t aout_buf;
    block_t *p_block;
    int i_buffer = in->i_buffer;
    char *p_buffer = in->p_buffer;
    id->i_dts = in->i_dts;
    *out = NULL;

    while( i_buffer )
    {
        id->i_buffer_pos = 0;

        /* decode as much data as possible */
        if( id->ff_dec )
        {
            int i_used;

            i_used = avcodec_decode_audio( id->ff_dec_c,
                         (int16_t*)id->p_buffer, &id->i_buffer_pos,
                         p_buffer, i_buffer );

#if 0
            msg_Warn( p_stream, "avcodec_decode_audio: %d used on %d",
                      i_used, i_buffer );
#endif
            if( i_used < 0 )
            {
                msg_Warn( p_stream, "error audio decoding");
                break;
            }

            i_buffer -= i_used;
            p_buffer += i_used;

            if ( id->i_buffer_pos < 0 )
            {
                msg_Warn( p_stream, "weird error audio decoding");
                break;
            }
        }
        else
        {
            int16_t *sout = (int16_t*)id->p_buffer;

            if( id->f_src.i_codec == VLC_FOURCC( 's', '8', ' ', ' ' ) ||
                id->f_src.i_codec == VLC_FOURCC( 'u', '8', ' ', ' ' ) )
            {
                int8_t *sin = (int8_t*)p_buffer;
                int i_used = __MIN( id->i_buffer/2, i_buffer );
                int i_samples = i_used;

                if( id->f_src.i_codec == VLC_FOURCC( 's', '8', ' ', ' ' ) )
                    while( i_samples > 0 )
                    {
                        *sout++ = ( *sin++ ) << 8;
                        i_samples--;
                    }
                else
                    while( i_samples > 0 )
                    {
                        *sout++ = ( *sin++ - 128 ) << 8;
                        i_samples--;
                    }

                i_buffer -= i_used;
                p_buffer += i_used;
                id->i_buffer_pos = i_used * 2;
            }
            else if( id->f_src.i_codec == VLC_FOURCC( 's', '1', '6', 'l' ) ||
                     id->f_src.i_codec == VLC_FOURCC( 's', '1', '6', 'b' ) )
            {
                int16_t *sin = (int16_t*)p_buffer;
                int i_used = __MIN( id->i_buffer, i_buffer );
                int i_samples = i_used / 2;

                /* first copy */
                memcpy( sout, sin, i_used );

#ifdef WORDS_BIGENDIAN
                if( id->f_src.i_codec == VLC_FOURCC( 's', '1', '6', 'l' ) )
#else
                if( id->f_src.i_codec == VLC_FOURCC( 's', '1', '6', 'b' ) )
#endif
                {
                    uint8_t *dat = (uint8_t*)sout;

                    while( i_samples > 0 )
                    {
                        uint8_t tmp;
                        tmp    = dat[0];
                        dat[0] = dat[1];
                        dat[1] = tmp;

                        dat += 2;

                        i_samples--;
                    }
                }

                i_buffer -= i_used;
                p_buffer += i_used;
                id->i_buffer_pos = i_used;
            }
        }

        if( id->i_buffer_pos == 0 ) continue;

        /* Encode as much data as possible */
        if( !id->b_enc_inited && id->p_encoder->pf_header )
        {
            p_block = id->p_encoder->pf_header( id->p_encoder );
            while( p_block )
            {
                block_t *p_out;
                block_t *p_prev_block = p_block;

                p_out = block_New( p_stream, p_block->i_buffer );
                memcpy( p_out->p_buffer, p_block->p_buffer, p_block->i_buffer);
                p_out->i_dts = p_out->i_pts = in->i_dts;
                p_out->i_length = 0;
                block_ChainAppend( out, p_out );

                p_block = p_block->p_next;
                block_Release( p_prev_block );
            }

            id->b_enc_inited = VLC_TRUE;
        }

        aout_buf.p_buffer = id->p_buffer;
        aout_buf.i_nb_bytes = id->i_buffer_pos;
        aout_buf.i_nb_samples = id->i_buffer_pos / 2 / id->f_src.audio.i_channels;
        aout_buf.start_date = id->i_dts;
        aout_buf.end_date = id->i_dts;

        id->i_dts += ( I64C(1000000) * id->i_buffer_pos / 2 /
            id->f_src.audio.i_channels / id->f_src.audio.i_rate );

        if( id->p_encoder->fmt_in.audio.i_channels == 1 &&
            id->f_src.audio.i_channels > 1 )
        {
            int16_t *p_sample = (int16_t *)aout_buf.p_buffer;
            int i_src_c = id->f_src.audio.i_channels;
            unsigned int i;

            for( i = 0; i < aout_buf.i_nb_samples; i++ )
            {
                int j, c = 0;

                for( j = 1; j < i_src_c; j++ )
                {
                    c += p_sample[i_src_c * i + j];
                }
                p_sample[i] = c / (i_src_c-1);
            }
            aout_buf.i_nb_bytes = i * 2;
        }
        else if( id->p_encoder->fmt_in.audio.i_channels == 2 &&
                 id->f_src.audio.i_channels > 2 )
        {
            int i_src_c = id->f_src.audio.i_channels;
            unsigned int i;

            static const float mixf_l[4][6] = /* [i_src_c - 3][channel index] */
            {
                { 0.00, 1.00, 0.00, 0.00, 0.00, 0.00 }, /* 3 channels */
                { 0.00, 0.50, 0.50, 0.00, 0.00, 0.00 }, /* 4 channels */
                { 0.00, 0.50, 0.00, 0.50, 0.00, 0.00 }, /* 5 channels */
                { 0.00, 0.34, 0.33, 0.00, 0.33, 0.00 }, /* 6 channels */
            };
            static const float mixf_r[4][6] = /* [i_src_c - 3][channel index] */
            {
                { 0.00, 1.00, 0.00, 0.00, 0.00, 0.00 }, /* 3 channels */
                { 0.00, 0.00, 0.50, 0.50, 0.00, 0.00 }, /* 4 channels */
                { 0.00, 0.00, 0.50, 0.00, 0.50, 0.00 }, /* 5 channels */
                { 0.00, 0.00, 0.33, 0.34, 0.00, 0.33 }, /* 6 channels */
            };


            for( i = 0; i < aout_buf.i_nb_samples; i++ )
            {
                int16_t *p_src = (int16_t *)aout_buf.p_buffer + i_src_c * i;
                int16_t *p_dst = (int16_t *)aout_buf.p_buffer + 2 * i;

                int j;
                float l = 0.0, r = 0.0;
                for( j = 0; j < i_src_c; j++ )
                {
                    l += mixf_l[i_src_c-3][j] * p_src[j];
                    r += mixf_r[i_src_c-3][j] * p_src[j];
                }

                p_dst[0] = (int)( l + 0.5 );
                p_dst[1] = (int)( r + 0.5 );
            }
            aout_buf.i_nb_bytes = i * 2 * 2;
        }
        else if( id->f_src.audio.i_channels !=
                 id->p_encoder->fmt_in.audio.i_channels )
        {
            unsigned int i;
            int j;

            /* This is for liba52 which is what ffmpeg uses to decode ac3 */
            static const int translation[7][6] =
            {{ 0, 0, 0, 0, 0, 0 },      /* 0 channels (rarely used) */
             { 0, 0, 0, 0, 0, 0 },       /* 1 ch */
             { 0, 1, 0, 0, 0, 0 },       /* 2 */
             { 1, 2, 0, 0, 0, 0 },       /* 3 */
             { 1, 3, 2, 0, 0, 0 },       /* 4 */
             { 1, 3, 4, 2, 0, 0 },       /* 5 */
             { 1, 3, 4, 5, 2, 0 }};      /* 6 */

            /* dumb downmixing */
            for( i = 0; i < aout_buf.i_nb_samples; i++ )
            {
                uint16_t *p_buffer = (uint16_t *)aout_buf.p_buffer;
                for( j = 0 ; j < id->p_encoder->fmt_in.audio.i_channels; j++ )
                {
                    p_buffer[i*id->p_encoder->fmt_in.audio.i_channels+j] =
                        p_buffer[i*id->f_src.audio.i_channels+
                                 translation[id->f_src.audio.i_channels][j]];
                }
            }
            aout_buf.i_nb_bytes = i*id->p_encoder->fmt_in.audio.i_channels * 2;
        }

        p_block = id->p_encoder->pf_encode_audio( id->p_encoder, &aout_buf );
        while( p_block )
        {
            block_t *p_out;
            block_t *p_prev_block = p_block;

            p_out = block_New( p_stream, p_block->i_buffer );
            memcpy( p_out->p_buffer, p_block->p_buffer, p_block->i_buffer);
            p_out->i_dts = p_block->i_dts;
            p_out->i_pts = p_block->i_pts;
            p_out->i_length = p_block->i_length;
            block_ChainAppend( out, p_out );

            p_block = p_block->p_next;
            block_Release( p_prev_block );
        }
    }

    return VLC_SUCCESS;
}


/*
 * video
 */
static int transcode_video_ffmpeg_new( sout_stream_t *p_stream,
                                       sout_stream_id_t *id )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    int i_ff_codec;

    /* Open decoder */
    if( id->f_src.i_codec == VLC_FOURCC( 'I', '4', '2', '0' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'I', '4', '2', '2' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'I', '4', '4', '4' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'Y', 'V', '1', '2' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'Y', 'U', 'Y', '2' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'I', 'Y', 'U', 'V' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'R', 'V', '1', '5' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'R', 'V', '1', '6' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'R', 'V', '2', '4' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'R', 'V', '3', '2' ) ||
        id->f_src.i_codec == VLC_FOURCC( 'G', 'R', 'E', 'Y' ) )
    {
        id->ff_dec              = NULL;
        id->ff_dec_c            = avcodec_alloc_context();
        id->ff_dec_c->width     = id->f_src.video.i_width;
        id->ff_dec_c->height    = id->f_src.video.i_height;
        id->ff_dec_c->pix_fmt   = get_ff_chroma( id->f_src.i_codec );

#if LIBAVCODEC_BUILD >= 4687
        if( id->ff_dec_c->width )
        id->ff_dec_c->sample_aspect_ratio =
            av_d2q( id->f_src.video.i_aspect / (double)VOUT_ASPECT_FACTOR *
                    id->ff_dec_c->height / id->ff_dec_c->width, 255 );
#else
        id->ff_dec_c->aspect_ratio =
            id->f_src.video.i_aspect / (float)VOUT_ASPECT_FACTOR;
#endif
    }
    else
    {
        /* find decoder */
        i_ff_codec = get_ff_codec( id->f_src.i_codec );
        if( i_ff_codec == 0 )
        {
            msg_Err( p_stream, "cannot find decoder" );
            return VLC_EGENERIC;
        }

        id->ff_dec = avcodec_find_decoder( i_ff_codec );
        if( !id->ff_dec )
        {
            msg_Err( p_stream, "cannot find decoder" );
            return VLC_EGENERIC;
        }

        id->ff_dec_c = avcodec_alloc_context();
        id->ff_dec_c->width         = id->f_src.video.i_width;
        id->ff_dec_c->height        = id->f_src.video.i_height;
        id->ff_dec_c->bits_per_sample=id->f_src.video.i_bits_per_pixel;
        /* id->ff_dec_c->bit_rate      = id->f_src.i_bitrate; */

        if( id->f_src.i_extra > 0 )
        {
            if( i_ff_codec == CODEC_ID_SVQ3 )
            {
                int i_size = id->f_src.i_extra;
                uint8_t *p;

                id->ff_dec_c->extradata_size = i_size + 12;
                p = id->ff_dec_c->extradata  = malloc( i_size + 12 );

                memcpy( &p[0],  "SVQ3", 4 );
                memset( &p[4], 0, 8 );
                memcpy( &p[12], id->f_src.p_extra, i_size );

                /* Now remove all atoms before the SMI one */
                if( id->ff_dec_c->extradata_size > 0x5a && strncmp( &p[0x56], "SMI ", 4 ) )
                {
                    uint8_t *psz = &p[0x52];

                    while( psz < &p[id->ff_dec_c->extradata_size - 8] )
                    {
                        int i_size = GetDWBE( psz );
                        if( i_size <= 1 )
                        {
                            /* FIXME handle 1 as long size */
                            break;
                        }
                        if( !strncmp( &psz[4], "SMI ", 4 ) )
                        {
                            memmove( &p[0x52], psz, &p[id->ff_dec_c->extradata_size] - psz );
                            break;
                        }
                        psz += i_size;
                    }
                }
            }
            else
            {
                id->ff_dec_c->extradata_size= id->f_src.i_extra;
                id->ff_dec_c->extradata = malloc( id->f_src.i_extra + FF_INPUT_BUFFER_PADDING_SIZE );

                memcpy( id->ff_dec_c->extradata, id->f_src.p_extra, id->f_src.i_extra );
                memset( (uint8_t*)id->ff_dec_c->extradata + id->f_src.i_extra, 0, FF_INPUT_BUFFER_PADDING_SIZE );
            }
        }
        id->ff_dec_c->workaround_bugs = FF_BUG_AUTODETECT;
        id->ff_dec_c->error_resilience= -1;
        id->ff_dec_c->get_buffer    = transcode_video_ffmpeg_getframebuf;
        id->ff_dec_c->opaque        = p_sys;

        if( avcodec_open( id->ff_dec_c, id->ff_dec ) < 0 )
        {
            msg_Err( p_stream, "cannot open decoder" );
            return VLC_EGENERIC;
        }
    }

    /* Open encoder */
    id->p_encoder = vlc_object_create( p_stream, VLC_OBJECT_ENCODER );

    /* Initialization of encoder format structures */
    es_format_Init( &id->p_encoder->fmt_in,
                    id->f_src.i_cat, get_vlc_chroma(id->ff_dec_c->pix_fmt) );

    /* The dimensions will be set properly later on.
     * Just put sensible values so we can test if there is an encoder. */
    id->p_encoder->fmt_in.video.i_width =
        id->f_src.video.i_width ?  id->f_src.video.i_width : 16;
    id->p_encoder->fmt_in.video.i_height =
        id->f_src.video.i_height ? id->f_src.video.i_height : 16;

    id->p_encoder->fmt_in.video.i_frame_rate = 25; /* FIXME as it break mpeg */
    id->p_encoder->fmt_in.video.i_frame_rate_base= 1;
    if( id->ff_dec )
    {
        id->p_encoder->fmt_in.video.i_frame_rate = id->ff_dec_c->frame_rate;
#if LIBAVCODEC_BUILD >= 4662
        id->p_encoder->fmt_in.video.i_frame_rate_base =
            id->ff_dec_c->frame_rate_base;
#endif

#if LIBAVCODEC_BUILD >= 4687
        if( id->ff_dec_c->height )
        id->p_encoder->fmt_in.video.i_aspect = VOUT_ASPECT_FACTOR *
            ( av_q2d(id->ff_dec_c->sample_aspect_ratio) *
              id->ff_dec_c->width / id->ff_dec_c->height );
#else
        id->p_encoder->fmt_in.video.i_aspect = VOUT_ASPECT_FACTOR *
            id->ff_dec_c->aspect_ratio;
#endif
    }

    /* Check whether a particular aspect ratio was requested */
    if( id->f_src.video.i_aspect )
    {
        id->p_encoder->fmt_in.video.i_aspect = id->f_src.video.i_aspect;
        id->f_dst.video.i_aspect = id->f_src.video.i_aspect;
    }

    id->p_encoder->fmt_out = id->p_encoder->fmt_in;
    id->p_encoder->fmt_out.i_codec = id->f_dst.i_codec;
    id->p_encoder->fmt_out.i_bitrate = id->f_dst.i_bitrate;

    id->p_encoder->i_vtolerance = p_sys->i_vtolerance;
    id->p_encoder->i_key_int = p_sys->i_key_int;
    id->p_encoder->i_b_frames = p_sys->i_b_frames;
    id->p_encoder->i_qmin = p_sys->i_qmin;
    id->p_encoder->i_qmax = p_sys->i_qmax;
    id->p_encoder->i_hq = p_sys->i_hq;
    id->p_encoder->b_strict_rc = p_sys->b_strict_rc;
    id->p_encoder->b_pre_me = p_sys->b_pre_me;
    id->p_encoder->b_hurry_up = p_sys->b_hurry_up;
    id->p_encoder->b_interlace = p_sys->b_interlace;
    id->p_encoder->i_rc_buffer_size = p_sys->i_rc_buffer_size;
    id->p_encoder->f_rc_buffer_aggressivity = p_sys->f_rc_buffer_aggressivity;
    id->p_encoder->f_i_quant_factor = p_sys->f_i_quant_factor;
    id->p_encoder->i_noise_reduction = p_sys->i_noise_reduction;
    id->p_encoder->b_mpeg4_matrix = p_sys->b_mpeg4_matrix;
    id->p_encoder->i_threads = p_sys->i_threads;
    id->p_encoder->b_trellis = p_sys->b_trellis;

    id->p_ff_pic         = avcodec_alloc_frame();
    id->p_ff_pic_tmp0    = NULL;
    id->p_ff_pic_tmp1    = NULL;
    id->p_ff_pic_tmp2    = NULL;
    id->p_vresample      = NULL;

    id->p_encoder->p_module =
        module_Need( id->p_encoder, "encoder", NULL, 0 );

    if( !id->p_encoder->p_module )
    {
        vlc_object_destroy( id->p_encoder );
        msg_Err( p_stream, "cannot find encoder" );
        return VLC_EGENERIC;
    }

    /* Close the encoder.
     * We'll open it only when we have the first frame */
    module_Unneed( id->p_encoder, id->p_encoder->p_module );
    id->p_encoder->p_module = NULL;

    id->b_enc_inited = VLC_FALSE;

    if ( p_sys->i_threads >= 1 )
    {
        p_sys->id_video = id;
        vlc_mutex_init( p_stream, &p_sys->lock_out );
        vlc_cond_init( p_stream, &p_sys->cond );
        memset( p_sys->pp_pics, 0, sizeof(p_sys->pp_pics) );
        p_sys->i_first_pic = 0;
        p_sys->i_last_pic = 0;
        p_sys->p_buffers = NULL;
        p_sys->b_die = p_sys->b_error = 0;
        if( vlc_thread_create( p_sys, "encoder", EncoderThread,
                               VLC_THREAD_PRIORITY_VIDEO, VLC_FALSE ) )
        {
            vlc_object_destroy( id->p_encoder );
            msg_Err( p_stream, "cannot spawn encoder thread" );
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static void transcode_video_ffmpeg_close ( sout_stream_t *p_stream,
                                           sout_stream_id_t *id )
{
    if ( p_stream->p_sys->i_threads >= 1 )
    {
       vlc_mutex_lock( &p_stream->p_sys->lock_out );
       p_stream->p_sys->b_die = 1;
       vlc_cond_signal( &p_stream->p_sys->cond );
       vlc_mutex_unlock( &p_stream->p_sys->lock_out );
       vlc_thread_join( p_stream->p_sys );
       vlc_mutex_destroy( &p_stream->p_sys->lock_out );
       vlc_cond_destroy( &p_stream->p_sys->cond );
    }

    /* Close decoder */
    if( id->ff_dec ) avcodec_close( id->ff_dec_c );
    av_free( id->ff_dec_c );

    /* Close encoder */
    if( id->p_encoder->p_module )
        module_Unneed( id->p_encoder, id->p_encoder->p_module );
    vlc_object_destroy( id->p_encoder );

    /* Misc cleanup */
    if( id->p_ff_pic)
    {
        free( id->p_ff_pic );
    }

    if( id->p_ff_pic_tmp0 )
    {
        free( id->p_ff_pic_tmp0->data[0] );
        free( id->p_ff_pic_tmp0 );
    }
    if( id->p_ff_pic_tmp1)
    {
        free( id->p_ff_pic_tmp1->data[0] );
        free( id->p_ff_pic_tmp1 );
    }
    if( id->p_ff_pic_tmp2)
    {
        free( id->p_ff_pic_tmp2->data[0] );
        free( id->p_ff_pic_tmp2 );
    }
    if( id->p_vresample )
    {
        img_resample_close( id->p_vresample );
    }
}

static int transcode_video_ffmpeg_process( sout_stream_t *p_stream,
               sout_stream_id_t *id, block_t *in, block_t **out )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;
    int     i_used;
    int     b_gotpicture;
    AVFrame *frame;

    int     i_data;
    uint8_t *p_data;

    *out = NULL;

    i_data = in->i_buffer;
    p_data = in->p_buffer;

    for( ;; )
    {
        block_t *p_block;
        picture_t * p_pic;
        int i_plane;

        /* decode frame */
        frame = id->p_ff_pic;
        p_sys->i_input_pts = in->i_pts;
        p_sys->i_input_dts = in->i_dts;
        if( id->ff_dec )
        {
            i_used = avcodec_decode_video( id->ff_dec_c, frame,
                                           &b_gotpicture,
                                           p_data, i_data );
        }
        else
        {
            /* raw video */
            avpicture_fill( (AVPicture*)frame, p_data,
                            id->ff_dec_c->pix_fmt,
                            id->ff_dec_c->width, id->ff_dec_c->height );
            i_used = i_data;
            b_gotpicture = 1;

            /* Set PTS */
            frame->pts = p_sys->i_input_pts ? p_sys->i_input_pts :
                         AV_NOPTS_VALUE;

            frame->pict_type = FF_I_TYPE;
        }

        if( i_used < 0 )
        {
            msg_Warn( p_stream, "error");
            return VLC_EGENERIC;
        }
        i_data -= i_used;
        p_data += i_used;

        if( !b_gotpicture )
        {
            return VLC_SUCCESS;
        }

        /* Get the pts of the decoded frame if any, otherwise keep the
         * interpolated one */
        if( frame->pts != AV_NOPTS_VALUE )
        {
            p_sys->i_output_pts = frame->pts;
        }

        /* Sanity check (seems to be needed for some streams ) */
        if( frame->pict_type == FF_B_TYPE )
        {
            p_sys->b_input_has_b_frames = VLC_TRUE;
        }

        if( !id->b_enc_inited )
        {
            /* Hack because of the copy packetizer which can fail to detect the
             * proper size (which forces us to wait until the 1st frame
             * is decoded) */
            int i_width = id->ff_dec_c->width - p_sys->i_crop_left -
                          p_sys->i_crop_right;
            int i_height = id->ff_dec_c->height - p_sys->i_crop_top -
                           p_sys->i_crop_bottom;

            if( id->f_dst.video.i_width <= 0 && id->f_dst.video.i_height <= 0
                && p_sys->f_scale )
            {
                /* Apply the scaling */
                id->f_dst.video.i_width = i_width * p_sys->f_scale;
                id->f_dst.video.i_height = i_height * p_sys->f_scale;
            }
            else if( id->f_dst.video.i_width > 0 &&
                     id->f_dst.video.i_height <= 0 )
            {
                id->f_dst.video.i_height =
                    id->f_dst.video.i_width / (double)i_width * i_height;
            }
            else if( id->f_dst.video.i_width <= 0 &&
                     id->f_dst.video.i_height > 0 )
            {
                id->f_dst.video.i_width =
                    id->f_dst.video.i_height / (double)i_height * i_width;
            }

            id->p_encoder->fmt_in.video.i_width =
              id->p_encoder->fmt_out.video.i_width =
                id->f_dst.video.i_width;
            id->p_encoder->fmt_in.video.i_height =
              id->p_encoder->fmt_out.video.i_height =
                id->f_dst.video.i_height;

            id->p_encoder->fmt_out.i_extra = 0;
            id->p_encoder->fmt_out.p_extra = NULL;

            id->p_encoder->p_module =
                module_Need( id->p_encoder, "encoder", NULL, 0 );
            if( !id->p_encoder->p_module )
            {
                vlc_object_destroy( id->p_encoder );
                msg_Err( p_stream, "cannot find encoder" );
                id->b_transcode = VLC_FALSE;
                return VLC_EGENERIC;
            }

            id->f_dst.i_extra = id->p_encoder->fmt_out.i_extra;
            id->f_dst.p_extra = id->p_encoder->fmt_out.p_extra;

            /* Hack for mp2v/mp1v transcoding support */
            if( id->f_dst.i_codec == VLC_FOURCC( 'm','p','1','v' ) ||
                id->f_dst.i_codec == VLC_FOURCC( 'm','p','2','v' ) )
            {
                id->f_dst.i_codec = VLC_FOURCC( 'm','p','g','v' );
            }

            if( !( id->id =
                     p_stream->p_sys->p_out->pf_add( p_stream->p_sys->p_out,
                                                     &id->f_dst ) ) )
            {
                msg_Err( p_stream, "cannot add this stream" );
                transcode_video_ffmpeg_close( p_stream, id );
                id->b_transcode = VLC_FALSE;
                return VLC_EGENERIC;
            }

            if( id->p_encoder->pf_header )
            {
                p_block = id->p_encoder->pf_header( id->p_encoder );
                while( p_block )
                {
                    block_t *p_out;
                    block_t *p_prev_block = p_block;

                    p_out = block_New( p_stream,
                                            p_block->i_buffer );
                    memcpy( p_out->p_buffer, p_block->p_buffer,
                            p_block->i_buffer);
                    p_out->i_dts = p_out->i_pts = in->i_dts;
                    p_out->i_length = 0;
                    block_ChainAppend( out, p_out );

                    p_block = p_block->p_next;
                    block_Release( p_prev_block );
                }
            }

            id->i_inter_pixfmt =
                get_ff_chroma( id->p_encoder->fmt_in.i_codec );

            id->b_enc_inited = VLC_TRUE;
        }

        /* deinterlace */
        if( p_stream->p_sys->b_deinterlace )
        {
            if( id->p_ff_pic_tmp0 == NULL )
            {
                int     i_size;
                uint8_t *buf;
                id->p_ff_pic_tmp0 = avcodec_alloc_frame();
                i_size = avpicture_get_size( id->ff_dec_c->pix_fmt,
                                             id->ff_dec_c->width, id->ff_dec_c->height );

                buf = malloc( i_size );

                avpicture_fill( (AVPicture*)id->p_ff_pic_tmp0, buf,
                                id->i_inter_pixfmt,
                                id->ff_dec_c->width, id->ff_dec_c->height );
            }

            avpicture_deinterlace( (AVPicture*)id->p_ff_pic_tmp0, (AVPicture*)frame,
                                   id->ff_dec_c->pix_fmt,
                                   id->ff_dec_c->width, id->ff_dec_c->height );

#if LIBAVCODEC_BUILD >= 4685
            id->p_ff_pic_tmp0->interlaced_frame = 0;
#endif
            id->p_ff_pic_tmp0->repeat_pict = frame->repeat_pict;
            frame = id->p_ff_pic_tmp0;
        }

        /* convert pix format */
        if( id->ff_dec_c->pix_fmt != id->i_inter_pixfmt )
        {
            if( id->p_ff_pic_tmp1 == NULL )
            {
                int     i_size;
                uint8_t *buf;
                id->p_ff_pic_tmp1 = avcodec_alloc_frame();
                i_size = avpicture_get_size( id->i_inter_pixfmt,
                                             id->ff_dec_c->width,
                                             id->ff_dec_c->height );

                buf = malloc( i_size );

                avpicture_fill( (AVPicture*)id->p_ff_pic_tmp1, buf,
                                id->i_inter_pixfmt,
                                id->ff_dec_c->width, id->ff_dec_c->height );
            }

            img_convert( (AVPicture*)id->p_ff_pic_tmp1, id->i_inter_pixfmt,
                         (AVPicture*)frame, id->ff_dec_c->pix_fmt,
                         id->ff_dec_c->width, id->ff_dec_c->height );

            id->p_ff_pic_tmp1->repeat_pict = frame->repeat_pict;
#if LIBAVCODEC_BUILD >= 4685
            id->p_ff_pic_tmp1->interlaced_frame = frame->interlaced_frame;
            id->p_ff_pic_tmp1->top_field_first = frame->top_field_first;
#endif
            frame = id->p_ff_pic_tmp1;
        }

        /* convert size and crop */
        if( id->ff_dec_c->width  != id->f_dst.video.i_width ||
            id->ff_dec_c->height != id->f_dst.video.i_height ||
            p_sys->i_crop_top > 0 || p_sys->i_crop_bottom > 0 ||
            p_sys->i_crop_left > 0 || p_sys->i_crop_right > 0 )
        {
            if( id->p_ff_pic_tmp2 == NULL )
            {
                int     i_size;
                uint8_t *buf;
                id->p_ff_pic_tmp2 = avcodec_alloc_frame();
                i_size = avpicture_get_size( id->i_inter_pixfmt,
                                             id->f_dst.video.i_width,
                                             id->f_dst.video.i_height );

                buf = malloc( i_size );

                avpicture_fill( (AVPicture*)id->p_ff_pic_tmp2, buf,
                                id->i_inter_pixfmt,
                                id->f_dst.video.i_width,
                                id->f_dst.video.i_height );

                id->p_vresample =
                    img_resample_full_init( id->f_dst.video.i_width,
                                            id->f_dst.video.i_height,
                                            id->ff_dec_c->width,
                                            id->ff_dec_c->height,
                                            p_stream->p_sys->i_crop_top,
                                            p_stream->p_sys->i_crop_bottom,
                                            p_stream->p_sys->i_crop_left,
                                            p_stream->p_sys->i_crop_right
#if LIBAVCODEC_BUILD >= 4708
                                            ,0, 0, 0, 0 );
#else
                                          );
#endif
            }

            img_resample( id->p_vresample, (AVPicture*)id->p_ff_pic_tmp2,
                          (AVPicture*)frame );

            id->p_ff_pic_tmp2->repeat_pict = frame->repeat_pict;
#if LIBAVCODEC_BUILD >= 4685
            id->p_ff_pic_tmp2->interlaced_frame = frame->interlaced_frame;
            id->p_ff_pic_tmp2->top_field_first = frame->top_field_first;
#endif
            frame = id->p_ff_pic_tmp2;
        }

        /* Encoding */
        p_pic = malloc(sizeof(picture_t));
        vout_InitPicture( VLC_OBJECT(p_stream), p_pic,
                          id->p_encoder->fmt_in.i_codec,
                          id->f_dst.video.i_width, id->f_dst.video.i_height,
                          id->f_dst.video.i_width * VOUT_ASPECT_FACTOR /
                          id->f_dst.video.i_height );

        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            p_pic->p[i_plane].i_pitch = frame->linesize[i_plane];
            if ( p_sys->i_threads >= 1 )
            {
                p_pic->p[i_plane].p_pixels = malloc(p_pic->p[i_plane].i_lines *
                                                    p_pic->p[i_plane].i_pitch);
                p_stream->p_vlc->pf_memcpy( p_pic->p[i_plane].p_pixels,
                    frame->data[i_plane], p_pic->p[i_plane].i_lines *
                     p_pic->p[i_plane].i_pitch );
            }
            else
            {
                p_pic->p[i_plane].p_pixels = frame->data[i_plane];
            }
        }

        /* Set the pts of the frame being encoded */
        p_pic->date = p_sys->i_output_pts;

        p_pic->i_nb_fields = frame->repeat_pict;
#if LIBAVCODEC_BUILD >= 4685
        p_pic->b_progressive = !frame->interlaced_frame;
        p_pic->b_top_field_first = frame->top_field_first;
#endif

        /* Interpolate the next PTS
         * (needed by the mpeg video packetizer which can send pts <= 0 ) */
        if( id->ff_dec_c && id->ff_dec_c->frame_rate > 0 )
        {
            p_sys->i_output_pts += I64C(1000000) * (2 + frame->repeat_pict) *
              id->ff_dec_c->frame_rate_base / (2 * id->ff_dec_c->frame_rate);
        }

        if ( p_sys->i_threads >= 1 )
        {
            vlc_mutex_lock( &p_sys->lock_out );
            p_sys->pp_pics[p_sys->i_last_pic++] = p_pic;
            p_sys->i_last_pic %= PICTURE_RING_SIZE;
            *out = p_sys->p_buffers;
            p_sys->p_buffers = NULL;
            vlc_cond_signal( &p_sys->cond );
            vlc_mutex_unlock( &p_sys->lock_out );
        }
        else
        {
            block_t *p_block;
            p_block = id->p_encoder->pf_encode_video( id->p_encoder, p_pic );
            if( p_block )
            {
                block_ChainAppend( out, p_block );
            }
            free( p_pic );
        }

        if( i_data <= 0 )
        {
            return VLC_SUCCESS;
        }
    }

    return VLC_SUCCESS;
}

static int EncoderThread( sout_stream_sys_t * p_sys )
{
    sout_stream_id_t * id = p_sys->id_video;
    picture_t * p_pic;
    int i_plane;

    while ( !p_sys->b_die && !p_sys->b_error )
    {
        block_t *p_block;

        vlc_mutex_lock( &p_sys->lock_out );
        while ( p_sys->i_last_pic == p_sys->i_first_pic )
        {
            vlc_cond_wait( &p_sys->cond, &p_sys->lock_out );
            if ( p_sys->b_die || p_sys->b_error )
                break;
        }
        if ( p_sys->b_die || p_sys->b_error )
        {
            vlc_mutex_unlock( &p_sys->lock_out );
            break;
        }

        p_pic = p_sys->pp_pics[p_sys->i_first_pic++];
        p_sys->i_first_pic %= PICTURE_RING_SIZE;
        vlc_mutex_unlock( &p_sys->lock_out );

        p_block = id->p_encoder->pf_encode_video( id->p_encoder, p_pic );
        vlc_mutex_lock( &p_sys->lock_out );
        if( p_block )
        {
            block_ChainAppend( &p_sys->p_buffers, p_block );
        }
        vlc_mutex_unlock( &p_sys->lock_out );

        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            free( p_pic->p[i_plane].p_pixels );
        }
        free( p_pic );
    }

    while ( p_sys->i_last_pic != p_sys->i_first_pic )
    {
        p_pic = p_sys->pp_pics[p_sys->i_first_pic++];
        p_sys->i_first_pic %= PICTURE_RING_SIZE;

        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            free( p_pic->p[i_plane].p_pixels );
        }
        free( p_pic );
    }

    block_ChainRelease( p_sys->p_buffers );

    return 0;
}

/*****************************************************************************
 * transcode_video_ffmpeg_getframebuf:
 *
 * Callback used by ffmpeg to get a frame buffer.
 * We use it to get the right PTS for each decoded picture.
 *****************************************************************************/
static int transcode_video_ffmpeg_getframebuf(struct AVCodecContext *p_context,
                                              AVFrame *p_frame)
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_context->opaque;

    /* Set PTS */
    if( p_sys->i_input_pts )
    {
        p_frame->pts = p_sys->i_input_pts;
    }
    else if( p_sys->i_input_dts )
    {
        /* Some demuxers/packetizers only set the dts so let's try to find a
         * useful timestamp from this */
        if( !p_context->has_b_frames || !p_sys->b_input_has_b_frames ||
            !p_frame->reference || !p_sys->i_output_pts )
        {
            p_frame->pts = p_sys->i_input_dts +
                /* Hack: ffmpeg encoding doesn't like frames with identical pts */
                (p_sys->i_output_pts ? 0 : 50000);
        }
        else p_frame->pts = AV_NOPTS_VALUE;
    }
    else p_frame->pts = AV_NOPTS_VALUE;

    if( p_sys->i_output_pts ) /* make sure 1st frame has a pts > 0 */
    {
        p_sys->i_input_pts = 0;
        p_sys->i_input_dts = 0;
    }

    return avcodec_default_get_buffer( p_context, p_frame );
}
