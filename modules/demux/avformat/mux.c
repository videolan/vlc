/*****************************************************************************
 * mux.c: muxer using libavformat
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_block.h>
#include <vlc_sout.h>

#include <libavformat/avformat.h>

#include "avformat.h"
#include "../../codec/avcodec/avcodec.h"
#include "../../codec/avcodec/avcommon.h"


//#define AVFORMAT_DEBUG 1

static const char *const ppsz_mux_options[] = {
    "mux", "options", NULL
};

/*****************************************************************************
 * mux_sys_t: mux descriptor
 *****************************************************************************/
struct sout_mux_sys_t
{
    AVIOContext     *io;
    int             io_buffer_size;
    uint8_t        *io_buffer;

    AVFormatContext *oc;

    bool     b_write_header;
    bool     b_error;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

static int IOWrite( void *opaque, uint8_t *buf, int buf_size );
static int64_t IOSeek( void *opaque, int64_t offset, int whence );

/*****************************************************************************
 * Open
 *****************************************************************************/
int OpenMux( vlc_object_t *p_this )
{
    AVOutputFormat *file_oformat;
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys;
    char *psz_mux;

    vlc_init_avformat();

    config_ChainParse( p_mux, "sout-avformat-", ppsz_mux_options, p_mux->p_cfg );

    /* Find the requested muxer */
    psz_mux = var_GetNonEmptyString( p_mux, "sout-avformat-mux" );
    if( psz_mux )
    {
        file_oformat = av_guess_format( psz_mux, NULL, NULL );
        free( psz_mux );
    }
    else
    {
        file_oformat =
            av_guess_format( NULL, p_mux->p_access->psz_path, NULL);
    }
    if (!file_oformat)
    {
      msg_Err( p_mux, "unable for find a suitable output format" );
      return VLC_EGENERIC;
    }

    p_mux->p_sys = p_sys = malloc( sizeof( sout_mux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->oc = avformat_alloc_context();
    p_sys->oc->oformat = file_oformat;
    /* If we use dummy access, let avformat write output */
    if( !strcmp( p_mux->p_access->psz_access, "dummy") )
        strcpy( p_sys->oc->filename, p_mux->p_access->psz_path );

    /* Create I/O wrapper */
    p_sys->io_buffer_size = 32768;  /* FIXME */
    p_sys->io_buffer = malloc( p_sys->io_buffer_size );

    p_sys->io = avio_alloc_context(
        p_sys->io_buffer, p_sys->io_buffer_size,
        1, p_mux, NULL, IOWrite, IOSeek );

    p_sys->oc->pb = p_sys->io;
    p_sys->oc->nb_streams = 0;

    p_sys->b_write_header = true;
    p_sys->b_error = false;

    /* Fill p_mux fields */
    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
void CloseMux( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    if( !p_sys->b_write_header && !p_sys->b_error && av_write_trailer( p_sys->oc ) < 0 )
    {
        msg_Err( p_mux, "could not write trailer" );
    }

    avformat_free_context(p_sys->oc);

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
    int i_codec_id;

    msg_Dbg( p_mux, "adding input" );

    if( !GetFfmpegCodec( p_input->p_fmt->i_codec, 0, &i_codec_id, 0 ) )
    {
        msg_Dbg( p_mux, "couldn't find codec for fourcc '%4.4s'",
                 (char *)&p_input->p_fmt->i_codec );
        return VLC_EGENERIC;
    }

    p_input->p_sys = malloc( sizeof( int ) );
    *((int *)p_input->p_sys) = p_sys->oc->nb_streams;

    if( p_input->p_fmt->i_cat != VIDEO_ES && p_input->p_fmt->i_cat != AUDIO_ES)
    {
        msg_Warn( p_mux, "Unhandled ES category" );
        return VLC_EGENERIC;
    }

    stream = avformat_new_stream( p_sys->oc, NULL);
    if( !stream )
    {
        free( p_input->p_sys );
        return VLC_EGENERIC;
    }
    codec = stream->codec;

    codec->opaque = p_mux;

    switch( p_input->p_fmt->i_cat )
    {
    case AUDIO_ES:
        codec->codec_type = AVMEDIA_TYPE_AUDIO;
        codec->channels = p_input->p_fmt->audio.i_channels;
        codec->sample_rate = p_input->p_fmt->audio.i_rate;
        codec->time_base = (AVRational){1, codec->sample_rate};
        codec->frame_size = p_input->p_fmt->audio.i_frame_length;
        break;

    case VIDEO_ES:
        if( !p_input->p_fmt->video.i_frame_rate ||
            !p_input->p_fmt->video.i_frame_rate_base )
        {
            msg_Warn( p_mux, "Missing frame rate, assuming 25fps" );
            p_input->p_fmt->video.i_frame_rate = 25;
            p_input->p_fmt->video.i_frame_rate_base = 1;
        }
        codec->codec_type = AVMEDIA_TYPE_VIDEO;
        codec->width = p_input->p_fmt->video.i_width;
        codec->height = p_input->p_fmt->video.i_height;
        av_reduce( &codec->sample_aspect_ratio.num,
                   &codec->sample_aspect_ratio.den,
                   p_input->p_fmt->video.i_sar_num,
                   p_input->p_fmt->video.i_sar_den, 1 << 30 /* something big */ );
        stream->sample_aspect_ratio.den = codec->sample_aspect_ratio.den;
        stream->sample_aspect_ratio.num = codec->sample_aspect_ratio.num;
        codec->time_base.den = p_input->p_fmt->video.i_frame_rate;
        codec->time_base.num = p_input->p_fmt->video.i_frame_rate_base;
        break;

    }

    codec->bit_rate = p_input->p_fmt->i_bitrate;
    codec->codec_tag = av_codec_get_tag( p_sys->oc->oformat->codec_tag, i_codec_id );
    if( !codec->codec_tag && i_codec_id == AV_CODEC_ID_MP2 )
    {
        i_codec_id = AV_CODEC_ID_MP3;
        codec->codec_tag = av_codec_get_tag( p_sys->oc->oformat->codec_tag, i_codec_id );
    }
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

static int MuxBlock( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    block_t *p_data = block_FifoGet( p_input->p_fifo );
    int i_stream = *((int *)p_input->p_sys);
    AVStream *p_stream = p_sys->oc->streams[i_stream];
    AVPacket pkt;

    memset( &pkt, 0, sizeof(AVPacket) );

    av_init_packet(&pkt);
    pkt.data = p_data->p_buffer;
    pkt.size = p_data->i_buffer;
    pkt.stream_index = i_stream;

    if( p_data->i_flags & BLOCK_FLAG_TYPE_I ) pkt.flags |= AV_PKT_FLAG_KEY;

    if( p_data->i_pts > 0 )
        pkt.pts = p_data->i_pts * p_stream->time_base.den /
            INT64_C(1000000) / p_stream->time_base.num;
    if( p_data->i_dts > 0 )
        pkt.dts = p_data->i_dts * p_stream->time_base.den /
            INT64_C(1000000) / p_stream->time_base.num;

    /* this is another hack to prevent libavformat from triggering the "non monotone timestamps" check in avformat/utils.c */
    p_stream->cur_dts = ( p_data->i_dts * p_stream->time_base.den /
            INT64_C(1000000) / p_stream->time_base.num ) - 1;

    if( av_write_frame( p_sys->oc, &pkt ) < 0 )
    {
        msg_Err( p_mux, "could not write frame (pts: %"PRId64", dts: %"PRId64") "
                 "(pkt pts: %"PRId64", dts: %"PRId64")",
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

    if( p_sys->b_error ) return VLC_EGENERIC;

    if( p_sys->b_write_header )
    {
        int error;
        msg_Dbg( p_mux, "writing header" );

        char *psz_opts = var_GetNonEmptyString( p_mux, "sout-avformat-options" );
        AVDictionary *options = NULL;
        if (psz_opts && *psz_opts)
            options = vlc_av_get_options(psz_opts);
        free(psz_opts);
        error = avformat_write_header( p_sys->oc, options ? &options : NULL);
        AVDictionaryEntry *t = NULL;
        while ((t = av_dict_get(options, "", t, AV_DICT_IGNORE_SUFFIX))) {
            msg_Err( p_mux, "Unknown option \"%s\"", t->key );
        }
        av_dict_free(&options);
        if( error < 0 )
        {
            errno = AVUNERROR(error);
            msg_Err( p_mux, "could not write header: %m" );
            p_sys->b_write_header = false;
            p_sys->b_error = true;
            return VLC_EGENERIC;
        }

        avio_flush( p_sys->oc->pb );
        p_sys->b_write_header = false;
    }

    for( ;; )
    {
        mtime_t i_dts;

        int i_stream = sout_MuxGetStream( p_mux, 1, &i_dts );
        if( i_stream < 0 )
            return VLC_SUCCESS;

        MuxBlock( p_mux, p_mux->pp_inputs[i_stream] );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    bool *pb_bool;

    switch( i_query )
    {
    case MUX_CAN_ADD_STREAM_WHILE_MUXING:
        pb_bool = (bool*)va_arg( args, bool * );
        *pb_bool = false;
        return VLC_SUCCESS;

    case MUX_GET_ADD_STREAM_WAIT:
        pb_bool = (bool*)va_arg( args, bool * );
        *pb_bool = true;
        return VLC_SUCCESS;

    case MUX_GET_MIME:
    {
        char **ppsz = (char**)va_arg( args, char ** );
        *ppsz = strdup( p_mux->p_sys->oc->oformat->mime_type );
        return VLC_SUCCESS;
    }

    default:
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * I/O wrappers for libavformat
 *****************************************************************************/
static int IOWrite( void *opaque, uint8_t *buf, int buf_size )
{
    sout_mux_t *p_mux = opaque;
    int i_ret;

#ifdef AVFORMAT_DEBUG
    msg_Dbg( p_mux, "IOWrite %i bytes", buf_size );
#endif

    block_t *p_buf = block_Alloc( buf_size );
    if( buf_size > 0 ) memcpy( p_buf->p_buffer, buf, buf_size );

    if( p_mux->p_sys->b_write_header )
        p_buf->i_flags |= BLOCK_FLAG_HEADER;

    i_ret = sout_AccessOutWrite( p_mux->p_access, p_buf );
    return i_ret ? i_ret : -1;
}

static int64_t IOSeek( void *opaque, int64_t offset, int whence )
{
    sout_mux_t *p_mux = opaque;

#ifdef AVFORMAT_DEBUG
    msg_Dbg( p_mux, "IOSeek offset: %"PRId64", whence: %i", offset, whence );
#endif

    switch( whence )
    {
    case SEEK_SET:
        return sout_AccessOutSeek( p_mux->p_access, offset );
    case SEEK_CUR:
    case SEEK_END:
    default:
        return -1;
    }
}
