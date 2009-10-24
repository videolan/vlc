/*****************************************************************************
 * alsa.c : alsa plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <errno.h>                                                 /* ENOMEM */
#include <vlc_dialog.h>

#include <vlc_aout.h>
#include <vlc_cpu.h>

/* ALSA part
   Note: we use the new API which is available since 0.9.0beta10a. */
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

/*#define ALSA_DEBUG*/

/*****************************************************************************
 * aout_sys_t: ALSA audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the ALSA specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    snd_pcm_t         * p_snd_pcm;
    unsigned int                 i_period_time;

#ifdef ALSA_DEBUG
    snd_output_t      * p_snd_stderr;
#endif

    bool b_playing;  /* playing status */
    mtime_t start_date;

    vlc_thread_t thread;
    vlc_mutex_t lock;
    vlc_cond_t  wait ;
};

#define A52_FRAME_NB 1536

/* These values are in frames.
   To convert them to a number of bytes you have to multiply them by the
   number of channel(s) (eg. 2 for stereo) and the size of a sample (eg.
   2 for int16_t). */
#define ALSA_DEFAULT_PERIOD_SIZE        1024
#define ALSA_DEFAULT_BUFFER_SIZE        ( ALSA_DEFAULT_PERIOD_SIZE << 8 )
#define ALSA_SPDIF_PERIOD_SIZE          A52_FRAME_NB
#define ALSA_SPDIF_BUFFER_SIZE          ( ALSA_SPDIF_PERIOD_SIZE << 4 )
/* Why << 4 ? --Meuuh */
/* Why not ? --Bozo */
/* Right. --Meuuh */

#define DEFAULT_ALSA_DEVICE N_("default")

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int   Open         ( vlc_object_t * );
static void  Close        ( vlc_object_t * );
static void  Play         ( aout_instance_t * );
static void* ALSAThread   ( void * );
static void  ALSAFill     ( aout_instance_t * );
static int FindDevicesCallback( vlc_object_t *p_this, char const *psz_name,
                                vlc_value_t newval, vlc_value_t oldval, void *p_unused );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static const char *const ppsz_devices[] = { "default" };
