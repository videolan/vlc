/*****************************************************************************
 * linsys_hdsdi.c: HDSDI capture for Linear Systems/Computer Modules cards
 *****************************************************************************
 * Copyright (C) 2010-2011 VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <poll.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_demux.h>

#include <vlc_fs.h>

#include "linsys_sdivideo.h"
#include "linsys_sdiaudio.h"

#undef HAVE_MMAP_SDIVIDEO
#undef HAVE_MMAP_SDIAUDIO

#define SDIVIDEO_DEVICE         "/dev/sdivideorx%u"
#define SDIVIDEO_BUFFERS_FILE   "/sys/class/sdivideo/sdivideorx%u/buffers"
#define SDIVIDEO_BUFSIZE_FILE   "/sys/class/sdivideo/sdivideorx%u/bufsize"
#define SDIVIDEO_MODE_FILE      "/sys/class/sdivideo/sdivideorx%u/mode"
#define SDIAUDIO_DEVICE         "/dev/sdiaudiorx%u"
#define SDIAUDIO_BUFFERS_FILE   "/sys/class/sdiaudio/sdiaudiorx%u/buffers"
#define SDIAUDIO_BUFSIZE_FILE   "/sys/class/sdiaudio/sdiaudiorx%u/bufsize"
#define SDIAUDIO_SAMPLESIZE_FILE "/sys/class/sdiaudio/sdiaudiorx%u/sample_size"
#define SDIAUDIO_CHANNELS_FILE  "/sys/class/sdiaudio/sdiaudiorx%u/channels"
#define NB_VBUFFERS             2
#define CLOCK_GAP               INT64_C(500000)
#define START_DATE              INT64_C(4294967296)

#define MAX_AUDIOS              4

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define LINK_TEXT N_("Link #")
#define LINK_LONGTEXT N_( \
    "Allows you to set the desired link of the board for the capture (starting at 0)." )
#define VIDEO_TEXT N_("Video ID")
#define VIDEO_LONGTEXT N_( \
    "Allows you to set the ES ID of the video." )
#define VIDEO_ASPECT_TEXT N_("Aspect ratio")
#define VIDEO_ASPECT_LONGTEXT N_( \
    "Allows you to force the aspect ratio of the video." )
#define AUDIO_TEXT N_("Audio configuration")
#define AUDIO_LONGTEXT N_( \
    "Allows you to set audio configuration (id=group,pair:id=group,pair...)." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_description( N_("HD-SDI Input") )
    set_shortname( N_("HD-SDI") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_integer( "linsys-hdsdi-link", 0,
        LINK_TEXT, LINK_LONGTEXT, true )

    add_integer( "linsys-hdsdi-id-video", 0,
        VIDEO_TEXT, VIDEO_LONGTEXT, true )
    add_string( "linsys-hdsdi-aspect-ratio", "",
        VIDEO_ASPECT_TEXT, VIDEO_ASPECT_LONGTEXT, true )
    add_string( "linsys-hdsdi-audio", "0=1,1",
        AUDIO_TEXT, AUDIO_LONGTEXT, true )

    set_capability( "access_demux", 0 )
    add_shortcut( "linsys-hdsdi" )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct hdsdi_audio_t
{
    int         i_channel; /* i_group * 2 + i_pair */

    /* HDSDI parser */
    int32_t     i_delay;

    /* ES stuff */
    int         i_id;
    es_out_id_t *p_es;
} hdsdi_audio_t;

struct demux_sys_t
{
    /* video device reader */
    int          i_vfd;
    unsigned int i_link;
    unsigned int i_standard;
#ifdef HAVE_MMAP_SDIVIDEO
    uint8_t      **pp_vbuffers;
    unsigned int i_vbuffers, i_current_vbuffer;
#endif
    unsigned int i_vbuffer_size;

    /* audio device reader */
    int          i_afd;
    int          i_max_channel;
    unsigned int i_sample_rate;
#ifdef HAVE_MMAP_SDIAUDIO
    uint8_t      **pp_abuffers;
    unsigned int i_abuffers, i_current_abuffer;
#endif
    unsigned int i_abuffer_size;

    /* picture decoding */
    unsigned int i_frame_rate, i_frame_rate_base;
    unsigned int i_width, i_height, i_aspect, i_forced_aspect;
    unsigned int i_vblock_size, i_ablock_size;
    mtime_t      i_next_vdate, i_next_adate;
    int          i_incr, i_aincr;

    /* ES stuff */
    int          i_id_video;
    es_out_id_t  *p_es_video;
    hdsdi_audio_t p_audios[MAX_AUDIOS];

    pthread_t thread;
    int evfd;
};

static int Control( demux_t *, int, va_list );
static void *Demux( void * );

