/*****************************************************************************
 * alsa.c : alsa plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: alsa.c,v 1.1 2002/08/07 21:36:55 massiot Exp $
 *
 * Authors: Henri Fallon <henri@videolan.org> - Original Author
 *          Jeffrey Baker <jwbaker@acm.org> - Port to ALSA 1.0 API
 *          John Paul Lorenti <jpl31@columbia.edu> - Device selection
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
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>

#include <alsa/asoundlib.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static int  SetFormat    ( aout_thread_t * );
static int  GetBufInfo   ( aout_thread_t *, int );
static void Play         ( aout_thread_t *, byte_t *, int );

static void HandleXrun   ( aout_thread_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("Device"), NULL );
    add_string( "alsa-device", NULL, NULL, N_("Name"), NULL );
    set_description( _("ALSA audio module") );
    set_capability( "audio output", 50 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Preamble
 *****************************************************************************/
typedef struct alsa_device_t
{
    int i_num;
} alsa_device_t;

typedef struct alsa_card_t
{
    int i_num;
} alsa_card_t;

/* here we store plugin dependant informations */

struct aout_sys_t
{
    snd_pcm_t   * p_alsa_handle;
    unsigned long buffer_time;
    unsigned long period_time;
    unsigned long chunk_size;
    unsigned long buffer_size;
    unsigned long rate;
    unsigned int  bytes_per_sample;
    unsigned int  samples_per_frame;
    unsigned int  bytes_per_frame;
};

/*****************************************************************************
 * Open: create a handle and open an alsa device
 *****************************************************************************
 * This function opens an alsa device, through the alsa API
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;

    /* Allows user to choose which ALSA device to use */
    char  psz_alsadev[128];
    char *psz_device, *psz_userdev;
    int   i_ret;

    /* Allocate structures */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return -1;
    }

    p_aout->pf_setformat = SetFormat;
    p_aout->pf_getbufinfo = GetBufInfo;
    p_aout->pf_play = Play;

    /* Read in ALSA device preferences from configuration */
    psz_userdev = config_GetPsz( p_aout, "alsa-device" );

    if( psz_userdev )
    {
        psz_device = psz_userdev;
    }
    else
    {
        /* Use the internal logic to decide on the device name */
        if( p_aout->i_format != AOUT_FMT_A52 )
        {
            psz_device = "default";
        }
        else
        {
            unsigned char s[4];
            s[0] = IEC958_AES0_CON_EMPHASIS_NONE | IEC958_AES0_NONAUDIO;
            s[1] = IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER;
            s[2] = 0;
            s[3] = IEC958_AES3_CON_FS_48000;
            sprintf( psz_alsadev,
                     "iec958:AES0=0x%x,AES1=0x%x,AES2=0x%x,AES3=0x%x",
                     s[0], s[1], s[2], s[3] );
            psz_device = psz_alsadev;
        }
    }

    /* Open device */
    i_ret = snd_pcm_open( &(p_aout->p_sys->p_alsa_handle),
                          psz_device, SND_PCM_STREAM_PLAYBACK, 0);
    if( i_ret != 0 )
    {
        msg_Err( p_aout, "cannot open ALSA device `%s' (%s)",
                         psz_device, snd_strerror(i_ret) );
        if( psz_userdev )
        {
            free( psz_userdev );
        }

        return -1;
    }

    if( psz_userdev )
    {
        free( psz_userdev );
    }

    return 0;
}

/*****************************************************************************
 * SetFormat : sets the alsa output format
 *****************************************************************************
 * This function prepares the device, sets the rate, format, the mode
 * ( "play as soon as you have data" ), and buffer information.
 *****************************************************************************/
