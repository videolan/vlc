/*****************************************************************************
 * v4l.c : Video4Linux input module for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: v4l.c,v 1.7 2003/04/09 09:59:59 titer Exp $
 *
 * Author: Samuel Hocevar <sam@zoy.org>
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
#include "encoder.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#include <fcntl.h>
#include <linux/videodev.h>

/* enable audio grabbing */
#undef _V4L_AUDIO_

#ifdef _V4L_AUDIO_
    #include <sys/soundcard.h>
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  AccessOpen  ( vlc_object_t * );
static void AccessClose ( vlc_object_t * );
static int  Read        ( input_thread_t *, byte_t *, size_t );

static int  DemuxOpen  ( vlc_object_t * );
static void DemuxClose ( vlc_object_t * );
static int  Demux      ( input_thread_t * );


/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
#define CACHING_TEXT N_("caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for v4l streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    set_description( _("Video4Linux input") );
    add_category_hint( N_("v4l"), NULL, VLC_TRUE );
        add_integer( "v4l-caching", DEFAULT_PTS_DELAY / 1000, NULL,
                     CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_submodule();
        add_shortcut( "v4l" );
        set_capability( "access", 0 );
        set_callbacks( AccessOpen, AccessClose );
    add_submodule();
        add_shortcut( "v4l" );
        set_capability( "demux", 200 );
        set_callbacks( DemuxOpen, DemuxClose );
vlc_module_end();


/****************************************************************************
 * I. Access Part
 ****************************************************************************/
struct access_sys_t
{
    char    *psz_video_device;
    int     fd;

    vlc_fourcc_t    i_codec;   // if i_codec != i_chroma then we need a compressor
    video_encoder_t *p_encoder;
    picture_t       pic;

    int i_channel;
    int i_audio;
    int i_norm;
    int i_tuner;
    int i_frequency;
    int i_chroma;
    int i_width;
    int i_height;

    struct video_capability vid_cap;
    struct video_mbuf       vid_mbuf;

    uint8_t *p_video_mmap;
    int     i_frame_pos;
    struct video_mmap   vid_mmap;

    uint8_t *p_video_frame;
    int     i_video_frame_size;
    int     i_video_frame_size_allocated;

#ifdef _V4L_AUDIO_
    char         *psz_adev;
    int          fd_audio;
    vlc_fourcc_t i_acodec_raw;
    int          i_sample_rate;
    vlc_bool_t   b_stereo;

    uint8_t *p_audio_frame;
    int     i_audio_frame_size;
    int     i_audio_frame_size_allocated;

#endif

    /* header */
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

static void    SetDWBE( uint8_t *p, uint32_t dw )
{
    p[0] = (dw >> 24)&0xff;
    p[1] = (dw >> 16)&0xff;
    p[2] = (dw >>  8)&0xff;
    p[3] = (dw      )&0xff;
}

static uint32_t GetDWBE( uint8_t *p_buff )
{
    return( ( p_buff[0] << 24 ) + ( p_buff[1] << 16 ) +
            ( p_buff[2] <<  8 ) + p_buff[3] );
}

/*****************************************************************************
 * Open: open device:
 *****************************************************************************
 *
 * url: <video device>::::
 *
 *****************************************************************************/
static int AccessOpen( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys;
    char           *psz_dup, *psz_parser;

    struct video_channel    vid_channel;

    /* create access private data */
    p_sys = malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );

    p_sys->psz_video_device = NULL;
    p_sys->fd               = -1;
    p_sys->i_channel        = -1;
    p_sys->i_audio          = -1;
    p_sys->i_norm           = VIDEO_MODE_AUTO;    // auto
    p_sys->i_tuner          = -1;
    p_sys->i_frequency      = -1;
    p_sys->i_width          = 0;
    p_sys->i_height         = 0;

    p_sys->i_frame_pos = 0;

    p_sys->i_codec          = VLC_FOURCC( 0, 0, 0, 0 );
    p_sys->i_video_frame_size_allocated = 0;
#ifdef _V4L_AUDIO_
    p_sys->psz_adev         = NULL;
    p_sys->fd_audio         = -1;
    p_sys->i_sample_rate    = 44100;
    p_sys->b_stereo         = VLC_TRUE;
#endif

    p_sys->i_data_size = 0;
    p_sys->i_data_pos  = 0;
    p_sys->p_data      = NULL;

    /* parse url and open device(s) */
    psz_dup = strdup( p_input->psz_name );
    psz_parser = psz_dup;

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
                p_sys->i_channel =
                    strtol( psz_parser + strlen( "channel=" ), &psz_parser, 0 );
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
            else if( !strncmp( psz_parser, "frequency=", strlen( "frequency=" ) ) )
            {
                p_sys->i_frequency =
                    strtol( psz_parser + strlen( "frequency=" ), &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "audio=", strlen( "audio=" ) ) )
            {
                p_sys->i_audio = strtol( psz_parser + strlen( "audio=" ), &psz_parser, 0 );
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
                        p_sys->i_height = strtol( psz_parser + 1, &psz_parser, 0 );
                    }
                    msg_Dbg( p_input, "WxH %dx%d", p_sys->i_width, p_sys->i_height );
                }
            }
            else if( !strncmp( psz_parser, "tuner=", strlen( "tuner=" ) ) )
            {
                p_sys->i_tuner = strtol( psz_parser + strlen( "tuner=" ), &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "codec=", strlen( "codec=" ) ) )
            {
                psz_parser += strlen( "codec=" );
                if( !strncmp( psz_parser, "mpeg4", strlen( "mpeg4" ) ) )
                {
                    p_sys->i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );
                }
                else if( !strncmp( psz_parser, "mpeg1", strlen( "mpeg1" ) ) )
                {
                    p_sys->i_codec = VLC_FOURCC( 'm', 'p', '1', 'v' );
                }
                else
                {
                    msg_Warn( p_input, "unknow codec" );
                }
            }