static int InitCapture( demux_t *p_demux );
static void CloseCapture( demux_t *p_demux );
static int Capture( demux_t *p_demux );

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;
    char        *psz_parser;

    /* Fill p_demux field */
    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( unlikely(!p_sys) )
        return VLC_ENOMEM;

    /* HDSDI AR */
    unsigned int i_num, i_den;
    if ( !var_InheritURational( p_demux, &i_num, &i_den,
                               "linsys-hdsdi-aspect-ratio" ) && i_den != 0 )
        p_sys->i_forced_aspect = p_sys->i_aspect =
                i_num * VOUT_ASPECT_FACTOR / i_den;
    else
        p_sys->i_forced_aspect = 0;

    /* */
    p_sys->i_id_video = var_InheritInteger( p_demux, "linsys-hdsdi-id-video" );

    /* Audio ES */
    char *psz_string = psz_parser = var_InheritString( p_demux,
                                                       "linsys-hdsdi-audio" );
    int i = 0;
    p_sys->i_max_channel = -1;

    while ( psz_parser != NULL && *psz_parser )
    {
        int i_id, i_group, i_pair;
        char *psz_next = strchr( psz_parser, '=' );
        if ( psz_next != NULL )
        {
            *psz_next = '\0';
            i_id = strtol( psz_parser, NULL, 0 );
            psz_parser = psz_next + 1;
        }
        else
            i_id = 0;

        psz_next = strchr( psz_parser, ':' );
        if ( psz_next != NULL )
        {
            *psz_next = '\0';
            psz_next++;
        }

        if ( sscanf( psz_parser, "%d,%d", &i_group, &i_pair ) == 2 )
        {
            p_sys->p_audios[i].i_channel = (i_group - 1) * 2 + (i_pair - 1);
            if ( p_sys->p_audios[i].i_channel > p_sys->i_max_channel )
                p_sys->i_max_channel = p_sys->p_audios[i].i_channel;
            p_sys->p_audios[i].i_id = i_id;
            i++;
        }
        else
            msg_Warn( p_demux, "malformed audio configuration (%s)",
                      psz_parser );

        psz_parser = psz_next;
    }
    free( psz_string );
    for ( ; i < MAX_AUDIOS; i++ )
        p_sys->p_audios[i].i_channel = -1;


    p_sys->i_link = var_InheritInteger( p_demux, "linsys-hdsdi-link" );

    p_sys->evfd = eventfd( 0, EFD_CLOEXEC );
    if( p_sys->evfd == -1 )
        goto error;

    if( pthread_create( &p_sys->thread, NULL, Demux, p_demux ) )
    {
        vlc_close( p_sys->evfd );
        goto error;
    }

    p_demux->pf_demux = NULL;
    p_demux->pf_control = Control;
    return VLC_SUCCESS;
error:
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * DemuxClose:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    write( p_sys->evfd, &(uint64_t){ 1 }, sizeof (uint64_t));
    pthread_join( p_sys->thread, NULL );
    vlc_close( p_sys->evfd );
    free( p_sys );
}

/*****************************************************************************
 * DemuxDemux:
 *****************************************************************************/
