/*****************************************************************************
 * aout_qnx.c : QNX audio output 
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 *
 * Authors: Henri Fallon <henri@videolan.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

#define MODULE_NAME qnx 
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <sys/asoundlib.h>

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "audio_output.h"                                   /* aout_thread_t */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
#include "main.h"

#include "modules.h"
#include "modules_export.h"

typedef struct aout_sys_s
{
    snd_pcm_t  * p_pcm_handle;
    int          i_card;
    int          i_device;
} aout_sys_t;

/*****************************************************************************
 * Local prototypes
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
 * aout_Probe: probes the audio device and return a score
 *****************************************************************************
 * This function tries to open the dps and returns a score to the plugin
 * manager so that it can make its choice.
 *****************************************************************************/
static int aout_Probe( probedata_t *p_data )
{
    int i_ret;
    aout_sys_t adev;

    /* open audio device */
    if( ( i_ret = snd_pcm_open_preferred( &adev.p_pcm_handle,
                                          &adev.i_card, &adev.i_device,
                                          SND_PCM_OPEN_PLAYBACK ) ) < 0 )
    {
        intf_WarnMsg( 2, "aout error: unable to open audio device (%s)",
                      snd_strerror( i_ret ) );
        return( 0 );
    }

    /* close audio device */
    if( ( i_ret = snd_pcm_close( adev.p_pcm_handle ) ) < 0 )
    {
        intf_WarnMsg( 2, "aout error: unable to close audio device (%s)",
                      snd_strerror( i_ret ) );
        return( 0 );
    }

    if( TestMethod( AOUT_METHOD_VAR, "qnx" ) )
    {
        return( 999 );
    }

    /* return score */
    return( 50 );
}    

/*****************************************************************************
 * aout_Open : creates a handle and opens an alsa device
 *****************************************************************************
 * This function opens an alsa device, through the alsa API
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    int i_ret;

    /* allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg( "aout error: unable to allocate memory (%s)",
                     strerror( ENOMEM ) );
        return( 1 );
    }

    /* initialize  */
    p_aout->i_format   = AOUT_FORMAT_DEFAULT;
    p_aout->i_channels = main_GetIntVariable( AOUT_STEREO_VAR,
                                              AOUT_STEREO_DEFAULT ) + 1;
    p_aout->l_rate     = main_GetIntVariable( AOUT_RATE_VAR,
                                              AOUT_RATE_DEFAULT );

    /* open audio device */
    if( ( i_ret = snd_pcm_open_preferred( &p_aout->p_sys->p_pcm_handle,
                                          &p_aout->p_sys->i_card,
                                          &p_aout->p_sys->i_device,
                                          SND_PCM_OPEN_PLAYBACK ) ) < 0 )
    {
        intf_ErrMsg( "aout error: unable to open audio device (%s)",
                      snd_strerror( i_ret ) );
        return( 1 );
    }

    /* disable mmap */
    if( ( i_ret = snd_pcm_plugin_set_disable( p_aout->p_sys->p_pcm_handle,
                                              PLUGIN_DISABLE_MMAP ) ) < 0 )
    {
        intf_ErrMsg( "aout error: unable to disable mmap (%s)",
                     snd_strerror( i_ret ) );
        aout_Close( p_aout );
        return( 1 );
    }

    return( 0 );
}