#ifdef _V4L_AUDIO_
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
            else if( !strncmp( psz_parser, "samplerate=", strlen( "samplerate=" ) ) )
            {
                p_sys->i_sample_rate = strtol( psz_parser + strlen( "samplerate=" ), &psz_parser, 0 );
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
#endif
            else
            {
                msg_Warn( p_input, "unknow option" );
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
        p_sys->psz_video_device = strdup( psz_dup );
    }
    else
    {
        p_sys->psz_video_device = strdup( "/dev/video" );
    }
    msg_Dbg( p_input, "video device=`%s'", p_sys->psz_video_device );

#ifdef _V4L_AUDIO_
    if( p_sys->psz_adev && *p_sys->psz_adev == '\0' )
    {
        p_sys->psz_adev = strdup( "/dev/dsp" );
    }
    msg_Dbg( p_input, "audio device=`%s'", p_sys->psz_adev );
#endif



    if( ( p_sys->fd = open( p_sys->psz_video_device, O_RDWR ) ) < 0 )
    {
        msg_Err( p_input, "cannot open device" );
        goto failed;
    }

    if( ioctl( p_sys->fd, VIDIOCGCAP, &p_sys->vid_cap ) < 0 )
    {
        msg_Err( p_input, "cannot get capabilities" );
        goto failed;
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
        msg_Dbg( p_input, "invalid width" );
        p_sys->i_width = 0;
    }
    if( p_sys->i_height < p_sys->vid_cap.minheight ||
        p_sys->i_height > p_sys->vid_cap.maxheight )
    {
        msg_Dbg( p_input, "invalid height" );
        p_sys->i_height = 0;
    }

    if( !( p_sys->vid_cap.type&VID_TYPE_CAPTURE ) )
    {
        msg_Err( p_input, "cannot grab" );
        goto failed;
    }

    vid_channel.channel = p_sys->i_channel;
    if( ioctl( p_sys->fd, VIDIOCGCHAN, &vid_channel ) < 0 )
    {
        msg_Err( p_input, "cannot get channel infos" );
        goto failed;
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
    if( ioctl( p_sys->fd, VIDIOCSCHAN, &vid_channel ) < 0 )
    {
        msg_Err( p_input, "cannot set channel" );
        goto failed;
    }

    if( vid_channel.flags & VIDEO_VC_TUNER )
    {

        /* set tuner */
#if 0
        struct video_tuner vid_tuner;
        if( p_sys->i_tuner >= 0 )
        {
            vid_tuner.tuner = p_sys->i_tuner;
            if( ioctl( p_sys->fd, VIDIOCGTUNER, &vid_tuner ) < 0 )
            {
                msg_Err( p_input, "cannot get tuner" );
                goto failed;
            }
            msg_Dbg( p_input,
                     "tuner %s low=%d high=%d, flags=0x%x mode=0x%x signal=0x%x",
                     vid_tuner.name,
                     vid_tuner.rangelow, vid_tuner.rangehigh,
                     vid_tuner.flags,
                     vid_tuner.mode,
                     vid_tuner.signal );

            msg_Dbg( p_input,
                     "setting tuner %s (%d)",
                     vid_tuner.name, vid_tuner.tuner );

            //vid_tuner.mode = p_sys->i_norm; /* FIXME FIXME to be checked FIXME FIXME */
            if( ioctl( p_sys->fd, VIDIOCSTUNER, &vid_tuner ) < 0 )
            {
                msg_Err( p_input, "cannot set tuner" );
                goto failed;
            }
        }
#endif
        /* set frequency */
        if( p_sys->i_frequency >= 0 )
        {
            if( ioctl( p_sys->fd, VIDIOCSFREQ, &p_sys->i_frequency ) < 0 )
            {
                msg_Err( p_input, "cannot set frequency" );
                goto failed;
            }
            msg_Dbg( p_input, "frequency %d", p_sys->i_frequency );
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
            if( ioctl( p_sys->fd, VIDIOCGAUDIO, &vid_audio ) < 0 )
            {
                msg_Err( p_input, "cannot get audio" );
                goto failed;
            }

            /* unmute audio */
            vid_audio.flags &= ~VIDEO_AUDIO_MUTE;

            if( ioctl( p_sys->fd, VIDIOCSAUDIO, &vid_audio ) < 0 )
            {
                msg_Err( p_input, "cannot set audio" );
                goto failed;
            }

#ifdef _V4L_AUDIO_
            if( p_sys->psz_adev )
            {
                int    i_format;
                if( ( p_sys->fd_audio = open( p_sys->psz_adev, O_RDONLY ) ) < 0 )
                {
                    msg_Err( p_input, "cannot open audio device" );
                    goto failed;
                }

                i_format = AFMT_S16_LE;
                if( ioctl( p_sys->fd_audio, SNDCTL_DSP_SETFMT, &i_format ) < 0 ||
                    i_format != AFMT_S16_LE )
                {
                    msg_Err( p_input, "cannot set audio format (16b little endian)" );
                    goto failed;
                }

                if( ioctl( p_sys->fd_audio, SNDCTL_DSP_STEREO, &p_sys->b_stereo ) < 0 )
                {
                    msg_Err( p_input, "cannot set audio channels count" );
                    goto failed;
                }

                if( ioctl( p_sys->fd_audio, SNDCTL_DSP_SPEED, &p_sys->i_sample_rate ) < 0 )
                {
                    msg_Err( p_input, "cannot set audio sample rate" );
                    goto failed;
                }

                msg_Dbg( p_input,
                         "adev=`%s' %s %dHz",
                         p_sys->psz_adev,
                         p_sys->b_stereo ? "stereo" : "mono",
                         p_sys->i_sample_rate );

                p_sys->i_audio_frame_size = 0;
                p_sys->i_audio_frame_size_allocated = 10*1024;
                p_sys->p_audio_frame = malloc( p_sys->i_audio_frame_size_allocated );
            }
#endif
        }
    }

    /* fix width/heigh */
    if( p_sys->i_width == 0 || p_sys->i_height == 0 )
    {
        struct video_window vid_win;

        if( ioctl( p_sys->fd, VIDIOCGWIN, &vid_win ) < 0 )
        {
            msg_Err( p_input, "cannot get win" );
            goto failed;
        }
        p_sys->i_width  = vid_win.width;
        p_sys->i_height = vid_win.height;

        msg_Dbg( p_input, "will use %dx%d", p_sys->i_width, p_sys->i_height );
    }

    if( ioctl( p_sys->fd, VIDIOCGMBUF, &p_sys->vid_mbuf ) < 0 )
    {
        msg_Err( p_input, "mmap unsupported" );
        goto failed;
    }

    p_sys->p_video_mmap = mmap( 0, p_sys->vid_mbuf.size,
                                PROT_READ|PROT_WRITE,
                                MAP_SHARED,
                                p_sys->fd,
                                0 );
    if( p_sys->p_video_mmap == MAP_FAILED )
    {
        /* FIXME -> normal read */
        msg_Err( p_input, "mmap failed" );
        goto failed;
    }

    p_sys->p_video_frame = NULL;

    /* init grabbing */
    p_sys->vid_mmap.frame  = 0;
    p_sys->vid_mmap.width  = p_sys->i_width;
    p_sys->vid_mmap.height = p_sys->i_height;
    p_sys->vid_mmap.format = VIDEO_PALETTE_YUV420P;
    p_sys->i_chroma = VLC_FOURCC( 'I', '4', '2', '0' );
    if( ioctl( p_sys->fd, VIDIOCMCAPTURE, &p_sys->vid_mmap ) < 0 )
    {
        msg_Warn( p_input, "%4.4s refused", (char*)&p_sys->i_chroma );

        p_sys->vid_mmap.format = VIDEO_PALETTE_YUV422P;
        p_sys->i_chroma = VLC_FOURCC( 'I', '4', '2', '2' );

        if( ioctl( p_sys->fd, VIDIOCMCAPTURE, &p_sys->vid_mmap ) < 0 )
        {
            msg_Warn( p_input, "%4.4s refused", (char*)&p_sys->i_chroma );

            msg_Err( p_input, "chroma selection failed" );
            goto failed;
        }
        else
        {
            p_sys->i_video_frame_size = p_sys->i_width * p_sys->i_height * 2;
        }
    }
    else
    {
        p_sys->i_video_frame_size = p_sys->i_width * p_sys->i_height * 3 / 2;
    }
    /* encoder part */
    if( p_sys->i_codec != VLC_FOURCC( 0, 0, 0, 0 ) )
    {
        msg_Dbg( p_input,
                 "need a rencoder from %4.4s to %4.4s",
                 (char*)&p_sys->i_chroma,
                 (char*)&p_sys->i_codec );
#define p_enc p_sys->p_encoder
        p_enc = vlc_object_create( p_input, sizeof( video_encoder_t ) );
        p_enc->i_codec = p_sys->i_codec;
        p_enc->i_chroma= p_sys->i_chroma;
        p_enc->i_width = p_sys->i_width;
        p_enc->i_height= p_sys->i_height;
        p_enc->i_aspect= 0;


        p_enc->p_module = module_Need( p_enc,
                                       "video encoder",
                                       "$video-encoder" );
        if( !p_enc->p_module )
        {
            msg_Warn( p_input,
                      "no suitable encoder to %4.4s",
                      (char*)&p_enc->i_codec );
            vlc_object_destroy( p_enc );
            goto failed;
        }

        /* *** init the codec *** */
        if( p_enc->pf_init( p_enc ) )
        {
            msg_Err( p_input, "failed to initialize video encoder plugin" );
            vlc_object_destroy( p_enc );
            goto failed;
        }

        /* *** alloacted buffer *** */
        if( p_enc->i_buffer_size <= 0 )
        {
            p_enc->i_buffer_size = 10 * p_enc->i_width * p_enc->i_height;
        }
        p_sys->i_video_frame_size = p_enc->i_buffer_size;
        p_sys->i_video_frame_size_allocated = p_enc->i_buffer_size;
        if( !( p_sys->p_video_frame = malloc( p_enc->i_buffer_size ) ) )
        {
            msg_Err( p_input, "out of memory" );
            goto failed;
        }

        switch( p_sys->i_chroma )
        {
            case VLC_FOURCC( 'I', '4', '2', '0' ):
                p_sys->pic.i_planes = 3;
                p_sys->pic.p[0].i_pitch = p_sys->i_width;
                p_sys->pic.p[0].i_lines = p_sys->i_height;
                p_sys->pic.p[0].i_pixel_pitch = 1;
                p_sys->pic.p[0].i_visible_pitch = p_sys->i_width;

                p_sys->pic.p[1].i_pitch = p_sys->i_width / 2;
                p_sys->pic.p[1].i_lines = p_sys->i_height / 2;
                p_sys->pic.p[1].i_pixel_pitch = 1;
                p_sys->pic.p[1].i_visible_pitch = p_sys->i_width / 2;

                p_sys->pic.p[2].i_pitch = p_sys->i_width / 2;
                p_sys->pic.p[2].i_lines = p_sys->i_height / 2;
                p_sys->pic.p[2].i_pixel_pitch = 1;
                p_sys->pic.p[2].i_visible_pitch = p_sys->i_width / 2;
                break;
            default:
                msg_Err( p_input, "unsuported chroma" );
                vlc_object_destroy( p_enc );
                goto failed;
        }
#undef p_enc
    }
    else
    {
        p_sys->i_codec  = p_sys->i_chroma;
        p_sys->p_encoder = NULL;
    }


    p_input->pf_read        = Read;
    p_input->pf_seek        = NULL;
    p_input->pf_set_area    = NULL;
    p_input->pf_set_program = NULL;

    p_input->p_access_data  = p_sys;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_FILE;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for access */
    p_input->i_pts_delay = config_GetInt( p_input, "v4l-caching" ) * 1000;

    msg_Info( p_input, "v4l grabbing started" );

    /* create header */
    p_sys->i_header_size = 8 + 20;
    p_sys->i_header_pos  = 0;
    p_sys->p_header      = malloc( p_sys->i_header_size );

    memcpy(  &p_sys->p_header[0], ".v4l", 4 );
    SetDWBE( &p_sys->p_header[4], 1 );

    memcpy(  &p_sys->p_header[ 8], "vids", 4 );
    memcpy(  &p_sys->p_header[12], &p_sys->i_codec, 4 );
    SetDWBE( &p_sys->p_header[16], p_sys->i_width );
    SetDWBE( &p_sys->p_header[20], p_sys->i_height );
    SetDWBE( &p_sys->p_header[24], 0 );

#ifdef _V4L_AUDIO_
    if( p_sys->fd_audio > 0 )
    {
        p_sys->i_header_size += 20;
        p_sys->p_header = realloc( p_sys->p_header, p_sys->i_header_size );

        SetDWBE( &p_sys->p_header[4], 2 );

        memcpy(  &p_sys->p_header[28], "auds", 4 );
        memcpy(  &p_sys->p_header[32], "araw", 4 );
        SetDWBE( &p_sys->p_header[36], p_sys->b_stereo ? 2 : 1 );
        SetDWBE( &p_sys->p_header[40], p_sys->i_sample_rate );
        SetDWBE( &p_sys->p_header[44], 16 );
    }
#endif
    return VLC_SUCCESS;

failed:
    free( p_sys->psz_video_device );
    if( p_sys->fd >= 0 )
    {
        close( p_sys->fd );
    }
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * V4lClose: close device
 *****************************************************************************/
static void AccessClose( vlc_object_t *p_this )
{
    input_thread_t  *p_input = (input_thread_t *)p_this;
    access_sys_t    *p_sys   = p_input->p_access_data;

    msg_Info( p_input, "v4l grabbing stoped" );

    free( p_sys->psz_video_device );
    close( p_sys->fd );
    if( p_sys->p_video_mmap )
    {
        munmap( p_sys->p_video_mmap, p_sys->vid_mbuf.size );
    }
#ifdef _V4L_AUDIO_
    if( p_sys->fd_audio >= 0 )
    {
        close( p_sys->fd_audio );
    }
#endif

    if( p_sys->p_encoder )
    {
        p_sys->p_encoder->pf_end( p_sys->p_encoder );

        module_Unneed( p_sys->p_encoder,
                       p_sys->p_encoder->p_module );
        vlc_object_destroy( p_sys->p_encoder );

        free( p_sys->p_video_frame );
    }
    free( p_sys );
}

#ifdef _V4L_AUDIO_
static int GrabAudio( input_thread_t * p_input,
                      uint8_t **pp_data,
                      int      *pi_data )
{
    access_sys_t    *p_sys   = p_input->p_access_data;
    fd_set  fds;
    struct timeval  timeout;
    int i_ret;
    int i_read;

    /* we first try to get an audio frame */
    FD_ZERO( &fds );
    FD_SET( p_sys->fd_audio, &fds );

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    i_ret = select( p_sys->fd_audio + 1, &fds, NULL, NULL, &timeout );

    if( i_ret < 0 && errno != EINTR )
    {
        msg_Warn( p_input, "audio select failed" );
        return VLC_EGENERIC;
    }

    if( !FD_ISSET( p_sys->fd_audio , &fds) )
    {
        return VLC_EGENERIC;
    }

    i_read = read( p_sys->fd_audio, p_sys->p_audio_frame, p_sys->i_audio_frame_size_allocated );

    if( i_read <= 0 )
    {
        return VLC_EGENERIC;
    }

    p_sys->i_audio_frame_size = i_read;


    *pp_data = p_sys->p_audio_frame;
    *pi_data = p_sys->i_audio_frame_size;
    return VLC_SUCCESS;
}
#endif

static int GrabVideo( input_thread_t * p_input,
                      uint8_t **pp_data,
                      int      *pi_data )
{
    access_sys_t    *p_sys   = p_input->p_access_data;

    p_sys->vid_mmap.frame = ( p_sys->i_frame_pos + 1 ) % p_sys->vid_mbuf.frames;
    for( ;; )
    {
        if( ioctl( p_sys->fd, VIDIOCMCAPTURE, &p_sys->vid_mmap ) >= 0 )
        {
            break;
        }

        if( errno != EAGAIN )
        {
            msg_Err( p_input, "failed while grabbing new frame" );
            return( -1 );
        }
        msg_Dbg( p_input, "another try ?" );
    }

    //msg_Warn( p_input, "grab a new frame" );

    while( ioctl(p_sys->fd, VIDIOCSYNC, &p_sys->i_frame_pos) < 0 &&
           ( errno == EAGAIN || errno == EINTR ) );


    p_sys->i_frame_pos = p_sys->vid_mmap.frame;

    if( p_sys->p_encoder )
    {
        p_sys->pic.p[0].p_pixels = p_sys->p_video_mmap + p_sys->vid_mbuf.offsets[p_sys->i_frame_pos];
        p_sys->pic.p[1].p_pixels = p_sys->pic.p[0].p_pixels +
                                        p_sys->pic.p[0].i_pitch * p_sys->pic.p[0].i_lines;
        p_sys->pic.p[2].p_pixels = p_sys->pic.p[1].p_pixels +
                                        p_sys->pic.p[1].i_pitch * p_sys->pic.p[1].i_lines;

        p_sys->i_video_frame_size = p_sys->i_video_frame_size_allocated;
        p_sys->p_encoder->pf_encode( p_sys->p_encoder,
                                     &p_sys->pic,
                                     p_sys->p_video_frame,
                                     &p_sys->i_video_frame_size );
    }
    else
    {
        p_sys->p_video_frame = p_sys->p_video_mmap + p_sys->vid_mbuf.offsets[p_sys->i_frame_pos];
    }

    *pp_data = p_sys->p_video_frame;
    *pi_data = p_sys->i_video_frame_size;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read: reads from the device into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int Read( input_thread_t * p_input, byte_t * p_buffer,
                 size_t i_len )
{
    access_sys_t    *p_sys   = p_input->p_access_data;
    int i_data = 0;
    int i_stream;

    msg_Info( p_input, "access read" );
    while( i_len > 0 )
    {

        /* First copy header if any */
        if( i_len > 0 && p_sys->i_header_pos < p_sys->i_header_size )
        {
            int i_copy;

            i_copy = __MIN( p_sys->i_header_size - p_sys->i_header_pos,
                            (int)i_len );
            memcpy( p_buffer,
                    &p_sys->p_header[p_sys->i_header_pos],
                    i_copy );
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

            memcpy( p_buffer,
                    &p_sys->p_data[p_sys->i_data_pos],
                    i_copy );
            p_sys->i_data_pos += i_copy;

            p_buffer += i_copy;
            i_len -= i_copy;
            i_data += i_copy;
        }

        if( i_len <= 0 )
        {
            return( i_data );
        }

        /* re fill data by grabbing audio/video */
        p_sys->i_data_pos = 0;

        /* try grabbing audio frames */
#ifdef _V4L_AUDIO_
        i_stream = 1;
        if( p_sys->fd_audio < 0 || GrabAudio( p_input, &p_sys->p_data, &p_sys->i_data_size ) )
        {
            /* and then get video frame if no audio */
            i_stream = 0;
            if( GrabVideo( p_input, &p_sys->p_data, &p_sys->i_data_size ) )
            {
                return -1;
            }
        }
#else
        /* and then get video frame if no audio */
        i_stream = 0;
        if( GrabVideo( p_input, &p_sys->p_data, &p_sys->i_data_size ) )
        {
            return -1;
        }
#endif

        /* create pseudo header */
        p_sys->i_header_size = 8;
        p_sys->i_header_pos  = 0;
        SetDWBE( &p_sys->p_header[0], i_stream );
        SetDWBE( &p_sys->p_header[4], p_sys->i_data_size );
    }

    return i_data;
}

/****************************************************************************
 * I. Demux Part
 ****************************************************************************/
#define MAX_PACKETS_IN_FIFO 3

static int  DemuxOpen  ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    //demux_sys_t    *p_sys;

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
        return( VLC_EGENERIC );
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
        return( VLC_EGENERIC );
    }

    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    p_input->stream.i_mux_rate =  0;

    i_streams = GetDWBE( &p_peek[4] );
    if( input_Peek( p_input, &p_peek, 8 + 20 * i_streams ) < 8 + 20 * i_streams )
    {
        msg_Err( p_input, "v4l plugin discarded (cannot peek)" );
        return( VLC_EGENERIC );
    }
    p_peek += 8;

    for( i = 0; i < i_streams; i++ )
    {
        es_descriptor_t *p_es;


        p_es = input_AddES( p_input, p_input->stream.pp_programs[0], i + 1, 0 );
        p_es->i_stream_id   = i + 1;
        if( !strncmp( p_peek, "auds", 4 ) )
        {
#define wf ((WAVEFORMATEX*)p_es->p_waveformatex)
            p_es->i_cat         = AUDIO_ES;
            p_es->i_fourcc      = VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] );
            p_es->p_waveformatex= malloc( sizeof( WAVEFORMATEX ) );

            wf->wFormatTag      = WAVE_FORMAT_UNKNOWN;
            wf->nChannels       = GetDWBE( &p_peek[8] );
            wf->nSamplesPerSec  = GetDWBE( &p_peek[12] );
            wf->wBitsPerSample  = GetDWBE( &p_peek[16] );
            wf->nBlockAlign     = wf->wBitsPerSample * wf->nChannels / 8;
            wf->nAvgBytesPerSec = wf->nBlockAlign * wf->nSamplesPerSec;
            wf->cbSize          = 0;

            msg_Dbg( p_input, "added new audio es %d channels %dHz",
                     wf->nChannels,
                     wf->nSamplesPerSec );

            input_SelectES( p_input, p_es );
#undef wf
        }
        else if( !strncmp( p_peek, "vids", 4 ) )
        {
#define bih ((BITMAPINFOHEADER*)p_es->p_bitmapinfoheader)
            p_es->i_cat = VIDEO_ES;
            p_es->i_fourcc      = VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] );

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
                     (char*)&p_es->i_fourcc,
                     bih->biWidth,
                     bih->biHeight );

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
    return VLC_SUCCESS;
}