static void *Demux( void *opaque )
{
    demux_t *p_demux = opaque;

    if( InitCapture( p_demux ) != VLC_SUCCESS )
        return NULL;

    while( Capture( p_demux ) == VLC_SUCCESS );

    CloseCapture( p_demux );
    return NULL;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    bool *pb;
    int64_t *pi64;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            /* TODO */
            pb = va_arg( args, bool * );
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = va_arg( args, int64_t * );
            *pi64 = INT64_C(1000)
                  * var_InheritInteger( p_demux, "live-caching" );
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * HDSDI syntax parsing stuff
 *****************************************************************************/
#define U   (uint16_t)(p_line[0])
#define Y1  (uint16_t)(p_line[1])
#define V   (uint16_t)(p_line[2])
#define Y2  (uint16_t)(p_line[3])

/* For lines 0 [4] or 1 [4] */
static void Unpack01( const uint8_t *p_line, unsigned int i_size,
                      uint8_t *p_y, uint8_t *p_u, uint8_t *p_v )
{
    const uint8_t *p_end = p_line + i_size;

    while ( p_line < p_end )
    {
        *p_u++ = U;
        *p_y++ = Y1;
        *p_v++ = V;
        *p_y++ = Y2;
        p_line += 4;
    }
}

/* For lines 2 [4] */
static void Unpack2( const uint8_t *p_line, unsigned int i_size,
                     uint8_t *p_y, uint8_t *p_u, uint8_t *p_v )
{
    const uint8_t *p_end = p_line + i_size;

    while ( p_line < p_end )
    {
        uint16_t tmp;
        tmp = 3 * *p_u;
        tmp += U;
        *p_u++ = tmp / 4;
        *p_y++ = Y1;
        tmp = 3 * *p_v;
        tmp += V;
        *p_v++ = tmp / 4;
        *p_y++ = Y2;
        p_line += 4;
    }
}

/* For lines 3 [4] */
static void Unpack3( const uint8_t *p_line, unsigned int i_size,
                     uint8_t *p_y, uint8_t *p_u, uint8_t *p_v )
{
    const uint8_t *p_end = p_line + i_size;

    while ( p_line < p_end )
    {
        uint16_t tmp;
        tmp = *p_u;
        tmp += 3 * U;
        *p_u++ = tmp / 4;
        *p_y++ = Y1;
        tmp = *p_v;
        tmp += 3 * V;
        *p_v++ = tmp / 4;
        *p_y++ = Y2;
        p_line += 4;
    }
}

#undef U
#undef Y1
#undef V
#undef Y2

static void SparseCopy( int16_t *p_dest, const int16_t *p_src,
                        size_t i_nb_samples, size_t i_offset, size_t i_stride )
{
    for ( size_t i = 0; i < i_nb_samples; i++ )
    {
        p_dest[2 * i] = p_src[i_offset];
        p_dest[2 * i + 1] = p_src[i_offset + 1];
        i_offset += 2 * i_stride;
    }
}

/*****************************************************************************
 * Video & audio decoding
 *****************************************************************************/
struct block_extension_t
{
    bool            b_progressive;          /**< is it a progressive frame ? */
    bool            b_top_field_first;             /**< which field is first */
    unsigned int    i_nb_fields;                  /**< # of displayed fields */
    unsigned int    i_aspect;                     /**< aspect ratio of frame */
};

static void StopDecode( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    es_out_Del( p_demux->out, p_sys->p_es_video );

    for ( int i = 0; i < MAX_AUDIOS; i++ )
    {
        hdsdi_audio_t *p_audio = &p_sys->p_audios[i];
        if ( p_audio->i_channel != -1 && p_audio->p_es != NULL )
        {
            es_out_Del( p_demux->out, p_audio->p_es );
            p_audio->p_es = NULL;
        }
    }
}

static int InitVideo( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    es_format_t fmt;

    msg_Dbg( p_demux, "found standard %d", p_sys->i_standard );
    switch ( p_sys->i_standard )
    {
    case SDIVIDEO_CTL_BT_601_576I_50HZ:
        /* PAL */
        p_sys->i_frame_rate      = 25;
        p_sys->i_frame_rate_base = 1;
        p_sys->i_width           = 720;
        p_sys->i_height          = 576;
        p_sys->i_aspect          = 4 * VOUT_ASPECT_FACTOR / 3;
        break;

    case SDIVIDEO_CTL_SMPTE_296M_720P_50HZ:
        p_sys->i_frame_rate      = 50;
        p_sys->i_frame_rate_base = 1;
        p_sys->i_width           = 1280;
        p_sys->i_height          = 720;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        break;

    case SDIVIDEO_CTL_SMPTE_296M_720P_60HZ:
        p_sys->i_frame_rate      = 60;
        p_sys->i_frame_rate_base = 1;
        p_sys->i_width           = 1280;
        p_sys->i_height          = 720;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        break;

    case SDIVIDEO_CTL_SMPTE_295M_1080I_50HZ:
    case SDIVIDEO_CTL_SMPTE_274M_1080I_50HZ:
    case SDIVIDEO_CTL_SMPTE_274M_1080PSF_25HZ:
        /* 1080i50 or 1080p25 */
        p_sys->i_frame_rate      = 25;
        p_sys->i_frame_rate_base = 1;
        p_sys->i_width           = 1920;
        p_sys->i_height          = 1080;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        break;

    case SDIVIDEO_CTL_SMPTE_274M_1080I_59_94HZ:
        p_sys->i_frame_rate      = 30000;
        p_sys->i_frame_rate_base = 1001;
        p_sys->i_width           = 1920;
        p_sys->i_height          = 1080;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        break;

    case SDIVIDEO_CTL_SMPTE_274M_1080I_60HZ:
        p_sys->i_frame_rate      = 30;
        p_sys->i_frame_rate_base = 1;
        p_sys->i_width           = 1920;
        p_sys->i_height          = 1080;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        break;

    default:
        msg_Err( p_demux, "unsupported standard %d", p_sys->i_standard );
        return VLC_EGENERIC;
    }

    p_sys->i_next_vdate = START_DATE;
    p_sys->i_incr = 1000000 * p_sys->i_frame_rate_base / p_sys->i_frame_rate;
    p_sys->i_vblock_size = p_sys->i_width * p_sys->i_height * 3 / 2
                            + sizeof(struct block_extension_t);

    /* Video ES */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_I420 );
    fmt.i_id                    = p_sys->i_id_video;
    fmt.video.i_frame_rate      = p_sys->i_frame_rate;
    fmt.video.i_frame_rate_base = p_sys->i_frame_rate_base;
    fmt.video.i_width           = fmt.video.i_visible_width = p_sys->i_width;
    fmt.video.i_height          = fmt.video.i_visible_height = p_sys->i_height;
    fmt.video.i_sar_num         = p_sys->i_aspect * fmt.video.i_height
                                  / fmt.video.i_width;
    fmt.video.i_sar_den         = VOUT_ASPECT_FACTOR;
    p_sys->p_es_video           = es_out_Add( p_demux->out, &fmt );

    return VLC_SUCCESS;
}

