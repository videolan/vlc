/*****************************************************************************
 * alsa.c : alsa plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: alsa.c,v 1.11 2002/09/18 21:21:23 massiot Exp $
 *
 * Authors: Henri Fallon <henri@videolan.org> - Original Author
 *          Jeffrey Baker <jwbaker@acm.org> - Port to ALSA 1.0 API
 *          John Paul Lorenti <jpl31@columbia.edu> - Device selection
 *          Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr> - S/PDIF and aout3
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

#include "aout_internal.h"

/* ALSA part */
#include <alsa/asoundlib.h>

/*****************************************************************************
 * aout_sys_t: ALSA audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the ALSA specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    snd_pcm_t         * p_snd_pcm;
    snd_pcm_sframes_t   i_buffer_size;
    int                 i_period_time;

    volatile vlc_bool_t b_can_sleek;

#ifdef DEBUG
    snd_output_t      * p_snd_stderr;
#endif
};

#define A52_FRAME_NB 1536

/* These values are in frames.
   To convert them to a number of bytes you have to multiply them by the
   number of channel(s) (eg. 2 for stereo) and the size of a sample (eg.
   2 for s16). */
#define ALSA_DEFAULT_PERIOD_SIZE        2048
#define ALSA_DEFAULT_BUFFER_SIZE        ( ALSA_DEFAULT_PERIOD_SIZE << 4 )
#define ALSA_SPDIF_PERIOD_SIZE          A52_FRAME_NB
#define ALSA_SPDIF_BUFFER_SIZE          ( ALSA_SPDIF_PERIOD_SIZE << 4 )

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Play         ( aout_instance_t * );
static int  ALSAThread   ( aout_instance_t * );
static void ALSAFill     ( aout_instance_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("ALSA"), NULL );
    add_string( "alsa-device", NULL, NULL, N_("device name"), NULL );
    set_description( _("ALSA audio module") );
    set_capability( "audio output", 50 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: create a handle and open an alsa device
 *****************************************************************************
 * This function opens an alsa device, through the alsa API
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;

    int i_snd_rc;

    char * psz_device;
    char psz_alsadev[128];
    char * psz_userdev;

    int i_format;
    int i_channels;

    snd_pcm_hw_params_t *p_hw;
    snd_pcm_sw_params_t *p_sw;

    /* Allocate structures */
    p_aout->output.p_sys = p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return -1;
    }

    p_aout->output.pf_play = Play;

#ifdef DEBUG
    snd_output_stdio_attach( &p_sys->p_snd_stderr, stderr, 0 );
