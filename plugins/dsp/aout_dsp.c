/*****************************************************************************
 * aout_dsp.c : dsp functions library
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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

/* TODO:
 *
 * - an aout_DspGetFormats() function
 * - dsp inline/static
 * - make this library portable (see mpg123)
 * - macroify aout_DspPlaySamples &/| aout_DspGetBufInfo ?
 *
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <sys/ioctl.h>                                            /* ioctl() */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#ifdef SYS_BSD
#include <machine/soundcard.h>       /* SNDCTL_DSP_RESET, SNDCTL_DSP_SETFMT,
                   SNDCTL_DSP_STEREO, SNDCTL_DSP_SPEED, SNDCTL_DSP_GETOSPACE */
#else
#include <sys/soundcard.h>           /* SNDCTL_DSP_RESET, SNDCTL_DSP_SETFMT,
                   SNDCTL_DSP_STEREO, SNDCTL_DSP_SPEED, SNDCTL_DSP_GETOSPACE */
#endif

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "audio_output.h"                                   /* aout_thread_t */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
#include "main.h"

/*****************************************************************************
 * vout_dsp_t: dsp audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the dsp specific properties of an audio device.
 *****************************************************************************/
typedef struct aout_sys_s
{
    audio_buf_info        audio_buf;

} aout_sys_t;

/*****************************************************************************
 * aout_DspOpen: opens the audio device (the digital sound processor)
 *****************************************************************************
 * - This function opens the dsp as an usual non-blocking write-only file, and
 *   modifies the p_aout->p_sys->i_fd with the file's descriptor.
 *****************************************************************************/
int aout_DspOpen( aout_thread_t *p_aout )
{
    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Initialize some variables */
    p_aout->i_format = AOUT_FORMAT_DEFAULT;
    p_aout->psz_device = main_GetPszVariable( AOUT_DSP_VAR, AOUT_DSP_DEFAULT );
    p_aout->i_channels = 1 + main_GetIntVariable( AOUT_STEREO_VAR, AOUT_STEREO_DEFAULT );
    p_aout->l_rate     = main_GetIntVariable( AOUT_RATE_VAR, AOUT_RATE_DEFAULT );

    /* Open the sound device */
    if ( (p_aout->i_fd = open( p_aout->psz_device, O_WRONLY|O_NONBLOCK )) < 0 )
    {
        intf_ErrMsg( "aout error: can't open audio device (%s)", p_aout->psz_device );
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * aout_DspReset: resets the dsp
 *****************************************************************************/
int aout_DspReset( aout_thread_t *p_aout )
{
    if ( ioctl( p_aout->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        intf_ErrMsg( "aout error: can't reset audio device (%s)", p_aout->psz_device );
    return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * aout_DspSetFormat: sets the dsp output format
 *****************************************************************************
 * This functions tries to initialize the dsp output format with the value
 * contained in the dsp structure, and if this value could not be set, the
 * default value returned by ioctl is set.
 *****************************************************************************/
int aout_DspSetFormat( aout_thread_t *p_aout )
{
    int i_format;

    i_format = p_aout->i_format;
    if ( ioctl( p_aout->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set audio output format (%i)", p_aout->i_format );
        return( -1 );
    }

    if ( i_format != p_aout->i_format )
    {
        intf_DbgMsg( "aout debug: audio output format not supported (%i)", p_aout->i_format );
        p_aout->i_format = i_format;
    }

    return( 0 );
}

/*****************************************************************************
 * aout_DspSetChannels: sets the dsp's stereo or mono mode
 *****************************************************************************
 * This function acts just like the previous one...
 *****************************************************************************/
int aout_DspSetChannels( aout_thread_t *p_aout )
{
    boolean_t b_stereo = p_aout->b_stereo;

    if ( ioctl( p_aout->i_fd, SNDCTL_DSP_STEREO, &b_stereo ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set number of audio channels (%i)", p_aout->i_channels );
        return( -1 );
    }

    if ( b_stereo != p_aout->b_stereo )
    {
        intf_DbgMsg( "aout debug: number of audio channels not supported (%i)", p_aout->i_channels );
        p_aout->b_stereo = b_stereo;
        p_aout->i_channels = 1 + b_stereo;
    }

    return( 0 );
}

/*****************************************************************************
 * aout_DspSetRate: sets the dsp's audio output rate
 *****************************************************************************
 * This function tries to initialize the dsp with the rate contained in the
 * dsp structure, but if the dsp doesn't support this value, the function uses
 * the value returned by ioctl...
 *****************************************************************************/
int aout_DspSetRate( aout_thread_t *p_aout )
{
    long l_rate;

    l_rate = p_aout->l_rate;
    if ( ioctl( p_aout->i_fd, SNDCTL_DSP_SPEED, &l_rate ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set audio output rate (%li)", p_aout->l_rate );
        return( -1 );
    }

    if ( l_rate != p_aout->l_rate )
    {
        intf_DbgMsg( "aout debug: audio output rate not supported (%li)", p_aout->l_rate );
        p_aout->l_rate = l_rate;
    }

    return( 0 );
}

/*****************************************************************************
 * aout_DspGetBufInfo: buffer status query
 *****************************************************************************
 * This function fills in the audio_buf_info structure :
 * - int fragments : number of available fragments (partially usend ones not
 *   counted)
 * - int fragstotal : total number of fragments allocated
 * - int fragsize : size of a fragment in bytes
 * - int bytes : available space in bytes (includes partially used fragments)
 * Note! 'bytes' could be more than fragments*fragsize
 *****************************************************************************/
long aout_DspGetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{
    ioctl( p_aout->i_fd, SNDCTL_DSP_GETOSPACE, &p_aout->p_sys->audio_buf );

    /* returns the allocated space in bytes */
    return ( (p_aout->p_sys->audio_buf.fragstotal
                 * p_aout->p_sys->audio_buf.fragsize)
            - p_aout->p_sys->audio_buf.bytes );
}

/*****************************************************************************
 * aout_DspPlaySamples: plays a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 *****************************************************************************/
void aout_DspPlaySamples( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    if( p_aout->b_active )
    {
        write( p_aout->i_fd, buffer, i_size );
    }
}

/*****************************************************************************
 * aout_DspClose: closes the dsp audio device
 *****************************************************************************/
void aout_DspClose( aout_thread_t *p_aout )
{
    close( p_aout->i_fd );
}

