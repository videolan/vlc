/*****************************************************************************
 * alsa.c : alsa plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: alsa.c,v 1.25 2003/03/30 18:14:36 gbazin Exp $
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

/* ALSA part
   Note: we use the new API which is available since 0.9.0beta10a. */
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
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
/* Why not ? --Bozo */
/* Right. --Meuuh */

#define DEFAULT_ALSA_DEVICE "default"

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
    add_category_hint( N_("ALSA"), NULL, VLC_FALSE );
    add_string( "alsadev", DEFAULT_ALSA_DEVICE, aout_FindAndRestart,
                N_("ALSA device name"), NULL, VLC_FALSE );
    set_description( _("ALSA audio output") );
    set_capability( "audio output", 50 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Probe: probe the audio device for available formats and channels
 *****************************************************************************/
static void Probe( aout_instance_t * p_aout,
                   const char * psz_device, const char * psz_iec_device,
                   int i_snd_pcm_format )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    vlc_value_t val;

    var_Create ( p_aout, "audio-device", VLC_VAR_STRING | VLC_VAR_HASCHOICE );

    /* Now test linear PCM capabilities */
    if ( !snd_pcm_open( &p_sys->p_snd_pcm, psz_device,
                        SND_PCM_STREAM_PLAYBACK, 0 ) )
    {
        int i_channels;
        snd_pcm_hw_params_t * p_hw;
        snd_pcm_hw_params_alloca (&p_hw);

        if ( snd_pcm_hw_params_any( p_sys->p_snd_pcm, p_hw ) < 0 )
        {
            msg_Warn( p_aout, "unable to retrieve initial hardware parameters"
                              ", disabling linear PCM audio" );
            snd_pcm_close( p_sys->p_snd_pcm );
            return;
        }

        if ( snd_pcm_hw_params_set_format( p_sys->p_snd_pcm, p_hw,
                                           i_snd_pcm_format ) < 0 )
        {
            /* Assume a FPU enabled computer can handle float32 format.
               If somebody tells us it's not always true then we'll have
               to change this */
            msg_Warn( p_aout, "unable to set stream sample size and word order"
                              ", disabling linear PCM audio" );
            snd_pcm_close( p_sys->p_snd_pcm );
            return;
        }

        i_channels = aout_FormatNbChannels( &p_aout->output.output );

        while ( i_channels > 0 )
        {
            /* Here we have to probe multi-channel capabilities but I have
               no idea (at the moment) of how its managed by the ALSA
               library.
               It seems that '6' channels aren't well handled on a stereo
               sound card like my i810 but it requires some more
               investigations. That's why '4' and '6' cases are disabled.
               -- Bozo */
            if ( !snd_pcm_hw_params_test_channels( p_sys->p_snd_pcm, p_hw,
                                                   i_channels ) )
            {
                switch ( i_channels )
                {
                case 1:
                    val.psz_string = N_("Mono");
                    var_Change( p_aout, "audio-device",
                                VLC_VAR_ADDCHOICE, &val );
                    break;
                case 2:
                    val.psz_string = N_("Stereo");
                    var_Change( p_aout, "audio-device",
                                VLC_VAR_ADDCHOICE, &val );
                    break;
/*
                case 4:
                    val.psz_string = N_("2 Front 2 Rear");
                    var_Change( p_aout, "audio-device",
                                VLC_VAR_ADDCHOICE, &val );
                    break;
                case 6:
                    val.psz_string = N_("5.1");
                    var_Change( p_aout, "audio-device",
                                VLC_VAR_ADDCHOICE, &val );
                    break;
*/
                }
            }

            --i_channels;
        }

        /* Close the previously opened device */
        snd_pcm_close( p_sys->p_snd_pcm );
    }

    /* Test for S/PDIF device if needed */
    if ( psz_iec_device )
    {
        /* Opening the device should be enough */
        if ( !snd_pcm_open( &p_sys->p_snd_pcm, psz_iec_device,
                                SND_PCM_STREAM_PLAYBACK, 0 ) )
        {
            val.psz_string = N_("A/52 over S/PDIF");
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );
            snd_pcm_close( p_sys->p_snd_pcm );
            if( config_GetInt( p_aout, "spdif" ) )
                var_Set( p_aout, "audio-device", val );
        }
    }

    /* Add final settings to the variable */
    var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart, NULL );
    val.b_bool = VLC_TRUE;
    var_Set( p_aout, "intf-change", val );
}

