/*****************************************************************************
 * oss.c : OSS /dev/dsp module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2002 VideoLAN
 * $Id: oss.c,v 1.3 2002/08/08 22:28:22 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <sys/ioctl.h>                                            /* ioctl() */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#endif

#include <vlc/aout.h>

#include "aout_internal.h"

/* SNDCTL_DSP_RESET, SNDCTL_DSP_SETFMT, SNDCTL_DSP_STEREO, SNDCTL_DSP_SPEED,
 * SNDCTL_DSP_GETOSPACE */
#ifdef HAVE_SOUNDCARD_H
#   include <soundcard.h>
#elif defined( HAVE_SYS_SOUNDCARD_H )
#   include <sys/soundcard.h>
#elif defined( HAVE_MACHINE_SOUNDCARD_H )
#   include <machine/soundcard.h>
#endif

/*****************************************************************************
 * aout_sys_t: OSS audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the dsp specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    int                   i_fd;
    volatile vlc_bool_t   b_die;
    volatile vlc_bool_t   b_initialized;
};

#define DEFAULT_FRAME_SIZE 2048

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static int  SetFormat    ( aout_instance_t * );
static void Play         ( aout_instance_t *, aout_buffer_t * );
static int  OSSThread    ( aout_instance_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("Audio"), NULL );
    add_file( "dspdev", "/dev/dsp", NULL, N_("OSS dsp device"), NULL );
    set_description( _("Linux OSS /dev/dsp module") );
    set_capability( "audio output", 100 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open the audio device (the digital sound processor)
 *****************************************************************************
 * This function opens the dsp as a usual non-blocking write-only file, and
 * modifies the p_aout->p_sys->i_fd with the file's descriptor.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;
    char * psz_device;

    /* Allocate structure */
    p_aout->output.p_sys = p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return 1;
    }

    /* Initialize some variables */
    if( (psz_device = config_GetPsz( p_aout, "dspdev" )) == NULL )
    {
        msg_Err( p_aout, "no audio device given (maybe /dev/dsp ?)" );
        free( p_sys );
        return -1;
    }

    /* Open the sound device */
    if( (p_sys->i_fd = open( psz_device, O_WRONLY )) < 0 )
    {
        msg_Err( p_aout, "cannot open audio device (%s)",
                          psz_device );
        free( psz_device );
        free( p_sys );
        return -1;
    }
    free( psz_device );

    /* Create OSS thread and wait for its readiness. */
    p_sys->b_die = 0;
    p_sys->b_initialized = VLC_FALSE;
    if( vlc_thread_create( p_aout, "aout", OSSThread, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create OSS thread (%s)", strerror(errno) );
        free( psz_device );
        free( p_sys );
        return -1;
    }

    p_aout->output.pf_setformat = SetFormat;
    p_aout->output.pf_play = Play;

    return 0;
}

/*****************************************************************************
 * SetFormat: reset the dsp and set its format
 *****************************************************************************
 * This functions resets the DSP device, tries to initialize the output
 * format with the value contained in the dsp structure, and if this value
 * could not be set, the default value returned by ioctl is set. It then
 * does the same for the stereo mode, and for the output rate.
 *****************************************************************************/
