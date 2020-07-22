/*****************************************************************************
 * demux.c: demuxer using libavformat
 *****************************************************************************
 * Copyright (C) 2004-2009 VLC authors and VideoLAN
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
#include <vlc_demux.h>
#include <vlc_stream.h>
#include <vlc_meta.h>
#include <vlc_input.h>
#include <vlc_charset.h>
#include <vlc_avcodec.h>

#include "../../codec/avcodec/avcodec.h"
#include "../../codec/avcodec/chroma.h"
#include "../../codec/avcodec/avcommon_compat.h"
#include "avformat.h"
#include "../xiph.h"
#include "../vobsub.h"

#include <libavformat/avformat.h>
#include <libavutil/display.h>

//#define AVFORMAT_DEBUG 1

# define HAVE_AVUTIL_CODEC_ATTACHMENT 1

struct avformat_track_s
{
    es_out_id_t *p_es;
    vlc_tick_t i_pcr;
};

/*****************************************************************************
 * demux_sys_t: demux descriptor
 *****************************************************************************/
typedef struct
{
    AVInputFormat  *fmt;
    AVFormatContext *ic;

    struct avformat_track_s *tracks;
    unsigned i_tracks;

    vlc_tick_t      i_pcr;

    unsigned    i_ssa_order;

    int                i_attachments;
    input_attachment_t **attachments;

    /* Only one title with seekpoints possible atm. */
    input_title_t *p_title;
    int i_seekpoint;
    unsigned i_update;
} demux_sys_t;

#define AVFORMAT_IOBUFFER_SIZE 32768  /* FIXME */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static int IORead( void *opaque, uint8_t *buf, int buf_size );
static int64_t IOSeek( void *opaque, int64_t offset, int whence );

static block_t *BuildSsaFrame( const AVPacket *p_pkt, unsigned i_order );
static void UpdateSeekPoint( demux_t *p_demux, vlc_tick_t i_time );
static void ResetTime( demux_t *p_demux, int64_t i_time );

static vlc_fourcc_t CodecTagToFourcc( uint32_t codec_tag )
{
    // convert from little-endian avcodec codec_tag to VLC native-endian fourcc
#ifdef WORDS_BIGENDIAN
    return vlc_bswap32(codec_tag);
#else
    return codec_tag;
#endif
}

/*****************************************************************************
 * Open
 *****************************************************************************/

static void get_rotation(es_format_t *fmt, AVStream *s)
{
    char const *kRotateKey = "rotate";
    AVDictionaryEntry *rotation = av_dict_get(s->metadata, kRotateKey, NULL, 0);
    long angle = 0;

    int32_t *matrix = (int32_t *)av_stream_get_side_data(s, AV_PKT_DATA_DISPLAYMATRIX, NULL);
    if( matrix ) {
        int64_t det = (int64_t)matrix[0] * matrix[4] - (int64_t)matrix[1] * matrix[3];
        if (det < 0) {
            /* Flip the matrix to decouple flip and rotation operations.
             * Always assume an horizontal flip for simplicity,
             * it can be changed later if rotation is 180º. */
            av_display_matrix_flip(matrix, 1, 0);
        }
        angle = lround(av_display_rotation_get(matrix));

        if (angle > 45 && angle < 135)
            fmt->video.orientation = ORIENT_ROTATED_270;

        else if (angle > 135 || angle < -135) {
            if (det < 0)
                fmt->video.orientation = ORIENT_VFLIPPED;
            else
                fmt->video.orientation = ORIENT_ROTATED_180;
        }
        else if (angle < -45 && angle > -135)
            fmt->video.orientation = ORIENT_ROTATED_90;

        else
            fmt->video.orientation = ORIENT_NORMAL;

        /* Flip is already applied to the 180º case. */
        if (det < 0 && !(angle > 135 || angle < -135)) {
            video_transform_t transform = (video_transform_t)fmt->video.orientation;
            /* Flip first then rotate */
            fmt->video.orientation = ORIENT_HFLIPPED;
            video_format_TransformBy(&fmt->video, transform);
        }

    } else if( rotation ) {
        angle = strtol(rotation->value, NULL, 10);

        if (angle > 45 && angle < 135)
            fmt->video.orientation = ORIENT_ROTATED_90;

        else if (angle > 135 && angle < 225)
            fmt->video.orientation = ORIENT_ROTATED_180;

        else if (angle > 225 && angle < 315)
            fmt->video.orientation = ORIENT_ROTATED_270;

        else
            fmt->video.orientation = ORIENT_NORMAL;
    }
}

static AVDictionary * BuildAVOptions( demux_t *p_demux )
{
    char *psz_opts = var_InheritString( p_demux, "avformat-options" );
    AVDictionary *options = NULL;
    if( psz_opts )
    {
        vlc_av_get_options( psz_opts, &options );
        free( psz_opts );
    }
    return options;
}

static void FreeUnclaimedOptions( demux_t *p_demux, AVDictionary **pp_dict )
{
    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(*pp_dict, "", t, AV_DICT_IGNORE_SUFFIX))) {
        msg_Err( p_demux, "Unknown option \"%s\"", t->key );
    }
    av_dict_free(pp_dict);
}