static const char *const ppsz_devices_text[] = { N_("Default") };
vlc_module_begin ()
    set_shortname( "ALSA" )
    set_description( N_("ALSA audio output") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_string( "alsa-audio-device", DEFAULT_ALSA_DEVICE, aout_FindAndRestart,
                N_("ALSA Device Name"), NULL, false )
        add_deprecated_alias( "alsadev" )   /* deprecated since 0.9.3 */
        change_string_list( ppsz_devices, ppsz_devices_text, FindDevicesCallback )
        change_action_add( FindDevicesCallback, N_("Refresh list") )

    set_capability( "audio output", 150 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Probe: probe the audio device for available formats and channels
 *****************************************************************************/
static void Probe( aout_instance_t * p_aout,
                   const char * psz_device, const char * psz_iec_device,
                   int *pi_snd_pcm_format )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    vlc_value_t val, text;
    int i_ret;

    var_Create ( p_aout, "audio-device", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Audio Device");
    var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

    /* We'll open the audio device in non blocking mode so we can just exit
     * when it is already in use, but for the real stuff we'll still use
     * the blocking mode */

    /* Now test linear PCM capabilities */
    if ( !(i_ret = snd_pcm_open( &p_sys->p_snd_pcm, psz_device,
                                 SND_PCM_STREAM_PLAYBACK,
                                 SND_PCM_NONBLOCK ) ) )
    {
        int i_channels;
        snd_pcm_hw_params_t * p_hw;
        snd_pcm_hw_params_alloca (&p_hw);

        if ( snd_pcm_hw_params_any( p_sys->p_snd_pcm, p_hw ) < 0 )
        {
            msg_Warn( p_aout, "unable to retrieve initial hardware parameters"
                              ", disabling linear PCM audio" );
            snd_pcm_close( p_sys->p_snd_pcm );
            var_Destroy( p_aout, "audio-device" );
            return;
        }

        if ( snd_pcm_hw_params_set_format( p_sys->p_snd_pcm, p_hw,
                                           *pi_snd_pcm_format ) < 0 )
        {
            int i_snd_rc = -1;

            if( *pi_snd_pcm_format != SND_PCM_FORMAT_S16 )
            {
                *pi_snd_pcm_format = SND_PCM_FORMAT_S16;
                i_snd_rc = snd_pcm_hw_params_set_format( p_sys->p_snd_pcm,
                                                    p_hw, *pi_snd_pcm_format );
            }
            if ( i_snd_rc < 0 )
            {
                msg_Warn( p_aout, "unable to set stream sample size and "
                          "word order, disabling linear PCM audio" );
                snd_pcm_close( p_sys->p_snd_pcm );
                var_Destroy( p_aout, "audio-device" );
                return;
            }
        }

        i_channels = aout_FormatNbChannels( &p_aout->output.output );

        while ( i_channels > 0 )
        {
            if ( !snd_pcm_hw_params_test_channels( p_sys->p_snd_pcm, p_hw,
                                                   i_channels ) )
            {
                switch ( i_channels )
                {
                case 1:
                    val.i_int = AOUT_VAR_MONO;
                    text.psz_string = _("Mono");
                    var_Change( p_aout, "audio-device",
                                VLC_VAR_ADDCHOICE, &val, &text );
                    break;
                case 2:
                    val.i_int = AOUT_VAR_STEREO;
                    text.psz_string = _("Stereo");
                    var_Change( p_aout, "audio-device",
                                VLC_VAR_ADDCHOICE, &val, &text );
                    var_Set( p_aout, "audio-device", val );
                    break;
                case 4:
                    val.i_int = AOUT_VAR_2F2R;
                    text.psz_string = _("2 Front 2 Rear");
                    var_Change( p_aout, "audio-device",
                                VLC_VAR_ADDCHOICE, &val, &text );
                    break;
                case 6:
                    val.i_int = AOUT_VAR_5_1;
                    text.psz_string = "5.1";
                    var_Change( p_aout, "audio-device",
                                VLC_VAR_ADDCHOICE, &val, &text );
                    break;
                }
            }

            --i_channels;
        }

        /* Special case for mono on stereo only boards */
        i_channels = aout_FormatNbChannels( &p_aout->output.output );
        var_Change( p_aout, "audio-device", VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int <= 0 && i_channels == 1 )
        {
            if ( !snd_pcm_hw_params_test_channels( p_sys->p_snd_pcm, p_hw, 2 ))
            {
                val.i_int = AOUT_VAR_STEREO;
                text.psz_string = (char*)N_("Stereo");
                var_Change( p_aout, "audio-device",
                            VLC_VAR_ADDCHOICE, &val, &text );
                var_Set( p_aout, "audio-device", val );
            }
        }

        /* Close the previously opened device */
        snd_pcm_close( p_sys->p_snd_pcm );
    }
    else if ( i_ret == -EBUSY )
    {
        msg_Warn( p_aout, "audio device: %s is already in use", psz_device );
    }

    /* Test for S/PDIF device if needed */
    if ( psz_iec_device )
    {
        /* Opening the device should be enough */
        if ( !(i_ret = snd_pcm_open( &p_sys->p_snd_pcm, psz_iec_device,
                                     SND_PCM_STREAM_PLAYBACK,
                                     SND_PCM_NONBLOCK ) ) )
        {
            val.i_int = AOUT_VAR_SPDIF;
            text.psz_string = (char*)N_("A/52 over S/PDIF");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            if( config_GetInt( p_aout, "spdif" ) )
                var_Set( p_aout, "audio-device", val );

            snd_pcm_close( p_sys->p_snd_pcm );
        }
        else if ( i_ret == -EBUSY )
        {
            msg_Warn( p_aout, "audio device: %s is already in use",
                      psz_iec_device );
        }
    }

    var_Change( p_aout, "audio-device", VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int <= 0 )
    {
        /* Probe() has failed. */
        msg_Dbg( p_aout, "failed to find a usable alsa configuration" );
        var_Destroy( p_aout, "audio-device" );
        return;
    }

    /* Add final settings to the variable */
    var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart, NULL );
    var_SetBool( p_aout, "intf-change", true );
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
    unsigned int i_old_rate;
    bool b_retry = true;

    /* Allocate structures */
    p_aout->output.p_sys = p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_sys->b_playing = false;
    p_sys->start_date = 0;
    vlc_cond_init( &p_sys->wait );
    vlc_mutex_init( &p_sys->lock );

    /* Get device name */
    if( (psz_device = config_GetPsz( p_aout, "alsa-audio-device" )) == NULL )
    {
        msg_Err( p_aout, "no audio device given (maybe \"default\" ?)" );
        dialog_Fatal( p_aout, _("No Audio Device"), "%s",
                        _("No audio device name was given. You might want to " \
                          "enter \"default\".") );
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
    if( HAVE_FPU )
    {
        i_vlc_pcm_format = VLC_CODEC_FL32;
        i_snd_pcm_format = SND_PCM_FORMAT_FLOAT;
    }
    else
    {
        i_vlc_pcm_format = VLC_CODEC_S16N;
        i_snd_pcm_format = SND_PCM_FORMAT_S16;
    }

    /* If the variable doesn't exist then it's the first time we're called
       and we have to probe the available audio formats and channels */
    if ( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout, psz_device, psz_iec_device, &i_snd_pcm_format );
    }

    if ( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        free( p_sys );
        free( psz_device );
        return VLC_EGENERIC;
    }

    p_aout->output.output.i_format = i_vlc_pcm_format;
    if ( val.i_int == AOUT_VAR_5_1 )
    {
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
               | AOUT_CHAN_LFE;
        free( psz_device );
        psz_device = strdup( "surround51" );
    }
    else if ( val.i_int == AOUT_VAR_2F2R )
    {
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        free( psz_device );
        psz_device = strdup( "surround40" );
    }
    else if ( val.i_int == AOUT_VAR_STEREO )
    {
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }
    else if ( val.i_int == AOUT_VAR_MONO )
    {
        p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
    }
    else if( val.i_int != AOUT_VAR_SPDIF )
    {
        /* This should not happen ! */
        msg_Err( p_aout, "internal: can't find audio-device (%i)", val.i_int );
        free( p_sys );
        free( psz_device );
        return VLC_EGENERIC;
    }

#ifdef ALSA_DEBUG
    snd_output_stdio_attach( &p_sys->p_snd_stderr, stderr, 0 );
#endif

    /* Open the device */
    if ( val.i_int == AOUT_VAR_SPDIF )
    {
        if ( ( i_snd_rc = snd_pcm_open( &p_sys->p_snd_pcm, psz_iec_device,
                            SND_PCM_STREAM_PLAYBACK, 0 ) ) < 0 )
        {
            msg_Err( p_aout, "cannot open ALSA device `%s' (%s)",
                             psz_iec_device, snd_strerror( i_snd_rc ) );
            dialog_Fatal( p_aout, _("Audio output failed"),
                            _("VLC could not open the ALSA device \"%s\" (%s)."),
                            psz_iec_device, snd_strerror( i_snd_rc ) );
            free( p_sys );
            free( psz_device );
            return VLC_EGENERIC;
        }
        i_buffer_size = ALSA_SPDIF_BUFFER_SIZE;
        i_snd_pcm_format = SND_PCM_FORMAT_S16;
        i_channels = 2;

        i_vlc_pcm_format = VLC_CODEC_SPDIFL;
        p_aout->output.i_nb_samples = i_period_size = ALSA_SPDIF_PERIOD_SIZE;
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->output.output.i_frame_length = A52_FRAME_NB;

        aout_VolumeNoneInit( p_aout );
    }
    else
    {
        int i;

        msg_Dbg( p_aout, "opening ALSA device `%s'", psz_device );

        /* Since it seems snd_pcm_close hasn't really released the device at
          the time it returns, probe if the device is available in loop for 1s.
          We cannot use blocking mode since the we would wait indefinitely when
          switching from a dmx device to surround51. */

        for( i = 10; i >= 0; i-- )
        {
            if ( ( i_snd_rc = snd_pcm_open( &p_sys->p_snd_pcm, psz_device,
                   SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK ) ) == -EBUSY )
            {
                if( i ) msleep( 100000 /* 100ms */ );
                else
                {
                    msg_Err( p_aout, "audio device: %s is already in use",
                              psz_device );
                    dialog_Fatal( p_aout, _("Audio output failed"),
                                    _("The audio device \"%s\" is already in use."),
                                    psz_device );
                }
                continue;
            }
            break;
        }
        if( i_snd_rc < 0 )
        {
            msg_Err( p_aout, "cannot open ALSA device `%s' (%s)",
                             psz_device, snd_strerror( i_snd_rc ) );
            dialog_Fatal( p_aout, _("Audio output failed"),
                            _("VLC could not open the ALSA device \"%s\" (%s)."),
                            psz_device, snd_strerror( i_snd_rc ) );
            free( p_sys );
            free( psz_device );
            return VLC_EGENERIC;
        }

        /* We want blocking mode */
        snd_pcm_nonblock( p_sys->p_snd_pcm, 0 );

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

    /* Due to some bugs in alsa with some drivers, we need to retry in s16l
       if snd_pcm_hw_params fails in fl32 */
    while ( b_retry )
    {
        b_retry = false;

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
            if( i_snd_pcm_format != SND_PCM_FORMAT_S16 )
            {
                i_snd_pcm_format = SND_PCM_FORMAT_S16;
                i_snd_rc = snd_pcm_hw_params_set_format( p_sys->p_snd_pcm,
                                                     p_hw, i_snd_pcm_format );
            }
            if ( i_snd_rc < 0 )
            {
                msg_Err( p_aout, "unable to set stream sample size and "
                     "word order (%s)", snd_strerror( i_snd_rc ) );
                goto error;
            }
        }
        if( i_vlc_pcm_format != VLC_CODEC_SPDIFL )
        switch( i_snd_pcm_format )
        {
        case SND_PCM_FORMAT_FLOAT:
            i_vlc_pcm_format = VLC_CODEC_FL32;
            break;
        case SND_PCM_FORMAT_S16:
            i_vlc_pcm_format = VLC_CODEC_S16N;
            break;
        }
        p_aout->output.output.i_format = i_vlc_pcm_format;

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
        i_old_rate = p_aout->output.output.i_rate;
        i_snd_rc = snd_pcm_hw_params_set_rate_near( p_sys->p_snd_pcm, p_hw,
                                                &p_aout->output.output.i_rate,
                                                NULL );
        if( i_snd_rc < 0 || p_aout->output.output.i_rate != i_old_rate )
        {
            msg_Warn( p_aout, "The rate %d Hz is not supported by your " \
                "hardware. Using %d Hz instead.\n", i_old_rate, \
                p_aout->output.output.i_rate );
        }

        /* Set period size. */
        if ( ( i_snd_rc = snd_pcm_hw_params_set_period_size_near( p_sys->p_snd_pcm,
                                    p_hw, &i_period_size, NULL ) ) < 0 )
        {
            msg_Err( p_aout, "unable to set period size (%s)",
                         snd_strerror( i_snd_rc ) );
            goto error;
        }
        p_aout->output.i_nb_samples = i_period_size;

/* Set buffer size. */
        if ( ( i_snd_rc = snd_pcm_hw_params_set_buffer_size_near( p_sys->p_snd_pcm,
                                    p_hw, &i_buffer_size ) ) < 0 )
        {
            msg_Err( p_aout, "unable to set buffer size (%s)",
                         snd_strerror( i_snd_rc ) );
            goto error;
        }

        /* Commit hardware parameters. */
        if ( ( i_snd_rc = snd_pcm_hw_params( p_sys->p_snd_pcm, p_hw ) ) < 0 )
        {
            if ( b_retry == false &&
                                i_snd_pcm_format == SND_PCM_FORMAT_FLOAT)
            {
                b_retry = true;
                i_snd_pcm_format = SND_PCM_FORMAT_S16;
                p_aout->output.output.i_format = VLC_CODEC_S16N;
                msg_Warn( p_aout, "unable to commit hardware configuration "
                                  "with fl32 samples. Retrying with s16l (%s)",                                     snd_strerror( i_snd_rc ) );
            }
            else
            {
                msg_Err( p_aout, "unable to commit hardware configuration (%s)",
                         snd_strerror( i_snd_rc ) );
                goto error;
            }
        }
    }

    if( ( i_snd_rc = snd_pcm_hw_params_get_period_time( p_hw,
                                    &p_sys->i_period_time, NULL ) ) < 0 )
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
    /* start playing when one period has been written */
    i_snd_rc = snd_pcm_sw_params_set_start_threshold( p_sys->p_snd_pcm, p_sw,
                                                      ALSA_DEFAULT_PERIOD_SIZE);
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "unable to set start threshold (%s)",
                          snd_strerror( i_snd_rc ) );
        goto error;
    }

    /* Commit software parameters. */
    if ( snd_pcm_sw_params( p_sys->p_snd_pcm, p_sw ) < 0 )
    {
        msg_Err( p_aout, "unable to set software configuration" );
        goto error;
    }

