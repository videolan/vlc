/*****************************************************************************
 * transcode.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: transcode.c,v 1.5 2003/04/29 22:44:08 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, sout_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, sout_buffer_t* );

static int  transcode_audio_ffmpeg_new    ( sout_stream_t *, sout_stream_id_t * );
static void transcode_audio_ffmpeg_close  ( sout_stream_t *, sout_stream_id_t * );
static int  transcode_audio_ffmpeg_process( sout_stream_t *, sout_stream_id_t *, sout_buffer_t *, sout_buffer_t ** );

static int  transcode_video_ffmpeg_new    ( sout_stream_t *, sout_stream_id_t * );
static void transcode_video_ffmpeg_close  ( sout_stream_t *, sout_stream_id_t * );
static int  transcode_video_ffmpeg_process( sout_stream_t *, sout_stream_id_t *, sout_buffer_t *, sout_buffer_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Transcode stream") );
    set_capability( "sout stream", 50 );
    add_shortcut( "transcode" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    sout_stream_t   *p_out;

    vlc_fourcc_t    i_acodec;   /* codec audio (0 if not transcode) */
    int             i_sample_rate;
    int             i_channels;
    int             i_abitrate;

    vlc_fourcc_t    i_vcodec;   /*    "   video  " "   "      " */
    int             i_vbitrate;
    int             i_width;
    int             i_height;
    vlc_bool_t      b_deinterlace;

    int             i_crop_top;
    int             i_crop_bottom;
    int             i_crop_right;
    int             i_crop_left;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char *codec;

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->p_out = sout_stream_new( p_stream->p_sout, p_stream->psz_next );

    p_sys->i_acodec      = 0;
    p_sys->i_sample_rate = 0;
    p_sys->i_channels    = 0;
    p_sys->i_abitrate    = 0;

    p_sys->i_vcodec     = 0;
    p_sys->i_vbitrate   = 0;
    p_sys->i_width      = 0;
    p_sys->i_height     = 0;
    p_sys->b_deinterlace= VLC_FALSE;

    p_sys->i_crop_top   = 0;
    p_sys->i_crop_bottom= 0;
    p_sys->i_crop_right = 0;
    p_sys->i_crop_left  = 0;

    if( ( codec = sout_cfg_find_value( p_stream->p_cfg, "acodec" ) ) )
    {
        char fcc[4] = "    ";
        char *val;

        memcpy( fcc, codec, strlen( codec ) );

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
        }

        msg_Dbg( p_stream, "codec audio=%4.4s %dHz %d channels %dKb/s",
                 fcc,
                 p_sys->i_sample_rate, p_sys->i_channels,
                 p_sys->i_abitrate / 1024 );
    }

    if( ( codec = sout_cfg_find_value( p_stream->p_cfg, "vcodec" ) ) )
    {
        char fcc[4] = "    ";
        char *val;

        memcpy( fcc, codec, strlen( codec ) );

        p_sys->i_vcodec = VLC_FOURCC( fcc[0], fcc[1], fcc[2], fcc[3] );

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
        }
        if( sout_cfg_find( p_stream->p_cfg, "deinterlace" ) )
        {
            p_sys->b_deinterlace = VLC_TRUE;
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

        msg_Dbg( p_stream, "codec video=%4.4s %dx%d %dkb/s",
                 fcc,
                 p_sys->i_width, p_sys->i_height,
                 p_sys->i_vbitrate / 1024 );
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
    free( p_sys );
}

struct sout_stream_id_t
{
    vlc_fourcc_t  b_transcode;
    sout_format_t f_src;        /* only if transcoding */
    sout_format_t f_dst;        /*  "   "      " */

    /* id of the out stream */
    void *id;

    /* ffmpeg part */
    AVCodec         *ff_dec;
    AVCodecContext  *ff_dec_c;


    vlc_fourcc_t    b_enc_inited;
    AVCodec         *ff_enc;
    AVCodecContext  *ff_enc_c;

    mtime_t         i_dts;
    mtime_t         i_length;

    int             i_buffer_in;
    int             i_buffer_in_pos;
    uint8_t         *p_buffer_in;

