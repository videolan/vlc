/*****************************************************************************
 * alsa.c : alsa plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: alsa.c,v 1.15 2002/10/22 20:55:27 sam Exp $
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
    int                 i_period_time;

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
/* Why << 4 ? --Meuuh */

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
    add_string( "alsa-device", "default", aout_FindAndRestart,
                N_("device name"), NULL );
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
    char * psz_device;
    int i_buffer_size = ALSA_DEFAULT_BUFFER_SIZE;
    int i_format = SND_PCM_FORMAT_S16;

    snd_pcm_hw_params_t *p_hw;
    snd_pcm_sw_params_t *p_sw;

    int i_snd_rc = -1;

    /* Allocate structures */
    p_aout->output.p_sys = p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return -1;
    }

    /* Get device name */
    if( (psz_device = config_GetPsz( p_aout, "dspdev" )) == NULL )
    {
        msg_Err( p_aout, "no audio device given (maybe \"default\" ?)" );
        free( p_sys );
        return VLC_EGENERIC;
    }

#ifdef DEBUG
    snd_output_stdio_attach( &p_sys->p_snd_stderr, stderr, 0 );
#endif

    /* Open the device */
    if ( AOUT_FMT_NON_LINEAR( &p_aout->output.output )
          && !strcmp( "default", psz_device ) )
    {
        /* ALSA doesn't understand "default" for S/PDIF. Cheat a little. */
        char psz_iecdev[128];

        if ( !strcmp( "default", psz_device ) )
        {
            snprintf( psz_iecdev, sizeof(psz_iecdev),
                 "iec958:AES0=0x%x,AES1=0x%x,AES2=0x%x,AES3=0x%x",
                 IEC958_AES0_CON_EMPHASIS_NONE | IEC958_AES0_NONAUDIO,
                 IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER,
                 0,
                 (p_aout->output.output.i_rate == 48000 ?
                  IEC958_AES3_CON_FS_48000 :
                  (p_aout->output.output.i_rate == 44100 ?
                   IEC958_AES3_CON_FS_44100 : IEC958_AES3_CON_FS_32000)) );
        }
        else
        {
            strncat( psz_iecdev, psz_device, sizeof(psz_iecdev) );
        }

        if ( (i_snd_rc = snd_pcm_open( &p_sys->p_snd_pcm, psz_iecdev,
                           SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0 )
        {
            /* No S/PDIF. */
            msg_Warn( p_aout, "cannot open S/PDIF ALSA device `%s' (%s)",
                      psz_device, snd_strerror(i_snd_rc) );
            p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
        }
        else
        {
            i_buffer_size = ALSA_SPDIF_BUFFER_SIZE;
            i_format = SND_PCM_FORMAT_S16;

            p_aout->output.i_nb_samples = ALSA_SPDIF_PERIOD_SIZE;
            p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');
            p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
            p_aout->output.output.i_frame_length = A52_FRAME_NB;

            aout_VolumeNoneInit( p_aout );
        }
    }

    if ( !AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        if ( (i_snd_rc = snd_pcm_open( &p_sys->p_snd_pcm, psz_device,
                           SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0 )
        {
            msg_Err( p_aout, "cannot open ALSA device `%s' (%s)",
                             psz_device, snd_strerror(i_snd_rc) );
            free( p_sys );
            free( psz_device );
            return VLC_EGENERIC;
        }

        if ( p_aout->p_libvlc->i_cpu & CPU_CAPABILITY_FPU )
        {
            p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
            i_format = SND_PCM_FORMAT_FLOAT;
        }
        else
        {
            p_aout->output.output.i_format = AOUT_FMT_S16_NE;
            i_format = SND_PCM_FORMAT_S16;
        }

        i_buffer_size = ALSA_DEFAULT_BUFFER_SIZE;
        p_aout->output.i_nb_samples = ALSA_DEFAULT_PERIOD_SIZE;

        aout_VolumeSoftInit( p_aout );
    }

    free( psz_device );
    p_aout->output.pf_play = Play;

    snd_pcm_hw_params_alloca(&p_hw);
    snd_pcm_sw_params_alloca(&p_sw);

    if ( snd_pcm_hw_params_any( p_sys->p_snd_pcm, p_hw ) < 0 )
    {
        msg_Err( p_aout, "unable to retrieve initial hardware parameters" );
        goto error;
    }

    /* Set format. */
    if ( snd_pcm_hw_params_set_format( p_sys->p_snd_pcm, p_hw, i_format ) < 0 )
    {
        msg_Err( p_aout, "unable to set stream sample size and word order" );
        goto error;
    }

    if ( !AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        int i_nb_channels;

        if ( snd_pcm_hw_params_set_access( p_sys->p_snd_pcm, p_hw,
                                       SND_PCM_ACCESS_RW_INTERLEAVED ) < 0 )
        {
            msg_Err( p_aout, "unable to set interleaved stream format" );
            goto error;
        }

        /* Set channels. */
        i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );

        if ( (i_snd_rc = snd_pcm_hw_params_set_channels( p_sys->p_snd_pcm,
                                 p_hw, i_nb_channels )) < 0 )
        {
            msg_Err( p_aout, "unable to set number of output channels" );
            goto error;
        }
        if ( i_snd_rc != i_nb_channels )
        {
            switch ( i_snd_rc )
            {
            case 1: p_aout->output.output.i_channels = AOUT_CHAN_MONO; break;
            case 2: p_aout->output.output.i_channels = AOUT_CHAN_STEREO; break;
            case 4: p_aout->output.output.i_channels = AOUT_CHAN_2F2R; break;
            default:
                msg_Err( p_aout, "Unsupported downmixing (%d)", i_snd_rc );
                goto error;
            }
        }

        /* Set rate. */
        if ( (i_snd_rc = snd_pcm_hw_params_set_rate_near( p_sys->p_snd_pcm,
                                 p_hw, p_aout->output.output.i_rate,
                                 NULL )) < 0 )
        {
            msg_Err( p_aout, "unable to set sample rate" );
            goto error;
        }
        p_aout->output.output.i_rate = i_snd_rc;
    }

    /* Set buffer size. */
    i_snd_rc = snd_pcm_hw_params_set_buffer_size_near( p_sys->p_snd_pcm, p_hw,
                                                       i_buffer_size );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set buffer size" );
        goto error;
    }

    /* Set period size. */
    i_snd_rc = snd_pcm_hw_params_set_period_size_near(
                  p_sys->p_snd_pcm, p_hw, p_aout->output.i_nb_samples, NULL );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set period size" );
        goto error;
    }
    p_aout->output.i_nb_samples = i_snd_rc;

    /* Write hardware configuration. */
    if ( snd_pcm_hw_params( p_sys->p_snd_pcm, p_hw ) < 0 )
    {
        msg_Err( p_aout, "unable to set hardware configuration" );
        goto error;
    }

    p_sys->i_period_time = snd_pcm_hw_params_get_period_time( p_hw, NULL );

    snd_pcm_sw_params_current( p_sys->p_snd_pcm, p_sw );
    i_snd_rc = snd_pcm_sw_params_set_sleep_min( p_sys->p_snd_pcm, p_sw, 0 );

    i_snd_rc = snd_pcm_sw_params_set_avail_min( p_sys->p_snd_pcm, p_sw,
                                                p_aout->output.i_nb_samples );

    /* Write software configuration. */
    if ( snd_pcm_sw_params( p_sys->p_snd_pcm, p_sw ) < 0 )
    {
        msg_Err( p_aout, "unable to set software configuration" );
        goto error;
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
        goto error;
    }

    return 0;

error:
    snd_pcm_close( p_sys->p_snd_pcm );
#ifdef DEBUG
    snd_output_close( p_sys->p_snd_stderr );
#endif
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
}