#ifdef ALSA_DEBUG
    snd_output_printf( p_sys->p_snd_stderr, "\nALSA hardware setup:\n\n" );
    snd_pcm_dump_hw_setup( p_sys->p_snd_pcm, p_sys->p_snd_stderr );
    snd_output_printf( p_sys->p_snd_stderr, "\nALSA software setup:\n\n" );
    snd_pcm_dump_sw_setup( p_sys->p_snd_pcm, p_sys->p_snd_stderr );
    snd_output_printf( p_sys->p_snd_stderr, "\n" );
#endif

    /* Create ALSA thread and wait for its readiness. */
    if( vlc_clone( &p_sys->thread, ALSAThread, p_aout,
                   VLC_THREAD_PRIORITY_OUTPUT ) )
    {
        msg_Err( p_aout, "cannot create ALSA thread (%m)" );
        goto error;
    }

    return 0;

error:
    snd_pcm_close( p_sys->p_snd_pcm );
#ifdef ALSA_DEBUG
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
    if( !p_aout->output.p_sys->b_playing )
    {
        p_aout->output.p_sys->b_playing = true;

        /* get the playing date of the first aout buffer */
        vlc_mutex_lock( &p_aout->output.p_sys->lock );
        p_aout->output.p_sys->start_date =
            aout_FifoFirstDate( p_aout, &p_aout->output.fifo );

        /* wake up the audio output thread */
        vlc_cond_signal( &p_aout->output.p_sys->wait );
        vlc_mutex_unlock( &p_aout->output.p_sys->lock );
    }
}