    int             i_buffer;
    int             i_buffer_pos;
    uint8_t         *p_buffer;

    int             i_buffer_out;
    int             i_buffer_out_pos;
    uint8_t         *p_buffer_out;

    AVFrame         *p_ff_pic;
    AVFrame         *p_ff_pic_tmp0; /* to do deinterlace */
    AVFrame         *p_ff_pic_tmp1; /* to do pix conversion */
    AVFrame         *p_ff_pic_tmp2; /* to do resample */

    ImgReSampleContext *p_vresample;
};


static sout_stream_id_t * Add      ( sout_stream_t *p_stream, sout_format_t *p_fmt )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;
    sout_stream_id_t    *id;

    id = malloc( sizeof( sout_stream_id_t ) );
    id->i_dts = 0;
    if( p_fmt->i_cat == AUDIO_ES && p_sys->i_acodec != 0 )
    {
        msg_Dbg( p_stream,
                 "creating audio transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
                 (char*)&p_fmt->i_fourcc,
                 (char*)&p_sys->i_acodec );

        /* src format */
        memcpy( &id->f_src, p_fmt, sizeof( sout_format_t ) );

        /* create dst format */
        id->f_dst.i_cat    = AUDIO_ES;
        id->f_dst.i_fourcc = p_sys->i_acodec;
        id->f_dst.i_sample_rate = p_sys->i_sample_rate  > 0 ? p_sys->i_sample_rate : id->f_src.i_sample_rate;
        id->f_dst.i_channels    = p_sys->i_channels > 0 ? p_sys->i_channels : id->f_src.i_channels;
        id->f_dst.i_bitrate     = p_sys->i_abitrate > 0 ? p_sys->i_abitrate : 64000;
        id->f_dst.i_block_align = 0;
        id->f_dst.i_extra_data  = 0;
        id->f_dst.p_extra_data  = NULL;

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
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_sys->i_vcodec != 0 )
    {
        msg_Dbg( p_stream,
                 "creating video transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
                 (char*)&p_fmt->i_fourcc,
                 (char*)&p_sys->i_vcodec );

        memcpy( &id->f_src, p_fmt, sizeof( sout_format_t ) );

        /* create dst format */
        id->f_dst.i_cat         = VIDEO_ES;
        id->f_dst.i_fourcc      = p_sys->i_vcodec;
        id->f_dst.i_width       = p_sys->i_width ; /* > 0 ? p_sys->i_width : id->f_src.i_width; */
        id->f_dst.i_height      = p_sys->i_height; /* > 0 ? p_sys->i_height: id->f_src.i_height; */
        id->f_dst.i_bitrate     = p_sys->i_vbitrate > 0 ? p_sys->i_vbitrate : 800*1000;
        id->f_dst.i_extra_data  = 0;
        id->f_dst.p_extra_data  = NULL;

        /* build decoder -> filter -> encoder */
        if( transcode_video_ffmpeg_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create video chain" );
            free( id );
            return NULL;
        }

        /* open output stream */
        id->id = p_sys->p_out->pf_add( p_sys->p_out, &id->f_dst );
        id->b_transcode = VLC_TRUE;
    }
    else
    {
        msg_Dbg( p_stream, "not transcoding a stream (fcc=`%4.4s')", (char*)&p_fmt->i_fourcc );
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

    p_sys->p_out->pf_del( p_sys->p_out, id->id );
    free( id );

    return VLC_SUCCESS;
}