static int InitAudio( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    es_format_t fmt;

    for ( int i = 0; i < MAX_AUDIOS; i++ )
    {
        hdsdi_audio_t *p_audio = &p_sys->p_audios[i];

        if ( p_audio->i_channel == -1 ) continue;

        msg_Dbg( p_demux, "starting audio %u/%u rate:%u delay:%d",
                 1 + p_audio->i_channel / 2, 1 + (p_audio->i_channel % 2),
                 p_sys->i_sample_rate, p_audio->i_delay );

        es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_S16L );
        fmt.i_id = p_audio->i_id;
        fmt.audio.i_channels          = 2;
        fmt.audio.i_physical_channels = AOUT_CHANS_STEREO;
        fmt.audio.i_rate              = p_sys->i_sample_rate;
        fmt.audio.i_bitspersample     = 16;
        fmt.audio.i_blockalign = fmt.audio.i_channels *
            fmt.audio.i_bitspersample / 8;
        fmt.i_bitrate = fmt.audio.i_channels * fmt.audio.i_rate *
            fmt.audio.i_bitspersample;
        p_audio->p_es = es_out_Add( p_demux->out, &fmt );
    }

    p_sys->i_next_adate = START_DATE;
    p_sys->i_ablock_size = p_sys->i_sample_rate * 4 * p_sys->i_frame_rate_base / p_sys->i_frame_rate;
    p_sys->i_aincr = 1000000. * p_sys->i_ablock_size / p_sys->i_sample_rate / 4;

    return VLC_SUCCESS;
}

static int HandleVideo( demux_t *p_demux, const uint8_t *p_buffer )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_current_picture = block_Alloc( p_sys->i_vblock_size );
    if( unlikely( !p_current_picture ) )
        return VLC_ENOMEM;
    uint8_t *p_y = p_current_picture->p_buffer;
    uint8_t *p_u = p_y + p_sys->i_width * p_sys->i_height;
    uint8_t *p_v = p_u + p_sys->i_width * p_sys->i_height / 4;
    unsigned int i_total_size = p_sys->i_width * 2;
    unsigned int i_current_line;
    struct block_extension_t ext;

    for ( i_current_line = 0; i_current_line < p_sys->i_height;
          i_current_line++ )
    {
        bool b_field = (i_current_line >= p_sys->i_height / 2);
        unsigned int i_field_line = b_field ?
            i_current_line - (p_sys->i_height + 1) / 2 :
            i_current_line;
        unsigned int i_real_line = b_field + i_field_line * 2;
        const uint8_t *p_line = p_buffer + i_current_line * p_sys->i_width * 2;

        if ( !(i_field_line % 2) && !b_field )
            Unpack01( p_line, i_total_size,
                      p_y + p_sys->i_width * i_real_line,
                      p_u + (p_sys->i_width / 2) * (i_real_line / 2),
                      p_v + (p_sys->i_width / 2) * (i_real_line / 2) );
        else if ( !(i_field_line % 2) )
            Unpack01( p_line, i_total_size,
                      p_y + p_sys->i_width * i_real_line,
                      p_u + (p_sys->i_width / 2) * (i_real_line / 2 + 1),
                      p_v + (p_sys->i_width / 2) * (i_real_line / 2 + 1) );
       else if ( !b_field )
            Unpack2( p_line, i_total_size,
                     p_y + p_sys->i_width * i_real_line,
                     p_u + (p_sys->i_width / 2) * (i_real_line / 2 - 1),
                     p_v + (p_sys->i_width / 2) * (i_real_line / 2 - 1) );
       else
            Unpack3( p_line, i_total_size,
                     p_y + p_sys->i_width * i_real_line,
                     p_u + (p_sys->i_width / 2) * (i_real_line / 2),
                     p_v + (p_sys->i_width / 2) * (i_real_line / 2) );
    }

    /* FIXME: progressive formats ? */
    ext.b_progressive = false;
    ext.i_nb_fields = 2;
    ext.b_top_field_first = true;
    ext.i_aspect = p_sys->i_forced_aspect ? p_sys->i_forced_aspect :
                   p_sys->i_aspect;

    memcpy( &p_current_picture->p_buffer[p_sys->i_vblock_size
                                          - sizeof(struct block_extension_t)],
            &ext, sizeof(struct block_extension_t) );

    p_current_picture->i_dts = p_current_picture->i_pts = p_sys->i_next_vdate;
    es_out_Send( p_demux->out, p_sys->p_es_video, p_current_picture );

    es_out_SetPCR( p_demux->out, p_sys->i_next_vdate );
    p_sys->i_next_vdate += p_sys->i_incr;
    return VLC_SUCCESS;
}

