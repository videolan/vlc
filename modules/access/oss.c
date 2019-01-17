/*****************************************************************************
 * oss.c : OSS input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Richard Hosking <richard at hovis dot net>
 *          Antoine Cellerier <dionoea at videolan d.t org>
 *          Dennis Lou <dlou99 at yahoo dot com>
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
#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_fs.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#ifdef HAVE_SYS_SOUNDCARD_H
#  include <sys/soundcard.h>
#endif
#ifdef HAVE_SOUNDCARD_H
#  include <soundcard.h>
#endif

#include <poll.h>

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/

static int  DemuxOpen ( vlc_object_t * );
static void DemuxClose( vlc_object_t * );


#define STEREO_TEXT N_( "Stereo" )
#define STEREO_LONGTEXT N_( \
    "Capture the audio stream in stereo." )
#define SAMPLERATE_TEXT N_( "Samplerate" )
#define SAMPLERATE_LONGTEXT N_( \
    "Samplerate of the captured audio stream, in Hz (eg: 11025, 22050, 44100, 48000)" )

#define OSS_DEFAULT "/dev/dsp"

#define CFG_PREFIX "oss-"

vlc_module_begin ()
    set_shortname( N_("OSS") )
    set_description( N_("OSS input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_shortcut( "oss" )
    set_capability( "access", 0 )
    set_callbacks( DemuxOpen, DemuxClose )

    add_bool( CFG_PREFIX "stereo", true, STEREO_TEXT, STEREO_LONGTEXT,
                true )
    add_integer( CFG_PREFIX "samplerate", 48000, SAMPLERATE_TEXT,
                SAMPLERATE_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/

static int DemuxControl( demux_t *, int, va_list );

static int Demux( demux_t * );

static block_t* GrabAudio( demux_t *p_demux );

static int OpenAudioDev( demux_t * );
static int OpenAudioDevOss( demux_t * );
static bool ProbeAudioDevOss( demux_t *, const char *psz_device );

struct buffer_t
{
    void *  start;
    size_t  length;
};

typedef struct
{
    const char *psz_device;  /* OSS device from MRL */

    int  i_fd;

    /* Audio */
    unsigned int i_sample_rate;
    bool b_stereo;
    size_t i_max_frame_size;
    block_t *p_block;
    es_out_id_t *p_es;

    vlc_tick_t i_next_demux_date; /* Used to handle oss:// as input-slave properly */
} demux_sys_t;