/*****************************************************************************
 * aout_SetFormat : set the audio output format 
 *****************************************************************************
 * This function prepares the device, sets the rate, format, the mode
 * ("play as soon as you have data"), and buffer information.
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
    int i_ret;
    snd_pcm_channel_info_t pi;
    snd_pcm_channel_params_t pp;

    memset( &pi, 0, sizeof(pi) );
    memset( &pp, 0, sizeof(pp) );

    pi.channel = SND_PCM_CHANNEL_PLAYBACK;
    if( ( i_ret = snd_pcm_plugin_info( p_aout->p_sys->p_pcm_handle,
                                       &pi ) ) < 0 )
    {
        intf_ErrMsg( "aout error: unable to get plugin info (%s)",
                     snd_strerror( i_ret ) );
        return( 1 );
    }

    pp.mode       = SND_PCM_MODE_STREAM;
    pp.channel    = SND_PCM_CHANNEL_PLAYBACK;
    pp.start_mode = SND_PCM_START_DATA;
    pp.stop_mode  = SND_PCM_STOP_STOP;

    pp.buf.stream.queue_size = pi.max_fragment_size;
    pp.buf.stream.fill       = SND_PCM_FILL_NONE;
    pp.buf.stream.max_fill   = 0;

    pp.format.interleave     = 1;
    pp.format.rate           = p_aout->l_rate;
    pp.format.voices         = p_aout->i_channels;

    switch( p_aout->i_format )
    {
        case AOUT_FMT_S16_LE:
            pp.format.format = SND_PCM_SFMT_S16_LE;
            break;

        default:
            pp.format.format = SND_PCM_SFMT_S16_BE;
            break;
    }

    /* set parameters */
    if( ( i_ret = snd_pcm_plugin_params( p_aout->p_sys->p_pcm_handle,
                                         &pp ) ) < 0 )
    {
        intf_ErrMsg( "aout error: unable to set parameters (%s)",
                     snd_strerror( i_ret ) );
        return( 1 );
    }

    /* prepare channel */
    if( ( i_ret = snd_pcm_plugin_prepare( p_aout->p_sys->p_pcm_handle,
                                          SND_PCM_CHANNEL_PLAYBACK ) ) < 0 )
    {
        intf_ErrMsg( "aout error: unable to prepare channel (%s)",
                     snd_strerror( i_ret ) );
        return( 1 );
    }

    p_aout->i_latency = 0;

    return( 0 );
}

/*****************************************************************************
 * aout_BufInfo: buffer status query
 *****************************************************************************
 * This function returns the number of used byte in the queue.
 * It also deals with errors : indeed if the device comes to run out
 * of data to play, it switches to the "underrun" status. It has to
 * be flushed and re-prepared
 *****************************************************************************/
static long aout_GetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{
    int i_ret;
    snd_pcm_channel_status_t status;

    /* get current pcm status */
    memset( &status, 0, sizeof(status) );
    if( ( i_ret = snd_pcm_plugin_status( p_aout->p_sys->p_pcm_handle,
                                         &status ) ) < 0 )
    {
        intf_ErrMsg( "aout error: unable to get device status (%s)",
                     snd_strerror( i_ret ) );
        return( -1 );
    }

    /* check for underrun */
    switch( status.status )
    {
        case SND_PCM_STATUS_READY:
        case SND_PCM_STATUS_UNDERRUN:
            if( ( i_ret = snd_pcm_plugin_prepare( p_aout->p_sys->p_pcm_handle,
                                          SND_PCM_CHANNEL_PLAYBACK ) ) < 0 )
            {
                intf_ErrMsg( "aout error: unable to prepare channel (%s)",
                             snd_strerror( i_ret ) );
            }
            break;
    }

    return( status.count );
}

/*****************************************************************************
 * aout_Play : plays a sample
 *****************************************************************************
 * Plays a sample using the snd_pcm_write function from the alsa API
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    int i_ret;

    if( ( i_ret = snd_pcm_plugin_write( p_aout->p_sys->p_pcm_handle,
                                        (void *) buffer, 
                                        (size_t) i_size ) ) <= 0 )
    {
        intf_ErrMsg( "aout error: unable to write data (%s)",
                     snd_strerror( i_ret ) );
    }
}

/*****************************************************************************
 * aout_Close : close the audio device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    int i_ret;

    if( ( i_ret = snd_pcm_close( p_aout->p_sys->p_pcm_handle ) ) < 0 )
    {
        intf_ErrMsg( "aout error: unable to close audio device (%s)",
                     snd_strerror( i_ret ) );
    }

    free( p_aout->p_sys );
}