static int HandleAudio( demux_t *p_demux, const uint8_t *p_buffer )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for ( int i = 0; i < MAX_AUDIOS; i++ )
    {
        hdsdi_audio_t *p_audio = &p_sys->p_audios[i];
        if ( p_audio->i_channel != -1 && p_audio->p_es != NULL )
        {
            block_t *p_block = block_Alloc( p_sys->i_ablock_size );
            if( unlikely( !p_block ) )
                return VLC_ENOMEM;
            SparseCopy( (int16_t *)p_block->p_buffer, (const int16_t *)p_buffer,
                        p_sys->i_ablock_size / 4,
                        p_audio->i_channel * 2, p_sys->i_max_channel + 1 );

            p_block->i_dts = p_block->i_pts
                = p_sys->i_next_adate + (mtime_t)p_audio->i_delay
                   * INT64_C(1000000) / p_sys->i_sample_rate;
            p_block->i_length = p_sys->i_aincr;
            es_out_Send( p_demux->out, p_audio->p_es, p_block );
        }
    }
    p_sys->i_next_adate += p_sys->i_aincr;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Low-level device stuff
 *****************************************************************************/
#define MAXLEN 256

static ssize_t WriteULSysfs( const char *psz_fmt, unsigned int i_link,
                             unsigned int i_buf )
{
    char psz_file[MAXLEN], psz_data[MAXLEN];
    int i_fd;
    ssize_t i_ret;

    snprintf( psz_file, sizeof(psz_file) -1, psz_fmt, i_link );

    snprintf( psz_data, sizeof(psz_data) -1, "%u\n", i_buf );

    if ( (i_fd = vlc_open( psz_file, O_WRONLY )) < 0 )
        return i_fd;

    i_ret = write( i_fd, psz_data, strlen(psz_data) + 1 );
    vlc_close( i_fd );
    return i_ret;
}

static int InitCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
#ifdef HAVE_MMAP_SDIVIDEO
    const int i_page_size = getpagesize();
    unsigned int i_bufmemsize;
