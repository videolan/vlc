/*****************************************************************************
 * mux.c: muxer using ffmpeg (libavformat).
 *****************************************************************************
 * Copyright (C) 2006 VideoLAN
 * $Id: demux.c 8444 2004-08-17 08:21:07Z gbazin $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVFORMAT_H
#   include <ffmpeg/avformat.h>
#else
#   include <avformat.h>
#endif

#include "ffmpeg.h"

//#define AVFORMAT_DEBUG 1

/* Version checking */
#if (LIBAVFORMAT_BUILD >= 4687) && (defined(HAVE_FFMPEG_AVFORMAT_H) || defined(HAVE_LIBAVFORMAT_TREE))

/*****************************************************************************
 * mux_sys_t: mux descriptor
 *****************************************************************************/
struct sout_mux_sys_t
{
    ByteIOContext   io;
    int             io_buffer_size;
    uint8_t        *io_buffer;

    AVFormatContext *oc;
    URLContext     url;
    URLProtocol    prot;

    vlc_bool_t     b_write_header;
    vlc_bool_t     b_error;

    int64_t        i_initial_dts;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

static int IOWrite( void *opaque, uint8_t *buf, int buf_size );
static offset_t IOSeek( void *opaque, offset_t offset, int whence );

/*****************************************************************************
 * Open
 *****************************************************************************/
int E_(OpenMux)( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys;
    AVFormatParameters params, *ap = &params;

    /* Should we call it only once ? */
    av_register_all();

    /* Find the requested muxer */
    AVOutputFormat *file_oformat =
        guess_format(NULL, p_mux->p_access->psz_name, NULL);
    if (!file_oformat)
    {
      msg_Err( p_mux, "unable for find a suitable output format" );
      return VLC_EGENERIC;
    }

    /* Fill p_mux fields */
    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys = p_sys = malloc( sizeof( sout_mux_sys_t ) );

    p_sys->oc = av_alloc_format_context();
    p_sys->oc->oformat = file_oformat;

    /* Create I/O wrapper */
    p_sys->io_buffer_size = 32768;  /* FIXME */
    p_sys->io_buffer = malloc( p_sys->io_buffer_size );
    p_sys->url.priv_data = p_mux;
    p_sys->url.prot = &p_sys->prot;
    p_sys->url.prot->name = "VLC I/O wrapper";
    p_sys->url.prot->url_open = 0;
    p_sys->url.prot->url_read = 0;
    p_sys->url.prot->url_write =
                    (int (*) (URLContext *, unsigned char *, int))IOWrite;
    p_sys->url.prot->url_seek =
                    (offset_t (*) (URLContext *, offset_t, int))IOSeek;
    p_sys->url.prot->url_close = 0;
    p_sys->url.prot->next = 0;
    init_put_byte( &p_sys->io, p_sys->io_buffer, p_sys->io_buffer_size,
                   1, &p_sys->url, NULL, IOWrite, IOSeek );

    memset( ap, 0, sizeof(*ap) );
    if( av_set_parameters( p_sys->oc, ap ) < 0 )
    {
        msg_Err( p_mux, "invalid encoding parameters" );
        av_free( p_sys->oc );
        free( p_sys->io_buffer );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->oc->pb = p_sys->io;
    p_sys->oc->nb_streams = 0;

    p_sys->b_write_header = VLC_TRUE;
    p_sys->b_error = VLC_FALSE;
    p_sys->i_initial_dts = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
void E_(CloseMux)( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i;

    if( av_write_trailer( p_sys->oc ) < 0 )
    {
        msg_Err( p_mux, "could not write trailer" );
    }

    for( i = 0 ; i < p_sys->oc->nb_streams; i++ )
    {
        if( p_sys->oc->streams[i]->codec->extradata )
            av_free( p_sys->oc->streams[i]->codec->extradata );
        av_free( p_sys->oc->streams[i]->codec );
        av_free( p_sys->oc->streams[i] );
    }
    av_free( p_sys->oc );

    free( p_sys->io_buffer );
    free( p_sys );
}

/*****************************************************************************
 * AddStream
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    AVCodecContext *codec;
    AVStream *stream;
    int i_codec_id, i_aspect_num, i_aspect_den;

    msg_Dbg( p_mux, "adding input" );

    if( !E_(GetFfmpegCodec)( p_input->p_fmt->i_codec, 0, &i_codec_id, 0 ) )
    {
        msg_Dbg( p_mux, "couldn't find codec for fourcc '%4.4s'",
                 (char *)&p_input->p_fmt->i_codec );
        return VLC_EGENERIC;
    }

    p_input->p_sys = malloc( sizeof( int ) );
    *((int *)p_input->p_sys) = p_sys->oc->nb_streams;

    stream = av_new_stream( p_sys->oc, p_sys->oc->nb_streams);
    if( !stream )
    {
        free( p_input->p_sys );
        return VLC_EGENERIC;
    }
    codec = stream->codec;

    switch( p_input->p_fmt->i_cat )
    {
    case AUDIO_ES:
        codec->codec_type = CODEC_TYPE_AUDIO;
        codec->channels = p_input->p_fmt->audio.i_channels;
        codec->sample_rate = p_input->p_fmt->audio.i_rate;
        codec->time_base = (AVRational){1, codec->sample_rate};
        break;

    case VIDEO_ES:
        if( !p_input->p_fmt->video.i_frame_rate ||
            !p_input->p_fmt->video.i_frame_rate_base )
        {
            msg_Warn( p_mux, "Missing frame rate, assuming 25fps" );
            p_input->p_fmt->video.i_frame_rate = 25;
            p_input->p_fmt->video.i_frame_rate_base = 1;
        }
        codec->codec_type = CODEC_TYPE_VIDEO;
        codec->width = p_input->p_fmt->video.i_width;
        codec->height = p_input->p_fmt->video.i_height;
        av_reduce( &i_aspect_num, &i_aspect_den,
                   p_input->p_fmt->video.i_aspect,
                   VOUT_ASPECT_FACTOR, 1 << 30 /* something big */ );
        av_reduce( &codec->sample_aspect_ratio.num,
                   &codec->sample_aspect_ratio.den,
                   i_aspect_num * (int64_t)codec->height,
                   i_aspect_den * (int64_t)codec->width, 1 << 30 );
        codec->time_base.den = p_input->p_fmt->video.i_frame_rate;
        codec->time_base.num = p_input->p_fmt->video.i_frame_rate_base;
        break;
    }