static void FindStreamInfo( demux_t *p_demux, AVDictionary *options )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned nb_streams = p_sys->ic->nb_streams;

    AVDictionary *streamsoptions[nb_streams ? nb_streams : 1];

    streamsoptions[0] = options;
    for ( unsigned i = 1; i < nb_streams; i++ )
    {
        streamsoptions[i] = NULL;
        if( streamsoptions[0] )
            av_dict_copy( &streamsoptions[i], streamsoptions[0], 0 );
    }

    vlc_avcodec_lock(); /* avformat calls avcodec behind our back!!! */
    int error = avformat_find_stream_info( p_sys->ic, streamsoptions );
    vlc_avcodec_unlock();

    FreeUnclaimedOptions( p_demux, &streamsoptions[0] );
    for ( unsigned i = 1; i < nb_streams; i++ ) {
        av_dict_free( &streamsoptions[i] );
    }

    if( error < 0 )
    {
        msg_Warn( p_demux, "Could not find stream info: %s",
                  vlc_strerror_c(AVUNERROR(error)) );
    }
}

static int avformat_ProbeDemux( vlc_object_t *p_this,
                                AVInputFormat **pp_fmt, const char *psz_url )
{
    demux_t       *p_demux = (demux_t*)p_this;
    AVProbeData   pd = { 0 };
    const uint8_t *peek;

    /* Init Probe data */
    pd.buf_size = vlc_stream_Peek( p_demux->s, &peek, 2048 + 213 );
    if( pd.buf_size <= 0 )
        return VLC_EGENERIC;

    pd.buf = malloc( pd.buf_size + AVPROBE_PADDING_SIZE );
    if( unlikely(pd.buf == NULL) )
        return VLC_ENOMEM;

    memcpy( pd.buf, peek, pd.buf_size );
    memset( pd.buf + pd.buf_size, 0, AVPROBE_PADDING_SIZE );

    if( psz_url != NULL )
        msg_Dbg( p_demux, "trying url: %s", psz_url );

    pd.filename = psz_url;

    vlc_init_avformat(p_this);

    /* Guess format */
    char *psz_format = var_InheritString( p_this, "avformat-format" );
    if( psz_format )
    {
        if( (*pp_fmt = av_find_input_format(psz_format)) )
            msg_Dbg( p_demux, "forcing format: %s", (*pp_fmt)->name );
        free( psz_format );
    }

    if( *pp_fmt == NULL )
        *pp_fmt = av_probe_input_format( &pd, 1 );

    free( pd.buf );

    if( *pp_fmt == NULL )
    {
        msg_Dbg( p_demux, "couldn't guess format" );
        return VLC_EGENERIC;
    }

    if( !p_demux->obj.force )
    {
        static const char ppsz_blacklist[][16] = {
            /* Don't handle MPEG unless forced */
            "mpeg", "vcd", "vob", "mpegts",
            /* libavformat's redirector won't work */
            "redir", "sdp",
            /* Don't handle subtitles format */
            "ass", "srt", "microdvd",
            /* No timestamps at all */
            "hevc", "h264",
            ""
        };

        for( int i = 0; *ppsz_blacklist[i]; i++ )
        {
            if( !strcmp( (*pp_fmt)->name, ppsz_blacklist[i] ) )
                return VLC_EGENERIC;
        }
    }

    /* Don't trigger false alarms on bin files */
    if( !p_demux->obj.force && !strcmp( (*pp_fmt)->name, "psxstr" ) )
    {
        int i_len;

        if( !p_demux->psz_filepath )
            return VLC_EGENERIC;

        i_len = strlen( p_demux->psz_filepath );
        if( i_len < 4 )
            return VLC_EGENERIC;

        if( strcasecmp( &p_demux->psz_filepath[i_len - 4], ".str" ) &&
            strcasecmp( &p_demux->psz_filepath[i_len - 4], ".xai" ) &&
            strcasecmp( &p_demux->psz_filepath[i_len - 3], ".xa" ) )
        {
            return VLC_EGENERIC;
        }
    }

    msg_Dbg( p_demux, "detected format: %s", (*pp_fmt)->name );

    return VLC_SUCCESS;
}