static int     Send     ( sout_stream_t *p_stream, sout_stream_id_t *id, sout_buffer_t *p_buffer )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if( id->b_transcode )
    {
        sout_buffer_t *p_buffer_out;
        if( id->f_src.i_cat == AUDIO_ES )
        {
            transcode_audio_ffmpeg_process( p_stream, id, p_buffer, &p_buffer_out );
        }
        else if( id->f_src.i_cat == VIDEO_ES )
        {
            transcode_video_ffmpeg_process( p_stream, id, p_buffer, &p_buffer_out );
        }
        sout_BufferDelete( p_stream->p_sout, p_buffer );

        if( p_buffer_out )
        {
            return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer_out );
        }
        return VLC_SUCCESS;
    }
    else
    {
        return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer );
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
    { VLC_FOURCC( 'a', '5', '2', ' ' ), CODEC_ID_AC3 },
    { VLC_FOURCC( 'a', 'c', '3', ' ' ), CODEC_ID_AC3 },
    { VLC_FOURCC( 'w', 'm', 'a', '1' ), CODEC_ID_WMAV1 },
    { VLC_FOURCC( 'w', 'm', 'a', '2' ), CODEC_ID_WMAV2 },

    /* video */
    { VLC_FOURCC( 'm', 'p', '4', 'v'),  CODEC_ID_MPEG4 },
    { VLC_FOURCC( 'm', 'p', 'g', 'v' ), CODEC_ID_MPEG1VIDEO },
    { VLC_FOURCC( 'D', 'I', 'V', '1' ), CODEC_ID_MSMPEG4V1 },
    { VLC_FOURCC( 'D', 'I', 'V', '2' ), CODEC_ID_MSMPEG4V2 },
    { VLC_FOURCC( 'D', 'I', 'V', '3' ), CODEC_ID_MSMPEG4V3 },
    { VLC_FOURCC( 'H', '2', '6', '3' ), CODEC_ID_H263 },
    { VLC_FOURCC( 'I', '2', '6', '3' ), CODEC_ID_H263I },
    { VLC_FOURCC( 'W', 'M', 'V', '1' ), CODEC_ID_WMV1 },
    { VLC_FOURCC( 'W', 'M', 'V', '2' ), CODEC_ID_WMV2 },
    { VLC_FOURCC( 'M', 'J', 'P', 'G' ), CODEC_ID_MJPEG },
    { VLC_FOURCC( 'm', 'j', 'p', 'b' ), CODEC_ID_MJPEGB },
    { VLC_FOURCC( 'd', 'v', 's', 'l' ), CODEC_ID_DVVIDEO },
    { VLC_FOURCC( 'S', 'V', 'Q', '1' ), CODEC_ID_SVQ1 },

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

static int transcode_audio_ffmpeg_new   ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    int i_ff_codec;

    /* find decoder */

    i_ff_codec = get_ff_codec( id->f_src.i_fourcc );
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
    id->ff_dec_c->sample_rate = id->f_src.i_sample_rate;
    id->ff_dec_c->channels    = id->f_src.i_channels;
    id->ff_dec_c->block_align = id->f_src.i_block_align;
    id->ff_dec_c->bit_rate    = id->f_src.i_bitrate;

    id->ff_dec_c->extradata_size = id->f_src.i_extra_data;
    id->ff_dec_c->extradata      = id->f_src.p_extra_data;
    if( avcodec_open( id->ff_dec_c, id->ff_dec ) )
    {
        msg_Err( p_stream, "cannot open decoder" );
        return VLC_EGENERIC;
    }

    /* find encoder */
    i_ff_codec = get_ff_codec( id->f_dst.i_fourcc );
    if( i_ff_codec == 0 )
    {
        msg_Err( p_stream, "cannot find encoder" );
        return VLC_EGENERIC;
    }

    id->ff_enc = avcodec_find_encoder( i_ff_codec );
    if( !id->ff_enc )
    {
        msg_Err( p_stream, "cannot find encoder" );
        return VLC_EGENERIC;
    }

    id->ff_enc_c = avcodec_alloc_context();
    id->ff_enc_c->bit_rate    = id->f_dst.i_bitrate;
    id->ff_enc_c->sample_rate = id->f_dst.i_sample_rate;
    id->ff_enc_c->channels    = id->f_dst.i_channels;

    if( avcodec_open( id->ff_enc_c, id->ff_enc ) )
    {
        msg_Err( p_stream, "cannot open encoder" );
        return VLC_EGENERIC;
    }


    id->i_buffer_in      = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    id->i_buffer_in_pos = 0;
    id->p_buffer_in      = malloc( id->i_buffer_in );

    id->i_buffer     = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    id->i_buffer_pos = 0;
    id->p_buffer     = malloc( id->i_buffer );

    id->i_buffer_out     = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    id->i_buffer_out_pos = 0;
    id->p_buffer_out     = malloc( id->i_buffer_out );

    return VLC_SUCCESS;
}