#endif
    char psz_vdev[MAXLEN];

    snprintf( psz_vdev, sizeof(psz_vdev), SDIVIDEO_DEVICE, p_sys->i_link );
    if ( (p_sys->i_vfd = vlc_open( psz_vdev, O_RDONLY ) ) < 0 )
    {
        msg_Err( p_demux, "couldn't open device %s", psz_vdev );
        return VLC_EGENERIC;
    }

    /* Wait for standard to settle down */
    struct pollfd pfd[2];

    pfd[0].fd = p_sys->i_vfd;
    pfd[0].events = POLLPRI;
    pfd[1].fd = p_sys->evfd;
    pfd[1].events = POLLIN;

    for( ;; )
    {
        if( poll( pfd, 2, -1 ) < 0 )
            continue;

        if ( pfd[0].revents & POLLPRI )
        {
            unsigned int i_val;

            if ( ioctl( p_sys->i_vfd, SDIVIDEO_IOC_RXGETEVENTS, &i_val ) < 0 )
                msg_Warn( p_demux, "couldn't SDIVIDEO_IOC_RXGETEVENTS: %s",
                          vlc_strerror_c(errno) );
            else
            {
                if ( i_val & SDIVIDEO_EVENT_RX_BUFFER )
                    msg_Warn( p_demux, "driver receive buffer queue overrun" );
                if ( i_val & SDIVIDEO_EVENT_RX_FIFO )
                    msg_Warn( p_demux, "onboard receive FIFO overrun");
                if ( i_val & SDIVIDEO_EVENT_RX_CARRIER )
                    msg_Warn( p_demux, "carrier status change");
                if ( i_val & SDIVIDEO_EVENT_RX_DATA )
                    msg_Warn( p_demux, "data status change");
                if ( i_val & SDIVIDEO_EVENT_RX_STD )
                {
                    msg_Warn( p_demux, "standard status change");
                    break;
                }
            }
        }

        if( pfd[1].revents )
        {
            vlc_close( p_sys->i_vfd );
            return VLC_EGENERIC;
        }
    }

    if ( ioctl( p_sys->i_vfd, SDIVIDEO_IOC_RXGETVIDSTATUS, &p_sys->i_standard )
          < 0 )
    {
        msg_Warn( p_demux, "couldn't SDIVIDEO_IOC_RXGETVIDSTATUS: %s",
                  vlc_strerror_c(errno) );
        vlc_close( p_sys->i_vfd );
        return VLC_EGENERIC;
    }
    vlc_close( p_sys->i_vfd );

    if ( InitVideo( p_demux ) != VLC_SUCCESS )
        return VLC_EGENERIC;
    p_sys->i_vbuffer_size = p_sys->i_height * p_sys->i_width * 2;

    /* First open the audio for synchronization reasons */
    if ( p_sys->i_max_channel != -1 )
    {
        unsigned int i_rate;
        char psz_adev[MAXLEN];

        snprintf( psz_adev, sizeof(psz_adev), SDIAUDIO_DEVICE, p_sys->i_link );
        if ( (p_sys->i_afd = vlc_open( psz_adev, O_RDONLY ) ) < 0 )
        {
            msg_Err( p_demux, "couldn't open device %s", psz_adev );
            return VLC_EGENERIC;
        }

        if ( ioctl( p_sys->i_afd, SDIAUDIO_IOC_RXGETAUDRATE, &i_rate ) < 0 )
        {
            msg_Warn( p_demux, "couldn't SDIAUDIO_IOC_RXGETAUDRATE: %s",
                      vlc_strerror_c(errno) );
            return VLC_EGENERIC;
        }
        switch ( i_rate )
        {
        case SDIAUDIO_CTL_ASYNC_48_KHZ:
        case SDIAUDIO_CTL_SYNC_48_KHZ:
            p_sys->i_sample_rate = 48000;
            break;
        case SDIAUDIO_CTL_ASYNC_44_1_KHZ:
        case SDIAUDIO_CTL_SYNC_44_1_KHZ:
            p_sys->i_sample_rate = 44100;
            break;
        case SDIAUDIO_CTL_ASYNC_32_KHZ:
        case SDIAUDIO_CTL_SYNC_32_KHZ:
            p_sys->i_sample_rate = 32000;
            break;
        case SDIAUDIO_CTL_ASYNC_96_KHZ:
        case SDIAUDIO_CTL_SYNC_96_KHZ:
            p_sys->i_sample_rate = 96000;
            break;
        case SDIAUDIO_CTL_ASYNC_FREE_RUNNING:
        case SDIAUDIO_CTL_SYNC_FREE_RUNNING:
        default:
            msg_Err( p_demux, "unknown sample rate %u", i_rate );
            return VLC_EGENERIC;
        }
        vlc_close( p_sys->i_afd );

        if ( InitAudio( p_demux ) != VLC_SUCCESS )
            return VLC_EGENERIC;
        p_sys->i_abuffer_size = p_sys->i_ablock_size
                                 * (1 + p_sys->i_max_channel);

        /* Use 16-bit audio */
        if ( WriteULSysfs( SDIAUDIO_SAMPLESIZE_FILE, p_sys->i_link,
                           SDIAUDIO_CTL_AUDSAMP_SZ_16 ) < 0 )
        {
            msg_Err( p_demux, "couldn't write file " SDIAUDIO_SAMPLESIZE_FILE,
                     p_sys->i_link );
            return VLC_EGENERIC;
        }

        if ( WriteULSysfs( SDIAUDIO_CHANNELS_FILE, p_sys->i_link,
                           (p_sys->i_max_channel + 1) * 2 ) < 0 )
        {
            msg_Err( p_demux, "couldn't write file " SDIAUDIO_CHANNELS_FILE,
                     p_sys->i_link );
            return VLC_EGENERIC;
        }

#ifdef HAVE_MMAP_SDIAUDIO
        if ( (p_sys->i_abuffers = ReadULSysfs( SDIAUDIO_BUFFERS_FILE,
                                               p_sys->i_link )) < 0 )
        {
            msg_Err( p_demux, "couldn't read file " SDIAUDIO_BUFFERS_FILE,
                     p_sys->i_link );
            return VLC_EGENERIC;
        }
        p_sys->i_current_abuffer = 0;
#endif

        if ( WriteULSysfs( SDIAUDIO_BUFSIZE_FILE, p_sys->i_link,
                           p_sys->i_abuffer_size ) < 0 )
        {
            msg_Err( p_demux, "couldn't write file " SDIAUDIO_BUFSIZE_FILE,
                     p_sys->i_link );
            return VLC_EGENERIC;
        }

        if ( (p_sys->i_afd = open( psz_adev, O_RDONLY ) ) < 0 )
        {
            msg_Err( p_demux, "couldn't open device %s", psz_adev );
            return VLC_EGENERIC;
        }

#ifdef HAVE_MMAP_SDIAUDIO
        i_bufmemsize = ((p_sys->i_abuffer_size + i_page_size - 1) / i_page_size)
                         * i_page_size;
        p_sys->pp_abuffers = vlc_alloc( p_sys->i_abuffers, sizeof(uint8_t *) );
        if( unlikely( !p_sys->pp_abuffers ) )
            return VLC_ENOMEM;
        for ( unsigned int i = 0; i < p_sys->i_abuffers; i++ )
        {
            if ( (p_sys->pp_abuffers[i] = mmap( NULL, p_sys->i_abuffer_size,
                                                PROT_READ, MAP_SHARED, p_sys->i_afd,
                                                i * i_bufmemsize )) == MAP_FAILED )
            {
                msg_Err( p_demux, "couldn't mmap(%d): %s", i,
                         vlc_strerror_c(errno) );
                return VLC_EGENERIC;
            }
        }
#endif
    }

    /* Use 8-bit video */
    if ( WriteULSysfs( SDIVIDEO_MODE_FILE, p_sys->i_link,
                       SDIVIDEO_CTL_MODE_UYVY ) < 0 )
    {
        msg_Err( p_demux, "couldn't write file " SDIVIDEO_MODE_FILE,
                 p_sys->i_link );
        return VLC_EGENERIC;
    }

    if ( WriteULSysfs( SDIVIDEO_BUFFERS_FILE, p_sys->i_link,
                       NB_VBUFFERS ) < 0 )
    {
        msg_Err( p_demux, "couldn't write file " SDIVIDEO_BUFFERS_FILE,
                 p_sys->i_link );
        return VLC_EGENERIC;
    }
