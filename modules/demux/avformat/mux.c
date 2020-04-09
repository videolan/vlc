/*****************************************************************************
 * mux.c: muxer using libavformat
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
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
#include <vlc_es.h>

#include <libavformat/avformat.h>

#include "avformat.h"
#include "../../codec/avcodec/avcodec.h"
#include "../../codec/avcodec/avcommon.h"
#include "../xiph.h"


//#define AVFORMAT_DEBUG 1

static const char *const ppsz_mux_options[] = {
    "mux", "options", "reset-ts", NULL
};

/*****************************************************************************
 * mux_sys_t: mux descriptor
 *****************************************************************************/
typedef struct
{
    AVIOContext     *io;
    int             io_buffer_size;
    uint8_t        *io_buffer;

    AVFormatContext *oc;

    bool     b_write_header;
    bool     b_write_keyframe;
    bool     b_error;
#if LIBAVFORMAT_VERSION_CHECK( 57, 7, 0, 40, 100 )
    bool     b_header_done;
#endif
} sout_mux_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static void DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

static int IOWrite( void *opaque, uint8_t *buf, int buf_size );
static int64_t IOSeek( void *opaque, int64_t offset, int whence );
#if LIBAVFORMAT_VERSION_CHECK( 57, 7, 0, 40, 100 )
static int IOWriteTyped(void *opaque, uint8_t *buf, int buf_size,
                              enum AVIODataMarkerType type, int64_t time);
#endif

/*****************************************************************************
 * Open
 *****************************************************************************/
int avformat_OpenMux( vlc_object_t *p_this )
{
    AVOutputFormat *file_oformat;
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    bool dummy = !strcmp( p_mux->p_access->psz_access, "dummy");

#if ( (LIBAVFORMAT_VERSION_MICRO >= 100) \
      && (LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 7, 100)) )
    if( dummy && strlen(p_mux->p_access->psz_path)
                              >= sizeof (((AVFormatContext *)NULL)->filename) )
        return VLC_EGENERIC;
#endif

    msg_Dbg( p_mux, "using %s %s", AVPROVIDER(LIBAVFORMAT), LIBAVFORMAT_IDENT );

    vlc_init_avformat(p_this);

    config_ChainParse( p_mux, "sout-avformat-", ppsz_mux_options, p_mux->p_cfg );

    /* Find the requested muxer */
    char *psz_mux = var_InheritString( p_mux, "sout-avformat-mux" );
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

    sout_mux_sys_t *p_sys = malloc( sizeof( sout_mux_sys_t ) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_mux->p_sys = p_sys;
    p_sys->oc = avformat_alloc_context();
    p_sys->oc->oformat = file_oformat;
    /* If we use dummy access, let avformat write output */
    if( dummy )
#if ( (LIBAVFORMAT_VERSION_MICRO >= 100) \
      && (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 7, 100)) )
        p_sys->oc->url = av_strdup(p_mux->p_access->psz_path);
#else
        strcpy( p_sys->oc->filename, p_mux->p_access->psz_path );
#endif

    /* Create I/O wrapper */
    p_sys->io_buffer_size = 10 * 1024 * 1024;  /* FIXME */
    p_sys->io_buffer = malloc( p_sys->io_buffer_size );

    bool b_can_seek;
    if( sout_AccessOutControl( p_mux->p_access, ACCESS_OUT_CAN_SEEK, &b_can_seek ) )
        b_can_seek = false;
    p_sys->io = avio_alloc_context(
        p_sys->io_buffer, p_sys->io_buffer_size,
        1, p_mux, NULL, IOWrite, b_can_seek ? IOSeek : NULL );

    p_sys->oc->pb = p_sys->io;
    p_sys->oc->nb_streams = 0;

    p_sys->b_write_header = true;
    p_sys->b_write_keyframe = false;
    p_sys->b_error = false;
#if LIBAVFORMAT_VERSION_CHECK( 57, 7, 0, 40, 100 )
    p_sys->io->write_data_type = IOWriteTyped;
    p_sys->b_header_done = false;
#endif
    if( var_GetBool( p_mux, "sout-avformat-reset-ts" ) )
        p_sys->oc->avoid_negative_ts = AVFMT_AVOID_NEG_TS_MAKE_ZERO;

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
void avformat_CloseMux( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    if( !p_sys->b_write_header && !p_sys->b_error && av_write_trailer( p_sys->oc ) < 0 )
    {
        msg_Err( p_mux, "could not write trailer" );
    }

    avformat_free_context(p_sys->oc);
    av_free(p_sys->io);

    free( p_sys->io_buffer );
    free( p_sys );
}