static void transcode_audio_ffmpeg_close ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    avcodec_close( id->ff_dec_c );
    avcodec_close( id->ff_enc_c );

    free( id->ff_dec_c );
    free( id->ff_enc_c );

    free( id->p_buffer );
}

static int transcode_audio_ffmpeg_process( sout_stream_t *p_stream, sout_stream_id_t *id,
                                           sout_buffer_t *in, sout_buffer_t **out )
{
    *out = NULL;

    /* gather data into p_buffer_in */
    id->i_dts = in->i_dts -
                (mtime_t)1000000 *
                (mtime_t)(id->i_buffer_pos / 2 / id->ff_enc_c->channels )/
                (mtime_t)id->ff_enc_c->sample_rate;

    memcpy( &id->p_buffer_in[id->i_buffer_in_pos],
            in->p_buffer,
            in->i_size );
    id->i_buffer_in_pos += in->i_size;

    /* decode as many data as possible */
    for( ;; )
    {
        int i_buffer_size;
        int i_used;

        i_buffer_size = id->i_buffer - id->i_buffer_pos;

        i_used = avcodec_decode_audio( id->ff_dec_c,
                                       (int16_t*)&id->p_buffer[id->i_buffer_pos], &i_buffer_size,
                                       id->p_buffer_in, id->i_buffer_in_pos );

        /* msg_Warn( p_stream, "avcodec_decode_audio: %d used", i_used ); */
        id->i_buffer_pos += i_buffer_size;

        if( i_used < 0 )
        {
            msg_Warn( p_stream, "error");
            id->i_buffer_in_pos = 0;
            break;
        }
        else if( i_used < id->i_buffer_in_pos )
        {
            memmove( id->p_buffer_in,
                     &id->p_buffer_in[i_used],
                     id->i_buffer_in - i_used );
            id->i_buffer_in_pos -= i_used;
        }
        else
        {
            id->i_buffer_in_pos = 0;
            break;
        }
    }

    /* encode as many data as possible */
    for( ;; )
    {
        int i_frame_size = id->ff_enc_c->frame_size * 2 * id->ff_enc_c->channels;
        int i_out_size;
        sout_buffer_t *p_out;

        if( id->i_buffer_pos < i_frame_size )
        {
            break;
        }

        /* msg_Warn( p_stream, "avcodec_encode_audio: frame size%d", i_frame_size); */
        i_out_size = avcodec_encode_audio( id->ff_enc_c,
                                           id->p_buffer_out, id->i_buffer_out,
                                           (int16_t*)id->p_buffer );

        if( i_out_size <= 0 )
        {
            break;
        }
        memmove( id->p_buffer,
                 &id->p_buffer[i_frame_size],
                 id->i_buffer - i_frame_size );
        id->i_buffer_pos -= i_frame_size;

        p_out = sout_BufferNew( p_stream->p_sout, i_out_size );
        memcpy( p_out->p_buffer, id->p_buffer_out, i_out_size );
        p_out->i_size = i_out_size;
        p_out->i_length = (mtime_t)1000000 * (mtime_t)id->ff_enc_c->frame_size / (mtime_t)id->ff_enc_c->sample_rate;
        /* FIXME */
        p_out->i_dts = id->i_dts;
        p_out->i_pts = id->i_dts;

        /* update dts */
        id->i_dts += p_out->i_length;

       /* msg_Warn( p_stream, "frame dts=%lld len %lld out=%d", p_out->i_dts, p_out->i_length, i_out_size ); */
        sout_BufferChain( out, p_out );
    }

    return VLC_SUCCESS;
}


/*
 * video
 */