/*****************************************************************************
 * Close: close the ALSA device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    int i_snd_rc;

    p_aout->b_die = 1;
    vlc_thread_join( p_aout );

    i_snd_rc = snd_pcm_close( p_sys->p_snd_pcm );

    if( i_snd_rc > 0 )
    {
        msg_Err( p_aout, "failed closing ALSA device (%s)",
                         snd_strerror( i_snd_rc ) );
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

        /* Why do we need to sleep ? --Meuuh */
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
        msg_Err( p_aout, "ALSA device not ready !!! (%s)",
                         snd_strerror( i_snd_rc ) );
        return;
    }

    /* Fill in the buffer until space or audio output buffer shortage */
    for ( ; ; )
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
                        (p_aout->output.output.i_format !=
                            VLC_FOURCC('s','p','d','i')) );

            /* Audio output buffer shortage -> stop the fill process and
               wait in ALSAThread */
            if( p_buffer == NULL )
                return;

            i_snd_rc = snd_pcm_writei( p_sys->p_snd_pcm, p_buffer->p_buffer,
                                       p_buffer->i_nb_bytes );

            if( i_snd_rc < 0 )
            {
                msg_Err( p_aout, "write failed (%s)",
                                 snd_strerror( i_snd_rc ) );
            }

            aout_BufferFree( p_buffer );
        }
    }
}

