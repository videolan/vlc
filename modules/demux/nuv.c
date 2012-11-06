/*****************************************************************************
 * nuv.c:
 *****************************************************************************
 * Copyright (C) 2005 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gertjan Van Droogenbroeck <gertjanvd _PLUS_ vlc _AT_ gmail _DOT_ com>
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

/* TODO:
 *  - test
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("Nuv demuxer") )
    set_capability( "demux", 145 )
    set_callbacks( Open, Close )
    add_shortcut( "nuv" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

/* */
typedef struct
{
    int64_t i_time;
    int64_t i_offset;

} demux_index_entry_t;

typedef struct
{
    int i_idx;
    int i_idx_max;

    demux_index_entry_t *idx;
} demux_index_t;


static void demux_IndexInit( demux_index_t * );
static void demux_IndexClean( demux_index_t * );
static void demux_IndexAppend( demux_index_t *,
                               int64_t i_time, int64_t i_offset );
/* Convert a time into offset */
static int64_t demux_IndexConvertTime( demux_index_t *, int64_t i_time );
/* Find the nearest offset in the index */
static int64_t demux_IndexFindOffset( demux_index_t *, int64_t i_offset );


/* */
typedef struct
{
    char id[12];       /* "NuppelVideo\0" or "MythTVVideo\0" */
    char version[5];    /* "x.xx\0" */

    int  i_width;
    int  i_height;
    int  i_width_desired;
    int  i_height_desired;

    char i_mode;            /* P progressive, I interlaced */

    double  d_aspect;       /* 1.0 squared pixel */
    double  d_fps;

    int     i_video_blocks; /* 0 no video, -1 unknown */
    int     i_audio_blocks;
    int     i_text_blocks;

    int     i_keyframe_distance;

} header_t;

#define NUV_FH_SIZE 12
typedef struct
{
    char i_type;        /* A: audio, V: video, S: sync; T: test
                           R: Seekpoint (string:RTjjjjjjjj)
                           D: Extra data for codec
                           X: extended data Q: seektable */
    char i_compression; /* V: 0 uncompressed
                              1 RTJpeg
                              2 RTJpeg+lzo
                              N black frame
                              L copy last
                           A: 0 uncompressed (44100 1-bits, 2ch)
                              1 lzo
                              2 layer 2
                              3 layer 3
                              F flac
                              S shorten
                              N null frame loudless
                              L copy last
                            S: B audio and vdeo sync point
                               A audio sync info (timecode == effective
                                    dsp frequency*100)
                               V next video sync (timecode == next video
                                    frame num)
                               S audio,video,text correlation */
    char i_keyframe;    /* 0 keyframe, else no no key frame */
    uint8_t i_filters;  /* 0x01: gauss 5 pixel (8,2,2,2,2)/16
                           0x02: gauss 5 pixel (8,1,1,1,1)/12
                           0x04: cartoon filter */

    int i_timecode;     /* ms */

    int i_length;       /* V,A,T: length of following data
                           S: length of packet correl */
} frame_header_t;

typedef struct
{
    int             i_version;
    vlc_fourcc_t    i_video_fcc;

    vlc_fourcc_t    i_audio_fcc;
    int             i_audio_sample_rate;
    int             i_audio_bits_per_sample;
    int             i_audio_channels;
    int             i_audio_compression_ratio;
    int             i_audio_quality;
    int             i_rtjpeg_quality;
    int             i_rtjpeg_luma_filter;
    int             i_rtjpeg_chroma_filter;
    int             i_lavc_bitrate;
    int             i_lavc_qmin;
    int             i_lavc_qmax;
    int             i_lavc_maxqdiff;
    int64_t         i_seektable_offset;
    int64_t         i_keyframe_adjust_offset;

} extended_header_t;

struct demux_sys_t
{
    header_t          hdr;
    extended_header_t exh;

    int64_t     i_pcr;
    es_out_id_t *p_es_video;
    int         i_extra_f;
    uint8_t     *p_extra_f;

    es_out_id_t *p_es_audio;