    codec->bit_rate = p_input->p_fmt->i_bitrate;
    codec->codec_tag = p_input->p_fmt->i_codec;
    codec->codec_id = i_codec_id;

    if( p_input->p_fmt->i_extra )
    {
        codec->extradata_size = p_input->p_fmt->i_extra;
        codec->extradata = av_malloc( p_input->p_fmt->i_extra );
        memcpy( codec->extradata, p_input->p_fmt->p_extra,
                p_input->p_fmt->i_extra );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream
 *****************************************************************************/
static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "removing input" );
    free( p_input->p_sys );
    return VLC_SUCCESS;
}

/*
 * TODO  move this function to src/stream_output.c (used by nearly all muxers)
 */
static int MuxGetStream( sout_mux_t *p_mux, int *pi_stream, mtime_t *pi_dts )
{
    mtime_t i_dts;
    int     i_stream, i;

    for( i = 0, i_dts = 0, i_stream = -1; i < p_mux->i_nb_inputs; i++ )
    {
        block_fifo_t  *p_fifo;

        p_fifo = p_mux->pp_inputs[i]->p_fifo;

        /* We don't really need to have anything in the SPU fifo */
        if( p_mux->pp_inputs[i]->p_fmt->i_cat == SPU_ES &&
            p_fifo->i_depth == 0 ) continue;

        if( p_fifo->i_depth )
        {
            block_t *p_buf;

            p_buf = block_FifoShow( p_fifo );
            if( i_stream < 0 || p_buf->i_dts < i_dts )
            {
                i_dts = p_buf->i_dts;
                i_stream = i;
            }
        }
        else return -1;

    }
    if( pi_stream ) *pi_stream = i_stream;
    if( pi_dts ) *pi_dts = i_dts;
    if( !p_mux->p_sys->i_initial_dts ) p_mux->p_sys->i_initial_dts = i_dts;
    return i_stream;
}