#endif

    /* Read in ALSA device preferences from configuration */
    psz_userdev = config_GetPsz( p_aout, "alsa-device" );

    if( psz_userdev )
    {
        psz_device = psz_userdev;
    }
    else
    {
        /* Use the internal logic to decide on the device name */
        if ( p_aout->output.output.i_format == AOUT_FMT_SPDIF )
        {
            /* Will probably need some little modification in the case
               we want to send some data at a different rate
               (32000, 44100 and 48000 are the possibilities) -- bozo */
            unsigned char s[4];
            s[0] = IEC958_AES0_CON_EMPHASIS_NONE | IEC958_AES0_NONAUDIO;
            s[1] = IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER;
            s[2] = 0;
            s[3] = IEC958_AES3_CON_FS_48000;
            sprintf( psz_alsadev,
                     "iec958:AES0=0x%x,AES1=0x%x,AES2=0x%x,AES3=0x%x",
                     s[0], s[1], s[2], s[3] );
            psz_device = psz_alsadev;

            aout_VolumeNoneInit( p_aout );
        }
        else
        {
            psz_device = "default";

            aout_VolumeSoftInit( p_aout );
        }
    }

    /* Open device */
    i_snd_rc = snd_pcm_open( &p_sys->p_snd_pcm, psz_device,
                             SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "cannot open ALSA device `%s' (%s)",
                         psz_device, snd_strerror(i_snd_rc) );
        if( psz_userdev )
            free( psz_userdev );
        p_sys->p_snd_pcm = NULL;
        return -1;
    }

    if( psz_userdev )
        free( psz_userdev );

    /* Default settings */
    p_sys->b_can_sleek = VLC_FALSE;
    i_channels = p_aout->output.output.i_channels;
    if ( p_aout->output.output.i_format == AOUT_FMT_SPDIF )
    {
        p_sys->i_buffer_size = ALSA_SPDIF_BUFFER_SIZE;
        p_aout->output.i_nb_samples = ALSA_SPDIF_PERIOD_SIZE;
    }
    else
    {
        p_sys->i_buffer_size = ALSA_DEFAULT_BUFFER_SIZE;
        p_aout->output.i_nb_samples = ALSA_DEFAULT_PERIOD_SIZE;
    }


    /* Compute the settings */
    switch (p_aout->output.output.i_format)
    {
        case AOUT_FMT_MU_LAW:    i_format = SND_PCM_FORMAT_MU_LAW; break;
        case AOUT_FMT_A_LAW:     i_format = SND_PCM_FORMAT_A_LAW; break;
        case AOUT_FMT_IMA_ADPCM: i_format = SND_PCM_FORMAT_IMA_ADPCM; break;
        case AOUT_FMT_U8:        i_format = SND_PCM_FORMAT_U8; break;
        case AOUT_FMT_S16_LE:    i_format = SND_PCM_FORMAT_S16_LE; break;
        case AOUT_FMT_S16_BE:    i_format = SND_PCM_FORMAT_S16_BE; break;
        case AOUT_FMT_S8:        i_format = SND_PCM_FORMAT_S8; break;
        case AOUT_FMT_U16_LE:    i_format = SND_PCM_FORMAT_U16_LE; break;
        case AOUT_FMT_U16_BE:    i_format = SND_PCM_FORMAT_U16_BE; break;
        case AOUT_FMT_FLOAT32:   i_format = SND_PCM_FORMAT_FLOAT; break;
        case AOUT_FMT_SPDIF:
            /* Override some settings to make S/PDIF work */
            p_sys->b_can_sleek = VLC_TRUE;
            i_format = SND_PCM_FORMAT_S16_LE;
            i_channels = 2;
            p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
            p_aout->output.output.i_frame_length = ALSA_SPDIF_PERIOD_SIZE;
            break;
        case AOUT_FMT_FIXED32:
        default:
            msg_Err( p_aout, "audio output format 0x%x not supported",
                     p_aout->output.output.i_format );
            return -1;
            break;
    }

    snd_pcm_hw_params_alloca(&p_hw);
    snd_pcm_sw_params_alloca(&p_sw);

    i_snd_rc = snd_pcm_hw_params_any( p_sys->p_snd_pcm, p_hw );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to retrieve initial hardware parameters" );
        return -1;
    }

    i_snd_rc = snd_pcm_hw_params_set_access( p_sys->p_snd_pcm, p_hw,
                                             SND_PCM_ACCESS_RW_INTERLEAVED );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set interleaved stream format" );
        return -1;
    }

    i_snd_rc = snd_pcm_hw_params_set_format( p_sys->p_snd_pcm, p_hw, i_format );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set stream sample size and word order" );
        return -1;
    }

    i_snd_rc = snd_pcm_hw_params_set_channels( p_sys->p_snd_pcm, p_hw,
                                               i_channels );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set number of output channels" );
        return -1;
    }

    i_snd_rc = snd_pcm_hw_params_set_rate( p_sys->p_snd_pcm, p_hw,
                                           p_aout->output.output.i_rate, 0 );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set sample rate" );
        return -1;
    }

    i_snd_rc = snd_pcm_hw_params_set_buffer_size_near( p_sys->p_snd_pcm, p_hw,
                                                       p_sys->i_buffer_size );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set buffer time" );
        return -1;
    }
    p_sys->i_buffer_size = i_snd_rc;

    i_snd_rc = snd_pcm_hw_params_set_period_size_near(
                    p_sys->p_snd_pcm, p_hw, p_aout->output.i_nb_samples, 0 );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set period size" );
        return -1;
    }
    p_aout->output.i_nb_samples = i_snd_rc;
    p_sys->i_period_time = snd_pcm_hw_params_get_period_time( p_hw, 0 );

    i_snd_rc = snd_pcm_hw_params( p_sys->p_snd_pcm, p_hw );
    if (i_snd_rc < 0)
    {
        msg_Err( p_aout, "unable to set hardware configuration" );
        return -1;
    }

    snd_pcm_sw_params_current( p_sys->p_snd_pcm, p_sw );
    i_snd_rc = snd_pcm_sw_params_set_sleep_min( p_sys->p_snd_pcm, p_sw, 0 );

    i_snd_rc = snd_pcm_sw_params_set_avail_min( p_sys->p_snd_pcm, p_sw,
                                                p_aout->output.i_nb_samples );

    i_snd_rc = snd_pcm_sw_params( p_sys->p_snd_pcm, p_sw );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set software configuration" );
        return -1;
    }