    /* index */
    demux_index_t idx;
    bool b_index;
    bool b_seekable;
    /* frameheader buffer */
    uint8_t fh_buffer[NUV_FH_SIZE];
    int64_t i_total_frames;
    int64_t i_total_length;
    /* first frame position (used for calculating size without seektable) */
    int i_first_frame_offset;
};

static int HeaderLoad( demux_t *, header_t *h );
static int FrameHeaderLoad( demux_t *, frame_header_t *h );
static int ExtendedHeaderLoad( demux_t *, extended_header_t *h );
static int SeekTableLoad( demux_t *, demux_sys_t * );
static int ControlSetPosition( demux_t *p_demux, int64_t i_pos, bool b_guess );

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    const uint8_t *p_peek;
    frame_header_t fh;

    /* Check id */
    if( stream_Peek( p_demux->s, &p_peek, 12 ) != 12 ||
        ( strncmp( (char *)p_peek, "MythTVVideo", 11 ) &&
          strncmp( (char *)p_peek, "NuppelVideo", 11 ) ) )
        return VLC_EGENERIC;

    p_sys = malloc( sizeof( demux_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    p_sys->p_es_video = NULL;
    p_sys->p_es_audio = NULL;
    p_sys->p_extra_f = NULL;
    p_sys->i_pcr = -1;
    p_sys->b_index = false;
    p_sys->i_total_frames = -1;
    p_sys->i_total_length = -1;
    demux_IndexInit( &p_sys->idx );

    p_demux->p_sys = p_sys;

    /* Info about the stream */
    stream_Control( p_demux->s, STREAM_CAN_SEEK, &p_sys->b_seekable );
#if 0
    if( p_sys->b_seekable )
        msg_Dbg( p_demux, "stream is seekable" );
    else
        msg_Dbg( p_demux, "stream is NOT seekable" );
#endif

    if( HeaderLoad( p_demux, &p_sys->hdr ) )
        goto error;

    /* Load 'D' */
    if( FrameHeaderLoad( p_demux, &fh ) || fh.i_type != 'D' )
        goto error;
    if( fh.i_length > 0 )
    {
        if( fh.i_compression == 'F' || fh.i_compression == 'R' )
        {
            /* ffmpeg extra data */
            p_sys->i_extra_f = fh.i_length;
            p_sys->p_extra_f = malloc( fh.i_length );
            if( p_sys->p_extra_f == NULL || stream_Read( p_demux->s,
                             p_sys->p_extra_f, fh.i_length ) != fh.i_length )
                goto error;
        }
        else
        {
            msg_Warn( p_demux, "unsupported 'D' frame (c=%c)", fh.i_compression );
            if( stream_Read( p_demux->s, NULL, fh.i_length ) != fh.i_length )
                goto error;
        }
    }

    /* Check and load extented */
    if( stream_Peek( p_demux->s, &p_peek, 1 ) != 1 )
        goto error;
    if( p_peek[0] == 'X' )
    {
        if( FrameHeaderLoad( p_demux, &fh ) )
            goto error;
        if( fh.i_length != 512 )
            goto error;

        if( ExtendedHeaderLoad( p_demux, &p_sys->exh ) )
            goto error;

        if( !p_sys->b_seekable )
            msg_Warn( p_demux, "stream is not seekable, skipping seektable" );
        else if( SeekTableLoad( p_demux, p_sys ) )
        {
            p_sys->b_index = false;
            msg_Warn( p_demux, "Seektable is broken, seek won't be accurate" );
        }
    }
    else
    {
        /* XXX: for now only file with extended chunk are supported
         * why: because else we need to have support for rtjpeg+stupid nuv shit */
        msg_Err( p_demux, "VLC doesn't support NUV without extended chunks (please upload samples)" );
        goto error;
    }

    /* Create audio/video (will work only with extended header and audio=mp3 */
    if( p_sys->hdr.i_video_blocks != 0 )
    {
        es_format_t fmt;

        es_format_Init( &fmt, VIDEO_ES, p_sys->exh.i_video_fcc );
        fmt.video.i_width = p_sys->hdr.i_width;
        fmt.video.i_height = p_sys->hdr.i_height;
        fmt.i_extra = p_sys->i_extra_f;
        fmt.p_extra = p_sys->p_extra_f;
        fmt.video.i_sar_num = p_sys->hdr.d_aspect * fmt.video.i_height;
        fmt.video.i_sar_den = fmt.video.i_width;

        p_sys->p_es_video = es_out_Add( p_demux->out, &fmt );
    }
    if( p_sys->hdr.i_audio_blocks != 0 )
    {
        es_format_t fmt;

        es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_MPGA );
        fmt.audio.i_rate = p_sys->exh.i_audio_sample_rate;
        fmt.audio.i_bitspersample = p_sys->exh.i_audio_bits_per_sample;

        p_sys->p_es_audio = es_out_Add( p_demux->out, &fmt );
    }
    if( p_sys->hdr.i_text_blocks != 0 )
    {
        msg_Warn( p_demux, "text not yet supported (upload samples)" );
    }

    p_sys->i_first_frame_offset = stream_Tell( p_demux->s );

    /* Fill p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    return VLC_SUCCESS;

error:
    msg_Warn( p_demux, "cannot load Nuv file" );
    p_demux->p_sys = NULL;
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t        *p_demux = (demux_t*)p_this;
    demux_sys_t    *p_sys = p_demux->p_sys;

    free( p_sys->p_extra_f );
    demux_IndexClean( &p_sys->idx );
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    frame_header_t fh;
    block_t *p_data;

    for( ;; )
    {
        if( !vlc_object_alive (p_demux) )
            return -1;

        if( FrameHeaderLoad( p_demux, &fh ) )
            return 0;

        if( fh.i_type == 'A' || fh.i_type == 'V' )
            break;

        /* TODO add support for some block type */

        if( fh.i_type != 'R' && fh.i_length > 0 )
        {
            if( stream_Read( p_demux->s, NULL, fh.i_length ) != fh.i_length )
                return -1;
        }
    }

    /* */
    if( ( p_data = stream_Block( p_demux->s, fh.i_length ) ) == NULL )
        return 0;

    p_data->i_dts = VLC_TS_0 + (int64_t)fh.i_timecode * 1000;
    p_data->i_pts = (fh.i_type == 'V') ? VLC_TS_INVALID : p_data->i_dts;

    /* only add keyframes to index */
    if( !fh.i_keyframe && !p_sys->b_index )
        demux_IndexAppend( &p_sys->idx,
                           p_data->i_dts - VLC_TS_0,
                           stream_Tell(p_demux->s) - NUV_FH_SIZE );

    /* */
    if( p_sys->i_pcr < 0 || p_sys->i_pcr < p_data->i_dts - VLC_TS_0 )
    {
        p_sys->i_pcr = p_data->i_dts - VLC_TS_0;
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_pcr );
    }

    if( fh.i_type == 'A' && p_sys->p_es_audio )
    {
        if( fh.i_compression == '3' )
            es_out_Send( p_demux->out, p_sys->p_es_audio, p_data );
        else
        {
            msg_Dbg( p_demux, "unsupported compression %c for audio (upload samples)", fh.i_compression );
            block_Release( p_data );
        }
    }
    else if( fh.i_type == 'V' && p_sys->p_es_video )
    {
        if( fh.i_compression >='0' && fh.i_compression <='3' )
        {
            /* for rtjpeg data, the header is also needed */
            p_data = block_Realloc( p_data, NUV_FH_SIZE, fh.i_length );
            if( unlikely(!p_data) )
                abort();
            memcpy( p_data->p_buffer, p_sys->fh_buffer, NUV_FH_SIZE );
        }
        /* 0,1,2,3 -> rtjpeg, >=4 mpeg4 */
        if( fh.i_compression >= '0' )
            es_out_Send( p_demux->out, p_sys->p_es_video, p_data );
        else
        {
            msg_Dbg( p_demux, "unsupported compression %c for video (upload samples)", fh.i_compression );
            block_Release( p_data );
        }
    }
    else
    {
        block_Release( p_data );
    }

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    double   f, *pf;
    int64_t i64, *pi64;

    switch( i_query )
    {

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );

            if( p_sys->i_total_length > 0 && p_sys->i_pcr >= 0 )
            {
                *pf = (double)p_sys->i_pcr / (double)p_sys->i_total_length;
            }
            else
            {
                i64 = stream_Size( p_demux->s );
                if( i64 > 0 )
                {
                    const double f_current = stream_Tell( p_demux->s );
                    *pf = f_current / (double)i64;
                }
                else
                {
                    *pf = 0.0;
                }
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
        {
            int64_t i_pos;

            f = (double)va_arg( args, double );

            p_sys->i_pcr = -1;

            /* first try to see if we can seek based on time (== GET_LENGTH works) */
            if( p_sys->i_total_length > 0 && ( i_pos = demux_IndexConvertTime( &p_sys->idx, p_sys->i_total_length * f ) ) > 0 )
                return ControlSetPosition( p_demux, i_pos, false );

            /* if not search based on total stream size */
            else if( ( i_pos = demux_IndexFindOffset( &p_sys->idx, stream_Size( p_demux->s ) * f ) ) >= 0 )
                return ControlSetPosition( p_demux, i_pos, false );

            else if( ( i_pos =  p_sys->i_first_frame_offset + ( stream_Size( p_demux->s ) - p_sys->i_first_frame_offset ) * f ) >= 0 )
                return ControlSetPosition( p_demux, i_pos, true );

            else
                return VLC_EGENERIC;
        }

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_pcr >= 0 ? p_sys->i_pcr : 0;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
        {
            int64_t i_pos;
            i64 = (int64_t)va_arg( args, int64_t );

            p_sys->i_pcr = -1;

            i_pos = demux_IndexConvertTime( &p_sys->idx, i64 );
            if( i_pos < 0 )
                return VLC_EGENERIC;
            else
                return ControlSetPosition( p_demux, i_pos, false );
        }

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_total_length >= 0 )
            {
                *pi64 = p_sys->i_total_length;
                return VLC_SUCCESS;
            }
            else if( stream_Tell( p_demux->s ) > p_sys->i_first_frame_offset )
            {
                /* This should give an approximation of the total duration */
                *pi64 = (double)( stream_Size( p_demux->s ) - p_sys->i_first_frame_offset ) /
                        (double)( stream_Tell( p_demux->s ) - p_sys->i_first_frame_offset )
                        * (double)( p_sys->i_pcr >= 0 ? p_sys->i_pcr : 0 );
                return VLC_SUCCESS;
            }
            else
                return VLC_EGENERIC;

        case DEMUX_GET_FPS:
            pf = (double*)va_arg( args, double * );
            *pf = p_sys->hdr.d_fps;
            return VLC_SUCCESS;

        case DEMUX_GET_META:
        default:
            return VLC_EGENERIC;

    }
}
static int ControlSetPosition( demux_t *p_demux, int64_t i_pos, bool b_guess )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    if( i_pos < 0 )
        return VLC_EGENERIC;

    /* if we can seek in the stream */
    if( p_sys->b_seekable && !b_guess )
    {
        if( stream_Seek( p_demux->s, i_pos ) )
            return VLC_EGENERIC;
    }
    else
    {
        /* forward seek */
        if( i_pos > stream_Tell( p_demux->s ) )
        {
            msg_Dbg( p_demux, "unable to seek, skipping frames (slow)" );
        }
        else
        {
            msg_Warn( p_demux, "unable to seek, only forward seeking is possible" );

            return VLC_EGENERIC;
        }
    }

    while( vlc_object_alive (p_demux) )
    {
        frame_header_t fh;
        int64_t i_tell;

        if( ( i_tell = stream_Tell( p_demux->s ) ) >= i_pos )
            break;

        if( FrameHeaderLoad( p_demux, &fh ) )
            return VLC_EGENERIC;

        if( fh.i_type == 'A' || fh.i_type == 'V' )
        {
            if( !fh.i_keyframe && !p_sys->b_index )
                demux_IndexAppend( &p_sys->idx,(int64_t)fh.i_timecode*1000, i_tell );
        }

        if( fh.i_type != 'R' && fh.i_length > 0 )
        {
            if( stream_Read( p_demux->s, NULL, fh.i_length ) != fh.i_length )
                return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline void GetDoubleLE( double *pd, void *src )
{
    /* FIXME works only if sizeof(double) == 8 */
#ifdef WORDS_BIGENDIAN
    uint8_t *p = (uint8_t*)pd, *q = (uint8_t*)src;
    int i;
    for( i = 0; i < 8; i++ )
        p[i] = q[7-i];
#else
    memcpy( pd, src, 8 );
#endif
}

/* HeaderLoad:
 */
static int HeaderLoad( demux_t *p_demux, header_t *h )
{
    uint8_t buffer[72];

    if( stream_Read( p_demux->s, buffer, 72 ) != 72 )
        return VLC_EGENERIC;

    /* XXX: they are alignment to take care of (another broken format) */
    memcpy( h->id,      &buffer[ 0], 12 );
    memcpy( h->version, &buffer[12], 5 );
    h->i_width = GetDWLE( &buffer[20] );
    h->i_height = GetDWLE( &buffer[24] );
    h->i_width_desired = GetDWLE( &buffer[28] );
    h->i_height_desired = GetDWLE( &buffer[32] );
    h->i_mode = buffer[36];
    GetDoubleLE( &h->d_aspect, &buffer[40] );
    GetDoubleLE( &h->d_fps, &buffer[48] );
    h->i_video_blocks = GetDWLE( &buffer[56] );
    h->i_audio_blocks = GetDWLE( &buffer[60] );
    h->i_text_blocks = GetDWLE( &buffer[64] );
    h->i_keyframe_distance = GetDWLE( &buffer[68] );
#if 0
    msg_Dbg( p_demux, "nuv: h=%s v=%s %dx%d a=%f fps=%f v=%d a=%d t=%d kfd=%d",
             h->id, h->version, h->i_width, h->i_height, h->d_aspect,
             h->d_fps, h->i_video_blocks, h->i_audio_blocks, h->i_text_blocks,
             h->i_keyframe_distance );
#endif
    return VLC_SUCCESS;
}

/* FrameHeaderLoad:
 */
static int FrameHeaderLoad( demux_t *p_demux, frame_header_t *h )
{
    uint8_t* buffer = p_demux->p_sys->fh_buffer;

    if( stream_Read( p_demux->s, buffer, 12 ) != 12 )
        return VLC_EGENERIC;

    h->i_type = buffer[0];
    h->i_compression = buffer[1];
    h->i_keyframe = buffer[2];
    h->i_filters = buffer[3];

    h->i_timecode = GetDWLE( &buffer[4] );
    h->i_length = GetDWLE( &buffer[8] );
#if 0
    msg_Dbg( p_demux, "frame hdr: t=%c c=%c k=%d f=0x%x timecode=%d l=%d",
             h->i_type,
             h->i_compression ? h->i_compression : ' ',
             h->i_keyframe ? h->i_keyframe : ' ',
             h->i_filters,
             h->i_timecode, h->i_length );
#endif
    return VLC_SUCCESS;
}

static int ExtendedHeaderLoad( demux_t *p_demux, extended_header_t *h )
{
    uint8_t buffer[512];

    if( stream_Read( p_demux->s, buffer, 512 ) != 512 )
        return VLC_EGENERIC;

    h->i_version = GetDWLE( &buffer[0] );
    h->i_video_fcc = VLC_FOURCC( buffer[4], buffer[5], buffer[6], buffer[7] );
    h->i_audio_fcc = VLC_FOURCC( buffer[8], buffer[9], buffer[10], buffer[11] );
    h->i_audio_sample_rate = GetDWLE( &buffer[12] );
    h->i_audio_bits_per_sample = GetDWLE( &buffer[16] );
    h->i_audio_channels = GetDWLE( &buffer[20] );
    h->i_audio_compression_ratio = GetDWLE( &buffer[24] );
    h->i_audio_quality = GetDWLE( &buffer[28] );
    h->i_rtjpeg_quality = GetDWLE( &buffer[32] );
    h->i_rtjpeg_luma_filter = GetDWLE( &buffer[36] );
    h->i_rtjpeg_chroma_filter = GetDWLE( &buffer[40] );
    h->i_lavc_bitrate = GetDWLE( &buffer[44] );
    h->i_lavc_qmin = GetDWLE( &buffer[48] );
    h->i_lavc_qmin = GetDWLE( &buffer[52] );
    h->i_lavc_maxqdiff = GetDWLE( &buffer[56] );
    h->i_seektable_offset = GetQWLE( &buffer[60] );
    h->i_keyframe_adjust_offset= GetQWLE( &buffer[68] );
#if 0
    msg_Dbg( p_demux, "ex hdr: v=%d vffc=%4.4s afcc=%4.4s %dHz %dbits ach=%d acr=%d aq=%d"
                      "rtjpeg q=%d lf=%d lc=%d lavc br=%d qmin=%d qmax=%d maxqdiff=%d seekableoff=%"PRIi64" keyfao=%"PRIi64,
             h->i_version,
             (char*)&h->i_video_fcc,
             (char*)&h->i_audio_fcc, h->i_audio_sample_rate, h->i_audio_bits_per_sample, h->i_audio_channels,
             h->i_audio_compression_ratio, h->i_audio_quality,
             h->i_rtjpeg_quality, h->i_rtjpeg_luma_filter, h->i_rtjpeg_chroma_filter,
             h->i_lavc_bitrate, h->i_lavc_qmin, h->i_lavc_qmax, h->i_lavc_maxqdiff,
             h->i_seektable_offset, h->i_keyframe_adjust_offset );
#endif
    return VLC_SUCCESS;
}

/*
    typedef struct
    {
      int64_t i_file_offset;
      int32_t i_keyframe_number;
    } seektable_entry_t;
    typedef struct
    {
       int32_t i_adjust;
       int32_t i_keyframe_number;
    } kfatable_entry_t;
*/

static int SeekTableLoad( demux_t *p_demux, demux_sys_t *p_sys )
{
    frame_header_t fh;
    int64_t i_original_pos;
    int64_t i_time, i_offset;
    int keyframe, last_keyframe = 0, frame = 0, kfa_entry_id = 0;

    if( p_sys->exh.i_seektable_offset <= 0 )
        return VLC_SUCCESS;

    /* Save current position */
    i_original_pos = stream_Tell( p_demux->s );
#if 0
    msg_Dbg( p_demux, "current offset %"PRIi64, i_original_pos );

    msg_Dbg( p_demux, "seeking in stream to %"PRIi64, p_sys->exh.i_seektable_offset );
#endif
    if( stream_Seek( p_demux->s, p_sys->exh.i_seektable_offset ) )
        return VLC_EGENERIC;

    if( FrameHeaderLoad( p_demux, &fh ) )
        return VLC_EGENERIC;

    if( fh.i_type != 'Q' )
    {
        msg_Warn( p_demux, "invalid seektable, frame type=%c", fh.i_type );
        stream_Seek( p_demux->s, i_original_pos );
        return VLC_EGENERIC;
    }

    /* */
    uint8_t *p_seek_table = malloc( fh.i_length );
    if( p_seek_table == NULL )
        return VLC_ENOMEM;

    if( stream_Read( p_demux->s, p_seek_table, fh.i_length ) != fh.i_length )
    {
        free( p_seek_table );
        return VLC_EGENERIC;
    }
    const int32_t i_seek_elements = fh.i_length / 12;

    /* Get keyframe adjust offsets */
    int32_t i_kfa_elements = 0;
    uint8_t *p_kfa_table = NULL;

    if( p_sys->exh.i_keyframe_adjust_offset > 0 )
    {
        msg_Dbg( p_demux, "seeking in stream to %"PRIi64, p_sys->exh.i_keyframe_adjust_offset );
        if( stream_Seek( p_demux->s, p_sys->exh.i_keyframe_adjust_offset ) )
        {
            free( p_seek_table );
            return VLC_EGENERIC;
        }

        if( FrameHeaderLoad( p_demux, &fh ) )
        {
            free( p_seek_table );
            return VLC_EGENERIC;
        }

        if( fh.i_type == 'K' && fh.i_length >= 8 )
        {
            p_kfa_table = malloc( fh.i_length );

            if( p_kfa_table == NULL )
            {
                free( p_seek_table );
                return VLC_ENOMEM;
            }

            if( stream_Read( p_demux->s, p_kfa_table, fh.i_length ) != fh.i_length )
            {
                free( p_seek_table );
                free( p_kfa_table );
                return VLC_EGENERIC;
            }

            i_kfa_elements = fh.i_length / 8;
        }
    }

    if( i_kfa_elements > 0 )
        msg_Warn( p_demux, "untested keyframe adjust support, upload samples" );

    for( int32_t j = 0; j < i_seek_elements; j++)
    {
#if 0
        uint8_t* p = p_seek_table + j * 12;
        msg_Dbg( p_demux, "%x %x %x %x %x %x %x %x %x %x %x %x",
        p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11]);
#endif
        keyframe = GetDWLE( p_seek_table + j * 12 + 8 );

        frame += (keyframe - last_keyframe) * p_sys->hdr.i_keyframe_distance;

        if( kfa_entry_id < i_kfa_elements && *(int32_t*)(p_kfa_table + kfa_entry_id * 12 + 4) == j )
        {
            frame -= *(int32_t*)(p_kfa_table + kfa_entry_id * 12);
            msg_Dbg( p_demux, "corrected keyframe %d with current frame number %d (corrected with %d)",
                        keyframe, frame, *(int32_t*)(p_kfa_table + kfa_entry_id * 12) );
            kfa_entry_id++;
        }

        i_time = (double)( (int64_t)frame * 1000000 ) / p_sys->hdr.d_fps;
        i_offset = GetQWLE( p_seek_table + j * 12 );

        if( i_offset == 0 && i_time != 0 )
            msg_Dbg( p_demux, "invalid file offset %d %"PRIi64, keyframe, i_offset );
        else
        {
            demux_IndexAppend( &p_sys->idx, i_time , i_offset );
#if 0
            msg_Dbg( p_demux, "adding entry position %d %"PRIi64 " file offset %"PRIi64, keyframe, i_time, i_offset );
#endif
        }

        last_keyframe = keyframe;
    }

    p_sys->i_total_frames = (int64_t)frame;

    p_sys->b_index = true;

    p_sys->i_total_length = p_sys->i_total_frames * 1000000 / p_sys->hdr.d_fps;

    msg_Dbg( p_demux, "index table loaded (%d elements)", i_seek_elements );

    if( i_kfa_elements )
        free ( p_kfa_table );

    free ( p_seek_table );

    /* Restore stream position */
    if( stream_Seek( p_demux->s, i_original_pos ) )
        return VLC_EGENERIC;

    return VLC_SUCCESS;

}

/*****************************************************************************/
#define DEMUX_INDEX_SIZE_MAX (100000)
static void demux_IndexInit( demux_index_t *p_idx )
{
    p_idx->i_idx = 0;
    p_idx->i_idx_max = 0;
    p_idx->idx = NULL;
}
static void demux_IndexClean( demux_index_t *p_idx )
{
    free( p_idx->idx );
    p_idx->idx = NULL;
}
static void demux_IndexAppend( demux_index_t *p_idx,
                               int64_t i_time, int64_t i_offset )
{
    /* Be sure to append new entry (we don't insert point) */
    if( p_idx->i_idx > 0 && p_idx->idx[p_idx->i_idx-1].i_time >= i_time )
        return;

    /* */
    if( p_idx->i_idx >= p_idx->i_idx_max )
    {
        if( p_idx->i_idx >= DEMUX_INDEX_SIZE_MAX )
        {
            /* Avoid too big index */
            const int64_t i_length = p_idx->idx[p_idx->i_idx-1].i_time -
                                                        p_idx->idx[0].i_time;
            const int i_count = DEMUX_INDEX_SIZE_MAX/2;
            int i, j;

            /* We try to reduce the resolution of the index by a factor 2 */
            for( i = 1, j = 1; i < p_idx->i_idx; i++ )
            {
                if( p_idx->idx[i].i_time < j * i_length / i_count )
                    continue;

                p_idx->idx[j++] = p_idx->idx[i];
            }
            p_idx->i_idx = j;

            if( p_idx->i_idx > 3 * DEMUX_INDEX_SIZE_MAX / 4 )
            {
                /* We haven't created enough space
                 * (This method won't create a good index but work for sure) */
                for( i = 0; i < p_idx->i_idx/2; i++ )
                    p_idx->idx[i] = p_idx->idx[2*i];
                p_idx->i_idx /= 2;
            }
        }
        else
        {
            p_idx->i_idx_max += 1000;
            p_idx->idx = xrealloc( p_idx->idx,
                                p_idx->i_idx_max*sizeof(demux_index_entry_t));
        }
    }

    /* */
    p_idx->idx[p_idx->i_idx].i_time = i_time;
    p_idx->idx[p_idx->i_idx].i_offset = i_offset;

    p_idx->i_idx++;
}
static int64_t demux_IndexConvertTime( demux_index_t *p_idx, int64_t i_time )
{
    int i_min = 0;
    int i_max = p_idx->i_idx-1;

    /* Empty index */
    if( p_idx->i_idx <= 0 )
        return -1;

    /* Special border case */
    if( i_time <= p_idx->idx[0].i_time )
        return p_idx->idx[0].i_offset;
    if( i_time >= p_idx->idx[i_max].i_time )
        return p_idx->idx[i_max].i_offset;

    /* Dicho */
    for( ;; )
    {
        int i_med;

        if( i_max - i_min <= 1 )
            break;

        i_med = (i_min+i_max)/2;
        if( p_idx->idx[i_med].i_time < i_time )
            i_min = i_med;
        else if( p_idx->idx[i_med].i_time > i_time )
            i_max = i_med;
        else
            return p_idx->idx[i_med].i_offset;
    }

    /* return nearest in time */
    if( i_time - p_idx->idx[i_min].i_time < p_idx->idx[i_max].i_time - i_time )
        return p_idx->idx[i_min].i_offset;
    else
        return p_idx->idx[i_max].i_offset;
}


static int64_t demux_IndexFindOffset( demux_index_t *p_idx, int64_t i_offset )
{
    int i_min = 0;
    int i_max = p_idx->i_idx-1;

    /* Empty index */
    if( p_idx->i_idx <= 0 )
        return -1;

    /* Special border case */
    if( i_offset <= p_idx->idx[0].i_offset )
        return p_idx->idx[0].i_offset;
    if( i_offset == p_idx->idx[i_max].i_offset )
        return p_idx->idx[i_max].i_offset;
    if( i_offset > p_idx->idx[i_max].i_offset )
        return -1;

    /* Dicho */
    for( ;; )
    {
        int i_med;

        if( i_max - i_min <= 1 )
            break;

        i_med = (i_min+i_max)/2;
        if( p_idx->idx[i_med].i_offset < i_offset )
            i_min = i_med;
        else if( p_idx->idx[i_med].i_offset > i_offset )
            i_max = i_med;
        else
            return p_idx->idx[i_med].i_offset;
    }

    /* return nearest */
    if( i_offset - p_idx->idx[i_min].i_offset < p_idx->idx[i_max].i_offset - i_offset )
        return p_idx->idx[i_min].i_offset;
    else
        return p_idx->idx[i_max].i_offset;
}