/*****************************************************************************
 * Close: close the ALSA device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    int i_snd_rc;

    /* Make sure that the thread will stop once it is waken up */
    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    /* make sure the audio output thread is waken up */
    vlc_mutex_lock( &p_aout->output.p_sys->lock );
    vlc_cond_signal( &p_aout->output.p_sys->wait );
    vlc_mutex_unlock( &p_aout->output.p_sys->lock );

    /* */
    i_snd_rc = snd_pcm_close( p_sys->p_snd_pcm );

    if( i_snd_rc > 0 )
    {
        msg_Err( p_aout, "failed closing ALSA device (%s)",
                         snd_strerror( i_snd_rc ) );
    }

#ifdef ALSA_DEBUG
    snd_output_close( p_sys->p_snd_stderr );
#endif

    free( p_sys );
}

static void pcm_drop(void *pcm)
{
    snd_pcm_drop(pcm);
}

/*****************************************************************************
 * ALSAThread: asynchronous thread used to DMA the data to the device
 *****************************************************************************/
static void* ALSAThread( void *data )
{
    aout_instance_t * p_aout = data;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    /* Wait for the exact time to start playing (avoids resampling) */
    vlc_mutex_lock( &p_sys->lock );
    mutex_cleanup_push( &p_sys->lock );
    while( !p_sys->start_date )
        vlc_cond_wait( &p_sys->wait, &p_sys->lock );
    vlc_cleanup_run();

    mwait( p_sys->start_date - AOUT_PTS_TOLERANCE / 4 );

    vlc_cleanup_push( pcm_drop, p_sys->p_snd_pcm );
    for(;;)
        ALSAFill( p_aout );

    assert(0);
    vlc_cleanup_pop();
}

