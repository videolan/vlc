/*****************************************************************************
 * aout_dsp.c : dsp functions library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: aout_dsp.c,v 1.17 2001/12/07 18:33:07 sam Exp $
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

#define MODULE_NAME dsp
#include "modules_inner.h"

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

#include "common.h"                                     /* boolean_t, byte_t */
#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "audio_output.h"                                   /* aout_thread_t */

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * aout_sys_t: dsp audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the dsp specific properties of an audio device.
 *****************************************************************************/
typedef struct aout_sys_s
{
    audio_buf_info        audio_buf;

} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     aout_Probe       ( probedata_t *p_data );
static int     aout_Open        ( aout_thread_t *p_aout );
static int     aout_SetFormat   ( aout_thread_t *p_aout );
static long    aout_GetBufInfo  ( aout_thread_t *p_aout, long l_buffer_info );
static void    aout_Play        ( aout_thread_t *p_aout,
                                  byte_t *buffer, int i_size );
static void    aout_Close       ( aout_thread_t *p_aout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( aout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = aout_Probe;
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Probe: probe the audio device and return a score
 *****************************************************************************
 * This function tries to open the DSP and returns a score to the plugin
 * manager so that it can choose the most appropriate one.
 *****************************************************************************/
static int aout_Probe( probedata_t *p_data )
{
    char * psz_device = main_GetPszVariable( AOUT_DSP_VAR, AOUT_DSP_DEFAULT );
    int i_fd = open( psz_device, O_WRONLY|O_NONBLOCK );

    /* If we were unable to open the device, there is no way we can use
     * the plugin. Return a score of 0. */
    if( i_fd < 0 )
    {
        return( 0 );
    }

    /* Otherwise, there are good chances we can use this plugin, return 100. */
    close( i_fd );

    if( TestMethod( AOUT_METHOD_VAR, "dsp" ) )
    {
        return( 999 );
    }

    return( 100 );
}

/*****************************************************************************
 * aout_Open: opens the audio device (the digital sound processor)
 *****************************************************************************
 * This function opens the dsp as a usual non-blocking write-only file, and
 * modifies the p_aout->i_fd with the file's descriptor.
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
    p_aout->psz_device = main_GetPszVariable( AOUT_DSP_VAR, AOUT_DSP_DEFAULT );
    p_aout->i_format   = AOUT_FORMAT_DEFAULT;
    p_aout->i_channels = 1 + main_GetIntVariable( AOUT_STEREO_VAR,
                                                  AOUT_STEREO_DEFAULT );
    p_aout->l_rate     = main_GetIntVariable( AOUT_RATE_VAR,
                                              AOUT_RATE_DEFAULT );

    /* Open the sound device */
    if( (p_aout->i_fd = open( p_aout->psz_device, O_WRONLY )) < 0 )
    {
        intf_ErrMsg( "aout error: can't open audio device (%s)",
                     p_aout->psz_device );
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
    long l_rate;
    boolean_t b_stereo = p_aout->b_stereo;

    /* Reset the DSP device */
    if( ioctl( p_aout->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        intf_ErrMsg( "aout error: can't reset audio device (%s)",
                     p_aout->psz_device );
        return( -1 );
    }

    /* Set the output format */
    i_format = p_aout->i_format;
    if( ioctl( p_aout->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set audio output format (%i)",
                     p_aout->i_format );
        return( -1 );
    }

    if( i_format != p_aout->i_format )
    {
        intf_WarnMsg( 2, "aout warning: audio output format not supported (%i)",
                      p_aout->i_format );
        p_aout->i_format = i_format;
    }

    /* Set the number of channels */
    if( ioctl( p_aout->i_fd, SNDCTL_DSP_STEREO, &b_stereo ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set number of audio channels (%i)",
                     p_aout->i_channels );
        return( -1 );
    }

    if( b_stereo != p_aout->b_stereo )
    {
        intf_WarnMsg( 2, "aout warning: number of audio channels not supported (%i)",
                      p_aout->i_channels );
        p_aout->b_stereo = b_stereo;
        p_aout->i_channels = 1 + b_stereo;
    }

    /* Set the output rate */
    l_rate = p_aout->l_rate;
    if( ioctl( p_aout->i_fd, SNDCTL_DSP_SPEED, &l_rate ) < 0 )
    {
        intf_ErrMsg( "aout error: can't set audio output rate (%li)",
                     p_aout->l_rate );
        return( -1 );
    }

    if( l_rate != p_aout->l_rate )
    {
        intf_WarnMsg( 1, "aout warning: audio output rate not supported (%li)",
                      p_aout->l_rate );
        p_aout->l_rate = l_rate;
    }

    p_aout->i_latency = 0;
    
    return( 0 );
}

/*****************************************************************************
 * aout_GetBufInfo: buffer status query
 *****************************************************************************
 * This function fills in the audio_buf_info structure :
 * - int fragments : number of available fragments (partially usend ones not
 *   counted)
 * - int fragstotal : total number of fragments allocated
 * - int fragsize : size of a fragment in bytes
 * - int bytes : available space in bytes (includes partially used fragments)
 * Note! 'bytes' could be more than fragments*fragsize
 *****************************************************************************/
static long aout_GetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{
    ioctl( p_aout->i_fd, SNDCTL_DSP_GETOSPACE, &p_aout->p_sys->audio_buf );

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
        write( p_aout->i_fd, buffer, i_size );
    }
}

/*****************************************************************************
 * aout_Close: closes the dsp audio device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    close( p_aout->i_fd );
}