static int transcode_video_ffmpeg_new   ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    int i_ff_codec;

    /* find decoder */
    i_ff_codec = get_ff_codec( id->f_src.i_fourcc );
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
    id->ff_dec_c->width         = id->f_src.i_width;
    id->ff_dec_c->height        = id->f_src.i_height;
    /* id->ff_dec_c->bit_rate      = id->f_src.i_bitrate; */
    id->ff_dec_c->extradata_size= id->f_src.i_extra_data;
    id->ff_dec_c->extradata     = id->f_src.p_extra_data;
    id->ff_dec_c->workaround_bugs = FF_BUG_AUTODETECT;
    id->ff_dec_c->error_resilience= -1;
    if( id->ff_dec->capabilities & CODEC_CAP_TRUNCATED )
    {
        id->ff_dec_c->flags |= CODEC_FLAG_TRUNCATED;
    }

    if( avcodec_open( id->ff_dec_c, id->ff_dec ) < 0 )
    {
        msg_Err( p_stream, "cannot open decoder" );
        return VLC_EGENERIC;
    }
#if 1
    if( i_ff_codec == CODEC_ID_MPEG4 && id->ff_dec_c->extradata_size > 0 )
    {
        int b_gotpicture;
        AVFrame frame;

        avcodec_decode_video( id->ff_dec_c, &frame,
                              &b_gotpicture,
                              id->ff_dec_c->extradata, id->ff_dec_c->extradata_size );
    }
#endif

    /* find encoder */
    i_ff_codec = get_ff_codec( id->f_dst.i_fourcc );
    if( i_ff_codec == 0 )
    {
        msg_Err( p_stream, "cannot find encoder" );
        return VLC_EGENERIC;
    }

    id->ff_enc = avcodec_find_encoder( i_ff_codec );
    if( !id->ff_enc )
    {
        msg_Err( p_stream, "cannot find encoder" );
        return VLC_EGENERIC;
    }

    id->ff_enc_c = avcodec_alloc_context();
    id->ff_enc_c->width          = id->f_dst.i_width;
    id->ff_enc_c->height         = id->f_dst.i_height;
    id->ff_enc_c->bit_rate       = id->f_dst.i_bitrate;
#if LIBAVCODEC_BUILD >= 4662
    id->ff_enc_c->frame_rate     = 25 ; /* FIXME as it break mpeg */
    id->ff_enc_c->frame_rate_base= 1;
#else
    id->ff_enc_c->frame_rate     = 25 * FRAME_RATE_BASE;
#endif
    id->ff_enc_c->gop_size       = 25;
    id->ff_enc_c->qmin           = 2;
    id->ff_enc_c->qmax           = 31;
#if 0
    /* XXX open it when we have the first frame */
    if( avcodec_open( id->ff_enc_c, id->ff_enc ) )
    {
        msg_Err( p_stream, "cannot open encoder" );
        return VLC_EGENERIC;
    }
#endif
    id->b_enc_inited     = VLC_FALSE;
    id->i_buffer_in      = 0;
    id->i_buffer_in_pos  = 0;
    id->p_buffer_in      = NULL;

    id->i_buffer     = 3*1024*1024;
    id->i_buffer_pos = 0;
    id->p_buffer     = malloc( id->i_buffer );

    id->i_buffer_out     = 0;
    id->i_buffer_out_pos = 0;
    id->p_buffer_out     = NULL;

    id->p_ff_pic         = avcodec_alloc_frame();
    id->p_ff_pic_tmp0    = NULL;
    id->p_ff_pic_tmp1    = NULL;
    id->p_ff_pic_tmp2    = NULL;
    id->p_vresample      = NULL;
    return VLC_SUCCESS;
}

static void transcode_video_ffmpeg_close ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    avcodec_close( id->ff_dec_c );
    if( id->b_enc_inited )
    {
        avcodec_close( id->ff_enc_c );
    }

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
        free( id->p_vresample );
    }

    free( id->ff_dec_c );
    free( id->ff_enc_c );

    free( id->p_buffer );
}