static int MuxBlock( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    block_t *p_data = block_FifoGet( p_input->p_fifo );
    int i_stream = *((int *)p_input->p_sys);
    AVStream *p_stream = p_sys->oc->streams[i_stream];
    AVPacket pkt = {0};

    av_init_packet(&pkt);
    pkt.data = p_data->p_buffer;
    pkt.size = p_data->i_buffer;
    pkt.stream_index = i_stream;

    if( p_data->i_flags & BLOCK_FLAG_TYPE_I ) pkt.flags |= PKT_FLAG_KEY;

    /* avformat expects pts/dts which start from 0 */
    p_data->i_dts -= p_mux->p_sys->i_initial_dts;
    p_data->i_pts -= p_mux->p_sys->i_initial_dts;

    if( p_data->i_pts > 0 )
        pkt.pts = p_data->i_pts * p_stream->time_base.den /
            I64C(1000000) / p_stream->time_base.num;
    if( p_data->i_dts > 0 )
        pkt.dts = p_data->i_dts * p_stream->time_base.den /
            I64C(1000000) / p_stream->time_base.num;

    if( av_write_frame( p_sys->oc, &pkt ) < 0 )
    {
        msg_Err( p_mux, "could not write frame (pts: "I64Fd", dts: "I64Fd") "
                 "(pkt pts: "I64Fd", dts: "I64Fd")",
                 p_data->i_pts, p_data->i_dts, pkt.pts, pkt.dts );
        block_Release( p_data );
        return VLC_EGENERIC;
    }

    block_Release( p_data );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Mux: multiplex available data in input fifos
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i_stream;

    if( p_sys->b_error ) return VLC_EGENERIC;

    if( p_sys->b_write_header )
    {
        msg_Dbg( p_mux, "writing header" );

        p_sys->b_write_header = VLC_FALSE;

        if( av_write_header( p_sys->oc ) < 0 )
        {
            msg_Err( p_mux, "could not write header" );
            p_sys->b_error = VLC_TRUE;
            return VLC_EGENERIC;
        }
    }

    for( ;; )
    {
        if( MuxGetStream( p_mux, &i_stream, 0 ) < 0 ) return VLC_SUCCESS;
        MuxBlock( p_mux, p_mux->pp_inputs[i_stream] );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    vlc_bool_t *pb_bool;

    switch( i_query )
    {
    case MUX_CAN_ADD_STREAM_WHILE_MUXING:
        pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
        *pb_bool = VLC_FALSE;
        return VLC_SUCCESS;

    case MUX_GET_ADD_STREAM_WAIT:
        pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
        *pb_bool = VLC_TRUE;
        return VLC_SUCCESS;

    case MUX_GET_MIME:
    default:
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * I/O wrappers for libavformat
 *****************************************************************************/
static int IOWrite( void *opaque, uint8_t *buf, int buf_size )
{
    URLContext *p_url = opaque;
    sout_mux_t *p_mux = p_url->priv_data;
    int i_ret;

#ifdef AVFORMAT_DEBUG
    msg_Dbg( p_mux, "IOWrite %i bytes", buf_size );
#endif

    block_t *p_buf = block_New( p_mux->p_sout, buf_size );
    if( buf_size > 0 ) memcpy( p_buf->p_buffer, buf, buf_size );

    i_ret = sout_AccessOutWrite( p_mux->p_access, p_buf );
    return i_ret ? i_ret : -1;
}

static offset_t IOSeek( void *opaque, offset_t offset, int whence )
{
    URLContext *p_url = opaque;
    sout_mux_t *p_mux = p_url->priv_data;
    int64_t i_absolute;

#ifdef AVFORMAT_DEBUG
    msg_Dbg( p_mux, "IOSeek offset: "I64Fd", whence: %i", offset, whence );
#endif

    switch( whence )
    {
    case SEEK_SET:
        i_absolute = offset;
        break;
    case SEEK_CUR:
    case SEEK_END:
    default:
        return -1;
    }

    if( sout_AccessOutSeek( p_mux->p_access, i_absolute ) )
    {
        return -1;
    }

    return 0;
}

#else /* LIBAVFORMAT_BUILD >= 4687 */

int E_(OpenMux)( vlc_object_t *p_this )
{
    return VLC_EGENERIC;
}

void E_(CloseMux)( vlc_object_t *p_this )
{
}

#endif /* LIBAVFORMAT_BUILD >= 4687 */