#ifdef HAVE_MMAP_SDIVIDEO
    p_sys->i_vbuffers = NB_VBUFFERS;
#endif

    if ( WriteULSysfs( SDIVIDEO_BUFSIZE_FILE, p_sys->i_link,
                       p_sys->i_vbuffer_size ) < 0 )
    {
        msg_Err( p_demux, "couldn't write file " SDIVIDEO_BUFSIZE_FILE,
                 p_sys->i_link );
        return VLC_EGENERIC;
    }

    if ( (p_sys->i_vfd = open( psz_vdev, O_RDONLY ) ) < 0 )
    {
        msg_Err( p_demux, "couldn't open device %s", psz_vdev );
        return VLC_EGENERIC;
    }

#ifdef HAVE_MMAP_SDIVIDEO
    p_sys->i_current_vbuffer = 0;
    i_bufmemsize = ((p_sys->i_vbuffer_size + i_page_size - 1) / i_page_size)
                     * i_page_size;
    p_sys->pp_vbuffers = vlc_alloc( p_sys->i_vbuffers, sizeof(uint8_t *) );
    if( unlikely( !p_sys->pp_vbuffers ) )
        return VLC_ENOMEM;
    for ( unsigned int i = 0; i < p_sys->i_vbuffers; i++ )
    {
        if ( (p_sys->pp_vbuffers[i] = mmap( NULL, p_sys->i_vbuffer_size,
                                            PROT_READ, MAP_SHARED, p_sys->i_vfd,
                                            i * i_bufmemsize )) == MAP_FAILED )
        {
            msg_Err( p_demux, "couldn't mmap(%d): %s", i,
                     vlc_strerror_c(errno) );
            return VLC_EGENERIC;
        }
    }
#endif

    return VLC_SUCCESS;
}

static void CloseCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    StopDecode( p_demux );
#ifdef HAVE_MMAP_SDIVIDEO
    for ( unsigned int i = 0; i < p_sys->i_vbuffers; i++ )
        munmap( p_sys->pp_vbuffers[i], p_sys->i_vbuffer_size );
    free( p_sys->pp_vbuffers );
#endif
    vlc_close( p_sys->i_vfd );
    if ( p_sys->i_max_channel != -1 )
    {
#ifdef HAVE_MMAP_SDIAUDIO
        for ( unsigned int i = 0; i < p_sys->i_abuffers; i++ )
            munmap( p_sys->pp_abuffers[i], p_sys->i_abuffer_size );
        free( p_sys->pp_abuffers );
#endif
        vlc_close( p_sys->i_afd );
    }
}

