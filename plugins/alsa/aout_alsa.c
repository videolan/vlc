/*****************************************************************************
 * aout_alsa.c : Alsa functions library
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
 *  Henri Fallon <henri@videolan.org>
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

#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <sys/asoundlib.h>
#include <linux/asound.h>

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "audio_output.h"                                   /* aout_thread_t */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
#include "main.h"




typedef struct alsa_device_s
{
    int i_num;
} alsa_device_t;

typedef struct alsa_card_s
{
    int i_num;
} alsa_card_t;

/* here we store plugin dependant informations */

typedef struct aout_sys_s
{
    snd_pcm_t         * p_alsa_handle;
    alsa_device_t       s_alsa_device;
    alsa_card_t         s_alsa_card;
    snd_pcm_channel_params_t s_alsa_channel_params;
    snd_pcm_format_t    s_alsa_format;
} aout_sys_t;



/*****************************************************************************
 * aout_AlsaOpen : creates a handle and opens an alsa device
 *****************************************************************************/

int aout_AlsaOpen( aout_thread_t *p_aout )
{

    int i_open_returns;
    
    /* Allocate structures */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg( "Alsa Plugin : Could not allocate memory" );
        intf_ErrMsg( "error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Initialize  */
    p_aout->p_sys->s_alsa_device.i_num = 0;
    p_aout->p_sys->s_alsa_card.i_num = 0;
    /* FIXME : why not other format ? */
    p_aout->i_format = AOUT_FMT_S16_LE;   
    /* FIXME : why always 2 channels ?*/
    p_aout->i_channels = 2;
    p_aout->l_rate = main_GetIntVariable( AOUT_RATE_VAR, AOUT_RATE_DEFAULT );
    
    /* Open device */
    if ( ( i_open_returns = snd_pcm_open( &(p_aout->p_sys->p_alsa_handle),
                p_aout->p_sys->s_alsa_card.i_num,
                p_aout->p_sys->s_alsa_device.i_num,
                SND_PCM_OPEN_PLAYBACK ) ) )
    {
        intf_ErrMsg ( "Could not open alsa device; exit = %i",
                      i_open_returns );
        intf_ErrMsg ( "This means : %s", snd_strerror(i_open_returns) );
        return ( 1 );
    }

    intf_DbgMsg("Alsa plugin : Alsa device successfully opened");
    return ( 0 );
}


/*****************************************************************************
 * aout_AlsaSetFormat : sets the alsa output format
 *****************************************************************************/

int aout_AlsaSetFormat ( aout_thread_t *p_aout )
{
    
    int i_set_param_returns;
    int i_prepare_playback_returns;
    int i_playback_go_returns;

    /* Fill with zeros */
    memset(&p_aout->p_sys->s_alsa_channel_params,0,
            sizeof(p_aout->p_sys->s_alsa_channel_params));
    
    /* Fill the s_alsa_channel_params structure */

    /* Tranfer mode and direction*/    
    p_aout->p_sys->s_alsa_channel_params.channel = SND_PCM_CHANNEL_PLAYBACK ;
    p_aout->p_sys->s_alsa_channel_params.mode = SND_PCM_MODE_STREAM;
    
    /* Format and rate */
    p_aout->p_sys->s_alsa_channel_params.format.interleave = 1;
    if ( p_aout->i_format == AOUT_FMT_S16_LE )
        p_aout->p_sys->s_alsa_channel_params.format.format = 
            SND_PCM_SFMT_S16_LE;
    else
        p_aout->p_sys->s_alsa_channel_params.format.format = 
            SND_PCM_SFMT_S16_BE;
    p_aout->p_sys->s_alsa_channel_params.format.rate = p_aout->l_rate;
    p_aout->p_sys->s_alsa_channel_params.format.voices = p_aout->i_channels ;
    
    /* When to start playing and when to stop */
    p_aout->p_sys->s_alsa_channel_params.start_mode = SND_PCM_START_DATA;
    p_aout->p_sys->s_alsa_channel_params.stop_mode = SND_PCM_STOP_STOP;

    /* Buffer information . I have chosen the stream mode here
     * instead of the block mode. I don't know whether i'm wrong 
     * but it seemed more logical */
    /* TODO : find the best value to put here. Probably depending
     * on many parameters */
    p_aout->p_sys->s_alsa_channel_params.buf.stream.queue_size = 131072; 
    
    p_aout->p_sys->s_alsa_channel_params.buf.stream.fill = SND_PCM_FILL_NONE ;
    p_aout->p_sys->s_alsa_channel_params.buf.stream.max_fill = 0 ; 
  
    /* Now we pass this to the driver */
    i_set_param_returns = snd_pcm_channel_params ( 
            p_aout->p_sys->p_alsa_handle, 
            &(p_aout->p_sys->s_alsa_channel_params) );
    
    if ( i_set_param_returns )
    {
        intf_ErrMsg ( "ALSA_PLUGIN : Unable to set parameters; exit = %i",
                i_set_param_returns );
        intf_ErrMsg( "This means : %s",
                snd_strerror( i_set_param_returns ) );
        return ( 1 );
    }

    /* we shall now prepare the channel */
    i_prepare_playback_returns = 
        snd_pcm_playback_prepare ( p_aout->p_sys->p_alsa_handle );

    if ( i_prepare_playback_returns )
    {
        intf_ErrMsg ( "ALSA_PLUGIN : Unable to prepare channel : exit = %i",
                i_prepare_playback_returns );
        intf_ErrMsg( "This means : %s",
                snd_strerror( i_set_param_returns ) );

        return ( 1 );
    }
    
   /* then we may go */
   i_playback_go_returns =
       snd_pcm_playback_go ( p_aout->p_sys->p_alsa_handle );
    if ( i_playback_go_returns )
    {
        intf_ErrMsg ( "ALSA_PLUGIN : Unable to prepare channel (bis) : 
                exit  = %i", i_playback_go_returns );
        intf_ErrMsg( "This means : %s",
                snd_strerror( i_set_param_returns ) );
        return ( 1 );
    }
    return ( 0 );
}

/*****************************************************************************
 * aout_AlsaReset: resets the dsp
 *****************************************************************************/
int aout_AlsaReset ( aout_thread_t *p_aout )
{
    /* TODO : put something in here, such as close and open again 
     * or check status, drain, flush, .... */ 
    return ( 0 );
}

/*****************************************************************************
 * aout_AlsaSetChannels: sets mono, stereo and other modes
 *****************************************************************************/
int aout_AlsaSetChannels ( aout_thread_t *p_aout )
{
    /* TODO : normally, nothing
     * everything should be done in the AlsaSetFormat, as far a I understand
     * the alsa documentation
     */
    return ( 0 );
}

/*****************************************************************************
 * aout_AlsaSetRate: sets the audio output rate
 *****************************************************************************
 * As in the previous function, the rate is supposed to be set in the
 * AlsaSetFormat function
 *****************************************************************************/
int aout_AlsaSetRate ( aout_thread_t *p_aout )
{
    return ( 0 );
}

/*****************************************************************************
 * aout_AlsaGetBufInfo: buffer status query
 *****************************************************************************/
long aout_AlsaGetBufInfo ( aout_thread_t *p_aout, long l_buffer_limit )
{
    snd_pcm_channel_status_t alsa_channel_status;
    int i_alsa_get_status_returns;
    
    memset (&alsa_channel_status, 0, sizeof(alsa_channel_status));
   
    i_alsa_get_status_returns = snd_pcm_channel_status ( 
            p_aout->p_sys->p_alsa_handle, &alsa_channel_status );

    if ( i_alsa_get_status_returns )
    {
        intf_ErrMsg ( "Error getting alsa buffer info; exit=%i",
                i_alsa_get_status_returns );
        intf_ErrMsg ( "This means : %s",
                snd_strerror ( i_alsa_get_status_returns ) );
        return ( 1 );
    }

    switch (alsa_channel_status.status)
    {
        case SND_PCM_STATUS_NOTREADY : intf_ErrMsg("Status NOT READY");
                                       break;
        case SND_PCM_STATUS_UNDERRUN : {
                                       int i_prepare_returns;
                                       intf_ErrMsg(
                                  "Status UNDERRUN ... reseting queue");
                                       i_prepare_returns = 
                                           snd_pcm_playback_prepare(
                                               p_aout->p_sys->p_alsa_handle );
                                       if ( i_prepare_returns )
                                       {
                                           intf_ErrMsg(
                                  "Error : could not flush : %i",
                                  i_prepare_returns);
                                           intf_ErrMsg(
                                  "This means : %s",
                                  snd_strerror(i_prepare_returns));
                                       }
                                       break;
                                       }
    } 
    return (  alsa_channel_status.count );
}

/*****************************************************************************
 * aout_AlsaPlaySamples
 *****************************************************************************/
void aout_AlsaPlaySamples ( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    int i_write_returns;

    i_write_returns = (int) snd_pcm_write (
            p_aout->p_sys->p_alsa_handle, (void *)buffer, (size_t) i_size );

    if ( i_write_returns <= 0 )
    {
        intf_ErrMsg ( "Error writing blocks; exit=%i", i_write_returns );
        intf_ErrMsg ( "This means : %s", snd_strerror( i_write_returns ) );
    }
}

/*****************************************************************************
 * aout_AlsaClose : close the Alsa device
 *****************************************************************************/
void aout_AlsaClose ( aout_thread_t *p_aout )
{
    int i_close_returns;

    i_close_returns = snd_pcm_close ( p_aout->p_sys->p_alsa_handle );

    if ( i_close_returns )
    {
        intf_ErrMsg( "Error closing alsa device; exit=%i",i_close_returns );
        intf_ErrMsg( "This means : %s",snd_strerror( i_close_returns ) );
    }
    free(p_aout->p_sys);
    
    intf_DbgMsg( "Alsa plugin : Alsa device closed");
}