/*****************************************************************************
 * AddStream
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    const es_format_t *fmt = p_input->p_fmt;
    unsigned i_codec_id;

    msg_Dbg( p_mux, "adding input" );

    if( !GetFfmpegCodec( fmt->i_cat, fmt->i_codec, &i_codec_id, NULL )
     || i_codec_id == AV_CODEC_ID_NONE )
    {
        msg_Dbg( p_mux, "couldn't find codec for fourcc '%4.4s'",
                 (char *)&fmt->i_codec );
        return VLC_EGENERIC;
    }

    unsigned opus_size[XIPH_MAX_HEADER_COUNT];
    const void *opus_packet[XIPH_MAX_HEADER_COUNT];
    if( fmt->i_codec == VLC_CODEC_OPUS )
    {
        unsigned count;
        /* Only transmits the first packet (OpusHead) */
        if( xiph_SplitHeaders(opus_size, opus_packet, &count, fmt->i_extra, fmt->p_extra ) ) {
            count = 0;
        }
        if (count != 2 || opus_size[0] < 19) {
            msg_Err(p_mux, "Invalid Opus header");
            return VLC_EGENERIC;
        }
    }
    else if( fmt->i_codec == VLC_CODEC_MPGA && fmt->i_profile == 3 )
    {
        i_codec_id = AV_CODEC_ID_MP3;
    }

    /* Whitelist allowed ES categories */
    switch( fmt->i_cat )
    {
        case VIDEO_ES:
        case AUDIO_ES:
        case SPU_ES:
            break;
        default:
            msg_Warn( p_mux, "Unhandled ES category" );
            return VLC_EGENERIC;
    }

    /* */
    p_input->p_sys = malloc( sizeof( int ) );
    if( unlikely(p_input->p_sys == NULL) )
        return VLC_ENOMEM;

    *((int *)p_input->p_sys) = p_sys->oc->nb_streams;

    /* */
    AVStream *stream = avformat_new_stream( p_sys->oc, NULL);
    if( !stream )
    {
        free( p_input->p_sys );
        return VLC_EGENERIC;
    }

#if (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 5, 0))
    AVCodecParameters *codecpar = stream->codecpar;
#else
    AVCodecContext *codecpar = stream->codec;
