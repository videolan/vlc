/*****************************************************************************
 * oss.c : OSS /dev/dsp module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2002 VideoLAN
 * $Id: oss.c,v 1.42 2003/01/07 14:58:33 massiot Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <sys/ioctl.h>                                            /* ioctl() */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#endif

#include <vlc/aout.h>

#include "aout_internal.h"

/* SNDCTL_DSP_RESET, SNDCTL_DSP_SETFMT, SNDCTL_DSP_STEREO, SNDCTL_DSP_SPEED,
 * SNDCTL_DSP_GETOSPACE */
#ifdef HAVE_SOUNDCARD_H
#   include <soundcard.h>
#elif defined( HAVE_SYS_SOUNDCARD_H )
#   include <sys/soundcard.h>
#elif defined( HAVE_MACHINE_SOUNDCARD_H )
#   include <machine/soundcard.h>
#endif

/*****************************************************************************
 * aout_sys_t: OSS audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the dsp specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    int i_fd;
    int b_workaround_buggy_driver;
    int i_fragstotal;
    mtime_t max_buffer_duration;
};

/* This must be a power of 2. */
#define FRAME_SIZE 1024
#define FRAME_COUNT 4

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static void Play         ( aout_instance_t * );
static int  OSSThread    ( aout_instance_t * );

static mtime_t BufferDuration( aout_instance_t * p_aout );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define BUGGY_TEXT N_("Try to work around buggy OSS drivers")
#define BUGGY_LONGTEXT N_( \
    "Some buggy OSS drivers just don't like when their internal buffers " \
    "are completely filled (the sound gets heavily hashed). If you have one " \
    "of these drivers, then you need to enable this option." )

#define SPDIF_TEXT N_("Try to use S/PDIF output")
#define SPDIF_LONGTEXT N_( \
    "Sometimes we attempt to use the S/PDIF output, even if nothing is " \
    "connected to it. Un-checking this option disables this behaviour, " \
    "and permanently selects analog PCM output." )

vlc_module_begin();
    add_category_hint( N_("OSS"), NULL );
    add_file( "dspdev", "/dev/dsp", aout_FindAndRestart,
              N_("OSS dsp device"), NULL );
    add_bool( "oss-buggy", 0, NULL, BUGGY_TEXT, BUGGY_LONGTEXT );
    add_bool( "spdif", 1, NULL, SPDIF_TEXT, SPDIF_LONGTEXT );
    set_description( _("Linux OSS /dev/dsp module") );
    set_capability( "audio output", 100 );
    add_shortcut( "oss" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Probe: probe the audio device for available formats and channels
 *****************************************************************************/
static void Probe( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    vlc_value_t val;
    int i_format, i_nb_channels;

    var_Create( p_aout, "audio-device", VLC_VAR_STRING | VLC_VAR_HASCHOICE );

    if( ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        msg_Err( p_aout, "cannot reset OSS audio device" );
        var_Destroy( p_aout, "audio-device" );
        return;
    }

    if ( config_GetInt( p_aout, "spdif" )
          && AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        i_format = AFMT_AC3;

        if( ioctl( p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) >= 0
             && i_format == AFMT_AC3 )
        {
            val.psz_string = N_("A/52 over S/PDIF");
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );
        }
    }

    /* Go to PCM mode. */
    i_format = AFMT_S16_NE;
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 ||
        ioctl( p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
    {
        return;
    }

#ifdef SNDCTL_DSP_GETCHANNELMASK
    if ( aout_FormatNbChannels( &p_aout->output.output ) > 2 )
    {
        /* Check that the device supports this. */

        int i_chanmask;
        if ( ioctl( p_sys->i_fd, SNDCTL_DSP_GETCHANNELMASK,
                    &i_chanmask ) == 0 )
        {
            if ( !(i_chanmask & DSP_BIND_FRONT) )
            {
                msg_Err( p_aout, "No front channels ! (%x)",
                         i_chanmask );
                return;
            }

            if ( (i_chanmask & (DSP_BIND_SURR | DSP_BIND_CENTER_LFE))
                  && (p_aout->output.output.i_physical_channels ==
                       (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                         | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                         | AOUT_CHAN_LFE)) )
            {
                val.psz_string = N_("5.1");
                var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );
            }

            if ( (i_chanmask & DSP_BIND_SURR)
                  && (p_aout->output.output.i_physical_channels &
                       (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                         | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT)) )
            {
                val.psz_string = N_("2 Front 2 Rear");
                var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );
            }
        }
    }
