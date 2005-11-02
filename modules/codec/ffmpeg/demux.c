/*****************************************************************************
 * demux.c: demuxer using ffmpeg (libavformat).
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include "vlc_meta.h"

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avformat.h>
#else
#   include <avformat.h>
#endif

#include "ffmpeg.h"

//#define AVFORMAT_DEBUG 1

/* Version checking */
#if (LIBAVFORMAT_BUILD >= 4629) && defined(HAVE_LIBAVFORMAT)

/*****************************************************************************
 * demux_sys_t: demux descriptor
 *****************************************************************************/
struct demux_sys_t
{
    ByteIOContext   io;
    int             io_buffer_size;
    uint8_t        *io_buffer;

    AVInputFormat  *fmt;
    AVFormatContext *ic;
    URLContext     url;
    URLProtocol    prot;

    int             i_tk;
    es_out_id_t     **tk;

    int64_t     i_pcr;
    int64_t     i_pcr_inc;
    int         i_pcr_tk;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static int IORead( void *opaque, uint8_t *buf, int buf_size );
static int IOSeek( void *opaque, offset_t offset, int whence );

/*****************************************************************************
 * Open
 *****************************************************************************/
int E_(OpenDemux)( vlc_object_t *p_this )
{
    demux_t       *p_demux = (demux_t*)p_this;
    demux_sys_t   *p_sys;
    AVProbeData   pd;
    AVInputFormat *fmt;
    int i;

    /* Init Probe data */
    pd.filename = p_demux->psz_path;
    if( ( pd.buf_size = stream_Peek( p_demux->s, &pd.buf, 2048 ) ) <= 0 )
    {
        msg_Warn( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }

    av_register_all(); /* Can be called several times */

    /* Guess format */
    if( !( fmt = av_probe_input_format( &pd, 1 ) ) )
    {
        msg_Dbg( p_demux, "couldn't guess format" );
        return VLC_EGENERIC;
    }

    /* Don't try to handle MPEG unless forced */
    if( !p_demux->b_force &&
        ( !strcmp( fmt->name, "mpeg" ) ||
          !strcmp( fmt->name, "vcd" ) ||
          !strcmp( fmt->name, "vob" ) ||
          !strcmp( fmt->name, "mpegts" ) ||
          /* libavformat's redirector won't work */
          !strcmp( fmt->name, "redir" ) ||
          !strcmp( fmt->name, "sdp" ) ) )
    {
        return VLC_EGENERIC;
    }

    /* Don't trigger false alarms on bin files */
    if( !p_demux->b_force && !strcmp( fmt->name, "psxstr" ) )
    {
        int i_len;

        if( !p_demux->psz_path ) return VLC_EGENERIC;

        i_len = strlen( p_demux->psz_path );
        if( i_len < 4 ) return VLC_EGENERIC;

        if( strcasecmp( &p_demux->psz_path[i_len - 4], ".str" ) &&
            strcasecmp( &p_demux->psz_path[i_len - 4], ".xai" ) &&
            strcasecmp( &p_demux->psz_path[i_len - 3], ".xa" ) )
        {
            return VLC_EGENERIC;
        }
    }

    msg_Dbg( p_demux, "detected format: %s", fmt->name );

    /* Fill p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->ic = 0;
    p_sys->fmt = fmt;
    p_sys->i_tk = 0;
    p_sys->tk = NULL;
    p_sys->i_pcr_tk = -1;
    p_sys->i_pcr = -1;

    /* Create I/O wrapper */
    p_sys->io_buffer_size = 32768;  /* FIXME */
    p_sys->io_buffer = malloc( p_sys->io_buffer_size );
    p_sys->url.priv_data = p_demux;
    p_sys->url.prot = &p_sys->prot;
    p_sys->url.prot->name = "VLC I/O wrapper";
    p_sys->url.prot->url_open = 0;
    p_sys->url.prot->url_read =
                    (int (*) (URLContext *, unsigned char *, int))IORead;
    p_sys->url.prot->url_write = 0;
    p_sys->url.prot->url_seek =
                    (offset_t (*) (URLContext *, offset_t, int))IOSeek;
    p_sys->url.prot->url_close = 0;
    p_sys->url.prot->next = 0;
    init_put_byte( &p_sys->io, p_sys->io_buffer, p_sys->io_buffer_size,
                   0, &p_sys->url, IORead, NULL, IOSeek );

    p_sys->fmt->flags |= AVFMT_NOFILE; /* libavformat must not fopen/fclose */

    /* Open it */
    if( av_open_input_stream( &p_sys->ic, &p_sys->io, p_demux->psz_path,
                              p_sys->fmt, NULL ) )
    {
        msg_Err( p_demux, "av_open_input_stream failed" );
        E_(CloseDemux)( p_this );
        return VLC_EGENERIC;
    }

    if( av_find_stream_info( p_sys->ic ) < 0 )
    {
        msg_Err( p_demux, "av_find_stream_info failed" );
        E_(CloseDemux)( p_this );
        return VLC_EGENERIC;
    }

    for( i = 0; i < p_sys->ic->nb_streams; i++ )
    {
        AVCodecContext *cc = p_sys->ic->streams[i]->codec;
        es_out_id_t  *es;
        es_format_t  fmt;
        vlc_fourcc_t fcc;

        if( !E_(GetVlcFourcc)( cc->codec_id, NULL, &fcc, NULL ) )
        {
            fcc = VLC_FOURCC( 'u', 'n', 'd', 'f' );

            /* Special case for raw video data */
            if( cc->codec_id == CODEC_ID_RAWVIDEO )
            {
                msg_Dbg( p_demux, "raw video, pixel format: %i", cc->pix_fmt );
                fcc = E_(GetVlcChroma)( cc->pix_fmt );
            }
        }

        switch( cc->codec_type )
        {
        case CODEC_TYPE_AUDIO:
            es_format_Init( &fmt, AUDIO_ES, fcc );
            fmt.audio.i_channels = cc->channels;
            fmt.audio.i_rate = cc->sample_rate;
            fmt.audio.i_bitspersample = cc->bits_per_sample;
            fmt.audio.i_blockalign = cc->block_align;
            break;
        case CODEC_TYPE_VIDEO:
            es_format_Init( &fmt, VIDEO_ES, fcc );
            fmt.video.i_width = cc->width;
            fmt.video.i_height = cc->height;
            if( cc->palctrl )
            {
                fmt.video.p_palette = malloc( sizeof(video_palette_t) );
                *fmt.video.p_palette = *(video_palette_t *)cc->palctrl;
            }
            break;
        default:
            break;
        }

        fmt.i_extra = cc->extradata_size;
        fmt.p_extra = cc->extradata;
        es = es_out_Add( p_demux->out, &fmt );

        msg_Dbg( p_demux, "adding es: %s codec = %4.4s",
                 cc->codec_type == CODEC_TYPE_AUDIO ? "audio" : "video",
                 (char*)&fcc );
        TAB_APPEND( p_sys->i_tk, p_sys->tk, es );
    }

    msg_Dbg( p_demux, "AVFormat supported stream" );
    msg_Dbg( p_demux, "    - format = %s (%s)",
             p_sys->fmt->name, p_sys->fmt->long_name );
    msg_Dbg( p_demux, "    - start time = "I64Fd,
             ( p_sys->ic->start_time != AV_NOPTS_VALUE ) ?
             p_sys->ic->start_time * 1000000 / AV_TIME_BASE : -1 );
    msg_Dbg( p_demux, "    - duration = "I64Fd,
             ( p_sys->ic->duration != AV_NOPTS_VALUE ) ?
             p_sys->ic->duration * 1000000 / AV_TIME_BASE : -1 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
void E_(CloseDemux)( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->ic ) av_close_input_file( p_sys->ic );
    if( p_sys->io_buffer ) free( p_sys->io_buffer );
    free( p_sys );
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    AVPacket    pkt;
    block_t     *p_frame;
    int64_t     i_start_time;

    /* Read a frame */
    if( av_read_frame( p_sys->ic, &pkt ) )
    {
        return 0;
    }
    if( pkt.stream_index < 0 || pkt.stream_index >= p_sys->i_tk )
    {
        av_free_packet( &pkt );
        return 1;
    }
    if( ( p_frame = block_New( p_demux, pkt.size ) ) == NULL )
    {
        return 0;
    }

    memcpy( p_frame->p_buffer, pkt.data, pkt.size );

    i_start_time = ( p_sys->ic->start_time != AV_NOPTS_VALUE ) ?
        p_sys->ic->start_time : 0;

    p_frame->i_dts = ( pkt.dts == AV_NOPTS_VALUE ) ?
        0 : (pkt.dts - i_start_time) * 1000000 *
        p_sys->ic->streams[pkt.stream_index]->time_base.num /
        p_sys->ic->streams[pkt.stream_index]->time_base.den;
    p_frame->i_pts = ( pkt.pts == AV_NOPTS_VALUE ) ?
        0 : (pkt.pts - i_start_time) * 1000000 *
        p_sys->ic->streams[pkt.stream_index]->time_base.num /
        p_sys->ic->streams[pkt.stream_index]->time_base.den;

#ifdef AVFORMAT_DEBUG
    msg_Dbg( p_demux, "tk[%d] dts="I64Fd" pts="I64Fd,
             pkt.stream_index, p_frame->i_dts, p_frame->i_pts );
#endif

    if( pkt.dts > 0  &&
        ( pkt.stream_index == p_sys->i_pcr_tk || p_sys->i_pcr_tk < 0 ) )
    {    
        p_sys->i_pcr_tk = pkt.stream_index;
        p_sys->i_pcr = p_frame->i_dts;

        es_out_Control( p_demux->out, ES_OUT_SET_PCR, (int64_t)p_sys->i_pcr );
    }

    es_out_Send( p_demux->out, p_sys->tk[pkt.stream_index], p_frame );
    av_free_packet( &pkt );
    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64, *pi64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*) va_arg( args, double* ); *pf = 0.0;
            i64 = stream_Size( p_demux->s );
            if( i64 > 0 )
            {
                *pf = (double)stream_Tell( p_demux->s ) / (double)i64;
            }

            if( p_sys->ic->duration != AV_NOPTS_VALUE && p_sys->i_pcr > 0 )
            {
                *pf = (double)p_sys->i_pcr / (double)p_sys->ic->duration;
            }

            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double) va_arg( args, double );
            i64 = stream_Tell( p_demux->s );
            if( i64 && p_sys->i_pcr > 0 )
            {
                int64_t i_size = stream_Size( p_demux->s );

                i64 = p_sys->i_pcr * i_size / i64 * f;
                if( p_sys->ic->start_time != AV_NOPTS_VALUE )
                    i64 += p_sys->ic->start_time;

                if( p_sys->ic->duration != AV_NOPTS_VALUE )
                    i64 = p_sys->ic->duration * f;

                msg_Warn( p_demux, "DEMUX_SET_POSITION: "I64Fd, i64 );

                if( av_seek_frame( p_sys->ic, -1, i64, 0 ) < 0 )
                {
                    return VLC_EGENERIC;
                }
                es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
                p_sys->i_pcr = -1; /* Invalidate time display */
            }
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->ic->duration != AV_NOPTS_VALUE )
            {
                *pi64 = p_sys->ic->duration;
            }
            else *pi64 = 0;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_pcr;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            if( p_sys->ic->start_time != AV_NOPTS_VALUE )
                i64 += p_sys->ic->start_time;

