/*****************************************************************************
 * aout_dsp.c : dsp functions library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: aout_dsp.c,v 1.26 2002/03/11 07:23:09 gbazin Exp $
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

/* TODO:
 *
 * - an aout_GetFormats() function
 * - dsp inline/static
 * - make this library portable (see mpg123)
 *
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <sys/ioctl.h>                                            /* ioctl() */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <videolan/vlc.h>

/* SNDCTL_DSP_RESET, SNDCTL_DSP_SETFMT, SNDCTL_DSP_STEREO, SNDCTL_DSP_SPEED,
 * SNDCTL_DSP_GETOSPACE */
#ifdef HAVE_SOUNDCARD_H
#   include <soundcard.h>
#elif defined( HAVE_SYS_SOUNDCARD_H )
#   include <sys/soundcard.h>
#elif defined( HAVE_MACHINE_SOUNDCARD_H )
#   include <machine/soundcard.h>
#endif

#include "audio_output.h"                                   /* aout_thread_t */

/*****************************************************************************
 * aout_sys_t: dsp audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the dsp specific properties of an audio device.
 *****************************************************************************/
typedef struct aout_sys_s
{
    audio_buf_info        audio_buf;

    /* Path to the audio output device */
    char *                psz_device;
    int                   i_fd;

} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     aout_Open        ( aout_thread_t *p_aout );
static int     aout_SetFormat   ( aout_thread_t *p_aout );
static int     aout_GetBufInfo  ( aout_thread_t *p_aout, int i_buffer_info );
static void    aout_Play        ( aout_thread_t *p_aout,
                                  byte_t *buffer, int i_size );
static void    aout_Close       ( aout_thread_t *p_aout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( aout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Open: opens the audio device (the digital sound processor)
 *****************************************************************************
 * This function opens the dsp as a usual non-blocking write-only file, and
 * modifies the p_aout->p_sys->i_fd with the file's descriptor.
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg("aout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Initialize some variables */
    if( !(p_aout->p_sys->psz_device = config_GetPszVariable( "dsp_dev" )) )
    {
        intf_ErrMsg( "aout error: don't know which audio device to open" );
        free( p_aout->p_sys );
        return( -1 );
    }

    /* Open the sound device */
    if( (p_aout->p_sys->i_fd = open( p_aout->p_sys->psz_device, O_WRONLY ))
        < 0 )
    {
        intf_ErrMsg( "aout error: can't open audio device (%s)",
                     p_aout->p_sys->psz_device );
        free( p_aout->p_sys->psz_device );
        free( p_aout->p_sys );
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: resets the dsp and sets its format
 *****************************************************************************
 * This functions resets the DSP device, tries to initialize the output
 * format with the value contained in the dsp structure, and if this value
 * could not be set, the default value returned by ioctl is set. It then
 * does the same for the stereo mode, and for the output rate.
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
    int i_format;
    int i_rate;
    boolean_t b_stereo;

    /* Reset the DSP device */
    if( ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        intf_ErrMsg( "aout error: can't reset audio device (%s)",
                     p_aout->p_sys->psz_device );
        return( -1 );
    }

    /* Set the output format */
    i_format = p_aout->i_format;
    if( ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set audio output format (%i)",
                     p_aout->i_format );
        return( -1 );
    }

    if( i_format != p_aout->i_format )
    {
        intf_WarnMsg( 2, "aout warning: audio output format not supported (%i)"
                      ,p_aout->i_format );
        p_aout->i_format = i_format;
    }

    /* Set the number of channels */
    b_stereo = ( p_aout->i_channels >= 2 );

    if( ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_STEREO, &b_stereo ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set number of audio channels (%i)",
                     p_aout->i_channels );
        return( -1 );
    }

    if( (1 + b_stereo) != p_aout->i_channels )
    {
        intf_WarnMsg( 2, "aout warning: %i audio channels not supported",
                      p_aout->i_channels );
        p_aout->i_channels = 1 + b_stereo;
    }

    /* Set the output rate */
    i_rate = p_aout->i_rate;
    if( ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_SPEED, &i_rate ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set audio output rate (%i)",
                     p_aout->i_rate );
        return( -1 );
    }

    if( i_rate != p_aout->i_rate )
    {
        intf_WarnMsg( 1, "aout warning: audio output rate not supported (%li)",
                      p_aout->i_rate );
        p_aout->i_rate = i_rate;
    }

    return( 0 );
}

/*****************************************************************************
 * aout_GetBufInfo: buffer status query
 *****************************************************************************
 * This function fills in the audio_buf_info structure :
 * - returns : number of available fragments (not partially used ones)
 * - int fragstotal : total number of fragments allocated
 * - int fragsize : size of a fragment in bytes
 * - int bytes : available space in bytes (includes partially used fragments)
 * Note! 'bytes' could be more than fragments*fragsize
 *****************************************************************************/
static int aout_GetBufInfo( aout_thread_t *p_aout, int i_buffer_limit )
{
    ioctl( p_aout->p_sys->i_fd, SNDCTL_DSP_GETOSPACE,
           &p_aout->p_sys->audio_buf );

    /* returns the allocated space in bytes */
    return ( (p_aout->p_sys->audio_buf.fragstotal
                 * p_aout->p_sys->audio_buf.fragsize)
            - p_aout->p_sys->audio_buf.bytes );
}

/*****************************************************************************
 * aout_Play: plays a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    if( p_aout->b_active )
    {
        int i_tmp;
        i_tmp = write( p_aout->p_sys->i_fd, buffer, i_size );
	if( i_tmp < 0 )
	    intf_ErrMsg("aout error: %s", strerror(ENOMEM) );

    }
}

/*****************************************************************************
 * aout_Close: closes the dsp audio device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    close( p_aout->p_sys->i_fd );
    free( p_aout->p_sys->psz_device );
}