static int SetFormat( aout_instance_t *p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    int i_format;
    int i_rate;
    vlc_bool_t b_stereo;

    p_sys->b_initialized = VLC_FALSE;

    /* Reset the DSP device */
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        msg_Err( p_aout, "cannot reset OSS audio device" );
        return -1;
    }

    /* Set the output format */
    i_format = AOUT_FMT_S16_NE;
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0
         || i_format != AOUT_FMT_S16_NE )
    {
        msg_Err( p_aout, "cannot set audio output format (%i)",
                          i_format );
        return -1;
    }
    p_aout->output.output.i_format = AOUT_FMT_S16_NE;

    /* FIXME */
    if ( p_aout->output.output.i_channels > 2 )
    {
        msg_Warn( p_aout, "only two channels are supported at the moment" );
        /* Trigger downmixing */
        p_aout->output.output.i_channels = 2;
    }

    /* Set the number of channels */
    b_stereo = p_aout->output.output.i_channels - 1;

    if( ioctl( p_sys->i_fd, SNDCTL_DSP_STEREO, &b_stereo ) < 0 )
    {
        msg_Err( p_aout, "cannot set number of audio channels (%i)",
                          p_aout->output.output.i_channels );
        return -1;
    }

    if ( b_stereo + 1 != p_aout->output.output.i_channels )
    {
        msg_Warn( p_aout, "driver forced up/downmixing %li->%li",
                          p_aout->output.output.i_channels,
                          b_stereo + 1 );
        p_aout->output.output.i_channels = b_stereo + 1;
    }

    /* Set the output rate */
    i_rate = p_aout->output.output.i_rate;
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_SPEED, &i_rate ) < 0 )
    {
        msg_Err( p_aout, "cannot set audio output rate (%i)",
                         p_aout->output.output.i_rate );
        return -1;
    }

    if( i_rate != p_aout->output.output.i_rate )
    {
        msg_Warn( p_aout, "driver forced resampling %li->%li",
                          p_aout->output.output.i_rate, i_rate );
        p_aout->output.output.i_rate = i_rate;
    }

    p_aout->output.i_nb_samples = DEFAULT_FRAME_SIZE;

    p_sys->b_initialized = VLC_TRUE;

    return 0;
}

/*****************************************************************************
 * Play: queue a buffer for playing by OSSThread
 *****************************************************************************/
static void Play( aout_instance_t *p_aout, aout_buffer_t * p_buffer )
{
    aout_FifoPush( p_aout, &p_aout->output.fifo, p_buffer );
}

/*****************************************************************************
 * Close: close the dsp audio device
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    p_sys->b_die = 1;
    vlc_thread_join( p_aout );

    close( p_sys->i_fd );
    free( p_sys );
}


/*****************************************************************************
 * GetBufInfo: buffer status query
 *****************************************************************************
 * This function fills in the audio_buf_info structure :
 * - returns : number of available fragments (not partially used ones)
 * - int fragstotal : total number of fragments allocated
 * - int fragsize : size of a fragment in bytes
 * - int bytes : available space in bytes (includes partially used fragments)
 * Note! 'bytes' could be more than fragments*fragsize
 *****************************************************************************/
static int GetBufInfo( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    audio_buf_info audio_buf;

    ioctl( p_sys->i_fd, SNDCTL_DSP_GETOSPACE, &audio_buf );

    /* returns the allocated space in bytes */
    return ( (audio_buf.fragstotal * audio_buf.fragsize) - audio_buf.bytes );
}

/*****************************************************************************
 * OSSThread: asynchronous thread used to DMA the data to the device
 *****************************************************************************/
static int OSSThread( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    while ( !p_sys->b_die )
    {
        int i_bytes_per_sample;
        aout_buffer_t * p_buffer;
        mtime_t next_date;
        int i_tmp;
        byte_t * p_bytes;

        /* Get the presentation date of the next write() operation. It
         * is equal to the current date + duration of buffered samples.
         * Order is important here, since GetBufInfo is believed to take
         * more time than mdate(). */
        if( !p_sys->b_initialized )
        {
            msleep( THREAD_SLEEP );
            continue;
        }

        i_bytes_per_sample = aout_FormatToBytes( &p_aout->output.output );
        next_date = (mtime_t)GetBufInfo( p_aout ) * 1000000
                      / i_bytes_per_sample
                      / p_aout->output.output.i_rate;
        next_date += mdate();

        p_buffer = aout_OutputNextBuffer( p_aout, next_date );

        if ( p_buffer != NULL )
        {
            p_bytes = p_buffer->p_buffer;
        }
        else
        {
            p_bytes = alloca( DEFAULT_FRAME_SIZE * i_bytes_per_sample );
            memset( p_bytes, 0, DEFAULT_FRAME_SIZE * i_bytes_per_sample );
        }

        i_tmp = write( p_sys->i_fd, p_bytes,
                       DEFAULT_FRAME_SIZE * i_bytes_per_sample );

        if( i_tmp < 0 )
        {
            msg_Err( p_aout, "write failed (%s)", strerror(errno) );
        }

        if ( p_buffer != NULL )
        {
            aout_BufferFree( p_buffer );
        }
    }

    return 0;
}