/*****************************************************************************
 * ALSAFill: function used to fill the ALSA buffer as much as possible
 *****************************************************************************/
static void ALSAFill( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    snd_pcm_t *p_pcm = p_sys->p_snd_pcm;
    snd_pcm_status_t * p_status;
    int i_snd_rc;
    mtime_t next_date;

    int canc = vlc_savecancel();
    /* Fill in the buffer until space or audio output buffer shortage */

    /* Get the status */
    snd_pcm_status_alloca(&p_status);
    i_snd_rc = snd_pcm_status( p_pcm, p_status );
    if( i_snd_rc < 0 )
    {
        msg_Err( p_aout, "cannot get device status" );
        goto error;
    }

    /* Handle buffer underruns and get the status again */
    if( snd_pcm_status_get_state( p_status ) == SND_PCM_STATE_XRUN )
    {
        /* Prepare the device */
        i_snd_rc = snd_pcm_prepare( p_pcm );
        if( i_snd_rc )
        {
            msg_Err( p_aout, "cannot recover from buffer underrun" );
            goto error;
        }

        msg_Dbg( p_aout, "recovered from buffer underrun" );

        /* Get the new status */
        i_snd_rc = snd_pcm_status( p_pcm, p_status );
        if( i_snd_rc < 0 )
        {
            msg_Err( p_aout, "cannot get device status after recovery" );
            goto error;
        }

        /* Underrun, try to recover as quickly as possible */
        next_date = mdate();
    }
    else
    {
        /* Here the device should be in RUNNING state, p_status is valid. */
        snd_pcm_sframes_t delay = snd_pcm_status_get_delay( p_status );
        if( delay == 0 ) /* workaround buggy alsa drivers */
            if( snd_pcm_delay( p_pcm, &delay ) < 0 )
                delay = 0; /* FIXME: use a positive minimal delay */

        size_t i_bytes = snd_pcm_frames_to_bytes( p_pcm, delay );
        mtime_t delay_us = CLOCK_FREQ * i_bytes
                / p_aout->output.output.i_bytes_per_frame
                / p_aout->output.output.i_rate
                * p_aout->output.output.i_frame_length;

#ifdef ALSA_DEBUG
        snd_pcm_state_t state = snd_pcm_status_get_state( p_status );
        if( state != SND_PCM_STATE_RUNNING )
            msg_Err( p_aout, "pcm status (%d) != RUNNING", state );

        msg_Dbg( p_aout, "Delay is %ld frames (%zu bytes)", delay, i_bytes );

        msg_Dbg( p_aout, "Bytes per frame: %d", p_aout->output.output.i_bytes_per_frame );
        msg_Dbg( p_aout, "Rate: %d", p_aout->output.output.i_rate );
        msg_Dbg( p_aout, "Frame length: %d", p_aout->output.output.i_frame_length );
        msg_Dbg( p_aout, "Next date: in %"PRId64" microseconds", delay_us );
#endif
        next_date = mdate() + delay_us;
    }

    block_t *p_buffer = aout_OutputNextBuffer( p_aout, next_date,
           (p_aout->output.output.i_format ==  VLC_CODEC_SPDIFL) );

    /* Audio output buffer shortage -> stop the fill process and wait */
    if( p_buffer == NULL )
        goto error;

    for (;;)
    {
        int n = snd_pcm_poll_descriptors_count(p_pcm);
        struct pollfd ufd[n];
        unsigned short revents;

        snd_pcm_poll_descriptors(p_pcm, ufd, n);
        do
        {
            vlc_restorecancel(canc);
            poll(ufd, n, -1);
            canc = vlc_savecancel();
            snd_pcm_poll_descriptors_revents(p_pcm, ufd, n, &revents);
        }
        while(!revents);

        if(revents & POLLOUT)
        {
            i_snd_rc = snd_pcm_writei( p_pcm, p_buffer->p_buffer,
                                       p_buffer->i_nb_samples );
            if( i_snd_rc != -ESTRPIPE )
                break;
        }

        /* a suspend event occurred
         * (stream is suspended and waiting for an application recovery) */
        msg_Dbg( p_aout, "entering in suspend mode, trying to resume..." );

        while( ( i_snd_rc = snd_pcm_resume( p_pcm ) ) == -EAGAIN )
        {
            vlc_restorecancel(canc);
            msleep(CLOCK_FREQ); /* device still suspended, wait... */
            canc = vlc_savecancel();
        }

        if( i_snd_rc < 0 )
            /* Device does not support resuming, restart it */
            i_snd_rc = snd_pcm_prepare( p_pcm );

    }

    if( i_snd_rc < 0 )
        msg_Err( p_aout, "cannot write: %s", snd_strerror( i_snd_rc ) );

    vlc_restorecancel(canc);
    block_Release( p_buffer );
    return;

error:
    if( i_snd_rc < 0 )
        msg_Err( p_aout, "ALSA error: %s", snd_strerror( i_snd_rc ) );

    vlc_restorecancel(canc);
    msleep(p_sys->i_period_time / 2);
}

