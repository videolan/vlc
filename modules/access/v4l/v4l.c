/*****************************************************************************
 * v4l.c : Video4Linux input module for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: v4l.c,v 1.32 2003/11/24 00:39:01 fenrir Exp $
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
 *         Paul Forgey <paulf at aphrodite dot com>
 *         Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdio.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/vout.h>
#include <codecs.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>

/* From GStreamer's v4l plugin:
 * Because of some really cool feature in video4linux1, also known as
 * 'not including sys/types.h and sys/time.h', we had to include it
 * ourselves. In all their intelligence, these people decided to fix
 * this in the next version (video4linux2) in such a cool way that it
 * breaks all compilations of old stuff...
 * The real problem is actually that linux/time.h doesn't use proper
 * macro checks before defining types like struct timeval. The proper
 * fix here is to either fuck the kernel header (which is what we do
 * by defining _LINUX_TIME_H, an innocent little hack) or by fixing it
 * upstream, which I'll consider doing later on. If you get compiler
 * errors here, check your linux/time.h && sys/time.h header setup.
*/
#define _LINUX_TIME_H

#include <linux/videodev.h>
#include "videodev_mjpeg.h"

#include <sys/soundcard.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  AccessOpen  ( vlc_object_t * );
static void AccessClose ( vlc_object_t * );
static int  Read        ( input_thread_t *, byte_t *, size_t );

static void ParseMRL    ( input_thread_t * );
static int  OpenVideoDev( input_thread_t *, char * );
static int  OpenAudioDev( input_thread_t *, char * );

static int  DemuxOpen  ( vlc_object_t * );
static void DemuxClose ( vlc_object_t * );
static int  Demux      ( input_thread_t * );

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for v4l streams. This " \
    "value should be set in miliseconds units." )
#define VDEV_TEXT N_("Video device name")
#define VDEV_LONGTEXT N_( \
    "Specify the name of the video device that will be used. " \
    "If you don't specify anything, no video device will be used.")
#define ADEV_TEXT N_("Audio device name")
#define ADEV_LONGTEXT N_( \
    "Specify the name of the audio device that will be used. " \
    "If you don't specify anything, no audio device will be used.")