#endif

    unsigned int i_bitrate = fmt->i_bitrate;
    unsigned int i_frame_rate = fmt->video.i_frame_rate;
    unsigned int i_frame_rate_base = fmt->video.i_frame_rate_base;
    switch( fmt->i_cat )
    {
    case AUDIO_ES:
        codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        codecpar->channels = fmt->audio.i_channels;
        codecpar->sample_rate = fmt->audio.i_rate;
        stream->time_base = (AVRational){1, codecpar->sample_rate};
        if (fmt->i_bitrate == 0) {
            msg_Warn( p_mux, "Missing audio bitrate, assuming 64k" );
            i_bitrate = 64000;
        }
        break;

    case SPU_ES:
        codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;
        /* psz_description and psz_language are expected to be compliant with
         * the muxing format. It should be the case when muxing a compliant
         * source to the same muxing format, but we have no general guarantee
         * at this point in the code. */
        if (fmt->psz_description != NULL && *fmt->psz_description != '\0')
            av_dict_set( &stream->metadata, "title", fmt->psz_description, 0 );
        if (fmt->psz_language != NULL && *fmt->psz_language != '\0')
            av_dict_set( &stream->metadata, "language", fmt->psz_language, 0 );
        break;

    case VIDEO_ES:
        if( !fmt->video.i_frame_rate || !fmt->video.i_frame_rate_base ) {
            msg_Warn( p_mux, "Missing frame rate, assuming 25fps" );
            i_frame_rate = 25;
            i_frame_rate_base = 1;
        } else
            msg_Dbg( p_mux, "Muxing framerate will be %d/%d = %.2f fps",
                    fmt->video.i_frame_rate,
                    fmt->video.i_frame_rate_base,
                    (double)fmt->video.i_frame_rate/(double)fmt->video.i_frame_rate_base );

        codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        codecpar->width = fmt->video.i_visible_width;
        codecpar->height = fmt->video.i_visible_height;
        av_reduce( &codecpar->sample_aspect_ratio.num,
                   &codecpar->sample_aspect_ratio.den,
                   fmt->video.i_sar_num,
                   fmt->video.i_sar_den, 1 << 30 /* something big */ );
        msg_Dbg(p_mux, "Muxing aspect ratio will be %d/%d",
                fmt->video.i_sar_num, fmt->video.i_sar_den);
        stream->sample_aspect_ratio.den = codecpar->sample_aspect_ratio.den;
        stream->sample_aspect_ratio.num = codecpar->sample_aspect_ratio.num;
        stream->time_base.den = i_frame_rate;
        stream->time_base.num = i_frame_rate_base;
        if(i_codec_id == AV_CODEC_ID_RAWVIDEO)
        {
            video_format_t vfmt;
            video_format_Copy(&vfmt, &fmt->video);
            video_format_FixRgb(&vfmt);
            if(GetFfmpegChroma(&codecpar->format, &vfmt))
                msg_Warn(p_mux, "can't match format RAW video %4.4s",
                         (const char *)&vfmt.i_chroma);
            video_format_Clean(&vfmt);
        }
        if (fmt->i_bitrate == 0) {
            msg_Warn( p_mux, "Missing video bitrate, assuming 512k" );
            i_bitrate = 512000;
        } else
            msg_Dbg( p_mux, "Muxing video bitrate will be %d", fmt->i_bitrate );
        break;

    default:
        vlc_assert_unreachable();
    }

    codecpar->bit_rate = i_bitrate;
    codecpar->codec_tag = av_codec_get_tag( p_sys->oc->oformat->codec_tag, i_codec_id );
    if( !codecpar->codec_tag && i_codec_id == AV_CODEC_ID_MP2 )
    {
        i_codec_id = AV_CODEC_ID_MP3;
        codecpar->codec_tag = av_codec_get_tag( p_sys->oc->oformat->codec_tag, i_codec_id );
    }
    codecpar->codec_id = i_codec_id;

    if( fmt->i_extra )
    {
        if( fmt->i_codec == VLC_CODEC_OPUS )
        {
            codecpar->extradata_size = opus_size[0];
            codecpar->extradata = av_malloc( opus_size[0] );
            memcpy( codecpar->extradata, opus_packet[0], opus_size[0] );
        }
        else
        {
            codecpar->extradata_size = fmt->i_extra;
            codecpar->extradata = av_malloc( fmt->i_extra );
            memcpy( codecpar->extradata, fmt->p_extra, fmt->i_extra );
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream
 *****************************************************************************/
static void DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "removing input" );
    free( p_input->p_sys );
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

    if( p_data->i_flags & BLOCK_FLAG_TYPE_I )
    {
#ifdef AVFMT_ALLOW_FLUSH
        /* Make sure we don't inadvertedly mark buffered data as keyframes. */
        if( p_sys->oc->oformat->flags & AVFMT_ALLOW_FLUSH )
            av_write_frame( p_sys->oc, NULL );
#endif

        p_sys->b_write_keyframe = true;
        pkt.flags |= AV_PKT_FLAG_KEY;
    }

    if( p_data->i_pts > 0 )
        pkt.pts = TO_AV_TS(p_data->i_pts * p_stream->time_base.den /
            CLOCK_FREQ / p_stream->time_base.num);
    if( p_data->i_dts > 0 )
        pkt.dts = TO_AV_TS(p_data->i_dts * p_stream->time_base.den /
            CLOCK_FREQ / p_stream->time_base.num);

    /* this is another hack to prevent libavformat from triggering the "non monotone timestamps" check in avformat/utils.c */
    p_stream->cur_dts = ( p_data->i_dts * p_stream->time_base.den /
            CLOCK_FREQ / p_stream->time_base.num ) - 1;

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

#if LIBAVFORMAT_VERSION_CHECK( 57, 7, 0, 40, 100 )
int IOWriteTyped(void *opaque, uint8_t *buf, int buf_size,
                              enum AVIODataMarkerType type, int64_t time)
{
    VLC_UNUSED(time);

    sout_mux_t *p_mux = opaque;
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    if ( !p_sys->b_header_done && type != AVIO_DATA_MARKER_HEADER )
        p_sys->b_header_done = true;
    return IOWrite(opaque, buf, buf_size);
}
#endif

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
        if (psz_opts) {
            vlc_av_get_options(psz_opts, &options);
            free(psz_opts);
        }
        av_dict_set( &p_sys->oc->metadata, "encoding_tool", "VLC "VERSION, 0 );
        error = avformat_write_header( p_sys->oc, options ? &options : NULL);
        AVDictionaryEntry *t = NULL;
        while ((t = av_dict_get(options, "", t, AV_DICT_IGNORE_SUFFIX))) {
            msg_Err( p_mux, "Unknown option \"%s\"", t->key );
        }
        av_dict_free(&options);
        if( error < 0 )
        {
            msg_Err( p_mux, "could not write header: %s",
                     vlc_strerror_c(AVUNERROR(error)) );
            p_sys->b_write_header = false;
            p_sys->b_error = true;
            return VLC_EGENERIC;
        }

        avio_flush( p_sys->oc->pb );
        p_sys->b_write_header = false;
    }

    for( ;; )
    {
        vlc_tick_t i_dts;

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
        pb_bool = va_arg( args, bool * );
        *pb_bool = false;
        return VLC_SUCCESS;

    case MUX_GET_MIME:
    {
        char **ppsz = va_arg( args, char ** );
        sout_mux_sys_t *p_sys = p_mux->p_sys;
        *ppsz = strdup( p_sys->oc->oformat->mime_type );
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
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i_ret;

#ifdef AVFORMAT_DEBUG
    msg_Dbg( p_mux, "IOWrite %i bytes", buf_size );
#endif

    block_t *p_buf = block_Alloc( buf_size );
    if( buf_size > 0 ) memcpy( p_buf->p_buffer, buf, buf_size );

    if( p_sys->b_write_header )
        p_buf->i_flags |= BLOCK_FLAG_HEADER;
#if LIBAVFORMAT_VERSION_CHECK( 57, 7, 0, 40, 100 )
    if( !p_sys->b_header_done )
        p_buf->i_flags |= BLOCK_FLAG_HEADER;
#endif

    if( p_sys->b_write_keyframe )
    {
        p_buf->i_flags |= BLOCK_FLAG_TYPE_I;
        p_sys->b_write_keyframe = false;
    }

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