static void GetDevicesForCard( module_config_t *p_item, int i_card );
static void GetDevices( module_config_t *p_item );

/*****************************************************************************
 * config variable callback
 *****************************************************************************/
static int FindDevicesCallback( vlc_object_t *p_this, char const *psz_name,
                               vlc_value_t newval, vlc_value_t oldval, void *p_unused )
{
    module_config_t *p_item;
    int i;
    (void)newval;
    (void)oldval;
    (void)p_unused;

    p_item = config_FindConfig( p_this, psz_name );
    if( !p_item ) return VLC_SUCCESS;

    /* Clear-up the current list */
    if( p_item->i_list )
    {
        /* Keep the first entrie */
        for( i = 1; i < p_item->i_list; i++ )
        {
            free( (char *)p_item->ppsz_list[i] );
            free( (char *)p_item->ppsz_list_text[i] );
        }
        /* TODO: Remove when no more needed */
        p_item->ppsz_list[i] = NULL;
        p_item->ppsz_list_text[i] = NULL;
    }
    p_item->i_list = 1;

    GetDevices( p_item );

    /* Signal change to the interface */
    p_item->b_dirty = true;

    return VLC_SUCCESS;
}


static void GetDevicesForCard( module_config_t *p_item, int i_card )
{
    int i_pcm_device = -1;
    int i_err = 0;
    snd_pcm_info_t *p_pcm_info;
    snd_ctl_t *p_ctl;
    char psz_dev[64];
    char *psz_card_name;

    sprintf( psz_dev, "hw:%i", i_card );

    if( ( i_err = snd_ctl_open( &p_ctl, psz_dev, 0 ) ) < 0 )
        return;

    if( ( i_err = snd_card_get_name( i_card, &psz_card_name ) ) != 0)
        psz_card_name = _("Unknown soundcard");

    snd_pcm_info_alloca( &p_pcm_info );

    for (;;)
    {
        char *psz_device, *psz_descr;
        if( ( i_err = snd_ctl_pcm_next_device( p_ctl, &i_pcm_device ) ) < 0 )
            i_pcm_device = -1;
        if( i_pcm_device < 0 )
            break;

        snd_pcm_info_set_device( p_pcm_info, i_pcm_device );
        snd_pcm_info_set_subdevice( p_pcm_info, 0 );
        snd_pcm_info_set_stream( p_pcm_info, SND_PCM_STREAM_PLAYBACK );

        if( ( i_err = snd_ctl_pcm_info( p_ctl, p_pcm_info ) ) < 0 )
        {
            if( i_err != -ENOENT )
            {
                /*printf( "get_devices_for_card(): "
                         "snd_ctl_pcm_info() "
                         "failed (%d:%d): %s.\n", i_card,
                         i_pcm_device, snd_strerror( -i_err ) );*/
            }
            continue;
        }

        if( asprintf( &psz_device, "hw:%d,%d", i_card, i_pcm_device ) == -1 )
            break;
        if( asprintf( &psz_descr, "%s: %s (%s)", psz_card_name,
                  snd_pcm_info_get_name(p_pcm_info), psz_device ) == -1 )
        {
            free( psz_device );
            break;
        }

        p_item->ppsz_list =
            (char **)realloc( p_item->ppsz_list,
                              (p_item->i_list + 2) * sizeof(char *) );
        p_item->ppsz_list_text =
            (char **)realloc( p_item->ppsz_list_text,
                              (p_item->i_list + 2) * sizeof(char *) );
        p_item->ppsz_list[ p_item->i_list ] = psz_device;
        p_item->ppsz_list_text[ p_item->i_list ] = psz_descr;
        p_item->i_list++;
        p_item->ppsz_list[ p_item->i_list ] = NULL;
        p_item->ppsz_list_text[ p_item->i_list ] = NULL;
    }

    snd_ctl_close( p_ctl );
}



static void GetDevices( module_config_t *p_item )
{
    int i_card = -1;
    int i_err = 0;

    if( ( i_err = snd_card_next( &i_card ) ) != 0 )
    {
        /*printf( "snd_card_next() failed: %s", snd_strerror( -i_err ) );*/
        return;
    }

    while( i_card > -1 )
    {
        GetDevicesForCard( p_item, i_card );
        if( ( i_err = snd_card_next( &i_card ) ) != 0 )
        {
            /*printf( "snd_card_next() failed: %s", snd_strerror( -i_err ) );*/
            break;
        }
    }
}