static int transcode_video_ffmpeg_process( sout_stream_t *p_stream, sout_stream_id_t *id,
                                           sout_buffer_t *in, sout_buffer_t **out )
{
    int     i_used;
    int     i_out;
    int     b_gotpicture;
    AVFrame *frame;

    int     i_data;
    uint8_t *p_data;

    *out = NULL;

    i_data = in->i_size;
    p_data = in->p_buffer;

    for( ;; )
    {
        /* decode frame */
        frame = id->p_ff_pic;
        i_used = avcodec_decode_video( id->ff_dec_c, frame,
                                       &b_gotpicture,
                                       p_data, i_data );

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

        if( !id->b_enc_inited )
        {
            /* XXX hack because of copy packetizer and mpeg4video that can failed
               detecting size */
            if( id->ff_enc_c->width == 0 || id->ff_enc_c->height == 0 )
            {
                id->ff_enc_c->width          = id->ff_dec_c->width;
                id->ff_enc_c->height         = id->ff_dec_c->height;
            }

            if( avcodec_open( id->ff_enc_c, id->ff_enc ) )
            {
                msg_Err( p_stream, "cannot open encoder" );
                return VLC_EGENERIC;
            }
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
                                id->ff_enc_c->pix_fmt,
                                id->ff_dec_c->width, id->ff_dec_c->height );
            }

            avpicture_deinterlace( (AVPicture*)id->p_ff_pic_tmp0, (AVPicture*)frame,
                                   id->ff_dec_c->pix_fmt,
                                   id->ff_dec_c->width, id->ff_dec_c->height );

            frame = id->p_ff_pic_tmp0;
        }

        /* convert pix format */
        if( id->ff_dec_c->pix_fmt != id->ff_enc_c->pix_fmt )
        {
            if( id->p_ff_pic_tmp1 == NULL )
            {
                int     i_size;
                uint8_t *buf;
                id->p_ff_pic_tmp1 = avcodec_alloc_frame();
                i_size = avpicture_get_size( id->ff_enc_c->pix_fmt,
                                             id->ff_dec_c->width, id->ff_dec_c->height );

                buf = malloc( i_size );

                avpicture_fill( (AVPicture*)id->p_ff_pic_tmp1, buf,
                                id->ff_enc_c->pix_fmt,
                                id->ff_dec_c->width, id->ff_dec_c->height );
            }

            img_convert( (AVPicture*)id->p_ff_pic_tmp1, id->ff_enc_c->pix_fmt,
                         (AVPicture*)frame,             id->ff_dec_c->pix_fmt,
                         id->ff_dec_c->width, id->ff_dec_c->height );

            frame = id->p_ff_pic_tmp1;
        }

        /* convert size and crop */
        if( ( id->ff_dec_c->width  != id->ff_enc_c->width ) ||
            ( id->ff_dec_c->height != id->ff_enc_c->height ) )
        {
            if( id->p_ff_pic_tmp2 == NULL )
            {
                int     i_size;
                uint8_t *buf;
                id->p_ff_pic_tmp2 = avcodec_alloc_frame();
                i_size = avpicture_get_size( id->ff_enc_c->pix_fmt,
                                             id->ff_enc_c->width, id->ff_enc_c->height );

                buf = malloc( i_size );

                avpicture_fill( (AVPicture*)id->p_ff_pic_tmp2, buf,
                                id->ff_enc_c->pix_fmt,
                                id->ff_enc_c->width, id->ff_enc_c->height );

                id->p_vresample =
                    img_resample_full_init( id->ff_enc_c->width, id->ff_enc_c->height,
                                            id->ff_dec_c->width, id->ff_dec_c->height,
                                            p_stream->p_sys->i_crop_top,
                                            p_stream->p_sys->i_crop_bottom,
                                            p_stream->p_sys->i_crop_left,
                                            p_stream->p_sys->i_crop_right );
            }

            img_resample( id->p_vresample, (AVPicture*)id->p_ff_pic_tmp2, (AVPicture*)frame );

            frame = id->p_ff_pic_tmp2;
        }

        /* encode frame */
        i_out = avcodec_encode_video( id->ff_enc_c, id->p_buffer, id->i_buffer, frame );

        if( i_out > 0 )
        {
            sout_buffer_t *p_out;
            p_out = sout_BufferNew( p_stream->p_sout, i_out );

            memcpy( p_out->p_buffer, id->p_buffer, i_out );

            p_out->i_size   = i_out;
            p_out->i_length = in->i_length;
            p_out->i_dts    = in->i_dts;
            p_out->i_pts    = in->i_dts; /* FIXME */

            sout_BufferChain( out, p_out );
        }

        if( i_data <= 0 )
        {
            return VLC_SUCCESS;
        }
    }

    return VLC_SUCCESS;
}