/*****************************************************************************
 * Open: create a handle and open an alsa device
 *****************************************************************************
 * This function opens an alsa device, through the alsa API.
 *
 * Note: the only heap-allocated string is psz_device. All the other pointers
 * are references to psz_device or to stack-allocated data.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;
    vlc_value_t val;

    char psz_default_iec_device[128]; /* Buffer used to store the default
                                         S/PDIF device */
    char * psz_device, * psz_iec_device; /* device names for PCM and S/PDIF
                                            output */

    int i_vlc_pcm_format; /* Audio format for VLC's data */
    int i_snd_pcm_format; /* Audio format for ALSA's data */

    snd_pcm_uframes_t i_buffer_size = 0;
    snd_pcm_uframes_t i_period_size = 0;
    int i_channels = 0;

    snd_pcm_hw_params_t *p_hw;
    snd_pcm_sw_params_t *p_sw;

    int i_snd_rc = -1;

    /* Allocate structures */
    p_aout->output.p_sys = p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_ENOMEM;
    }

    /* Get device name */
    if( (psz_device = config_GetPsz( p_aout, "alsadev" )) == NULL )
    {
        msg_Err( p_aout, "no audio device given (maybe \"default\" ?)" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Choose the IEC device for S/PDIF output:
       if the device is overriden by the user then it will be the one
       otherwise we compute the default device based on the output format. */
    if( AOUT_FMT_NON_LINEAR( &p_aout->output.output )
        && !strcmp( psz_device, DEFAULT_ALSA_DEVICE ) )
    {
        snprintf( psz_default_iec_device, sizeof(psz_default_iec_device),
                  "iec958:AES0=0x%x,AES1=0x%x,AES2=0x%x,AES3=0x%x",
                  IEC958_AES0_CON_EMPHASIS_NONE | IEC958_AES0_NONAUDIO,
                  IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER,
                  0,
                  ( p_aout->output.output.i_rate == 48000 ?
                    IEC958_AES3_CON_FS_48000 :
                    ( p_aout->output.output.i_rate == 44100 ?
                      IEC958_AES3_CON_FS_44100 : IEC958_AES3_CON_FS_32000 ) ) );
        psz_iec_device = psz_default_iec_device;
    }
    else if( AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        psz_iec_device = psz_device;
    }
    else
    {
        psz_iec_device = NULL;
    }

    /* Choose the linear PCM format (read the comment above about FPU
       and float32) */
    if( p_aout->p_libvlc->i_cpu & CPU_CAPABILITY_FPU )
    {
        i_vlc_pcm_format = VLC_FOURCC('f','l','3','2');
        i_snd_pcm_format = SND_PCM_FORMAT_FLOAT;
    }
    else
    {
        i_vlc_pcm_format = AOUT_FMT_S16_NE;
        i_snd_pcm_format = SND_PCM_FORMAT_S16;
    }

    /* If the variable doesn't exist then it's the first time we're called
       and we have to probe the available audio formats and channels */
    if ( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout, psz_device, psz_iec_device, i_snd_pcm_format );
    }

    if ( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        free( p_sys );
        free( psz_device );
        return VLC_EGENERIC;
    }

    if ( !strcmp( val.psz_string, N_("A/52 over S/PDIF") ) )
    {
        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');
    }
    else if ( !strcmp( val.psz_string, N_("5.1") ) )
    {
        p_aout->output.output.i_format = i_vlc_pcm_format;
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
               | AOUT_CHAN_LFE;
    }
    else if ( !strcmp( val.psz_string, N_("2 Front 2 Rear") ) )
    {
        p_aout->output.output.i_format = i_vlc_pcm_format;
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
    }
    else if ( !strcmp( val.psz_string, N_("Stereo") ) )
    {
        p_aout->output.output.i_format = i_vlc_pcm_format;
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }
    else if ( !strcmp( val.psz_string, N_("Mono") ) )
    {
        p_aout->output.output.i_format = i_vlc_pcm_format;
        p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
    }

    else
    {
        /* This should not happen ! */
        msg_Err( p_aout, "internal: can't find audio-device (%s)",
                 val.psz_string );
        free( p_sys );
        free( val.psz_string );
        return VLC_EGENERIC;
    }
    free( val.psz_string );

#ifdef DEBUG
    snd_output_stdio_attach( &p_sys->p_snd_stderr, stderr, 0 );
#endif

    /* Open the device */
    if ( AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        if ( ( i_snd_rc = snd_pcm_open( &p_sys->p_snd_pcm, psz_iec_device,
                            SND_PCM_STREAM_PLAYBACK, 0 ) ) < 0 )
        {
            msg_Err( p_aout, "cannot open ALSA device `%s' (%s)",
                             psz_iec_device, snd_strerror( i_snd_rc ) );
            free( p_sys );
            free( psz_device );
            return VLC_EGENERIC;
        }
        i_buffer_size = ALSA_SPDIF_BUFFER_SIZE;
        i_snd_pcm_format = SND_PCM_FORMAT_S16;
        i_channels = 2;

        p_aout->output.i_nb_samples = i_period_size = ALSA_SPDIF_PERIOD_SIZE;
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->output.output.i_frame_length = A52_FRAME_NB;

        aout_VolumeNoneInit( p_aout );
    }
    else
    {
        if ( ( i_snd_rc = snd_pcm_open( &p_sys->p_snd_pcm, psz_device,
                            SND_PCM_STREAM_PLAYBACK, 0 ) ) < 0 )
        {
            msg_Err( p_aout, "cannot open ALSA device `%s' (%s)",
                             psz_device, snd_strerror( i_snd_rc ) );
            free( p_sys );
            free( psz_device );
            return VLC_EGENERIC;
        }
        i_buffer_size = ALSA_DEFAULT_BUFFER_SIZE;
        i_channels = aout_FormatNbChannels( &p_aout->output.output );

        p_aout->output.i_nb_samples = i_period_size = ALSA_DEFAULT_PERIOD_SIZE;

        aout_VolumeSoftInit( p_aout );
    }

    /* Free psz_device so that all the remaining data is stack-allocated */
    free( psz_device );

    p_aout->output.pf_play = Play;

    snd_pcm_hw_params_alloca(&p_hw);
    snd_pcm_sw_params_alloca(&p_sw);

    /* Get Initial hardware parameters */
    if ( ( i_snd_rc = snd_pcm_hw_params_any( p_sys->p_snd_pcm, p_hw ) ) < 0 )
    {
        msg_Err( p_aout, "unable to retrieve initial hardware parameters (%s)",
                         snd_strerror( i_snd_rc ) );
        goto error;
    }

    /* Set format. */
    if ( ( i_snd_rc = snd_pcm_hw_params_set_format( p_sys->p_snd_pcm, p_hw,
                                                    i_snd_pcm_format ) ) < 0 )
    {
        msg_Err( p_aout, "unable to set stream sample size and word order (%s)",
                         snd_strerror( i_snd_rc ) );
        goto error;
    }

    if ( ( i_snd_rc = snd_pcm_hw_params_set_access( p_sys->p_snd_pcm, p_hw,
                                    SND_PCM_ACCESS_RW_INTERLEAVED ) ) < 0 )
    {
        msg_Err( p_aout, "unable to set interleaved stream format (%s)",
                         snd_strerror( i_snd_rc ) );
        goto error;
    }

    /* Set channels. */
    if ( ( i_snd_rc = snd_pcm_hw_params_set_channels( p_sys->p_snd_pcm, p_hw,
                                                      i_channels ) ) < 0 )
    {
        msg_Err( p_aout, "unable to set number of output channels (%s)",
                         snd_strerror( i_snd_rc ) );
        goto error;
    }

    /* Set rate. */
#ifdef HAVE_ALSA_NEW_API
    if ( ( i_snd_rc = snd_pcm_hw_params_set_rate_near( p_sys->p_snd_pcm, p_hw,
                                &p_aout->output.output.i_rate, NULL ) ) < 0 )
#else
    if ( ( i_snd_rc = snd_pcm_hw_params_set_rate_near( p_sys->p_snd_pcm, p_hw,
                                p_aout->output.output.i_rate, NULL ) ) < 0 )
#endif
    {
        msg_Err( p_aout, "unable to set sample rate (%s)",
                         snd_strerror( i_snd_rc ) );
        goto error;
    }

    /* Set buffer size. */
#ifdef HAVE_ALSA_NEW_API
    if ( ( i_snd_rc = snd_pcm_hw_params_set_buffer_size_near( p_sys->p_snd_pcm,
                                    p_hw, &i_buffer_size ) ) < 0 )
#else
    if ( ( i_snd_rc = snd_pcm_hw_params_set_buffer_size_near( p_sys->p_snd_pcm,
                                    p_hw, i_buffer_size ) ) < 0 )
#endif
    {
        msg_Err( p_aout, "unable to set buffer size (%s)",
                         snd_strerror( i_snd_rc ) );
        goto error;
    }

    /* Set period size. */
#ifdef HAVE_ALSA_NEW_API
    if ( ( i_snd_rc = snd_pcm_hw_params_set_period_size_near( p_sys->p_snd_pcm,
                                    p_hw, &i_period_size, NULL ) ) < 0 )
#else
    if ( ( i_snd_rc = snd_pcm_hw_params_set_period_size_near( p_sys->p_snd_pcm,
                                    p_hw, i_period_size, NULL ) ) < 0 )
#endif
    {
        msg_Err( p_aout, "unable to set period size (%s)",
                         snd_strerror( i_snd_rc ) );
        goto error;
    }
    p_aout->output.i_nb_samples = i_period_size;

    /* Commit hardware parameters. */
    if ( ( i_snd_rc = snd_pcm_hw_params( p_sys->p_snd_pcm, p_hw ) ) < 0 )
    {
        msg_Err( p_aout, "unable to commit hardware configuration (%s)",
                         snd_strerror( i_snd_rc ) );
        goto error;
    }

#ifdef HAVE_ALSA_NEW_API
    if( ( i_snd_rc = snd_pcm_hw_params_get_period_time( p_hw,
                                    &p_sys->i_period_time, NULL ) ) < 0 )
#else
    if( ( p_sys->i_period_time =
                  snd_pcm_hw_params_get_period_time( p_hw, NULL ) ) < 0 )
#endif
    {
        msg_Err( p_aout, "unable to get period time (%s)",
                         snd_strerror( i_snd_rc ) );
        goto error;
    }

    /* Get Initial software parameters */
    snd_pcm_sw_params_current( p_sys->p_snd_pcm, p_sw );

    i_snd_rc = snd_pcm_sw_params_set_sleep_min( p_sys->p_snd_pcm, p_sw, 0 );

    i_snd_rc = snd_pcm_sw_params_set_avail_min( p_sys->p_snd_pcm, p_sw,
                                                p_aout->output.i_nb_samples );

    /* Commit software parameters. */
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

    p_aout->b_die = VLC_TRUE;
    vlc_thread_join( p_aout );
    p_aout->b_die = VLC_FALSE;

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
        /* Maybe because I don't want to eat all the cpu by looping
           all the time. --Bozo */
        /* Shouldn't snd_pcm_wait() make us wait ? --Meuuh */
        msleep( p_sys->i_period_time >> 1 );
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
    mtime_t next_date;

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

        snd_pcm_status_get_tstamp( p_status, &ts_next );
        next_date = (mtime_t)ts_next.tv_sec * 1000000 + ts_next.tv_usec;
        next_date += (mtime_t)snd_pcm_status_get_delay(p_status)
                     * 1000000 / p_aout->output.output.i_rate;

        p_buffer = aout_OutputNextBuffer( p_aout, next_date,
                        (p_aout->output.output.i_format ==
                         VLC_FOURCC('s','p','d','i')) );

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

        aout_BufferFree( p_buffer );

        msleep( p_sys->i_period_time >> 2 );
    }
}