static int Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    struct pollfd pfd[3];

    pfd[0].fd = p_sys->evfd;
    pfd[0].events = POLLIN;
    pfd[1].fd = p_sys->i_vfd;
    pfd[1].events = POLLIN | POLLPRI;
    if ( p_sys->i_max_channel != -1 )
    {
        pfd[2].fd = p_sys->i_afd;
        pfd[2].events = POLLIN | POLLPRI;
    }

    if( poll( pfd, 2 + (p_sys->i_max_channel != -1), -1 ) < 0 )
        return VLC_SUCCESS;

    if( pfd[0].revents )
        return VLC_EGENERIC; /* Stop! */

    if( pfd[1].revents & POLLPRI )
    {
        unsigned int i_val;

        if ( ioctl( p_sys->i_vfd, SDIVIDEO_IOC_RXGETEVENTS, &i_val ) < 0 )
            msg_Warn( p_demux, "couldn't SDIVIDEO_IOC_RXGETEVENTS: %s",
                      vlc_strerror_c(errno) );
        else
        {
            if ( i_val & SDIVIDEO_EVENT_RX_BUFFER )
                msg_Warn( p_demux, "driver receive buffer queue overrun" );
            if ( i_val & SDIVIDEO_EVENT_RX_FIFO )
                msg_Warn( p_demux, "onboard receive FIFO overrun");
            if ( i_val & SDIVIDEO_EVENT_RX_CARRIER )
                msg_Warn( p_demux, "carrier status change");
            if ( i_val & SDIVIDEO_EVENT_RX_DATA )
                msg_Warn( p_demux, "data status change");
            if ( i_val & SDIVIDEO_EVENT_RX_STD )
                msg_Warn( p_demux, "standard status change");
        }

        p_sys->i_next_adate += CLOCK_GAP;
        p_sys->i_next_vdate += CLOCK_GAP;
    }

    if( p_sys->i_max_channel != -1 && (pfd[2].revents & POLLPRI) )
    {
        unsigned int i_val;

        if ( ioctl( p_sys->i_afd, SDIAUDIO_IOC_RXGETEVENTS, &i_val ) < 0 )
            msg_Warn( p_demux, "couldn't SDIAUDIO_IOC_RXGETEVENTS: %s",
                      vlc_strerror_c(errno) );
        else
        {
            if ( i_val & SDIAUDIO_EVENT_RX_BUFFER )
                msg_Warn( p_demux, "driver receive buffer queue overrun" );
            if ( i_val & SDIAUDIO_EVENT_RX_FIFO )
                msg_Warn( p_demux, "onboard receive FIFO overrun");
            if ( i_val & SDIAUDIO_EVENT_RX_CARRIER )
                msg_Warn( p_demux, "carrier status change");
            if ( i_val & SDIAUDIO_EVENT_RX_DATA )
                msg_Warn( p_demux, "data status change");
        }

        p_sys->i_next_adate += CLOCK_GAP;
        p_sys->i_next_vdate += CLOCK_GAP;
    }

    if( pfd[1].revents & POLLIN )
    {
#ifdef HAVE_MMAP_SDIVIDEO
        if ( ioctl( p_sys->i_vfd, SDIVIDEO_IOC_DQBUF, p_sys->i_current_vbuffer )
              < 0 )
        {
            msg_Warn( p_demux, "couldn't SDIVIDEO_IOC_DQBUF: %s",
                      vlc_strerror_c(errno) );
            return VLC_EGENERIC;
        }

        if( HandleVideo( p_demux, p_sys->pp_vbuffers[p_sys->i_current_vbuffer] ) != VLC_SUCCESS )
            return VLC_ENOMEM;

        if ( ioctl( p_sys->i_vfd, SDIVIDEO_IOC_QBUF, p_sys->i_current_vbuffer )
              < 0 )
        {
            msg_Warn( p_demux, "couldn't SDIVIDEO_IOC_QBUF: %s",
                      vlc_strerror_c(errno) );
            return VLC_EGENERIC;
        }

        p_sys->i_current_vbuffer++;
        p_sys->i_current_vbuffer %= p_sys->i_vbuffers;
#else
        uint8_t *p_buffer = malloc( p_sys->i_vbuffer_size );
        if( unlikely( !p_buffer ) )
            return VLC_ENOMEM;

        if ( read( p_sys->i_vfd, p_buffer, p_sys->i_vbuffer_size ) < 0 )
        {
            msg_Warn( p_demux, "couldn't read: %s", vlc_strerror_c(errno) );
            free( p_buffer );
            return VLC_EGENERIC;
        }

        if( HandleVideo( p_demux, p_buffer ) != VLC_SUCCESS )
        {
            free( p_buffer );
            return VLC_ENOMEM;
        }
        free( p_buffer );
#endif
    }

    if( p_sys->i_max_channel != -1 && (pfd[2].revents & POLLIN) )
    {
#ifdef HAVE_MMAP_SDIAUDIO
        if ( ioctl( p_sys->i_afd, SDIAUDIO_IOC_DQBUF, p_sys->i_current_abuffer )
              < 0 )
        {
            msg_Warn( p_demux, "couldn't SDIAUDIO_IOC_DQBUF: %s",
                      vlc_strerror_c(errno) );
            return VLC_EGENERIC;
        }

        if( HandleAudio( p_demux, p_sys->pp_abuffers[p_sys->i_current_abuffer] ) != VLC_SUCCESS )
            return VLC_ENOMEM;

        if ( ioctl( p_sys->i_afd, SDIAUDIO_IOC_QBUF, p_sys->i_current_abuffer )
              < 0 )
        {
            msg_Warn( p_demux, "couldn't SDIAUDIO_IOC_QBUF: %s",
                      vlc_strerror_c(errno) );
            return VLC_EGENERIC;
        }

        p_sys->i_current_abuffer++;
        p_sys->i_current_abuffer %= p_sys->i_abuffers;
#else
        uint8_t *p_buffer = malloc( p_sys->i_abuffer_size );
        if( unlikely( !p_buffer ) )
            return VLC_ENOMEM;

        if ( read( p_sys->i_afd, p_buffer, p_sys->i_abuffer_size ) < 0 )
        {
            msg_Warn( p_demux, "couldn't read: %s", vlc_strerror_c(errno) );
            free( p_buffer );
            return VLC_EGENERIC;
        }

        if( HandleAudio( p_demux, p_buffer ) != VLC_SUCCESS )
        {
            free( p_buffer );
            return VLC_ENOMEM;
        }
        free( p_buffer );
#endif
    }

    return VLC_SUCCESS;
}
