/*****************************************************************************
 * v4l2.c : Video4Linux2 input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id: v4l.c 16084 2006-07-19 09:45:02Z zorglub $
 *
 * Author: Benjamin Pracht <bigben at videolan dot org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/vout.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

/*****************************************************************************
 * Module descriptior
  *****************************************************************************/

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define DEV_TEXT N_("Device name")
#define DEV_LONGTEXT N_( \
    "Name of the device to use. " \
    "If you don't specify anything, /dev/video0 will be used.")


vlc_module_begin();
    set_shortname( _("Video4Linux2") );
    set_description( _("Video4Linux2 input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_string( "v4l2-dev", "/dev/video0", 0, DEV_TEXT, DEV_LONGTEXT,
                VLC_FALSE );

    add_shortcut( "v4l2" );
    set_capability( "access_demux", 10 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/

static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

static int OpenDev( demux_t * );

struct demux_sys_t
{
    char *psz_device;

    int i_fd_video;

    struct v4l2_capability dev_cap;

    int i_input;
    struct v4l2_input *p_inputs;

    int i_audio;
    /* V4L2 devices cannot have more than 32 audio inputs */
    struct v4l2_audio p_audios[32];

    int i_tuner;
    struct v4l2_tuner *p_tuners;
};

/*****************************************************************************
 * Open: opens v4l device
 *****************************************************************************
 *
 * url: <video device>::::
 *
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    demux_sys_t sys;

    /* Set up p_demux */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( p_sys == NULL ) return VLC_ENOMEM;
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    p_sys->psz_device = var_CreateGetString( p_demux, "v4l2-dev" );

    if( OpenDev( p_demux ) < 0 ) return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close device, free resources
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    if( p_sys->psz_device ) free( p_sys->psz_device );
    if( p_sys->p_inputs ) free( p_sys->p_inputs );

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
}


/*****************************************************************************
 * OpenDev: open the device and probe for capabilities
 *****************************************************************************/
int OpenDev( demux_t *p_demux )
{
    int i_device_index;

    int i_fd;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( ( i_fd = open( p_sys->psz_device, O_RDWR ) ) < 0 )
    {
        msg_Err( p_demux, "cannot open device (%s)", strerror( errno ) );
        goto open_failed;
    }

    /* Get device capabilites */

    if( ioctl( i_fd, VIDIOC_QUERYCAP, &p_sys->dev_cap ) < 0 )
    {
        msg_Err( p_demux, "cannot get capabilities (%s)", strerror( errno ) );
        goto open_failed;
    }

  msg_Dbg( p_demux, "V4L2 device: %s using driver: %s (version: %u.%u.%u) on %s",
                            p_sys->dev_cap.card,
                            p_sys->dev_cap.driver,
                            (p_sys->dev_cap.version >> 16) & 0xFF,
                            (p_sys->dev_cap.version >> 8) & 0xFF,
                            p_sys->dev_cap.version & 0xFF,
                            p_sys->dev_cap.bus_info );

    msg_Dbg( p_demux, "The device has the capabilities: (%c) Video Capure, "
                                                       "(%c) Audio, "
                                                       "(%c) Tuner",
             ( p_sys->dev_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE  ? 'X':' '),
             ( p_sys->dev_cap.capabilities & V4L2_CAP_AUDIO  ? 'X':' '),
             ( p_sys->dev_cap.capabilities & V4L2_CAP_TUNER  ? 'X':' ') );

    msg_Dbg( p_demux, "Supported I/O methods are: (%c) Read/Write, "
                                                 "(%c) Streaming, "
                                                 "(%c) Asynchronous",
            ( p_sys->dev_cap.capabilities & V4L2_CAP_READWRITE ? 'X':' ' ),
            ( p_sys->dev_cap.capabilities & V4L2_CAP_STREAMING ? 'X':' ' ),
            ( p_sys->dev_cap.capabilities & V4L2_CAP_ASYNCIO ? 'X':' ' ) );

    /* Now, enumerate all the video inputs. This is useless at the moment
       since we have no way to present that info to the user except with
       debug messages */

    if( p_sys->dev_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE )
    {
        while( ioctl( i_fd, VIDIOC_S_INPUT, &p_sys->i_input ) >= 0 )
        {
            p_sys->i_input++;
        }

        p_sys->p_inputs = malloc( p_sys->i_input * sizeof( struct v4l2_input ) );
        if( !p_sys->p_inputs ) goto open_failed;
        memset( p_sys->p_inputs, 0, sizeof( struct v4l2_input ) );

        for( i_device_index = 0; i_device_index < p_sys->i_input; i_device_index++ )
        {
            p_sys->p_inputs[i_device_index].index = i_device_index;

            if( ioctl( i_fd, VIDIOC_ENUMINPUT, &p_sys->p_inputs[i_device_index] ) )
            {
                msg_Err( p_demux, "cannot get video input characteristics (%s)",
                                                 strerror( errno ) );
                goto open_failed;
            }
            msg_Dbg( p_demux, "Video input %i (%s) has type: %s",
                                i_device_index,
                                p_sys->p_inputs[i_device_index].name,
                                p_sys->p_inputs[i_device_index].type
                                        == V4L2_INPUT_TYPE_TUNER ?
                                        "Tuner adapter" :
                                        "External analog input" );
        }
    }

    /* initialize the structures for the ioctls */
    for( i_device_index = 0; i_device_index < 32; i_device_index++ )
    {
        p_sys->p_audios[i_device_index].index = i_device_index;
    }

    /* Probe audio inputs */

    if( p_sys->dev_cap.capabilities & V4L2_CAP_AUDIO )
    {
        while( p_sys->i_audio < 32 &&
               ioctl( i_fd, VIDIOC_S_AUDIO, &p_sys->p_audios[p_sys->i_audio] ) >= 0 )
        {
            if( ioctl( i_fd, VIDIOC_G_AUDIO, &p_sys->p_audios[ p_sys->i_audio] ) < 0 )
            {
                msg_Err( p_demux, "cannot get video input characteristics (%s)",
                                                 strerror( errno ) );
                goto open_failed;
            }

            msg_Dbg( p_demux, "Audio device %i (%s) is %s",
                                p_sys->i_audio,
                                p_sys->p_audios[p_sys->i_audio].name,
                                p_sys->p_audios[p_sys->i_audio].capability &
                                                    V4L2_AUDCAP_STEREO ?
                                        "Stereo" : "Mono" );

            p_sys->i_audio++;
        }
    }

    if( p_sys->dev_cap.capabilities & V4L2_CAP_TUNER )
    {
        struct v4l2_tuner tuner;
        memset( &tuner, 0, sizeof( struct v4l2_tuner ) );

        while( ioctl( i_fd, VIDIOC_S_TUNER, &tuner ) >= 0 )
        {
            p_sys->i_tuner++;
            tuner.index = p_sys->i_tuner;
        }

        p_sys->p_tuners = malloc( p_sys->i_tuner * sizeof( struct v4l2_tuner ) );
        if( !p_sys->p_tuners ) goto open_failed;
        memset( p_sys->p_tuners, 0, sizeof( struct v4l2_tuner ) );

        for( i_device_index = 0; i_device_index < p_sys->i_tuner; i_device_index++ )
        {
            p_sys->p_tuners[i_device_index].index = i_device_index;

            if( ioctl( i_fd, VIDIOC_G_TUNER, &p_sys->p_tuners[i_device_index] ) )
            {
                msg_Err( p_demux, "cannot get tuner characteristics (%s)",
                                                 strerror( errno ) );
                goto open_failed;
            }
            msg_Dbg( p_demux, "Tuner %i (%s) has type: %s, "
                              "frequency range: %.1f %s -> %.1f %s",
                                i_device_index,
                                p_sys->p_tuners[i_device_index].name,
                                p_sys->p_tuners[i_device_index].type
                                        == V4L2_TUNER_RADIO ?
                                        "Radio" : "Analog TV",
                                p_sys->p_tuners[i_device_index].rangelow * 62.5,
                                p_sys->p_tuners[i_device_index].capability &
                                        V4L2_TUNER_CAP_LOW ?
                                        "Hz" : "kHz",
                                p_sys->p_tuners[i_device_index].rangehigh * 62.5,
                                p_sys->p_tuners[i_device_index].capability &
                                        V4L2_TUNER_CAP_LOW ?
                                        "Hz" : "kHz" );
        }
    }

    if( i_fd >= 0 ) close( i_fd );
    return VLC_SUCCESS;

open_failed:
    
    if( i_fd >= 0 ) close( i_fd );
    return VLC_EGENERIC;

}
