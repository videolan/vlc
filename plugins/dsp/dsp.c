/*****************************************************************************
 * dsp.c : OSS /dev/dsp module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: dsp.c,v 1.19 2002/07/31 20:56:51 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <vlc/aout.h>

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
 * aout_sys_t: dsp audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the dsp specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    audio_buf_info        audio_buf;

    /* Path to the audio output device */
    char *                psz_device;
    int                   i_fd;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static int  SetFormat    ( aout_thread_t * );
static int  GetBufInfo   ( aout_thread_t *, int );
static void Play         ( aout_thread_t *, byte_t *, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_file( "dspdev", "/dev/dsp", NULL, N_("OSS dsp device"), NULL );
    set_description( _("Linux OSS /dev/dsp module") );
    set_capability( "audio output", 100 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: opens the audio device (the digital sound processor)
 *****************************************************************************
 * This function opens the dsp as a usual non-blocking write-only file, and
 * modifies the p_aout->p_sys->i_fd with the file's descriptor.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( 1 );
    }

    /* Initialize some variables */
    if( !(p_aout->p_sys->psz_device = config_GetPsz( p_aout, "dspdev" )) )
    {
        msg_Err( p_aout, "don't know which audio device to open" );
        free( p_aout->p_sys );
        return( -1 );
    }

    p_aout->pf_setformat = SetFormat;
    p_aout->pf_getbufinfo = GetBufInfo;
    p_aout->pf_play = Play;

    /* Open the sound device */
    if( (p_aout->p_sys->i_fd = open( p_aout->p_sys->psz_device, O_WRONLY ))
        < 0 )
    {
        msg_Err( p_aout, "cannot open audio device (%s)",
                          p_aout->p_sys->psz_device );
        free( p_aout->p_sys->psz_device );
        free( p_aout->p_sys );
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * SetFormat: resets the dsp and sets its format
 *****************************************************************************
 * This functions resets the DSP device, tries to initialize the output
 * format with the value contained in the dsp structure, and if this value
 * could not be set, the default value returned by ioctl is set. It then
 * does the same for the stereo mode, and for the output rate.
 *****************************************************************************/
static int SetFormat( aout_thread_t *p_aout )
{
    int i_format;
    int i_rate;
    vlc_bool_t b_stereo;

    /* Reset the DSP device */
    if( ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        msg_Err( p_aout, "cannot reset audio device (%s)",
                          p_aout->p_sys->psz_device );
        return( -1 );
    }

    /* Set the output format */
    i_format = p_aout->i_format;
    if( ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
    {
        msg_Err( p_aout, "cannot set audio output format (%i)",
                          p_aout->i_format );
        return( -1 );
    }

    if( i_format != p_aout->i_format )
    {
        msg_Warn( p_aout, "audio output format not supported (%i)",
                              p_aout->i_format );
        p_aout->i_format = i_format;
    }

    /* Set the number of channels */
    b_stereo = ( p_aout->i_channels >= 2 );

    if( ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_STEREO, &b_stereo ) < 0 )
    {
        msg_Err( p_aout, "cannot set number of audio channels (%i)",
                          p_aout->i_channels );
        return( -1 );
    }

    if( (1 + b_stereo) != p_aout->i_channels )
    {
        msg_Warn( p_aout, "%i audio channels not supported",
                           p_aout->i_channels );
        p_aout->i_channels = 1 + b_stereo;
    }

    /* Set the output rate */
    i_rate = p_aout->i_rate;
    if( ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_SPEED, &i_rate ) < 0 )
    {
        msg_Err( p_aout, "cannot set audio output rate (%i)", p_aout->i_rate );
        return( -1 );
    }

    if( i_rate != p_aout->i_rate )
    {
        msg_Warn( p_aout, "audio output rate not supported (%li)",
                          p_aout->i_rate );
        p_aout->i_rate = i_rate;
    }

    return( 0 );
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
static int GetBufInfo( aout_thread_t *p_aout, int i_buffer_limit )
{
    ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_GETOSPACE,
           &p_aout->p_sys->audio_buf );

    /* returns the allocated space in bytes */
    return ( (p_aout->p_sys->audio_buf.fragstotal
                 * p_aout->p_sys->audio_buf.fragsize)
            - p_aout->p_sys->audio_buf.bytes );
}

/*****************************************************************************
 * Play: plays a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 *****************************************************************************/
static void Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    int i_tmp;
    i_tmp = write( p_aout->p_sys->i_fd, buffer, i_size );

    if( i_tmp < 0 )
    {
        msg_Err( p_aout, "write failed (%s)", strerror(errno) );
    }
}

/*****************************************************************************
 * Close: closes the dsp audio device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;

    close( p_aout->p_sys->i_fd );
    free( p_aout->p_sys->psz_device );
}