            msg_Warn( p_demux, "DEMUX_SET_TIME: "I64Fd, i64 );

            if( av_seek_frame( p_sys->ic, -1, i64, 0 ) < 0 )
            {
                return VLC_EGENERIC;
            }
            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
            p_sys->i_pcr = -1; /* Invalidate time display */
            return VLC_SUCCESS;

        case DEMUX_GET_META:
        {
            vlc_meta_t **pp_meta = (vlc_meta_t**)va_arg( args, vlc_meta_t** );
            vlc_meta_t *meta;

            if( !p_sys->ic->title[0] || !p_sys->ic->author[0] ||
                !p_sys->ic->copyright[0] || !p_sys->ic->comment[0] ||
                /*!p_sys->ic->album[0] ||*/ !p_sys->ic->genre[0] )
            {
                return VLC_EGENERIC;
            }

            *pp_meta = meta = vlc_meta_New();

            if( p_sys->ic->title[0] )
                vlc_meta_Add( meta, VLC_META_TITLE, p_sys->ic->title );
            if( p_sys->ic->author[0] )
                vlc_meta_Add( meta, VLC_META_AUTHOR, p_sys->ic->author );
            if( p_sys->ic->copyright[0] )
                vlc_meta_Add( meta, VLC_META_COPYRIGHT, p_sys->ic->copyright );
            if( p_sys->ic->comment[0] )
                vlc_meta_Add( meta, VLC_META_DESCRIPTION, p_sys->ic->comment );
            if( p_sys->ic->genre[0] )
                vlc_meta_Add( meta, VLC_META_GENRE, p_sys->ic->genre );
            return VLC_SUCCESS;
        }

        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * I/O wrappers for libavformat
 *****************************************************************************/
static int IORead( void *opaque, uint8_t *buf, int buf_size )
{
    URLContext *p_url = opaque;
    demux_t *p_demux = p_url->priv_data;
    int i_ret = stream_Read( p_demux->s, buf, buf_size );
    return i_ret ? i_ret : -1;
}

static int IOSeek( void *opaque, offset_t offset, int whence )
{
    URLContext *p_url = opaque;
    demux_t *p_demux = p_url->priv_data;
    int64_t i_absolute;

#ifdef AVFORMAT_DEBUG
    msg_Warn( p_demux, "IOSeek offset: "I64Fd", whence: %i", offset, whence );
#endif

    switch( whence )
    {
        case SEEK_SET:
            i_absolute = offset;
            break;
        case SEEK_CUR:
            i_absolute = stream_Tell( p_demux->s ) + offset;
            break;
        case SEEK_END:
            i_absolute = stream_Size( p_demux->s ) - offset;
            break;
        default:
            return -1;

    }

    if( stream_Seek( p_demux->s, i_absolute ) )
    {
        return -1;
    }

    return 0;
}

#else /* LIBAVFORMAT_BUILD >= 4611 */

int E_(OpenDemux)( vlc_object_t *p_this )
{
    return VLC_EGENERIC;
}

void E_(CloseDemux)( vlc_object_t *p_this )
{
}

#endif /* LIBAVFORMAT_BUILD >= 4629 */