#ifdef DEBUG
    snd_output_printf( p_sys->p_snd_stderr, "\nALSA hardware setup:\n\n" );
    snd_pcm_dump_hw_setup( p_sys->p_snd_pcm, p_sys->p_snd_stderr );
    snd_output_printf( p_sys->p_snd_stderr, "\nALSA software setup:\n\n" );
    snd_pcm_dump_sw_setup( p_sys->p_snd_pcm, p_sys->p_snd_stderr );
    snd_output_printf( p_sys->p_snd_stderr, "\n" );
#endif

    /* Create ALSA thread and wait for its readiness. */
    if( vlc_thread_create( p_aout, "aout", ALSAThread,
                           VLC_THREAD_PRIORITY_OUTPUT, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create ALSA thread (%s)", strerror(errno) );
        free( p_sys );
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
}

/*****************************************************************************
 * Close: close the Alsa device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    int i_snd_rc;

    p_aout->b_die = 1;
    vlc_thread_join( p_aout );

    if( p_sys->p_snd_pcm )
    {
        i_snd_rc = snd_pcm_close( p_sys->p_snd_pcm );

        if( i_snd_rc > 0 )
        {
            msg_Err( p_aout, "failed closing ALSA device (%s)",
                             snd_strerror( i_snd_rc ) );
        }
    }

#ifdef DEBUG
    snd_output_close( p_sys->p_snd_stderr );
#endif

    free( p_sys );
}

/*****************************************************************************
 * ALSAThread: asynchronous thread used to DMA the data to the device
 *****************************************************************************/
static int ALSAThread( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    while ( !p_aout->b_die )
    {
        ALSAFill( p_aout );

        /* Sleep during less than one period to avoid a lot of buffer
           underruns */
        msleep( p_sys->i_period_time >> 2 );
    }

    return 0;
}

/*****************************************************************************
 * ALSAFill: function used to fill the ALSA buffer as much as possible
 *****************************************************************************/
static void ALSAFill( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    aout_buffer_t * p_buffer;
    snd_pcm_status_t * p_status;
    snd_timestamp_t ts_next;
    int i_snd_rc;
    snd_pcm_uframes_t i_avail;

    snd_pcm_status_alloca( &p_status );

    /* Wait for the device's readiness (ie. there is enough space in the
       buffer to write at least one complete chunk) */
    i_snd_rc = snd_pcm_wait( p_sys->p_snd_pcm, THREAD_SLEEP );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "alsa device not ready !!! (%s)",
                         snd_strerror( i_snd_rc ) );
        return;
    }

    /* Fill in the buffer until space or audio output buffer shortage */
    while( VLC_TRUE )
    {
        /* Get the status */
        i_snd_rc = snd_pcm_status( p_sys->p_snd_pcm, p_status );
        if( i_snd_rc < 0 )
        {
            msg_Err( p_aout, "unable to get the device's status (%s)",
                             snd_strerror( i_snd_rc ) );
            return;
        }

        /* Handle buffer underruns and reget the status */
        if( snd_pcm_status_get_state( p_status ) == SND_PCM_STATE_XRUN )
        {
            /* Prepare the device */
            i_snd_rc = snd_pcm_prepare( p_sys->p_snd_pcm );

            if( i_snd_rc == 0 )
            {
                msg_Warn( p_aout, "recovered from buffer underrun" );

                /* Reget the status */
                i_snd_rc = snd_pcm_status( p_sys->p_snd_pcm, p_status );
                if( i_snd_rc < 0 )
                {
                    msg_Err( p_aout,
                        "unable to get the device's status after recovery (%s)",
                        snd_strerror( i_snd_rc ) );
                    return;
                }
            }
            else
            {
                msg_Err( p_aout, "unable to recover from buffer underrun" );
                return;
            }
        }

        /* Here the device should be either in the RUNNING state either in
           the PREPARE state. p_status is valid. */

        /* Try to write only if there is enough space */
        i_avail = snd_pcm_status_get_avail( p_status );

        if( i_avail >= p_aout->output.i_nb_samples )
        {
            mtime_t next_date;
            snd_pcm_status_get_tstamp( p_status, &ts_next );
            next_date = (mtime_t)ts_next.tv_sec * 1000000 + ts_next.tv_usec;

            p_buffer = aout_OutputNextBuffer( p_aout, next_date,
                                              p_sys->b_can_sleek );

            /* Audio output buffer shortage -> stop the fill process and
               wait in ALSAThread */
            if( p_buffer == NULL )
                return;

            i_snd_rc = snd_pcm_writei( p_sys->p_snd_pcm, p_buffer->p_buffer,
                                       p_buffer->i_nb_samples );

            if( i_snd_rc < 0 )
            {
                msg_Err( p_aout, "write failed (%s)",
                                 snd_strerror( i_snd_rc ) );
            }
            else
            {
                aout_BufferFree( p_buffer );
            }
        }
    }
}