vlc_module_begin();
    set_description( _("Video4Linux input") );
    add_category_hint( N_("v4l"), NULL, VLC_TRUE );
    add_integer( "v4l-caching", DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_shortcut( "v4l" );
    set_capability( "access", 10 );
    set_callbacks( AccessOpen, AccessClose );

    add_string( "v4l-vdev", "/dev/video", 0, VDEV_TEXT, VDEV_LONGTEXT,
                VLC_FALSE );
    add_string( "v4l-adev", "/dev/dsp", 0, ADEV_TEXT, ADEV_LONGTEXT,
                VLC_FALSE );

    add_submodule();
        set_description( _("Video4Linux demuxer") );
        add_shortcut( "v4l" );
        set_capability( "demux", 200 );
        set_callbacks( DemuxOpen, DemuxClose );
vlc_module_end();


/****************************************************************************
 * I. Access Part
 ****************************************************************************/
#define MJPEG_BUFFER_SIZE (256*1024)

struct quicktime_mjpeg_app1
{
    uint32_t    i_reserved;             /* set to 0 */
    uint32_t    i_tag;                  /* 'mjpg' */
    uint32_t    i_field_size;           /* offset following EOI */
    uint32_t    i_padded_field_size;    /* offset following EOI+pad */
    uint32_t    i_next_field;           /* offset to next field */
    uint32_t    i_DQT_offset;
    uint32_t    i_DHT_offset;
    uint32_t    i_SOF_offset;
    uint32_t    i_SOS_offset;
    uint32_t    i_data_offset;          /* following SOS marker data */
};

struct access_sys_t
{
    /* Devices */
    char *psz_device;         /* Main device from MRL, can be video or audio */

    char *psz_vdev;
    int  fd_video;

    char *psz_adev;
    int  fd_audio;

    /* Video properties */
    picture_t pic;

    int i_fourcc;
    int i_channel;
    int i_audio;
    int i_norm;
    int i_tuner;
    int i_frequency;
    int i_width;
    int i_height;

    float f_fps;            /* <= 0.0 mean to grab at full rate */
    mtime_t i_video_pts;    /* only used when f_fps > 0 */

    vlc_bool_t b_mjpeg;
    int i_decimation;
    int i_quality;

    struct video_capability vid_cap;
    struct video_mbuf       vid_mbuf;
    struct mjpeg_requestbuffers mjpeg_buffers;

    uint8_t *p_video_mmap;
    int     i_frame_pos;

    struct video_mmap   vid_mmap;
    struct video_picture vid_picture;

    uint8_t *p_video_frame;
    int     i_video_frame_size;
    int     i_video_frame_size_allocated;

    /* Audio properties */
    vlc_fourcc_t i_acodec_raw;
    int          i_sample_rate;
    vlc_bool_t   b_stereo;

    uint8_t *p_audio_frame;
    int     i_audio_frame_size;
    int     i_audio_frame_size_allocated;

    /* header */
    int     i_streams;
    int     i_header_size;
    int     i_header_pos;
    uint8_t *p_header;      // at lest 8 bytes allocated

    /* data */
    int     i_data_size;
    int     i_data_pos;
    uint8_t *p_data;        // never allocated

};

/*
 * header:
 *  fcc  ".v4l"
 *  u32    stream count
 *      fcc "auds"|"vids"       0
 *      fcc codec               4
 *      if vids
 *          u32 width           8
 *          u32 height          12
 *          u32 padding         16
 *      if auds
 *          u32 channels        12
 *          u32 samplerate      8
 *          u32 samplesize      16
 *
 * data:
 *  u32     stream number
 *  u32     data size
 *  u8      data
 */

static void SetDWBE( uint8_t *p, uint32_t dw )
{
    p[0] = (dw >> 24)&0xff;
    p[1] = (dw >> 16)&0xff;
    p[2] = (dw >>  8)&0xff;
    p[3] = (dw      )&0xff;
}

static void SetQWBE( uint8_t *p, uint64_t qw )
{
    SetDWBE( p,     (qw >> 32)&0xffffffff );
    SetDWBE( &p[4], qw&0xffffffff);
}

/*****************************************************************************
 * AccessOpen: opens v4l device
 *****************************************************************************
 *
 * url: <video device>::::
 *
 *****************************************************************************/
static int AccessOpen( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys;
    vlc_value_t    val;

    /* create access private data */
    p_input->p_access_data = p_sys = malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );

    p_sys->i_audio          = -1;
    p_sys->i_norm           = VIDEO_MODE_AUTO;    // auto
    p_sys->i_tuner          = -1;
    p_sys->i_frequency      = -1;

    p_sys->f_fps            = -1.0;
    p_sys->i_video_pts      = -1;

    p_sys->b_mjpeg     = VLC_FALSE;
    p_sys->i_decimation = 1;
    p_sys->i_quality = 100;

    p_sys->i_video_frame_size_allocated = 0;
    p_sys->i_frame_pos = 0;

    p_sys->p_audio_frame  = 0;
    p_sys->i_sample_rate  = 44100;
    p_sys->b_stereo       = VLC_TRUE;

    p_sys->p_header    = NULL;
    p_sys->i_data_size = 0;
    p_sys->i_data_pos  = 0;
    p_sys->p_data      = NULL;

    p_sys->psz_device = p_sys->psz_vdev = p_sys->psz_adev = NULL;
    p_sys->fd_video = -1;
    p_sys->fd_audio = -1;

    ParseMRL( p_input );

    /* Find main device (video or audio) */
    if( p_sys->psz_device && *p_sys->psz_device )
    {
        msg_Dbg( p_input, "main device=`%s'", p_sys->psz_device );

        /* Try to open as video device */
        p_sys->fd_video = OpenVideoDev( p_input, p_sys->psz_device );

        if( p_sys->fd_video < 0 )
        {
            /* Try to open as audio device */
            p_sys->fd_audio = OpenAudioDev( p_input, p_sys->psz_device );
            if( p_sys->fd_audio >= 0 )
            {
                if( p_sys->psz_adev ) free( p_sys->psz_adev );
                p_sys->psz_adev = p_sys->psz_device;
                p_sys->psz_device = NULL;
            }
        }
        else
        {
            if( p_sys->psz_vdev ) free( p_sys->psz_vdev );
            p_sys->psz_vdev = p_sys->psz_device;
            p_sys->psz_device = NULL;
        }
    }

    /* If no device opened, only continue if the access was forced */
    if( p_sys->fd_video < 0 && p_sys->fd_audio < 0 )
    {
        if( !p_input->psz_access ||
            strcmp( p_input->psz_access, "v4l" ) )
        {
            AccessClose( p_this );
            return VLC_EGENERIC;
        }
    }

    /* Find video device */
    if( p_sys->fd_video < 0 )
    {
        if( !p_sys->psz_vdev || !*p_sys->psz_vdev )
        {
            var_Create( p_input, "v4l-vdev",
                        VLC_VAR_STRING | VLC_VAR_DOINHERIT );
            var_Get( p_input, "v4l-vdev", &val );

            if( p_sys->psz_vdev ) free( p_sys->psz_vdev );
            p_sys->psz_vdev = val.psz_string;
        }

        if( p_sys->psz_vdev && *p_sys->psz_vdev )
        {
            p_sys->fd_video = OpenVideoDev( p_input, p_sys->psz_vdev );
        }
    }

    /* Find audio device */
    if( p_sys->fd_audio < 0 )
    {
        if( !p_sys->psz_adev || !*p_sys->psz_adev )
        {
            var_Create( p_input, "v4l-adev",
                        VLC_VAR_STRING | VLC_VAR_DOINHERIT );
            var_Get( p_input, "v4l-adev", &val );

            if( p_sys->psz_adev ) free( p_sys->psz_adev );
            p_sys->psz_adev = val.psz_string;
        }

        if( p_sys->psz_adev && *p_sys->psz_adev )
        {
            p_sys->fd_audio = OpenAudioDev( p_input, p_sys->psz_adev );
        }
    }

    if( p_sys->fd_video < 0 && p_sys->fd_audio < 0 )
    {
        AccessClose( p_this );
        return VLC_EGENERIC;
    }

    p_input->pf_read        = Read;
    p_input->pf_seek        = NULL;
    p_input->pf_set_area    = NULL;
    p_input->pf_set_program = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_FILE;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for access */
    var_Create( p_input, "v4l-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Get( p_input, "v4l-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    msg_Info( p_input, "v4l grabbing started" );

    /* create header */
    p_sys->i_streams     = 0;
    p_sys->i_header_size = 8;
    p_sys->i_header_pos  = 8;
    p_sys->p_header      = malloc( p_sys->i_header_size );

    memcpy( p_sys->p_header, ".v4l", 4 );

    if( p_sys->fd_video >= 0 )
    {
        p_sys->i_header_size += 20;
        p_sys->p_header = realloc( p_sys->p_header, p_sys->i_header_size );

        SetDWBE( &p_sys->p_header[4], ++p_sys->i_streams );

        memcpy(  &p_sys->p_header[p_sys->i_header_pos], "vids", 4 );
        memcpy(  &p_sys->p_header[p_sys->i_header_pos+4],
                 &p_sys->i_fourcc, 4 );
        SetDWBE( &p_sys->p_header[p_sys->i_header_pos+8], p_sys->i_width );
        SetDWBE( &p_sys->p_header[p_sys->i_header_pos+12], p_sys->i_height );
        SetDWBE( &p_sys->p_header[p_sys->i_header_pos+16], 0 );

        p_sys->i_header_pos = p_sys->i_header_size;
    }

    if( p_sys->fd_audio >= 0 )
    {
        p_sys->i_header_size += 20;
        p_sys->p_header = realloc( p_sys->p_header, p_sys->i_header_size );

        SetDWBE( &p_sys->p_header[4], ++p_sys->i_streams );

        memcpy(  &p_sys->p_header[p_sys->i_header_pos], "auds", 4 );
        memcpy(  &p_sys->p_header[p_sys->i_header_pos+4], "araw", 4 );
        SetDWBE( &p_sys->p_header[p_sys->i_header_pos+8],
                 p_sys->b_stereo ? 2 : 1 );
        SetDWBE( &p_sys->p_header[p_sys->i_header_pos+12],
                 p_sys->i_sample_rate );
        SetDWBE( &p_sys->p_header[p_sys->i_header_pos+16], 16 );

        p_sys->i_header_pos = p_sys->i_header_size;
    }
    p_sys->i_header_pos = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParseMRL: parse the options contained in the MRL
 *****************************************************************************/
static void ParseMRL( input_thread_t *p_input )
{
    access_sys_t *p_sys = p_input->p_access_data;

    char *psz_dup = strdup( p_input->psz_name );
    char *psz_parser = psz_dup;

    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }

    if( *psz_parser == ':' )
    {
        /* read options */
        for( ;; )
        {
            *psz_parser++ = '\0';
            if( !strncmp( psz_parser, "channel=", strlen( "channel=" ) ) )
            {
                p_sys->i_channel = strtol( psz_parser + strlen( "channel=" ),
                                           &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "norm=", strlen( "norm=" ) ) )
            {
                psz_parser += strlen( "norm=" );
                if( !strncmp( psz_parser, "pal", strlen( "pal" ) ) )
                {
                    p_sys->i_norm = VIDEO_MODE_PAL;
                    psz_parser += strlen( "pal" );
                }
                else if( !strncmp( psz_parser, "ntsc", strlen( "ntsc" ) ) )
                {
                    p_sys->i_norm = VIDEO_MODE_NTSC;
                    psz_parser += strlen( "ntsc" );
                }
                else if( !strncmp( psz_parser, "secam", strlen( "secam" ) ) )
                {
                    p_sys->i_norm = VIDEO_MODE_SECAM;
                    psz_parser += strlen( "secam" );
                }
                else if( !strncmp( psz_parser, "auto", strlen( "auto" ) ) )
                {
                    p_sys->i_norm = VIDEO_MODE_AUTO;
                    psz_parser += strlen( "auto" );
                }
                else
                {
                    p_sys->i_norm = strtol( psz_parser, &psz_parser, 0 );
                }
            }
            else if( !strncmp( psz_parser, "frequency=",
                               strlen( "frequency=" ) ) )
            {
                p_sys->i_frequency =
                    strtol( psz_parser + strlen( "frequency=" ),
                            &psz_parser, 0 );
                if( p_sys->i_frequency < 30000 )
                {
                    msg_Warn( p_input, "v4l syntax has changed : "
                              "'frequency' is now channel frequency in kHz");
                }
            }
            else if( !strncmp( psz_parser, "audio=", strlen( "audio=" ) ) )
            {
                p_sys->i_audio = strtol( psz_parser + strlen( "audio=" ),
                                         &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "size=", strlen( "size=" ) ) )
            {
                psz_parser += strlen( "size=" );
                if( !strncmp( psz_parser, "subqcif", strlen( "subqcif" ) ) )
                {
                    p_sys->i_width  = 128;
                    p_sys->i_height = 96;
                }
                else if( !strncmp( psz_parser, "qsif", strlen( "qsif" ) ) )
                {
                    p_sys->i_width  = 160;
                    p_sys->i_height = 120;
                }
                else if( !strncmp( psz_parser, "qcif", strlen( "qcif" ) ) )
                {
                    p_sys->i_width  = 176;
                    p_sys->i_height = 144;
                }
                else if( !strncmp( psz_parser, "sif", strlen( "sif" ) ) )
                {
                    p_sys->i_width  = 320;
                    p_sys->i_height = 244;
                }
                else if( !strncmp( psz_parser, "cif", strlen( "cif" ) ) )
                {
                    p_sys->i_width  = 352;
                    p_sys->i_height = 288;
                }
                else if( !strncmp( psz_parser, "vga", strlen( "vga" ) ) )
                {
                    p_sys->i_width  = 640;
                    p_sys->i_height = 480;
                }
                else
                {
                    /* widthxheight */
                    p_sys->i_width = strtol( psz_parser, &psz_parser, 0 );
                    if( *psz_parser == 'x' || *psz_parser == 'X')
                    {
                        p_sys->i_height = strtol( psz_parser + 1,
                                                  &psz_parser, 0 );
                    }
                    msg_Dbg( p_input, "WxH %dx%d", p_sys->i_width,
                             p_sys->i_height );
                }
            }
            else if( !strncmp( psz_parser, "tuner=", strlen( "tuner=" ) ) )
            {
                p_sys->i_tuner = strtol( psz_parser + strlen( "tuner=" ),
                                         &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "adev=", strlen( "adev=" ) ) )
            {
                int  i_len;

                psz_parser += strlen( "adev=" );
                if( strchr( psz_parser, ':' ) )
                {
                    i_len = strchr( psz_parser, ':' ) - psz_parser;
                }
                else
                {
                    i_len = strlen( psz_parser );
                }

                p_sys->psz_adev = strndup( psz_parser, i_len );

                psz_parser += i_len;
            }
            else if( !strncmp( psz_parser, "samplerate=",
                               strlen( "samplerate=" ) ) )
            {
                p_sys->i_sample_rate =
                    strtol( psz_parser + strlen( "samplerate=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "stereo", strlen( "stereo" ) ) )
            {
                psz_parser += strlen( "stereo" );

                p_sys->b_stereo = VLC_TRUE;
            }
            else if( !strncmp( psz_parser, "mono", strlen( "mono" ) ) )
            {
                psz_parser += strlen( "mono" );

                p_sys->b_stereo = VLC_FALSE;
            }
            else if( !strncmp( psz_parser, "mjpeg", strlen( "mjpeg" ) ) )
            {
                psz_parser += strlen( "mjpeg" );

                p_sys->b_mjpeg = VLC_TRUE;
            }
            else if( !strncmp( psz_parser, "decimation=",
                        strlen( "decimation=" ) ) )
            {
                p_sys->i_decimation =
                    strtol( psz_parser + strlen( "decimation=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "quality=",
                        strlen( "quality=" ) ) )
            {
                p_sys->i_quality =
                    strtol( psz_parser + strlen( "quality=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "fps=", strlen( "fps=" ) ) )
            {
                p_sys->f_fps = strtof( psz_parser + strlen( "fps=" ),
                                       &psz_parser );
            }
            else
            {
                msg_Warn( p_input, "unknown option" );
            }

            while( *psz_parser && *psz_parser != ':' )
            {
                psz_parser++;
            }

            if( *psz_parser == '\0' )
            {
                break;
            }
        }
    }

    if( *psz_dup )
    {
        p_sys->psz_device = strdup( psz_dup );
    }
}

/*****************************************************************************
 * OpenVideoDev:
 *****************************************************************************/
int OpenVideoDev( input_thread_t *p_input, char *psz_device )
{
    access_sys_t *p_sys = p_input->p_access_data;
    int i_fd;

    struct video_channel vid_channel;
    struct mjpeg_params mjpeg;
    int i;

    if( ( i_fd = open( psz_device, O_RDWR ) ) < 0 )
    {
        msg_Err( p_input, "cannot open device (%s)", strerror( errno ) );
        goto vdev_failed;
    }

    if( ioctl( i_fd, VIDIOCGCAP, &p_sys->vid_cap ) < 0 )
    {
        msg_Err( p_input, "cannot get capabilities (%s)", strerror( errno ) );
        goto vdev_failed;
    }

    msg_Dbg( p_input,
             "V4L device %s %d channels %d audios %d < w < %d %d < h < %d",
             p_sys->vid_cap.name,
             p_sys->vid_cap.channels,
             p_sys->vid_cap.audios,
             p_sys->vid_cap.minwidth,  p_sys->vid_cap.maxwidth,
             p_sys->vid_cap.minheight, p_sys->vid_cap.maxheight );

    if( p_sys->i_channel < 0 || p_sys->i_channel >= p_sys->vid_cap.channels )
    {
        msg_Dbg( p_input, "invalid channel, falling back on channel 0" );
        p_sys->i_channel = 0;
    }
    if( p_sys->i_audio >= p_sys->vid_cap.audios )
    {
        msg_Dbg( p_input, "invalid audio, falling back with no audio" );
        p_sys->i_audio = -1;
    }

    if( p_sys->i_width < p_sys->vid_cap.minwidth ||
        p_sys->i_width > p_sys->vid_cap.maxwidth )
    {
        msg_Dbg( p_input, "invalid width %i", p_sys->i_width );
        p_sys->i_width = 0;
    }
    if( p_sys->i_height < p_sys->vid_cap.minheight ||
        p_sys->i_height > p_sys->vid_cap.maxheight )
    {
        msg_Dbg( p_input, "invalid height %i", p_sys->i_height );
        p_sys->i_height = 0;
    }

    if( !( p_sys->vid_cap.type & VID_TYPE_CAPTURE ) )
    {
        msg_Err( p_input, "cannot grab" );
        goto vdev_failed;
    }

    vid_channel.channel = p_sys->i_channel;
    if( ioctl( i_fd, VIDIOCGCHAN, &vid_channel ) < 0 )
    {
        msg_Err( p_input, "cannot get channel infos (%s)",
                          strerror( errno ) );
        goto vdev_failed;
    }
    msg_Dbg( p_input,
             "setting channel %s(%d) %d tuners flags=0x%x type=0x%x norm=0x%x",
             vid_channel.name,
             vid_channel.channel,
             vid_channel.tuners,
             vid_channel.flags,
             vid_channel.type,
             vid_channel.norm );

    if( p_sys->i_tuner >= vid_channel.tuners )
    {
        msg_Dbg( p_input, "invalid tuner, falling back on tuner 0" );
        p_sys->i_tuner = 0;
    }

    vid_channel.norm = p_sys->i_norm;
    if( ioctl( i_fd, VIDIOCSCHAN, &vid_channel ) < 0 )
    {
        msg_Err( p_input, "cannot set channel (%s)", strerror( errno ) );
        goto vdev_failed;
    }

    if( vid_channel.flags & VIDEO_VC_TUNER )
    {

        /* set tuner */
#if 0
        struct video_tuner vid_tuner;
        if( p_sys->i_tuner >= 0 )
        {
            vid_tuner.tuner = p_sys->i_tuner;
            if( ioctl( i_fd, VIDIOCGTUNER, &vid_tuner ) < 0 )
            {
                msg_Err( p_input, "cannot get tuner (%s)", strerror( errno ) );
                goto vdev_failed;
            }
            msg_Dbg( p_input, "tuner %s low=%d high=%d, flags=0x%x "
                     "mode=0x%x signal=0x%x",
                     vid_tuner.name, vid_tuner.rangelow, vid_tuner.rangehigh,
                     vid_tuner.flags, vid_tuner.mode, vid_tuner.signal );

            msg_Dbg( p_input, "setting tuner %s (%d)",
                     vid_tuner.name, vid_tuner.tuner );

            /* FIXME FIXME to be checked FIXME FIXME */
            //vid_tuner.mode = p_sys->i_norm;
            if( ioctl( i_fd, VIDIOCSTUNER, &vid_tuner ) < 0 )
            {
                msg_Err( p_input, "cannot set tuner (%s)", strerror( errno ) );
                goto vdev_failed;
            }
        }
#endif

        /* Show a warning if frequency is < than 30000.
         * User is certainly usint old syntax. */


        /* set frequency */
        if( p_sys->i_frequency >= 0 )
        {
            int driver_frequency = p_sys->i_frequency * 16 /1000;
            if( ioctl( i_fd, VIDIOCSFREQ, &driver_frequency ) < 0 )
            {
                msg_Err( p_input, "cannot set frequency (%s)",
                                  strerror( errno ) );
                goto vdev_failed;
            }
            msg_Dbg( p_input, "frequency %d (%d)", p_sys->i_frequency,
                                                   driver_frequency );
        }
    }

    /* set audio */
    if( vid_channel.flags & VIDEO_VC_AUDIO )
    {
        struct video_audio      vid_audio;

        /* XXX TODO volume, balance, ... */
        if( p_sys->i_audio >= 0 )
        {
            vid_audio.audio = p_sys->i_audio;
            if( ioctl( i_fd, VIDIOCGAUDIO, &vid_audio ) < 0 )
            {
                msg_Err( p_input, "cannot get audio (%s)", strerror( errno ) );
                goto vdev_failed;
            }

            /* unmute audio */
            vid_audio.flags &= ~VIDEO_AUDIO_MUTE;

            if( ioctl( i_fd, VIDIOCSAUDIO, &vid_audio ) < 0 )
            {
                msg_Err( p_input, "cannot set audio (%s)", strerror( errno ) );
                goto vdev_failed;
            }
        }

    }

    /* establish basic params with input and norm before feeling width
     * or height */
    if( p_sys->b_mjpeg )
    {
        struct quicktime_mjpeg_app1 *p_app1;
        int32_t i_offset;

        if( ioctl( i_fd, MJPIOC_G_PARAMS, &mjpeg ) < 0 )
        {
            msg_Err( p_input, "cannot get mjpeg params (%s)",
                              strerror( errno ) );
            goto vdev_failed;
        }
        mjpeg.input = p_sys->i_channel;
        mjpeg.norm  = p_sys->i_norm;
        mjpeg.decimation = p_sys->i_decimation;

        if( p_sys->i_width )
            mjpeg.img_width = p_sys->i_width / p_sys->i_decimation;
        if( p_sys->i_height )
            mjpeg.img_height = p_sys->i_height / p_sys->i_decimation;

        /* establish Quicktime APP1 marker while we are here */
        mjpeg.APPn = 1;
        mjpeg.APP_len = 40;

        /* aligned */
        p_app1 = (struct quicktime_mjpeg_app1 *)mjpeg.APP_data;
        p_app1->i_reserved = 0;
        p_app1->i_tag = VLC_FOURCC( 'm','j','p','g' );
        p_app1->i_field_size = 0;
        p_app1->i_padded_field_size = 0;
        p_app1->i_next_field = 0;
        /* XXX WARNING XXX */
        /* these's nothing magic about these values.  We are dangerously
         * assuming the encoder card is encoding mjpeg-a and is not throwing
         * in marker tags we aren't expecting.  It's bad enough we have to
         * search through the jpeg output for every frame we grab just to
         * find the first field's end marker, so we take this risk to boost
         * performance.
         * This is really something the driver could do for us because this
         * does conform to standards outside of Apple Quicktime.
         */
        i_offset = 0x2e;
        p_app1->i_DQT_offset = hton32( i_offset );
        i_offset = 0xb4;
        p_app1->i_DHT_offset = hton32( i_offset );
        i_offset = 0x258;
        p_app1->i_SOF_offset = hton32( i_offset );
        i_offset = 0x26b;
        p_app1->i_SOS_offset = hton32( i_offset );
        i_offset = 0x279;
        p_app1->i_data_offset = hton32( i_offset );

        /* SOF and SOS aren't specified by the mjpeg API because they aren't
         * optional.  They will be present in the output. */
        mjpeg.jpeg_markers = JPEG_MARKER_DHT | JPEG_MARKER_DQT;

        if( ioctl( i_fd, MJPIOC_S_PARAMS, &mjpeg ) < 0 )
        {
            msg_Err( p_input, "cannot set mjpeg params (%s)",
                              strerror( errno ) );
            goto vdev_failed;
        }

        p_sys->i_width = mjpeg.img_width * mjpeg.HorDcm;
        p_sys->i_height = mjpeg.img_height * mjpeg.VerDcm *
            mjpeg.field_per_buff;
    }

    /* fix width/height */
    if( !p_sys->b_mjpeg && ( p_sys->i_width == 0 || p_sys->i_height == 0 ) )
    {
        struct video_window vid_win;

        if( ioctl( i_fd, VIDIOCGWIN, &vid_win ) < 0 )
        {
            msg_Err( p_input, "cannot get win (%s)", strerror( errno ) );
            goto vdev_failed;
        }
        p_sys->i_width  = vid_win.width;
        p_sys->i_height = vid_win.height;

        msg_Dbg( p_input, "will use %dx%d", p_sys->i_width, p_sys->i_height );
    }

    p_sys->p_video_frame = NULL;

    if( !p_sys->b_mjpeg )
    {
        /* Find out video format used by device */
        if( ioctl( i_fd, VIDIOCGPICT, &p_sys->vid_picture ) == 0 )
        {
            int i_chroma = VLC_FOURCC( ' ', ' ', ' ', ' ' );
            struct video_picture vid_picture = p_sys->vid_picture;

            /* Try to set the format to something easy to encode */
            vid_picture.palette = VIDEO_PALETTE_YUV420P;
            if( ioctl( i_fd, VIDIOCSPICT, &vid_picture ) == 0 )
            {
                p_sys->vid_picture = vid_picture;
            }
            else
            {
                vid_picture.palette = VIDEO_PALETTE_YUV422P;
                if( ioctl( i_fd, VIDIOCSPICT, &vid_picture ) == 0 )
                {
                    p_sys->vid_picture = vid_picture;
                }
            }

            /* Find out final format */
            switch( p_sys->vid_picture.palette )
            {
            case VIDEO_PALETTE_GREY:
                i_chroma = VLC_FOURCC( 'G', 'R', 'E', 'Y' );
                break;
            case VIDEO_PALETTE_HI240:
                i_chroma = VLC_FOURCC( 'I', '2', '4', '0' );
                break;
            case VIDEO_PALETTE_RGB565:
                i_chroma = VLC_FOURCC( 'R', 'V', '1', '6' );
                break;
            case VIDEO_PALETTE_RGB555:
                i_chroma = VLC_FOURCC( 'R', 'V', '1', '5' );
                break;
            case VIDEO_PALETTE_RGB24:
                i_chroma = VLC_FOURCC( 'R', 'V', '2', '4' );
                break;
            case VIDEO_PALETTE_RGB32:
                i_chroma = VLC_FOURCC( 'R', 'V', '3', '2' );
                break;
            case VIDEO_PALETTE_YUV422:
                i_chroma = VLC_FOURCC( 'I', '4', '2', '2' );
                break;
            case VIDEO_PALETTE_YUYV:
                i_chroma = VLC_FOURCC( 'Y', 'U', 'Y', 'V' );
                break;
            case VIDEO_PALETTE_UYVY:
                i_chroma = VLC_FOURCC( 'U', 'Y', 'V', 'Y' );
                break;
            case VIDEO_PALETTE_YUV420:
                i_chroma = VLC_FOURCC( 'I', '4', '2', 'N' );
                break;
            case VIDEO_PALETTE_YUV411:
                i_chroma = VLC_FOURCC( 'I', '4', '1', 'N' );
                break;
            case VIDEO_PALETTE_RAW:
                i_chroma = VLC_FOURCC( 'G', 'R', 'A', 'W' );
                break;
            case VIDEO_PALETTE_YUV422P:
                i_chroma = VLC_FOURCC( 'I', '4', '2', '2' );
                break;
            case VIDEO_PALETTE_YUV420P:
                i_chroma = VLC_FOURCC( 'I', '4', '2', '0' );
                break;
            case VIDEO_PALETTE_YUV411P:
                i_chroma = VLC_FOURCC( 'I', '4', '1', '1' );
                break;
            }
            p_sys->i_fourcc = i_chroma;
        }
        else
        {
            msg_Err( p_input, "ioctl VIDIOCGPICT failed" );
            goto vdev_failed;
        }
    }

    if( p_sys->b_mjpeg )
    {
        int i;

        p_sys->mjpeg_buffers.count = 8;
        p_sys->mjpeg_buffers.size = MJPEG_BUFFER_SIZE;

        if( ioctl( i_fd, MJPIOC_REQBUFS, &p_sys->mjpeg_buffers ) < 0 )
        {
            msg_Err( p_input, "mmap unsupported" );
            goto vdev_failed;
        }

        p_sys->p_video_mmap = mmap( 0,
                p_sys->mjpeg_buffers.size * p_sys->mjpeg_buffers.count,
                PROT_READ | PROT_WRITE, MAP_SHARED, i_fd, 0 );
        if( p_sys->p_video_mmap == MAP_FAILED )
        {
            msg_Err( p_input, "mmap failed" );
            goto vdev_failed;
        }

        p_sys->i_fourcc  = VLC_FOURCC( 'm','j','p','g' );
        p_sys->i_frame_pos = -1;

        /* queue up all the frames */
        for( i = 0; i < (int)p_sys->mjpeg_buffers.count; i++ )
        {
            if( ioctl( i_fd, MJPIOC_QBUF_CAPT, &i ) < 0 )
            {
                msg_Err( p_input, "unable to queue frame" );
                goto vdev_failed;
            }
        }
    }
    else
    {
        /* Fill in picture_t fields */
        vout_InitPicture( VLC_OBJECT(p_input), &p_sys->pic, p_sys->i_fourcc,
                          p_sys->i_width, p_sys->i_height, p_sys->i_width *
                          VOUT_ASPECT_FACTOR / p_sys->i_height );
        if( !p_sys->pic.i_planes )
        {
            msg_Err( p_input, "unsupported chroma" );
            goto vdev_failed;
        }
        p_sys->i_video_frame_size = 0;
        for( i = 0; i < p_sys->pic.i_planes; i++ )
        {
            p_sys->i_video_frame_size += p_sys->pic.p[i].i_lines *
              p_sys->pic.p[i].i_visible_pitch;
        }

        msg_Dbg( p_input, "v4l device uses frame size: %i",
                 p_sys->i_video_frame_size );
        msg_Dbg( p_input, "v4l device uses chroma: %4.4s",
                (char*)&p_sys->i_fourcc );

        /* Allocate mmap buffer */
        if( ioctl( i_fd, VIDIOCGMBUF, &p_sys->vid_mbuf ) < 0 )
        {
            msg_Err( p_input, "mmap unsupported" );
            goto vdev_failed;
        }

        p_sys->p_video_mmap = mmap( 0, p_sys->vid_mbuf.size,
                                    PROT_READ|PROT_WRITE, MAP_SHARED,
                                    i_fd, 0 );
        if( p_sys->p_video_mmap == MAP_FAILED )
        {
            /* FIXME -> normal read */
            msg_Err( p_input, "mmap failed" );
            goto vdev_failed;
        }

        /* init grabbing */
        p_sys->vid_mmap.frame  = 0;
        p_sys->vid_mmap.width  = p_sys->i_width;
        p_sys->vid_mmap.height = p_sys->i_height;
        p_sys->vid_mmap.format = p_sys->vid_picture.palette;
        if( ioctl( i_fd, VIDIOCMCAPTURE, &p_sys->vid_mmap ) < 0 )
        {
            msg_Warn( p_input, "%4.4s refused", (char*)&p_sys->i_fourcc );
            msg_Err( p_input, "chroma selection failed" );
            goto vdev_failed;
        }
    }
    return i_fd;

vdev_failed:

    if( i_fd >= 0 ) close( i_fd );
    return -1;
}

/*****************************************************************************
 * OpenAudioDev:
 *****************************************************************************/
int OpenAudioDev( input_thread_t *p_input, char *psz_device )
{
    access_sys_t *p_sys = p_input->p_access_data;
    int i_fd, i_format;

    if( (i_fd = open( psz_device, O_RDONLY | O_NONBLOCK )) < 0 )
    {
        msg_Err( p_input, "cannot open audio device (%s)", strerror( errno ) );
        goto adev_fail;
    }

    i_format = AFMT_S16_LE;
    if( ioctl( i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0
        || i_format != AFMT_S16_LE )
    {
        msg_Err( p_input, "cannot set audio format (16b little endian) "
                 "(%s)", strerror( errno ) );
        goto adev_fail;
    }

    if( ioctl( i_fd, SNDCTL_DSP_STEREO,
               &p_sys->b_stereo ) < 0 )
    {
        msg_Err( p_input, "cannot set audio channels count (%s)",
                 strerror( errno ) );
        goto adev_fail;
    }

    if( ioctl( i_fd, SNDCTL_DSP_SPEED,
               &p_sys->i_sample_rate ) < 0 )
    {
        msg_Err( p_input, "cannot set audio sample rate (%s)",
                 strerror( errno ) );
        goto adev_fail;
    }

    msg_Dbg( p_input, "openened adev=`%s' %s %dHz",
             psz_device, p_sys->b_stereo ? "stereo" : "mono",
             p_sys->i_sample_rate );

    p_sys->i_audio_frame_size = 0;
    p_sys->i_audio_frame_size_allocated = 6*1024;
    p_sys->p_audio_frame = malloc( p_sys->i_audio_frame_size_allocated );

    return i_fd;

 adev_fail:

    if( i_fd >= 0 ) close( i_fd );
    return -1;
}

/*****************************************************************************
 * AccessClose: close device, free resources
 *****************************************************************************/
static void AccessClose( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys   = p_input->p_access_data;

    if( p_sys->psz_device ) free( p_sys->psz_device );
    if( p_sys->psz_vdev )   free( p_sys->psz_vdev );
    if( p_sys->psz_adev )   free( p_sys->psz_adev );
    if( p_sys->fd_video >= 0 ) close( p_sys->fd_video );
    if( p_sys->fd_audio >= 0 ) close( p_sys->fd_audio );

    if( p_sys->p_header ) free( p_sys->p_header );
    if( p_sys->p_audio_frame ) free( p_sys->p_audio_frame );

    if( p_sys->b_mjpeg )
    {
        int i_noframe = -1;
        ioctl( p_sys->fd_video, MJPIOC_QBUF_CAPT, &i_noframe );
    }

    if( p_sys->p_video_mmap && p_sys->p_video_mmap != MAP_FAILED )
    {
        if( p_sys->b_mjpeg )
            munmap( p_sys->p_video_mmap, p_sys->mjpeg_buffers.size *
                    p_sys->mjpeg_buffers.count );
        else
            munmap( p_sys->p_video_mmap, p_sys->vid_mbuf.size );
    }

    free( p_sys );
}

/*****************************************************************************
 * GrabAudio: grab audio
 *****************************************************************************/
static int GrabAudio( input_thread_t * p_input,
                      uint8_t **pp_data,
                      int      *pi_data,
                      mtime_t  *pi_pts )
{
    access_sys_t    *p_sys   = p_input->p_access_data;
    struct audio_buf_info buf_info;
    int i_read;
    int i_correct;

    i_read = read( p_sys->fd_audio, p_sys->p_audio_frame,
                   p_sys->i_audio_frame_size_allocated );

    if( i_read <= 0 )
    {
        return VLC_EGENERIC;
    }

    p_sys->i_audio_frame_size = i_read;

    /* from vls : correct the date because of kernel buffering */
    i_correct = i_read;
    if( ioctl( p_sys->fd_audio, SNDCTL_DSP_GETISPACE, &buf_info ) == 0 )
    {
        i_correct += buf_info.bytes;
    }


    *pp_data = p_sys->p_audio_frame;
    *pi_data = p_sys->i_audio_frame_size;
    *pi_pts  = mdate() - (mtime_t)1000000 * (mtime_t)i_correct /
                         2 / ( p_sys->b_stereo ? 2 : 1) / p_sys->i_sample_rate;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * GrabVideo:
 *****************************************************************************/
static uint8_t *GrabCapture( input_thread_t *p_input )
{
    access_sys_t *p_sys = p_input->p_access_data;
    p_sys->vid_mmap.frame = ( p_sys->i_frame_pos + 1 ) %
                            p_sys->vid_mbuf.frames;
    for( ;; )
    {
        if( ioctl( p_sys->fd_video, VIDIOCMCAPTURE, &p_sys->vid_mmap ) >= 0 )
        {
            break;
        }

        if( errno != EAGAIN )
        {
            msg_Err( p_input, "failed while grabbing new frame" );
            return( NULL );
        }
        msg_Dbg( p_input, "another try ?" );
    }

    //msg_Warn( p_input, "grab a new frame" );

    while( ioctl(p_sys->fd_video, VIDIOCSYNC, &p_sys->i_frame_pos) < 0 &&
           ( errno == EAGAIN || errno == EINTR ) );

    p_sys->i_frame_pos = p_sys->vid_mmap.frame;
    /* leave i_video_frame_size alone */
    return p_sys->p_video_mmap + p_sys->vid_mbuf.offsets[p_sys->i_frame_pos];
}

static uint8_t *GrabMJPEG( input_thread_t *p_input )
{
    access_sys_t *p_sys = p_input->p_access_data;
    struct mjpeg_sync sync;
    uint8_t *p_frame, *p_field, *p;
    uint16_t tag;
    uint32_t i_size;
    struct quicktime_mjpeg_app1 *p_app1 = NULL;

    /* re-queue the last frame we sync'd */
    if( p_sys->i_frame_pos != -1 )
        while( ioctl( p_sys->fd_video, MJPIOC_QBUF_CAPT, &p_sys->i_frame_pos ) < 0 &&
                ( errno == EAGAIN || errno == EINTR ) );

    /* sync on the next frame */
    while( ioctl( p_sys->fd_video, MJPIOC_SYNC, &sync ) < 0 &&
            ( errno == EAGAIN || errno == EINTR ) );

    p_sys->i_frame_pos = sync.frame;
    p_frame = p_sys->p_video_mmap + p_sys->mjpeg_buffers.size * sync.frame;

    /* p_frame now points to the data.  fix up the Quicktime APP1 marker */
    tag = 0xffd9;
    tag = hton16( tag );
    p_field = p_frame;

    /* look for EOI */
    p = memmem( p_field, sync.length, &tag, 2 );

    if( p )
    {
        p += 2; /* data immediately following EOI */
        /* UNALIGNED! */
        p_app1 = (struct quicktime_mjpeg_app1 *)(p_field + 6);

        i_size = ((uint32_t)(p - p_field));
        i_size = hton32( i_size );
        memcpy( &p_app1->i_field_size, &i_size, 4 );

        while( *p == 0xff && *(p+1) == 0xff )
            p++;

        i_size = ((uint32_t)(p - p_field));
        i_size = hton32( i_size );
        memcpy( &p_app1->i_padded_field_size, &i_size, 4 );
    }

    tag = 0xffd8;
    tag = hton16( tag );
    p_field = memmem( p, sync.length - (size_t)(p - p_frame), &tag, 2 );

    if( p_field )
    {
        i_size = (uint32_t)(p_field - p_frame);
        i_size = hton32( i_size );
        memcpy( &p_app1->i_next_field, &i_size, 4 );

        /* UNALIGNED! */
        p_app1 = (struct quicktime_mjpeg_app1 *)(p_field + 6);
        tag = 0xffd9;
        tag = hton16( tag );
        p = memmem( p_field, sync.length - (size_t)(p_field - p_frame),
                &tag, 2 );

        if( !p )
        {
            /* sometimes the second field doesn't have the EOI.  just put it
             * there
             */
            p = p_frame + sync.length;
            memcpy( p, &tag, 2 );
            sync.length += 2;
        }

        p += 2;
        i_size = (uint32_t)(p - p_field);
        i_size = hton32( i_size );
        memcpy( &p_app1->i_field_size, &i_size, 4 );
        i_size = (uint32_t)(sync.length - (uint32_t)(p_field - p_frame));
        i_size = hton32( i_size );
        memcpy( &p_app1->i_padded_field_size, &i_size, 4 );
    }

    p_sys->i_video_frame_size = sync.length;
    return p_frame;
}

static int GrabVideo( input_thread_t * p_input,
                      uint8_t **pp_data,
                      int *pi_data,
                      mtime_t  *pi_pts )
{
    access_sys_t *p_sys   = p_input->p_access_data;
    uint8_t      *p_frame;


    if( p_sys->f_fps >= 0.1 && p_sys->i_video_pts > 0 )
    {
        mtime_t i_dur = (mtime_t)((double)1000000 / (double)p_sys->f_fps);

        /* Dif we wait long enougth ? */
        if( p_sys->i_video_pts + i_dur > mdate() )
        {
            return VLC_EGENERIC;
        }
    }

    if( p_sys->b_mjpeg )
        p_frame = GrabMJPEG( p_input );
    else
        p_frame = GrabCapture( p_input );

    if( !p_frame )
        return VLC_EGENERIC;

    p_sys->i_video_pts   = mdate();
    p_sys->p_video_frame = p_frame;

    *pp_data = p_sys->p_video_frame;
    *pi_data = p_sys->i_video_frame_size;
    *pi_pts  = p_sys->i_video_pts;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read: reads from the device into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_input->p_access_data;
    int          i_data = 0;
    int          i_stream;
    mtime_t      i_pts;

    while( i_len > 0 )
    {
        /* First copy header if any */
        if( p_sys->i_header_pos < p_sys->i_header_size )
        {
            int i_copy;

            i_copy = __MIN( p_sys->i_header_size - p_sys->i_header_pos,
                            (int)i_len );
            memcpy( p_buffer, &p_sys->p_header[p_sys->i_header_pos], i_copy );
            p_sys->i_header_pos += i_copy;

            p_buffer += i_copy;
            i_len -= i_copy;
            i_data += i_copy;
        }

        /* then data */
        if( i_len > 0 && p_sys->i_data_pos < p_sys->i_data_size )
        {
            int i_copy;

            i_copy = __MIN( p_sys->i_data_size - p_sys->i_data_pos,
                            (int)i_len );

            memcpy( p_buffer, &p_sys->p_data[p_sys->i_data_pos], i_copy );
            p_sys->i_data_pos += i_copy;

            p_buffer += i_copy;
            i_len -= i_copy;
            i_data += i_copy;
        }

        /* The caller got what he wanted */
        if( i_len == 0 )
        {
            return i_data;
        }

        /* Read no more than one frame at a time.
         * That kills latency, especially for encoded v4l streams */
        if( p_sys->i_data_size && p_sys->i_data_pos == p_sys->i_data_size )
        {
            p_sys->i_data_pos = 0; p_sys->i_data_size = 0;
            return i_data;
        }

        /* Re-fill data by grabbing audio/video */
        p_sys->i_data_pos = p_sys->i_data_size = 0;

        /* Try grabbing audio frames first */
        i_stream = p_sys->i_streams - 1;
        if( p_sys->fd_audio < 0 ||
            GrabAudio( p_input, &p_sys->p_data,
                       &p_sys->i_data_size, &i_pts ) != VLC_SUCCESS )
        {
            /* Try grabbing video frame */
            i_stream = 0;
            if( p_sys->fd_video < 0 ||
                GrabVideo( p_input, &p_sys->p_data,
                           &p_sys->i_data_size, &i_pts ) != VLC_SUCCESS )
            {
                /* Sleep so we do not consume all the cpu, 10ms seems
                 * like a good value (100fps) */
                msleep( 10000 );
                continue;
            }
        }

        /* create pseudo header */
        p_sys->i_header_size = 16;
        p_sys->i_header_pos  = 0;
        SetDWBE( &p_sys->p_header[0], i_stream );
        SetDWBE( &p_sys->p_header[4], p_sys->i_data_size );
        SetQWBE( &p_sys->p_header[8], i_pts );
    }

    return i_data;
}

/****************************************************************************
 * I. Demux Part
 ****************************************************************************/
static int DemuxOpen( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    uint8_t        *p_peek;
    int            i_streams;
    int            i;

    data_packet_t  *p_pk;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE ;
    }

    /* a little test to see if it's a v4l stream */
    if( input_Peek( p_input, &p_peek, 8 ) < 8 )
    {
        msg_Warn( p_input, "v4l plugin discarded (cannot peek)" );
        return VLC_EGENERIC;
    }

    if( strncmp( p_peek, ".v4l", 4 ) || GetDWBE( &p_peek[4] ) <= 0 )
    {
        msg_Warn( p_input, "v4l plugin discarded (not a valid stream)" );
        return VLC_EGENERIC;
    }

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        return( VLC_EGENERIC );
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        return VLC_EGENERIC;
    }

    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    p_input->stream.i_mux_rate =  0;

    i_streams = GetDWBE( &p_peek[4] );
    if( input_Peek( p_input, &p_peek, 8 + 20 * i_streams )
        < 8 + 20 * i_streams )
    {
        msg_Err( p_input, "v4l plugin discarded (cannot peek)" );
        return VLC_EGENERIC;
    }
    p_peek += 8;

    for( i = 0; i < i_streams; i++ )
    {
        es_descriptor_t *p_es;

        if( !strncmp( p_peek, "auds", 4 ) )
        {
#define wf ((WAVEFORMATEX*)p_es->p_waveformatex)
            p_es = input_AddES( p_input, p_input->stream.pp_programs[0],
                                i + 1, AUDIO_ES, NULL, 0 );
            p_es->i_stream_id   = i + 1;
            p_es->i_fourcc      =
                VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] );

            p_es->p_waveformatex= malloc( sizeof( WAVEFORMATEX ) );

            wf->wFormatTag      = WAVE_FORMAT_UNKNOWN;
            wf->nChannels       = GetDWBE( &p_peek[8] );
            wf->nSamplesPerSec  = GetDWBE( &p_peek[12] );
            wf->wBitsPerSample  = GetDWBE( &p_peek[16] );
            wf->nBlockAlign     = wf->wBitsPerSample * wf->nChannels / 8;
            wf->nAvgBytesPerSec = wf->nBlockAlign * wf->nSamplesPerSec;
            wf->cbSize          = 0;

            msg_Dbg( p_input, "added new audio es %d channels %dHz",
                     wf->nChannels, wf->nSamplesPerSec );

            input_SelectES( p_input, p_es );
#undef wf
        }
        else if( !strncmp( p_peek, "vids", 4 ) )
        {
#define bih ((BITMAPINFOHEADER*)p_es->p_bitmapinfoheader)
            p_es = input_AddES( p_input, p_input->stream.pp_programs[0],
                                i + 1, VIDEO_ES, NULL, 0 );
            p_es->i_stream_id   = i + 1;
            p_es->i_fourcc  =
                VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] );

            p_es->p_bitmapinfoheader = malloc( sizeof( BITMAPINFOHEADER ) );

            bih->biSize     = sizeof( BITMAPINFOHEADER );
            bih->biWidth    = GetDWBE( &p_peek[8] );
            bih->biHeight   = GetDWBE( &p_peek[12] );
            bih->biPlanes   = 0;
            bih->biBitCount = 0;
            bih->biCompression      = 0;
            bih->biSizeImage= 0;
            bih->biXPelsPerMeter    = 0;
            bih->biYPelsPerMeter    = 0;
            bih->biClrUsed  = 0;
            bih->biClrImportant     = 0;

            msg_Dbg( p_input, "added new video es %4.4s %dx%d",
                     (char*)&p_es->i_fourcc, bih->biWidth, bih->biHeight );

            input_SelectES( p_input, p_es );
#undef bih
        }

        p_peek += 20;
    }

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( input_SplitBuffer( p_input, &p_pk, 8 + i_streams * 20 ) > 0 )
    {
        input_DeletePacket( p_input->p_method_data, p_pk );
    }

    p_input->pf_demux = Demux;
    p_input->pf_demux_control = demux_vaControlDefault;
    return VLC_SUCCESS;
}

static void DemuxClose( vlc_object_t *p_this )
{
    return;
}

#define MAX_PACKETS_IN_FIFO 3

static int Demux( input_thread_t *p_input )
{
    es_descriptor_t *p_es;
    pes_packet_t    *p_pes;

    int i_stream;
    int i_size;

    uint8_t *p_peek;
    mtime_t i_pts;

    if( input_Peek( p_input, &p_peek, 16 ) < 16 )
    {
        msg_Warn( p_input, "cannot peek (EOF ?)" );
        return( 0 );
    }

    i_stream = GetDWBE( &p_peek[0] );
    i_size   = GetDWBE( &p_peek[4] );
    i_pts    = GetQWBE( &p_peek[8] );

    p_es = p_input->stream.p_selected_program->pp_es[i_stream];
    if( !p_es )
    {
        msg_Err( p_input, "cannot find ES" );
    }

    p_pes = input_NewPES( p_input->p_method_data );
    if( p_pes == NULL )
    {
        msg_Warn( p_input, "cannot allocate PES" );
        msleep( 1000 );
        return( 1 );
    }
    i_size += 16;
    while( i_size > 0 )
    {
        data_packet_t   *p_data;
        int i_read;

        if( (i_read = input_SplitBuffer( p_input, &p_data,
                                         __MIN( i_size, 10000 ) ) ) <= 0 )
        {
            input_DeletePES( p_input->p_method_data, p_pes );
            return( 0 );
        }
        if( !p_pes->p_first )
        {
            p_pes->p_first = p_data;
            p_pes->i_nb_data = 1;
            p_pes->i_pes_size = i_read;
        }
        else
        {
            p_pes->p_last->p_next  = p_data;
            p_pes->i_nb_data++;
            p_pes->i_pes_size += i_read;
        }
        p_pes->p_last  = p_data;
        i_size -= i_read;
    }
    p_pes->p_first->p_payload_start += 16;
    p_pes->i_pes_size               -= 16;

    if( p_es && p_es->p_dec )
    {
        p_pes->i_pts =
        p_pes->i_dts = i_pts + p_input->i_pts_delay;

        input_DecodePES( p_es->p_dec, p_pes );
    }
    else
    {
        input_DeletePES( p_input->p_method_data, p_pes );
    }

    return 1;
}