static int SetFormat( aout_thread_t *p_aout )
{
    int i_rv;
    int i_format;

    snd_pcm_hw_params_t *p_hw;
    snd_pcm_sw_params_t *p_sw;

    snd_pcm_hw_params_alloca(&p_hw);
    snd_pcm_sw_params_alloca(&p_sw);

    /* default value for snd_pcm_hw_params_set_buffer_time_near() */
    p_aout->p_sys->buffer_time = AOUT_BUFFER_DURATION;

    switch (p_aout->i_format)
    {
        case AOUT_FMT_S16_LE:
            i_format = SND_PCM_FORMAT_S16_LE;
            p_aout->p_sys->bytes_per_sample = 2;
            break;

        case AOUT_FMT_A52:
            i_format = SND_PCM_FORMAT_S16_LE;
            p_aout->p_sys->bytes_per_sample = 2;
            /* buffer_time must be 500000 to avoid a system crash */
            p_aout->p_sys->buffer_time = 500000;
            break;

        default:
            i_format = SND_PCM_FORMAT_S16_BE;
            p_aout->p_sys->bytes_per_sample = 2;
            break;
    }

    p_aout->p_sys->samples_per_frame = p_aout->i_channels;
    p_aout->p_sys->bytes_per_frame = p_aout->p_sys->samples_per_frame *
                                     p_aout->p_sys->bytes_per_sample;

    i_rv = snd_pcm_hw_params_any( p_aout->p_sys->p_alsa_handle, p_hw );
    if( i_rv < 0 )
    {
        msg_Err( p_aout, "unable to retrieve initial parameters" );
        return -1;
    }

    i_rv = snd_pcm_hw_params_set_access( p_aout->p_sys->p_alsa_handle, p_hw,
                                         SND_PCM_ACCESS_RW_INTERLEAVED );
    if( i_rv < 0 )
    {
        msg_Err( p_aout, "unable to set interleaved stream format" );
        return -1;
    }

    i_rv = snd_pcm_hw_params_set_format( p_aout->p_sys->p_alsa_handle,
                                         p_hw, i_format );
    if( i_rv < 0 )
    {
        msg_Err( p_aout, "unable to set stream sample size and word order" );
        return -1;
    }

    i_rv = snd_pcm_hw_params_set_channels( p_aout->p_sys->p_alsa_handle, p_hw,
                                           p_aout->i_channels );
    if( i_rv < 0 )
    {
        msg_Err( p_aout, "unable to set number of output channels" );
        return -1;
    }

    i_rv = snd_pcm_hw_params_set_rate_near( p_aout->p_sys->p_alsa_handle, p_hw,
                                            p_aout->i_rate, 0 );
    if( i_rv < 0 )
    {
        msg_Err( p_aout, "unable to set sample rate" );
        return -1;
    }
    p_aout->p_sys->rate = i_rv;

    i_rv = snd_pcm_hw_params_set_buffer_time_near( p_aout->p_sys->p_alsa_handle,
                                                   p_hw,
                                                   p_aout->p_sys->buffer_time,
                                                   0 );
    if( i_rv < 0 )
    {
        msg_Err( p_aout, "unable to set buffer time" );
        return -1;
    }
    p_aout->p_sys->buffer_time = i_rv;

    i_rv = snd_pcm_hw_params_set_period_time_near( p_aout->p_sys->p_alsa_handle,
         p_hw, p_aout->p_sys->buffer_time / p_aout->p_sys->bytes_per_frame, 0 );
    if( i_rv < 0 )
    {
        msg_Err( p_aout, "unable to set period time" );
        return -1;
    }
    p_aout->p_sys->period_time = i_rv;

    i_rv = snd_pcm_hw_params(p_aout->p_sys->p_alsa_handle, p_hw);
    if (i_rv < 0)
    {
        msg_Err( p_aout, "unable to set hardware configuration" );
        return -1;
    }

    p_aout->p_sys->chunk_size = snd_pcm_hw_params_get_period_size( p_hw, 0 );
    p_aout->p_sys->buffer_size = snd_pcm_hw_params_get_buffer_size( p_hw );

    snd_pcm_sw_params_current( p_aout->p_sys->p_alsa_handle, p_sw );
    i_rv = snd_pcm_sw_params_set_sleep_min( p_aout->p_sys->p_alsa_handle, p_sw,
                                            0 );

    i_rv = snd_pcm_sw_params_set_avail_min( p_aout->p_sys->p_alsa_handle, p_sw,
                                            p_aout->p_sys->chunk_size );

    /* Worked with the CVS version but not with 0.9beta3
    i_rv = snd_pcm_sw_params_set_start_threshold( p_aout->p_sys->p_alsa_handle,
                                            p_sw, p_aout->p_sys->buffer_size );

    i_rv = snd_pcm_sw_params_set_stop_threshold( p_aout->p_sys->p_alsa_handle,
                                             p_sw, p_aout->p_sys->buffer_size);
    */
    i_rv = snd_pcm_sw_params( p_aout->p_sys->p_alsa_handle, p_sw );
    if( i_rv < 0 )
    {
        msg_Err( p_aout, "unable to set software configuration" );
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * HandleXrun : reprepare the output
 *****************************************************************************
 * When buffer gets empty, the driver goes in "Xrun" state, where it needs
 * to be reprepared before playing again
 *****************************************************************************/
static void HandleXrun(aout_thread_t *p_aout)
{
    int i_rv;

    msg_Err( p_aout, "resetting output after buffer underrun" );

//    i_rv = snd_pcm_reset( p_aout->p_sys->p_alsa_handle );
    i_rv = snd_pcm_prepare( p_aout->p_sys->p_alsa_handle );
    if( i_rv < 0 )
    {
        msg_Err( p_aout, "unable to recover from buffer underrun (%s)",
                         snd_strerror( i_rv ) );
    }
}


/*****************************************************************************
 * BufInfo: buffer status query
 *****************************************************************************
 * This function returns the number of used byte in the queue.
 * It also deals with errors : indeed if the device comes to run out
 * of data to play, it switches to the "underrun" status. It has to
 * be flushed and re-prepared
 *****************************************************************************/
static int GetBufInfo( aout_thread_t *p_aout, int i_buffer_limit )
{
    snd_pcm_status_t *p_status;
    int i_alsa_get_status_returns;

    snd_pcm_status_alloca( &p_status );

    i_alsa_get_status_returns = snd_pcm_status( p_aout->p_sys->p_alsa_handle,
                                                p_status );

    if( i_alsa_get_status_returns )
    {
        msg_Err( p_aout, "failed getting alsa buffer info (%s)",
                         snd_strerror ( i_alsa_get_status_returns ) );
        return ( -1 );
    }

    switch( snd_pcm_status_get_state( p_status ) )
    {
        case SND_PCM_STATE_XRUN :
            HandleXrun( p_aout );
            break;

        case SND_PCM_STATE_OPEN:
        case SND_PCM_STATE_PREPARED:
        case SND_PCM_STATE_RUNNING:
            break;

        default:
            msg_Err( p_aout, "unhandled condition %i",
                             snd_pcm_status_get_state( p_status ) );
            break;
    }

    return snd_pcm_status_get_avail(p_status) * p_aout->p_sys->bytes_per_frame;
}

/*****************************************************************************
 * Play : plays a sample
 *****************************************************************************
 * Plays a sample using the snd_pcm_writei function from the alsa API
 *****************************************************************************/
static void Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    snd_pcm_uframes_t tot_frames;
    snd_pcm_uframes_t frames_left;
    snd_pcm_uframes_t rv;

    tot_frames = i_size / p_aout->p_sys->bytes_per_frame;
    frames_left = tot_frames;

    while( frames_left > 0 )
    {
        rv = snd_pcm_writei( p_aout->p_sys->p_alsa_handle, buffer +
                             (tot_frames - frames_left) *
                             p_aout->p_sys->bytes_per_frame, frames_left );

        if( (signed int) rv < 0 )
        {
            msg_Err( p_aout, "failed writing to output (%s)",
                             snd_strerror( rv ) );
            return;
        }

        frames_left -= rv;
    }
}

/*****************************************************************************
 * Close: close the Alsa device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;
    int i_close_returns;

    i_close_returns = snd_pcm_close( p_aout->p_sys->p_alsa_handle );

    if( i_close_returns )
    {
        msg_Err( p_aout, "failed closing ALSA device (%s)",
                         snd_strerror( i_close_returns ) );
    }

    free( p_aout->p_sys );
}