int avformat_OpenDemux( vlc_object_t *p_this )
{
    demux_t       *p_demux = (demux_t*)p_this;
    demux_sys_t   *p_sys;
    AVInputFormat *fmt = NULL;
    vlc_tick_t    i_start_time = VLC_TICK_INVALID;
    bool          b_can_seek;
    const char    *psz_url;
    int           error;

    if( p_demux->psz_filepath )
        psz_url = p_demux->psz_filepath;
    else
        psz_url = p_demux->psz_url;

    if( avformat_ProbeDemux( p_this, &fmt, psz_url ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    vlc_stream_Control( p_demux->s, STREAM_CAN_SEEK, &b_can_seek );

    /* Fill p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->ic = 0;
    p_sys->fmt = fmt;
    p_sys->tracks = NULL;
    p_sys->i_ssa_order = 0;
    TAB_INIT( p_sys->i_attachments, p_sys->attachments);
    p_sys->p_title = NULL;
    p_sys->i_seekpoint = 0;
    p_sys->i_update = 0;

    /* Create I/O wrapper */
    unsigned char * p_io_buffer = av_malloc( AVFORMAT_IOBUFFER_SIZE );
    if( !p_io_buffer )
    {
        avformat_CloseDemux( p_this );
        return VLC_ENOMEM;
    }

    p_sys->ic = avformat_alloc_context();
    if( !p_sys->ic )
    {
        av_free( p_io_buffer );
        avformat_CloseDemux( p_this );
        return VLC_ENOMEM;
    }

    AVIOContext *pb = p_sys->ic->pb = avio_alloc_context( p_io_buffer,
        AVFORMAT_IOBUFFER_SIZE, 0, p_demux, IORead, NULL, IOSeek );
    if( !pb )
    {
        av_free( p_io_buffer );
        avformat_CloseDemux( p_this );
        return VLC_ENOMEM;
    }

    /* get all options, open_input will consume its own options from the dict */
    AVDictionary *options = BuildAVOptions( p_demux );

    p_sys->ic->pb->seekable = b_can_seek ? AVIO_SEEKABLE_NORMAL : 0;
    error = avformat_open_input( &p_sys->ic, psz_url, p_sys->fmt, &options );
    if( error < 0 )
    {
        msg_Err( p_demux, "Could not open %s: %s", psz_url,
                 vlc_strerror_c(AVUNERROR(error)) );
        av_dict_free( &options );
        av_free( pb->buffer );
        av_free( pb );
        p_sys->ic = NULL;
        avformat_CloseDemux( p_this );
        return VLC_EGENERIC;
    }

    /* pass remaining options for as streams options */
    FindStreamInfo( p_demux, options );

    unsigned nb_streams = p_sys->ic->nb_streams; /* it may have changed */
    if( !nb_streams )
    {
        msg_Err( p_demux, "No streams found");
        avformat_CloseDemux( p_this );
        return VLC_EGENERIC;
    }
    p_sys->tracks = calloc( nb_streams, sizeof(*p_sys->tracks) );
    if( !p_sys->tracks )
    {
        avformat_CloseDemux( p_this );
        return VLC_ENOMEM;
    }
    p_sys->i_tracks = nb_streams;

    if( error < 0 )
    {
        msg_Warn( p_demux, "Could not find stream info: %s",
                  vlc_strerror_c(AVUNERROR(error)) );
    }

    for( unsigned i = 0; i < nb_streams; i++ )
    {
        struct avformat_track_s *p_track = &p_sys->tracks[i];
        AVStream *s = p_sys->ic->streams[i];
        const AVCodecParameters *cp = s->codecpar;
        es_format_t es_fmt;
        const char *psz_type = "unknown";

        /* Do not use the cover art as a stream */
        if( s->disposition == AV_DISPOSITION_ATTACHED_PIC )
            continue;

        vlc_fourcc_t fcc = GetVlcFourcc( cp->codec_id );
        switch( cp->codec_type )
        {
        case AVMEDIA_TYPE_AUDIO:
            es_format_Init( &es_fmt, AUDIO_ES, fcc );
            es_fmt.i_original_fourcc = CodecTagToFourcc( cp->codec_tag );
            es_fmt.i_bitrate = cp->bit_rate;
            es_fmt.audio.i_channels = cp->channels;
            es_fmt.audio.i_rate = cp->sample_rate;
            es_fmt.audio.i_bitspersample = cp->bits_per_coded_sample;
            es_fmt.audio.i_blockalign = cp->block_align;
            psz_type = "audio";

            if(cp->codec_id == AV_CODEC_ID_AAC_LATM)
            {
                es_fmt.i_original_fourcc = VLC_FOURCC('L','A','T','M');
                es_fmt.b_packetized = false;
            }
            else if(cp->codec_id == AV_CODEC_ID_AAC && p_sys->fmt->long_name &&
                    strstr(p_sys->fmt->long_name, "raw ADTS AAC"))
            {
                es_fmt.i_original_fourcc = VLC_FOURCC('A','D','T','S');
                es_fmt.b_packetized = false;
            }
            break;

        case AVMEDIA_TYPE_VIDEO:
            es_format_Init( &es_fmt, VIDEO_ES, fcc );
            es_fmt.i_original_fourcc = CodecTagToFourcc( cp->codec_tag );

            es_fmt.video.i_bits_per_pixel = cp->bits_per_coded_sample;
            /* Special case for raw video data */
            if( cp->codec_id == AV_CODEC_ID_RAWVIDEO )
            {
                msg_Dbg( p_demux, "raw video, pixel format: %i", cp->format );
                if( GetVlcChroma( &es_fmt.video, cp->format ) != VLC_SUCCESS)
                {
                    msg_Err( p_demux, "was unable to find a FourCC match for raw video" );
                }
                else
                    es_fmt.i_codec = es_fmt.video.i_chroma;
            }
            /* We need this for the h264 packetizer */
            else if( cp->codec_id == AV_CODEC_ID_H264 && ( p_sys->fmt == av_find_input_format("flv") ||
                p_sys->fmt == av_find_input_format("matroska") || p_sys->fmt == av_find_input_format("mp4") ) )
                es_fmt.i_original_fourcc = VLC_FOURCC( 'a', 'v', 'c', '1' );

            es_fmt.video.i_width = cp->width;
            es_fmt.video.i_height = cp->height;
            es_fmt.video.i_visible_width = es_fmt.video.i_width;
            es_fmt.video.i_visible_height = es_fmt.video.i_height;

            get_rotation(&es_fmt, s);

# warning FIXME: implement palette transmission
            psz_type = "video";

            AVRational rate;
#if (LIBAVUTIL_VERSION_MICRO < 100) /* libav */
# if (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(55, 20, 0))
            rate.num = s->time_base.num;
            rate.den = s->time_base.den;
# else
            rate.num = s->codec->time_base.num;
            rate.den = s->codec->time_base.den;
# endif
            rate.den *= __MAX( s->codec->ticks_per_frame, 1 );
#else /* ffmpeg */
            rate = av_guess_frame_rate( p_sys->ic, s, NULL );
#endif
            if( rate.den && rate.num )
            {
                es_fmt.video.i_frame_rate = rate.num;
                es_fmt.video.i_frame_rate_base = rate.den;
            }

            AVRational ar;
#if (LIBAVUTIL_VERSION_MICRO < 100) /* libav */
            ar.num = s->sample_aspect_ratio.num;
            ar.den = s->sample_aspect_ratio.den;
#else
            ar = av_guess_sample_aspect_ratio( p_sys->ic, s, NULL );
#endif
            if( ar.num && ar.den )
            {
                es_fmt.video.i_sar_den = ar.den;
                es_fmt.video.i_sar_num = ar.num;
            }
            break;

        case AVMEDIA_TYPE_SUBTITLE:
            es_format_Init( &es_fmt, SPU_ES, fcc );
            es_fmt.i_original_fourcc = CodecTagToFourcc( cp->codec_tag );
            if( strncmp( p_sys->ic->iformat->name, "matroska", 8 ) == 0 &&
                cp->codec_id == AV_CODEC_ID_DVD_SUBTITLE &&
                cp->extradata != NULL &&
                cp->extradata_size > 0 )
            {
                char *psz_start;
                char *psz_buf = malloc( cp->extradata_size + 1);
                if( psz_buf != NULL )
                {
                    memcpy( psz_buf, cp->extradata , cp->extradata_size );
                    psz_buf[cp->extradata_size] = '\0';

                    psz_start = strstr( psz_buf, "size:" );
                    if( psz_start &&
                        vobsub_size_parse( psz_start,
                                           &es_fmt.subs.spu.i_original_frame_width,
                                           &es_fmt.subs.spu.i_original_frame_height ) == VLC_SUCCESS )
                    {
                        msg_Dbg( p_demux, "original frame size: %dx%d",
                                 es_fmt.subs.spu.i_original_frame_width,
                                 es_fmt.subs.spu.i_original_frame_height );
                    }
                    else
                    {
                        msg_Warn( p_demux, "reading original frame size failed" );
                    }

                    psz_start = strstr( psz_buf, "palette:" );
                    if( psz_start &&
                        vobsub_palette_parse( psz_start, &es_fmt.subs.spu.palette[1] ) == VLC_SUCCESS )
                    {
                        es_fmt.subs.spu.palette[0] = SPU_PALETTE_DEFINED;
                        msg_Dbg( p_demux, "vobsub palette read" );
                    }
                    else
                    {
                        msg_Warn( p_demux, "reading original palette failed" );
                    }
                    free( psz_buf );
                }
            }
            else if( cp->codec_id == AV_CODEC_ID_DVB_SUBTITLE &&
                     cp->extradata_size > 3 )
            {
                es_fmt.subs.dvb.i_id = GetWBE( cp->extradata ) |
                                      (GetWBE( cp->extradata + 2 ) << 16);
            }
            else if( cp->codec_id == AV_CODEC_ID_MOV_TEXT )
            {
                if( cp->extradata_size && (es_fmt.p_extra = malloc(cp->extradata_size)) )
                {
                    memcpy( es_fmt.p_extra, cp->extradata, cp->extradata_size );
                    es_fmt.i_extra = cp->extradata_size;
                }
            }
            psz_type = "subtitle";
            break;

        default:
            es_format_Init( &es_fmt, UNKNOWN_ES, 0 );
            es_fmt.i_original_fourcc = CodecTagToFourcc( cp->codec_tag );
#ifdef HAVE_AVUTIL_CODEC_ATTACHMENT
            if( cp->codec_type == AVMEDIA_TYPE_ATTACHMENT )
            {
                input_attachment_t *p_attachment;

                psz_type = "attachment";
                if( cp->codec_id == AV_CODEC_ID_TTF )
                {
                    AVDictionaryEntry *filename = av_dict_get( s->metadata, "filename", NULL, 0 );
                    if( filename && filename->value )
                    {
                        p_attachment = vlc_input_attachment_New(
                                filename->value, "application/x-truetype-font",
                                NULL, cp->extradata, (int)cp->extradata_size );
                        if( p_attachment )
                            TAB_APPEND( p_sys->i_attachments, p_sys->attachments,
                                        p_attachment );
                    }
                }
                else msg_Warn( p_demux, "unsupported attachment type (%u) in avformat demux", cp->codec_id );
            }
            else
#endif
            {
                if( cp->codec_type == AVMEDIA_TYPE_DATA )
                    psz_type = "data";

                msg_Warn( p_demux, "unsupported track type (%u:%u) in avformat demux", cp->codec_type, cp->codec_id );
            }
            break;
        }

        AVDictionaryEntry *language = av_dict_get( s->metadata, "language", NULL, 0 );
        if ( language && language->value )
            es_fmt.psz_language = strdup( language->value );

        if( s->disposition & AV_DISPOSITION_DEFAULT )
            es_fmt.i_priority = ES_PRIORITY_SELECTABLE_MIN + 1000;

#ifdef HAVE_AVUTIL_CODEC_ATTACHMENT
        if( cp->codec_type != AVMEDIA_TYPE_ATTACHMENT )
#endif
        if( cp->codec_type != AVMEDIA_TYPE_DATA )
        {
            const bool    b_ogg = !strcmp( p_sys->fmt->name, "ogg" );
            const uint8_t *p_extra = cp->extradata;
            unsigned      i_extra  = cp->extradata_size;

            if( cp->codec_id == AV_CODEC_ID_THEORA && b_ogg )
            {
                unsigned pi_size[3];
                const void *pp_data[3];
                unsigned i_count;
                for( i_count = 0; i_count < 3; i_count++ )
                {
                    if( i_extra < 2 )
                        break;
                    pi_size[i_count] = GetWBE( p_extra );
                    pp_data[i_count] = &p_extra[2];
                    if( i_extra < pi_size[i_count] + 2 )
                        break;

                    p_extra += 2 + pi_size[i_count];
                    i_extra -= 2 + pi_size[i_count];
                }
                if( i_count > 0 && xiph_PackHeaders( &es_fmt.i_extra, &es_fmt.p_extra,
                                                     pi_size, pp_data, i_count ) )
                {
                    es_fmt.i_extra = 0;
                    es_fmt.p_extra = NULL;
                }
            }
            else if( cp->codec_id == AV_CODEC_ID_SPEEX && b_ogg )
            {
                const uint8_t p_dummy_comment[] = {
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                };
                unsigned pi_size[2];
                const void *pp_data[2];

                pi_size[0] = i_extra;
                pp_data[0] = p_extra;

                pi_size[1] = sizeof(p_dummy_comment);
                pp_data[1] = p_dummy_comment;

                if( pi_size[0] > 0 && xiph_PackHeaders( &es_fmt.i_extra, &es_fmt.p_extra,
                                                        pi_size, pp_data, 2 ) )
                {
                    es_fmt.i_extra = 0;
                    es_fmt.p_extra = NULL;
                }
            }
            else if( cp->codec_id == AV_CODEC_ID_OPUS )
            {
                const uint8_t p_dummy_comment[] = {
                    'O', 'p', 'u', 's',
                    'T', 'a', 'g', 's',
                    0, 0, 0, 0, /* Vendor String length */
                                /* Vendor String */
                    0, 0, 0, 0, /* User Comment List Length */

                };
                unsigned pi_size[2];
                const void *pp_data[2];

                pi_size[0] = i_extra;
                pp_data[0] = p_extra;

                pi_size[1] = sizeof(p_dummy_comment);
                pp_data[1] = p_dummy_comment;

                if( pi_size[0] > 0 && xiph_PackHeaders( &es_fmt.i_extra, &es_fmt.p_extra,
                                                        pi_size, pp_data, 2 ) )
                {
                    es_fmt.i_extra = 0;
                    es_fmt.p_extra = NULL;
                }
            }
            else if( cp->extradata_size > 0 && !es_fmt.i_extra )
            {
                es_fmt.p_extra = malloc( i_extra );
                if( es_fmt.p_extra )
                {
                    es_fmt.i_extra = i_extra;
                    memcpy( es_fmt.p_extra, p_extra, i_extra );
                }
            }

            es_fmt.i_id = i;
            p_track->p_es = es_out_Add( p_demux->out, &es_fmt );
            if( p_track->p_es && (s->disposition & AV_DISPOSITION_DEFAULT) )
                es_out_Control( p_demux->out, ES_OUT_SET_ES_DEFAULT, p_track->p_es );

            msg_Dbg( p_demux, "adding es: %s codec = %4.4s (%d)",
                     psz_type, (char*)&fcc, cp->codec_id  );
        }
        es_format_Clean( &es_fmt );
    }

    if( p_sys->ic->start_time != (int64_t)AV_NOPTS_VALUE )
        i_start_time = FROM_AV_TS(p_sys->ic->start_time);

    msg_Dbg( p_demux, "AVFormat(%s %s) supported stream", AVPROVIDER(LIBAVFORMAT), LIBAVFORMAT_IDENT );
    msg_Dbg( p_demux, "    - format = %s (%s)",
             p_sys->fmt->name, p_sys->fmt->long_name );
    msg_Dbg( p_demux, "    - start time = %"PRId64, i_start_time );
    msg_Dbg( p_demux, "    - duration = %"PRId64,
             ( p_sys->ic->duration != (int64_t)AV_NOPTS_VALUE ) ?
             FROM_AV_TS(p_sys->ic->duration) : -1 );

    if( p_sys->ic->nb_chapters > 0 )
    {
        p_sys->p_title = vlc_input_title_New();
        p_sys->p_title->i_length = FROM_AV_TS(p_sys->ic->duration);
    }

    for( unsigned i = 0; i < p_sys->ic->nb_chapters; i++ )
    {
        seekpoint_t *s = vlc_seekpoint_New();

        AVDictionaryEntry *title = av_dict_get( p_sys->ic->metadata, "title", NULL, 0);
        if( title && title->value )
        {
            s->psz_name = strdup( title->value );
            EnsureUTF8( s->psz_name );
            msg_Dbg( p_demux, "    - chapter %d: %s", i, s->psz_name );
        }
        s->i_time_offset = vlc_tick_from_samples( p_sys->ic->chapters[i]->start *
            p_sys->ic->chapters[i]->time_base.num,
            p_sys->ic->chapters[i]->time_base.den ) -
            (i_start_time != VLC_TICK_INVALID ? i_start_time : 0 );
        TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
    }

    ResetTime( p_demux, 0 );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
void avformat_CloseDemux( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    free( p_sys->tracks );

    if( p_sys->ic )
    {
        if( p_sys->ic->pb )
        {
            av_free( p_sys->ic->pb->buffer );
            av_free( p_sys->ic->pb );
        }
        avformat_close_input( &p_sys->ic );
    }

    for( int i = 0; i < p_sys->i_attachments; i++ )
        vlc_input_attachment_Delete( p_sys->attachments[i] );
    TAB_CLEAN( p_sys->i_attachments, p_sys->attachments);

    if( p_sys->p_title )
        vlc_input_title_Delete( p_sys->p_title );

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
    vlc_tick_t  i_start_time;

    /* Read a frame */
    int i_av_ret = av_read_frame( p_sys->ic, &pkt );
    if( i_av_ret )
    {
        /* Avoid EOF if av_read_frame returns AVERROR(EAGAIN) */
        if( i_av_ret == AVERROR(EAGAIN) )
            return 1;

        return 0;
    }
    if( pkt.stream_index < 0 || (unsigned) pkt.stream_index >= p_sys->i_tracks )
    {
        av_packet_unref( &pkt );
        return 1;
    }
    struct avformat_track_s *p_track = &p_sys->tracks[pkt.stream_index];
    const AVStream *p_stream = p_sys->ic->streams[pkt.stream_index];
    if( p_stream->time_base.den <= 0 )
    {
        msg_Warn( p_demux, "Invalid time base for the stream %d", pkt.stream_index );
        av_packet_unref( &pkt );
        return 1;
    }
    if( p_stream->codecpar->codec_id == AV_CODEC_ID_SSA )
    {
        p_frame = BuildSsaFrame( &pkt, p_sys->i_ssa_order++ );
        if( !p_frame )
        {
            av_packet_unref( &pkt );
            return 1;
        }
    }
    else if( p_stream->codecpar->codec_id == AV_CODEC_ID_DVB_SUBTITLE )
    {
        if( ( p_frame = block_Alloc( pkt.size + 3 ) ) == NULL )
        {
            av_packet_unref( &pkt );
            return 0;
        }
        p_frame->p_buffer[0] = 0x20;
        p_frame->p_buffer[1] = 0x00;
        memcpy( &p_frame->p_buffer[2], pkt.data, pkt.size );
        p_frame->p_buffer[p_frame->i_buffer - 1] = 0x3f;
    }
    else
    {
        if( ( p_frame = block_Alloc( pkt.size ) ) == NULL )
        {
            av_packet_unref( &pkt );
            return 0;
        }
        memcpy( p_frame->p_buffer, pkt.data, pkt.size );
    }

    if( pkt.flags & AV_PKT_FLAG_KEY )
        p_frame->i_flags |= BLOCK_FLAG_TYPE_I;

    /* Used to avoid timestamps overlow */
    if( p_sys->ic->start_time != (int64_t)AV_NOPTS_VALUE )
    {
        i_start_time = vlc_tick_from_frac(p_sys->ic->start_time, AV_TIME_BASE);
    }
    else
        i_start_time = 0;

    if( pkt.dts == (int64_t)AV_NOPTS_VALUE )
        p_frame->i_dts = VLC_TICK_INVALID;
    else
    {
        p_frame->i_dts = vlc_tick_from_frac( pkt.dts * p_stream->time_base.num, p_stream->time_base.den )
                - i_start_time + VLC_TICK_0;
    }

    if( pkt.pts == (int64_t)AV_NOPTS_VALUE )
        p_frame->i_pts = VLC_TICK_INVALID;
    else
    {
        p_frame->i_pts = vlc_tick_from_frac( pkt.pts * p_stream->time_base.num, p_stream->time_base.den )
                - i_start_time + VLC_TICK_0;
    }
    if( pkt.duration > 0 && p_frame->i_length <= 0 )
        p_frame->i_length = vlc_tick_from_samples(pkt.duration *
            p_stream->time_base.num,
            p_stream->time_base.den );

    /* Add here notoriously bugged file formats/samples */
    if( !strcmp( p_sys->fmt->name, "flv" ) )
    {
        /* FLV and video PTS */
        if( p_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            pkt.dts != (int64_t)AV_NOPTS_VALUE && pkt.dts == pkt.pts )
                p_frame->i_pts = VLC_TICK_INVALID;

        /* Handle broken dts/pts increase with AAC. Duration is correct.
         * sky_the80s_aacplus.flv #8195 */
        if( p_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
            p_stream->codecpar->codec_id == AV_CODEC_ID_AAC )
        {
            if( p_track->i_pcr != VLC_TICK_INVALID &&
                p_track->i_pcr + p_frame->i_length > p_frame->i_dts )
            {
                p_frame->i_dts = p_frame->i_pts = p_track->i_pcr + p_frame->i_length;
            }
        }
    }
#ifdef AVFORMAT_DEBUG
    msg_Dbg( p_demux, "tk[%d] dts=%"PRId64" pts=%"PRId64,
             pkt.stream_index, p_frame->i_dts, p_frame->i_pts );
#endif
    if( p_frame->i_dts != VLC_TICK_INVALID && p_track->p_es != NULL )
        p_track->i_pcr = p_frame->i_dts;

    vlc_tick_t i_ts_max = INT64_MIN;
    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        if( p_sys->tracks[i].p_es != NULL )
            i_ts_max = __MAX( i_ts_max, p_sys->tracks[i].i_pcr );
    }

    vlc_tick_t i_ts_min = INT64_MAX;
    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        if( p_sys->tracks[i].p_es != NULL &&
                p_sys->tracks[i].i_pcr != VLC_TICK_INVALID &&
                p_sys->tracks[i].i_pcr + VLC_TICK_FROM_SEC(10)>= i_ts_max )
            i_ts_min = __MIN( i_ts_min, p_sys->tracks[i].i_pcr );
    }
    if( i_ts_min >= p_sys->i_pcr && likely(i_ts_min != INT64_MAX) )
    {
        p_sys->i_pcr = i_ts_min;
        es_out_SetPCR( p_demux->out, p_sys->i_pcr );
        UpdateSeekPoint( p_demux, p_sys->i_pcr );
    }

    if( p_track->p_es != NULL )
        es_out_Send( p_demux->out, p_track->p_es, p_frame );
    else
        block_Release( p_frame );

    av_packet_unref( &pkt );
    return 1;
}

static void UpdateSeekPoint( demux_t *p_demux, vlc_tick_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    if( !p_sys->p_title )
        return;

    for( i = 0; i < p_sys->p_title->i_seekpoint; i++ )
    {
        if( i_time < p_sys->p_title->seekpoint[i]->i_time_offset )
            break;
    }
    i--;

    if( i != p_sys->i_seekpoint && i >= 0 )
    {
        p_sys->i_seekpoint = i;
        p_sys->i_update |= INPUT_UPDATE_SEEKPOINT;
    }
}

static void ResetTime( demux_t *p_demux, int64_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_tick_t t;

    if( p_sys->ic->start_time == (int64_t)AV_NOPTS_VALUE || i_time < 0 )
    {
        t = VLC_TICK_INVALID;
    }
    else
    {
#if CLOCK_FREQ == AV_TIME_BASE
        t = FROM_AV_TS(i_time);
#else
        lldiv_t q = lldiv( i_time, AV_TIME_BASE );
        t = vlc_tick_from_sec(q.quot) + FROM_AV_TS(q.rem);
#endif

        if( t == VLC_TICK_INVALID )
            t = VLC_TICK_0;
    }

    p_sys->i_pcr = t;
    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
        p_sys->tracks[i].i_pcr = VLC_TICK_INVALID;

    if( t != VLC_TICK_INVALID )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, t );
        UpdateSeekPoint( p_demux, t );
    }
}