#endif

    /* Test for stereo. */
    i_nb_channels = 2;
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_CHANNELS, &i_nb_channels ) >= 0
         && i_nb_channels == 2 )
    {
        val.psz_string = N_("Stereo");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );
    }

    /* Reset all. */
    i_format = AFMT_S16_NE;
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 ||
        ioctl( p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
    {
        msg_Err( p_aout, "cannot reset OSS audio device" );
        var_Destroy( p_aout, "audio-device" );
        return;
    }

    /* Test for mono. */
    i_nb_channels = 1;
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_CHANNELS, &i_nb_channels ) >= 0  
         && i_nb_channels == 1 )
    {
        val.psz_string = N_("Mono");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );        
    }

    val.b_bool = VLC_TRUE;
    var_Set( p_aout, "intf-change", val );
}

/*****************************************************************************
 * Open: open the audio device (the digital sound processor)
 *****************************************************************************
 * This function opens the dsp as a usual non-blocking write-only file, and
 * modifies the p_aout->p_sys->i_fd with the file's descriptor.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;
    char * psz_device;
    vlc_value_t val;

    /* Allocate structure */
    p_aout->output.p_sys = p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_ENOMEM;
    }

    /* Get device name */
    if( (psz_device = config_GetPsz( p_aout, "dspdev" )) == NULL )
    {
        msg_Err( p_aout, "no audio device given (maybe /dev/dsp ?)" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Open the sound device */
    p_sys->i_fd = open( psz_device, O_WRONLY );
    if( p_sys->i_fd < 0 )
    {
        msg_Err( p_aout, "cannot open audio device (%s)", psz_device );
        free( p_sys );
        return VLC_EGENERIC;
    }
    free( psz_device );

    p_aout->output.pf_play = Play;

    if ( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout );
    }

    if ( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        /* Probe() has failed. */
        free( p_sys );
        return VLC_EGENERIC;
    }

    if ( !strcmp( val.psz_string, N_("A/52 over S/PDIF") ) )
    {
        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');
    }
    else if ( !strcmp( val.psz_string, N_("5.1") ) )
    {
        p_aout->output.output.i_format = AOUT_FMT_S16_NE;
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARLEFT
               | AOUT_CHAN_LFE;
    }
    else if ( !strcmp( val.psz_string, N_("2 Front 2 Rear") ) )
    {
        p_aout->output.output.i_format = AOUT_FMT_S16_NE;
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARLEFT;
    }
    else if ( !strcmp( val.psz_string, "Stereo" ) )
    {
        p_aout->output.output.i_format = AOUT_FMT_S16_NE;
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }
    else if ( !strcmp( val.psz_string, "Mono" ) )
    {
        p_aout->output.output.i_format = AOUT_FMT_S16_NE;
        p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
    }
    free( val.psz_string );

    /* Reset the DSP device */
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        msg_Err( p_aout, "cannot reset OSS audio device" );
        close( p_sys->i_fd );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Set the output format */
    if ( AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        int i_format = AFMT_AC3;

        if( ioctl( p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0
             || i_format != AFMT_AC3 )
        {
            msg_Err( p_aout, "cannot reset OSS audio device" );
            close( p_sys->i_fd );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');
        p_aout->output.i_nb_samples = A52_FRAME_NB;
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->output.output.i_frame_length = A52_FRAME_NB;

        aout_VolumeNoneInit( p_aout );
    }

    if ( !AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        unsigned int i_format = AFMT_S16_NE;
        unsigned int i_frame_size, i_fragments;
        unsigned int i_rate;
        unsigned int i_nb_channels;
        audio_buf_info audio_buf;

        if( ioctl( p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
        {
            msg_Err( p_aout, "cannot set audio output format" );
            close( p_sys->i_fd );
            free( p_sys );
            return VLC_EGENERIC;
        }

        switch ( i_format )
        {
        case AFMT_U8:
            p_aout->output.output.i_format = VLC_FOURCC('u','8',' ',' ');
            break;
        case AFMT_S8:
            p_aout->output.output.i_format = VLC_FOURCC('s','8',' ',' ');
            break;
        case AFMT_U16_LE:
            p_aout->output.output.i_format = VLC_FOURCC('u','1','6','l');
            break;
        case AFMT_S16_LE:
            p_aout->output.output.i_format = VLC_FOURCC('s','1','6','l');
            break;
        case AFMT_U16_BE:
            p_aout->output.output.i_format = VLC_FOURCC('u','1','6','b');
            break;
        case AFMT_S16_BE:
            p_aout->output.output.i_format = VLC_FOURCC('s','1','6','b');
            break;
        default:
            msg_Err( p_aout, "OSS fell back to an unknown format (%d)",
                     i_format );
            close( p_sys->i_fd );
            free( p_sys );
            return VLC_EGENERIC;
        }

        i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );

        /* Set the number of channels */
        if( ioctl( p_sys->i_fd, SNDCTL_DSP_CHANNELS, &i_nb_channels ) < 0 ||
            i_nb_channels != aout_FormatNbChannels( &p_aout->output.output ) )
        {
            msg_Err( p_aout, "cannot set number of audio channels (%s)",
                     aout_FormatPrintChannels( &p_aout->output.output) );
            close( p_sys->i_fd );
            free( p_sys );
            return VLC_EGENERIC;
        }

        /* Set the output rate */
        i_rate = p_aout->output.output.i_rate;
        if( ioctl( p_sys->i_fd, SNDCTL_DSP_SPEED, &i_rate ) < 0 )
        {
            msg_Err( p_aout, "cannot set audio output rate (%i)",
                             p_aout->output.output.i_rate );
            close( p_sys->i_fd );
            free( p_sys );
            return VLC_EGENERIC;
        }

        if( i_rate != p_aout->output.output.i_rate )
        {
            p_aout->output.output.i_rate = i_rate;
        }

        /* Set the fragment size */
        aout_FormatPrepare( &p_aout->output.output );

        /* i_fragment = xxxxyyyy where: xxxx        is fragtotal
         *                              1 << yyyy   is fragsize */
        i_fragments = 0;
        i_frame_size = FRAME_SIZE * p_aout->output.output.i_bytes_per_frame;
        while( i_frame_size >>= 1 )
        {
            ++i_fragments;
        }
        i_fragments |= FRAME_COUNT << 16;
        if( ioctl( p_sys->i_fd, SNDCTL_DSP_SETFRAGMENT, &i_fragments ) < 0 )
        {
            msg_Warn( p_aout, "cannot set fragment size (%.8x)", i_fragments );
        }

        if( ioctl( p_sys->i_fd, SNDCTL_DSP_GETOSPACE, &audio_buf ) < 0 )
        {
            msg_Err( p_aout, "cannot get fragment size" );
            close( p_sys->i_fd );
            free( p_sys );
            return VLC_EGENERIC;
        }
        else
        {
            /* Number of fragments actually allocated */
            p_aout->output.p_sys->i_fragstotal = audio_buf.fragstotal;

            /* Maximum duration the soundcard's buffer can hold */
            p_aout->output.p_sys->max_buffer_duration =
                (mtime_t)audio_buf.fragstotal * audio_buf.fragsize * 1000000
                / p_aout->output.output.i_bytes_per_frame
                / p_aout->output.output.i_rate
                * p_aout->output.output.i_frame_length;

            p_aout->output.i_nb_samples = audio_buf.fragsize /
                p_aout->output.output.i_bytes_per_frame;
        }

        aout_VolumeSoftInit( p_aout );
    }

    p_aout->output.p_sys->b_workaround_buggy_driver =
        config_GetInt( p_aout, "oss-buggy" );

    /* Create OSS thread and wait for its readiness. */
    if( vlc_thread_create( p_aout, "aout", OSSThread,
                           VLC_THREAD_PRIORITY_OUTPUT, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create OSS thread (%s)", strerror(errno) );
        close( p_sys->i_fd );
        free( p_sys );
        return VLC_ETHREAD;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
}

/*****************************************************************************
 * Close: close the dsp audio device
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    p_aout->b_die = VLC_TRUE;
    vlc_thread_join( p_aout );
    p_aout->b_die = VLC_FALSE;

    ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL );
    close( p_sys->i_fd );

    free( p_sys );
}

/*****************************************************************************
 * BufferDuration: buffer status query
 *****************************************************************************
 * This function returns the duration in microseconds of the current buffer.
 *****************************************************************************/
static mtime_t BufferDuration( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    audio_buf_info audio_buf;
    int i_bytes;

    /* Fill the audio_buf_info structure:
     * - fragstotal: total number of fragments allocated
     * - fragsize: size of a fragment in bytes
     * - bytes: available space in bytes (includes partially used fragments)
     * Note! 'bytes' could be more than fragments*fragsize */
    ioctl( p_sys->i_fd, SNDCTL_DSP_GETOSPACE, &audio_buf );

    /* calculate number of available fragments (not partially used ones) */
    i_bytes = (audio_buf.fragstotal * audio_buf.fragsize) - audio_buf.bytes;

    /* Return the fragment duration */
    return (mtime_t)i_bytes * 1000000
            / p_aout->output.output.i_bytes_per_frame
            / p_aout->output.output.i_rate
            * p_aout->output.output.i_frame_length;
}

/*****************************************************************************
 * OSSThread: asynchronous thread used to DMA the data to the device
 *****************************************************************************/
static int OSSThread( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    mtime_t next_date = 0;

    while ( !p_aout->b_die )
    {
        aout_buffer_t * p_buffer = NULL;
        int i_tmp, i_size;
        byte_t * p_bytes;

        if ( p_aout->output.output.i_format != VLC_FOURCC('s','p','d','i') )
        {
            mtime_t buffered = BufferDuration( p_aout );

            if( p_aout->output.p_sys->b_workaround_buggy_driver )
            {
#define i_fragstotal p_aout->output.p_sys->i_fragstotal
                /* Wait a bit - we don't want our buffer to be full */
                if( buffered > (p_aout->output.p_sys->max_buffer_duration
                                / i_fragstotal * (i_fragstotal - 1)) )
                {
                    msleep((p_aout->output.p_sys->max_buffer_duration
                                / i_fragstotal ));
                    buffered = BufferDuration( p_aout );
                }
#undef i_fragstotal
            }

            /* Next buffer will be played at mdate() + buffered */
            p_buffer = aout_OutputNextBuffer( p_aout, mdate() + buffered,
                                              VLC_FALSE );

            if( p_buffer == NULL &&
                buffered > ( p_aout->output.p_sys->max_buffer_duration
                             / p_aout->output.p_sys->i_fragstotal ) )
            {
                /* If we have at least a fragment full, then we can wait a
                 * little and retry to get a new audio buffer instead of
                 * playing a blank sample */
                msleep( ( p_aout->output.p_sys->max_buffer_duration
                          / p_aout->output.p_sys->i_fragstotal / 2 ) );
                continue;
            }
        }
        else
        {
            /* emu10k1 driver does not report Buffer Duration correctly in
             * passthrough mode so we have to cheat */
            if( !next_date )
            {
                next_date = mdate();
            }
            else
            {
                mtime_t delay = next_date - mdate();
                if( delay > AOUT_PTS_TOLERANCE )
                {
                    msleep( delay / 2 );
                }
            }
            
            while( !p_aout->b_die && ! ( p_buffer =
                aout_OutputNextBuffer( p_aout, next_date, VLC_TRUE ) ) )
            {
                msleep( 1000 );
                next_date = mdate();
            }
        }

        if ( p_buffer != NULL )
        {
            p_bytes = p_buffer->p_buffer;
            i_size = p_buffer->i_nb_bytes;
            /* This is theoretical ... we'll see next iteration whether
             * we're drifting */
            next_date += p_buffer->end_date - p_buffer->start_date;
        }
        else
        {
            i_size = FRAME_SIZE / p_aout->output.output.i_frame_length
                      * p_aout->output.output.i_bytes_per_frame;
            p_bytes = malloc( i_size );
            memset( p_bytes, 0, i_size );
            next_date = 0;
        }

        i_tmp = write( p_sys->i_fd, p_bytes, i_size );

        if( i_tmp < 0 )
        {
            msg_Err( p_aout, "write failed (%s)", strerror(errno) );
        }

        if ( p_buffer != NULL )
        {
            aout_BufferFree( p_buffer );
        }
        else
        {
            free( p_bytes );
        }
    }

    return VLC_SUCCESS;
}