static void DemuxClose ( vlc_object_t *p_this )
{
    return;
}


static int  Demux      ( input_thread_t *p_input )
{
    es_descriptor_t *p_es;
    pes_packet_t    *p_pes;

    int i_stream;
    int i_size;
    uint8_t *p_peek;

    if( input_Peek( p_input, &p_peek, 8 ) < 8 )
    {
        msg_Warn( p_input, "cannot peek (EOF ?)" );
        return( 0 );
    }

    i_stream = GetDWBE( &p_peek[0] );
    i_size   = GetDWBE( &p_peek[4] );

    msg_Dbg( p_input, "stream=%d size=%d", i_stream, i_size );
//    p_es = input_FindES( p_input, i_stream );
    p_es = p_input->stream.p_selected_program->pp_es[i_stream];

    if( p_es == NULL || p_es->p_decoder_fifo == NULL )
    {
        msg_Err( p_input, "cannot find decoder" );
        return 0;
    }

    p_pes = input_NewPES( p_input->p_method_data );
    if( p_pes == NULL )
    {
        msg_Warn( p_input, "cannot allocate PES" );
        msleep( 1000 );
        return( 1 );
    }
    i_size += 8;
    while( i_size > 0 )
    {
        data_packet_t   *p_data;
        int i_read;

        if( (i_read = input_SplitBuffer( p_input,
                                         &p_data,
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
//    input_SplitBuffer( p_input, &p_pk, i_size + 8 );
    p_pes->p_first->p_payload_start += 8;

    vlc_mutex_lock( &p_es->p_decoder_fifo->data_lock );
    if( p_es->p_decoder_fifo->i_depth >= MAX_PACKETS_IN_FIFO )
    {
        /* Wait for the decoder. */
        vlc_cond_wait( &p_es->p_decoder_fifo->data_wait, &p_es->p_decoder_fifo->data_lock );
    }
    vlc_mutex_unlock( &p_es->p_decoder_fifo->data_lock );

    p_pes->i_pts = mdate() + p_input->i_pts_delay;

    input_DecodePES( p_es->p_decoder_fifo, p_pes );

    return 1;
}