static block_t *BuildSsaFrame( const AVPacket *p_pkt, unsigned i_order )
{
    if( p_pkt->size <= 0 )
        return NULL;

    char buffer[256];
    const size_t i_buffer_size = __MIN( (int)sizeof(buffer) - 1, p_pkt->size );
    memcpy( buffer, p_pkt->data, i_buffer_size );
    buffer[i_buffer_size] = '\0';

    /* */
    int i_layer;
    int h0, m0, s0, c0;
    int h1, m1, s1, c1;
    int i_position = 0;
    if( sscanf( buffer, "Dialogue: %d,%d:%d:%d.%d,%d:%d:%d.%d,%n", &i_layer,
                &h0, &m0, &s0, &c0, &h1, &m1, &s1, &c1, &i_position ) < 9 )
        return NULL;
    if( i_position <= 0 || (unsigned)i_position >= i_buffer_size )
        return NULL;

    char *p;
    if( asprintf( &p, "%u,%d,%.*s", i_order, i_layer, p_pkt->size - i_position, p_pkt->data + i_position ) < 0 )
        return NULL;

    block_t *p_frame = block_heap_Alloc( p, strlen(p) + 1 );
    if( p_frame )
        p_frame->i_length = vlc_tick_from_sec((h1-h0) * 3600 +
                                         (m1-m0) * 60 +
                                         (s1-s0) * 1) +
                             VLC_TICK_FROM_MS (c1-c0) / 100;
    return p_frame;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int64_t i_start_time = p_sys->ic->start_time != AV_NOPTS_VALUE ? p_sys->ic->start_time : 0;
    double f, *pf;
    int64_t i64;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = true;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * ); *pf = 0.0;
            i64 = stream_Size( p_demux->s );
            if( i64 > 0 )
            {
                double current = vlc_stream_Tell( p_demux->s );
                *pf = current / (double)i64;
            }

            if( (p_sys->ic->duration != (int64_t)AV_NOPTS_VALUE) && (p_sys->i_pcr > 0) )
            {
                *pf = (double)p_sys->i_pcr / (double)p_sys->ic->duration;
            }

            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
        {
            f = va_arg( args, double );
            bool precise = va_arg( args, int );
            i64 = p_sys->ic->duration * f + i_start_time;

            msg_Warn( p_demux, "DEMUX_SET_POSITION: %"PRId64, i64 );

            /* If we have a duration, we prefer to seek by time
               but if we don't, or if the seek fails, try BYTE seeking */
            if( p_sys->ic->duration == (int64_t)AV_NOPTS_VALUE ||
                (av_seek_frame( p_sys->ic, -1, i64, AVSEEK_FLAG_BACKWARD ) < 0) )
            {
                int64_t i_size = stream_Size( p_demux->s );
                i64 = (i_size * f);

                msg_Warn( p_demux, "DEMUX_SET_BYTE_POSITION: %"PRId64, i64 );
                if( av_seek_frame( p_sys->ic, -1, i64, AVSEEK_FLAG_BYTE ) < 0 )
                    return VLC_EGENERIC;

                ResetTime( p_demux, -1 );
            }
            else
            {
                if( precise )
                    ResetTime( p_demux, i64 - i_start_time );
                else
                    ResetTime( p_demux, -1 );
            }
            return VLC_SUCCESS;
        }

        case DEMUX_GET_LENGTH:
            if( p_sys->ic->duration != (int64_t)AV_NOPTS_VALUE )
                *va_arg( args, vlc_tick_t * ) = FROM_AV_TS(p_sys->ic->duration);
            else
                *va_arg( args, vlc_tick_t * ) = 0;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_pcr;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
        {
            i64 = TO_AV_TS(va_arg( args, vlc_tick_t ));
            bool precise = va_arg( args, int );

            msg_Warn( p_demux, "DEMUX_SET_TIME: %"PRId64, i64 );

            if( av_seek_frame( p_sys->ic, -1, i64 + i_start_time, AVSEEK_FLAG_BACKWARD ) < 0 )
            {
                return VLC_EGENERIC;
            }
            if( precise )
                ResetTime( p_demux, i64 - i_start_time );
            else
                ResetTime( p_demux, -1 );
            return VLC_SUCCESS;
        }

        case DEMUX_HAS_UNSUPPORTED_META:
        {
            bool *pb_bool = va_arg( args, bool* );
            *pb_bool = true;
            return VLC_SUCCESS;
        }


        case DEMUX_GET_META:
        {
            static const char names[][10] = {
                [vlc_meta_Title] = "title",
                [vlc_meta_Artist] = "artist",
                [vlc_meta_Genre] = "genre",
                [vlc_meta_Copyright] = "copyright",
                [vlc_meta_Album] = "album",
                //[vlc_meta_TrackNumber] -- TODO: parse number/total value
                [vlc_meta_Description] = "comment",
                //[vlc_meta_Rating]
                [vlc_meta_Date] = "date",
                [vlc_meta_Setting] = "encoder",
                //[vlc_meta_URL]
                [vlc_meta_Language] = "language",
                //[vlc_meta_NowPlaying]
                [vlc_meta_Publisher] = "publisher",
                [vlc_meta_EncodedBy] = "encoded_by",
                //[vlc_meta_ArtworkURL]
                //[vlc_meta_TrackID]
                //[vlc_meta_TrackTotal]
            };
            vlc_meta_t *p_meta = va_arg( args, vlc_meta_t * );
            AVDictionary *dict = p_sys->ic->metadata;

            for( unsigned i = 0; i < sizeof(names) / sizeof(*names); i++)
            {
                if( !names[i][0] )
                    continue;

                AVDictionaryEntry *e = av_dict_get( dict, names[i], NULL, 0 );
                if( e != NULL && e->value != NULL && IsUTF8(e->value) )
                    vlc_meta_Set( p_meta, i, e->value );
            }
            return VLC_SUCCESS;
        }

        case DEMUX_GET_ATTACHMENTS:
        {
            input_attachment_t ***ppp_attach =
                va_arg( args, input_attachment_t*** );
            int *pi_int = va_arg( args, int * );
            int i;

            if( p_sys->i_attachments <= 0 )
                return VLC_EGENERIC;

            *ppp_attach = vlc_alloc( p_sys->i_attachments, sizeof(input_attachment_t*) );
            if( *ppp_attach == NULL )
                return VLC_EGENERIC;

            for( i = 0; i < p_sys->i_attachments; i++ )
            {
                (*ppp_attach)[i] = vlc_input_attachment_Duplicate( p_sys->attachments[i] );
                if((*ppp_attach)[i] == NULL)
                    break;
            }
            *pi_int = i;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***ppp_title = va_arg( args, input_title_t *** );
            int *pi_int = va_arg( args, int * );
            int *pi_title_offset = va_arg( args, int * );
            int *pi_seekpoint_offset = va_arg( args, int * );

            if( !p_sys->p_title )
                return VLC_EGENERIC;

            *ppp_title = malloc( sizeof( input_title_t*) );
            if( *ppp_title == NULL )
                return VLC_EGENERIC;
            (*ppp_title)[0] = vlc_input_title_Duplicate( p_sys->p_title );
            *pi_int = (*ppp_title)[0] ? 1 : 0;
            *pi_title_offset = 0;
            *pi_seekpoint_offset = 0;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_TITLE:
        {
            const int i_title = va_arg( args, int );
            if( !p_sys->p_title || i_title != 0 )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_SEEKPOINT:
        {
            const int i_seekpoint = va_arg( args, int );
            if( !p_sys->p_title )
                return VLC_EGENERIC;

            i64 = TO_AV_TS(p_sys->p_title->seekpoint[i_seekpoint]->i_time_offset) +
                  i_start_time;

            msg_Warn( p_demux, "DEMUX_SET_SEEKPOINT: %"PRId64, i64 );

            if( av_seek_frame( p_sys->ic, -1, i64, AVSEEK_FLAG_BACKWARD ) < 0 )
            {
                return VLC_EGENERIC;
            }
            ResetTime( p_demux, i64 - i_start_time );
            return VLC_SUCCESS;
        }
        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict flags = va_arg(args, unsigned *);
            *flags &= p_sys->i_update;
            p_sys->i_update &= ~*flags;
            return VLC_SUCCESS;
        }
        case DEMUX_GET_TITLE:
            if( p_sys->p_title == NULL )
                return VLC_EGENERIC;
            *va_arg( args, int * ) = 0;
            return VLC_SUCCESS;
        case DEMUX_GET_SEEKPOINT:
            if( p_sys->p_title == NULL )
                return VLC_EGENERIC;
            *va_arg( args, int * ) = p_sys->i_seekpoint;
            return VLC_SUCCESS;
        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * I/O wrappers for libavformat
 *****************************************************************************/
static int IORead( void *opaque, uint8_t *buf, int buf_size )
{
    demux_t *p_demux = opaque;
    if( buf_size < 0 ) return -1;
    int i_ret = vlc_stream_Read( p_demux->s, buf, buf_size );
    return i_ret >= 0 ? i_ret : -1;
}

static int64_t IOSeek( void *opaque, int64_t offset, int whence )
{
    demux_t *p_demux = opaque;
    int64_t i_absolute;
    int64_t i_size = stream_Size( p_demux->s );

#ifdef AVFORMAT_DEBUG
    msg_Warn( p_demux, "IOSeek offset: %"PRId64", whence: %i", offset, whence );
#endif

    switch( whence )
    {
#ifdef AVSEEK_SIZE
        case AVSEEK_SIZE:
            return i_size;
#endif
        case SEEK_SET:
            i_absolute = (int64_t)offset;
            break;
        case SEEK_CUR:
            i_absolute = vlc_stream_Tell( p_demux->s ) + (int64_t)offset;
            break;
        case SEEK_END:
            i_absolute = i_size + (int64_t)offset;
            break;
        default:
            return -1;

    }

    if( i_absolute < 0 )
    {
        msg_Dbg( p_demux, "Trying to seek before the beginning" );
        return -1;
    }

    if( i_size > 0 && i_absolute >= i_size )
    {
        msg_Dbg( p_demux, "Trying to seek too far : EOF?" );
        return -1;
    }

    if( vlc_stream_Seek( p_demux->s, i_absolute ) )
    {
        msg_Warn( p_demux, "we were not allowed to seek, or EOF " );
        return -1;
    }

    return vlc_stream_Tell( p_demux->s );
}