static int FindMainDevice( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    msg_Dbg( p_demux, "opening device '%s'", p_sys->psz_device );
    if( ProbeAudioDevOss( p_demux, p_sys->psz_device ) )
    {
        msg_Dbg( p_demux, "'%s' is an audio device", p_sys->psz_device );
        p_sys->i_fd = OpenAudioDev( p_demux );
    }

    if( p_sys->i_fd < 0 )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DemuxOpen: opens oss device, access_demux callback
 *****************************************************************************
 *
 * url: <oss device>::::
 *
 *****************************************************************************/
static int DemuxOpen( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    if (p_demux->out == NULL)
        return VLC_EGENERIC;

    /* Set up p_demux */
    p_demux->pf_control = DemuxControl;
    p_demux->pf_demux = Demux;

    p_demux->p_sys = p_sys = vlc_obj_calloc( p_this, 1, sizeof( demux_sys_t ) );
    if( p_sys == NULL ) return VLC_ENOMEM;

    p_sys->i_sample_rate = var_InheritInteger( p_demux, CFG_PREFIX "samplerate" );
    p_sys->b_stereo = var_InheritBool( p_demux, CFG_PREFIX "stereo" );
    p_sys->i_fd = -1;
    p_sys->p_es = NULL;
    p_sys->p_block = NULL;
    p_sys->i_next_demux_date = -1;

    if( p_demux->psz_location && *p_demux->psz_location )
        p_sys->psz_device = p_demux->psz_location;
    else
        p_sys->psz_device = OSS_DEFAULT;

    if( FindMainDevice( p_demux ) != VLC_SUCCESS )
    {
        DemuxClose( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close device, free resources
 *****************************************************************************/
static void DemuxClose( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    if( p_sys->i_fd >= 0 )
        vlc_close( p_sys->i_fd );

    if( p_sys->p_block ) block_Release( p_sys->p_block );
}

/*****************************************************************************
 * DemuxControl:
 *****************************************************************************/
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool *pb;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            pb = va_arg( args, bool * );
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS(var_InheritInteger( p_demux, "live-caching" ));
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = vlc_tick_now();
            return VLC_SUCCESS;

        case DEMUX_SET_NEXT_DEMUX_TIME:
            p_sys->i_next_demux_date = va_arg( args, vlc_tick_t );
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Demux: Processes the audio frame
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    struct pollfd fd;
    fd.fd = p_sys->i_fd;
    fd.events = POLLIN|POLLPRI;
    fd.revents = 0;

    block_t *p_block = NULL;

    do
    {
        if( p_block )
        {
            es_out_Send( p_demux->out, p_sys->p_es, p_block );
            p_block = NULL;
        }

        /* Wait for data */
        if( poll( &fd, 1, 10 ) ) /* Timeout after 0.01 seconds. Bigger delays are an issue when used with/as an input-slave since all the inputs run in the same thread. */
        {
            if( errno == EINTR )
                continue;
            if( fd.revents & (POLLIN|POLLPRI) )
            {
                p_block = GrabAudio( p_demux );
                if( p_block )
                    es_out_SetPCR( p_demux->out, p_block->i_pts );
            }
        }
    } while( p_block && p_sys->i_next_demux_date > 0 &&
             p_block->i_pts < p_sys->i_next_demux_date );

    if( p_block )
        es_out_Send( p_demux->out, p_sys->p_es, p_block );

    return 1;
}

/*****************************************************************************
 * GrabAudio: Grab an audio frame
 *****************************************************************************/
static block_t* GrabAudio( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    struct audio_buf_info buf_info;
    int i_read = 0, i_correct;
    block_t *p_block;

    if( p_sys->p_block ) p_block = p_sys->p_block;
    else p_block = block_Alloc( p_sys->i_max_frame_size );

    if( !p_block )
    {
        msg_Warn( p_demux, "cannot get block" );
        return NULL;
    }

    p_sys->p_block = p_block;

    i_read = read( p_sys->i_fd, p_block->p_buffer,
                p_sys->i_max_frame_size );

    if( i_read <= 0 ) return NULL;

    p_block->i_buffer = i_read;
    p_sys->p_block = NULL;

    /* Correct the date because of kernel buffering */
    i_correct = i_read;
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_GETISPACE, &buf_info ) == 0 )
    {
        i_correct += buf_info.bytes;
    }

    /* Timestamp */
    p_block->i_pts = p_block->i_dts =
        vlc_tick_now() - vlc_tick_from_samples(i_correct,
                        2 * ( p_sys->b_stereo ? 2 : 1) * p_sys->i_sample_rate);

    return p_block;
}

/*****************************************************************************
 * OpenAudioDev: open and set up the audio device and probe for capabilities
 *****************************************************************************/
static int OpenAudioDevOss( demux_t *p_demux )
{
    demux_sys_t *p_sys = (demux_sys_t *)p_demux->p_sys;
    int i_fd;
    int i_format;

    i_fd = vlc_open( p_sys->psz_device, O_RDONLY | O_NONBLOCK );

    if( i_fd < 0 )
    {
        msg_Err( p_demux, "cannot open OSS audio device (%s)",
                 vlc_strerror_c(errno) );
        goto adev_fail;
    }

    i_format = AFMT_S16_LE;
    if( ioctl( i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0
        || i_format != AFMT_S16_LE )
    {
        msg_Err( p_demux,
                 "cannot set audio format (16b little endian) (%s)",
                 vlc_strerror_c(errno) );
        goto adev_fail;
    }

    if( ioctl( i_fd, SNDCTL_DSP_STEREO, &p_sys->b_stereo ) < 0 )
    {
        msg_Err( p_demux, "cannot set audio channels count (%s)",
                 vlc_strerror_c(errno) );
        goto adev_fail;
    }

    if( ioctl( i_fd, SNDCTL_DSP_SPEED, &p_sys->i_sample_rate ) < 0 )
    {
        msg_Err( p_demux, "cannot set audio sample rate (%s)",
                 vlc_strerror_c(errno) );
        goto adev_fail;
    }

    p_sys->i_max_frame_size = 6 * 1024;

    return i_fd;

 adev_fail:

    if( i_fd >= 0 )
        vlc_close( i_fd );
    return -1;
}

static int OpenAudioDev( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_fd = OpenAudioDevOss( p_demux );

    if( i_fd < 0 )
        return i_fd;

    msg_Dbg( p_demux, "opened adev=`%s' %s %dHz",
             p_sys->psz_device, p_sys->b_stereo ? "stereo" : "mono",
             p_sys->i_sample_rate );

    es_format_t fmt;
    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_S16L );

    fmt.audio.i_channels = p_sys->b_stereo ? 2 : 1;
    fmt.audio.i_rate = p_sys->i_sample_rate;
    fmt.audio.i_bitspersample = 16;
    fmt.audio.i_blockalign = fmt.audio.i_channels * fmt.audio.i_bitspersample / 8;
    fmt.i_bitrate = fmt.audio.i_channels * fmt.audio.i_rate * fmt.audio.i_bitspersample;

    msg_Dbg( p_demux, "new audio es %d channels %dHz",
             fmt.audio.i_channels, fmt.audio.i_rate );

    p_sys->p_es = es_out_Add( p_demux->out, &fmt );

    return i_fd;
}

/*****************************************************************************
 * ProbeAudioDev: probe audio for capabilities
 *****************************************************************************/
static bool ProbeAudioDevOss( demux_t *p_demux, const char *psz_device )
{
    int i_caps;
    int i_fd = vlc_open( psz_device, O_RDONLY | O_NONBLOCK );

    if( i_fd < 0 )
    {
        msg_Err( p_demux, "cannot open device %s for OSS audio (%s)",
                 psz_device, vlc_strerror_c(errno) );
        goto open_failed;
    }

    /* this will fail if the device is video */
    if( ioctl( i_fd, SNDCTL_DSP_GETCAPS, &i_caps ) < 0 )
    {
        msg_Err( p_demux, "cannot get audio caps (%s)",
                 vlc_strerror_c(errno) );
        goto open_failed;
    }

    if( i_fd >= 0 )
        vlc_close( i_fd );

    return true;

open_failed:
    if( i_fd >= 0 )
        vlc_close( i_fd );
    return false;
}
