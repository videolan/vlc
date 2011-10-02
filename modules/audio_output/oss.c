/*****************************************************************************
 * oss.c : OSS /dev/dsp module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2002 the VideoLAN team
 * $Id$
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Sam Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <sys/ioctl.h>                                            /* ioctl() */
#include <unistd.h>                                      /* write(), close() */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>

#include <vlc_aout.h>

/* SNDCTL_DSP_RESET, SNDCTL_DSP_SETFMT, SNDCTL_DSP_STEREO, SNDCTL_DSP_SPEED,
 * SNDCTL_DSP_GETOSPACE */
#ifdef HAVE_SOUNDCARD_H
#   include <soundcard.h>
#elif defined( HAVE_SYS_SOUNDCARD_H )
#   include <sys/soundcard.h>
#endif

/* Patches for ignorant OSS versions */
#ifndef AFMT_AC3
#   define AFMT_AC3     0x00000400        /* Dolby Digital AC3 */
#endif

#ifndef AFMT_S16_NE
#   ifdef WORDS_BIGENDIAN
#       define AFMT_S16_NE AFMT_S16_BE
#   else
#       define AFMT_S16_NE AFMT_S16_LE
#   endif
#endif

/*****************************************************************************
 * aout_sys_t: OSS audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the DSP specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    aout_packet_t packet;
    int i_fd;
    int i_fragstotal;
    mtime_t max_buffer_duration;
    vlc_thread_t thread;
};

/* This must be a power of 2. */
#define FRAME_SIZE 1024
#define FRAME_COUNT 32

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static void Play         ( audio_output_t *, block_t * );
static void* OSSThread   ( void * );

static mtime_t BufferDuration( audio_output_t * p_aout );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( "OSS" )
    set_description( N_("Open Sound System") )

    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_loadfile( "oss-audio-device", "/dev/dsp",
                  N_("OSS DSP device"), NULL, false )

    set_capability( "audio output", 100 )
    add_shortcut( "oss" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Probe: probe the audio device for available formats and channels
 *****************************************************************************/
static void Probe( audio_output_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->sys;
    vlc_value_t val, text;
    int i_format, i_nb_channels;

    var_Create( p_aout, "audio-device", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Audio Device");
    var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

    /* Test for multi-channel. */
#ifdef SNDCTL_DSP_GETCHANNELMASK
    if ( aout_FormatNbChannels( &p_aout->format ) > 2 )
    {
        /* Check that the device supports this. */

        int i_chanmask;

        /* Reset all. */
        i_format = AFMT_S16_NE;
        if( ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 ||
            ioctl( p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
        {
            msg_Err( p_aout, "cannot reset OSS audio device" );
            var_Destroy( p_aout, "audio-device" );
            return;
        }

        if ( ioctl( p_sys->i_fd, SNDCTL_DSP_GETCHANNELMASK,
                    &i_chanmask ) == 0 )
        {
            if ( !(i_chanmask & DSP_BIND_FRONT) )
            {
                msg_Err( p_aout, "no front channels! (%x)",
                         i_chanmask );
                return;
            }

            if ( (i_chanmask & (DSP_BIND_SURR | DSP_BIND_CENTER_LFE))
                  && (p_aout->format.i_physical_channels ==
                       (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                         | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                         | AOUT_CHAN_LFE)) )
            {
                val.i_int = AOUT_VAR_5_1;
                text.psz_string = (char*) "5.1";
                var_Change( p_aout, "audio-device",
                            VLC_VAR_ADDCHOICE, &val, &text );
            }

            if ( (i_chanmask & DSP_BIND_SURR)
                  && (p_aout->format.i_physical_channels &
                       (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                         | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT)) )
            {
                val.i_int = AOUT_VAR_2F2R;
                text.psz_string = _("2 Front 2 Rear");
                var_Change( p_aout, "audio-device",
                            VLC_VAR_ADDCHOICE, &val, &text );
            }
        }
    }
#endif

    /* Reset all. */
    i_format = AFMT_S16_NE;
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 ||
        ioctl( p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0 )
    {
        msg_Err( p_aout, "cannot reset OSS audio device" );
        var_Destroy( p_aout, "audio-device" );
        return;
    }

    /* Test for stereo. */
    i_nb_channels = 2;
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_CHANNELS, &i_nb_channels ) >= 0
         && i_nb_channels == 2 )
    {
        val.i_int = AOUT_VAR_STEREO;
        text.psz_string = _("Stereo");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
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
        val.i_int = AOUT_VAR_MONO;
        text.psz_string = _("Mono");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        if ( p_aout->format.i_physical_channels == AOUT_CHAN_CENTER )
        {
            var_Set( p_aout, "audio-device", val );
        }
    }

    if( ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        msg_Err( p_aout, "cannot reset OSS audio device" );
        var_Destroy( p_aout, "audio-device" );
        return;
    }

    /* Test for spdif. */
    if ( AOUT_FMT_SPDIF( &p_aout->format ) )
    {
        i_format = AFMT_AC3;

        if( ioctl( p_sys->i_fd, SNDCTL_DSP_SETFMT, &i_format ) >= 0
             && i_format == AFMT_AC3 )
        {
            val.i_int = AOUT_VAR_SPDIF;
            text.psz_string = _("A/52 over S/PDIF");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            if( var_InheritBool( p_aout, "spdif" ) )
                var_Set( p_aout, "audio-device", val );
        }
        else if( var_InheritBool( p_aout, "spdif" ) )
        {
            msg_Warn( p_aout, "S/PDIF not supported by card" );
        }
    }

    var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart,
                     NULL );
}

/*****************************************************************************
 * Open: open the audio device (the digital sound processor)
 *****************************************************************************
 * This function opens the DSP as a usual non-blocking write-only file, and
 * modifies the p_aout->p_sys->i_fd with the file's descriptor.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    audio_output_t * p_aout = (audio_output_t *)p_this;
    struct aout_sys_t * p_sys;
    char * psz_device;
    vlc_value_t val;

    /* Allocate structure */
    p_aout->sys = p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* Get device name */
    if( (psz_device = var_InheritString( p_aout, "oss-audio-device" )) == NULL )
    {
        msg_Err( p_aout, "no audio device specified (maybe /dev/dsp?)" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Open the sound device in non-blocking mode, because ALSA's OSS
     * emulation and some broken OSS drivers would make a blocking call
     * wait forever until the device is available. Since this breaks the
     * OSS spec, we immediately put it back to blocking mode if the
     * operation was successful. */
    p_sys->i_fd = vlc_open( psz_device, O_WRONLY | O_NDELAY );
    if( p_sys->i_fd < 0 )
    {
        msg_Err( p_aout, "cannot open audio device (%s)", psz_device );
        free( psz_device );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* if the opening was ok, put the device back in blocking mode */
    fcntl( p_sys->i_fd, F_SETFL,
            fcntl( p_sys->i_fd, F_GETFL ) &~ FNDELAY );

    free( psz_device );

    p_aout->pf_play = aout_PacketPlay;
    p_aout->pf_pause = aout_PacketPause;
    p_aout->pf_flush = aout_PacketFlush;

    if ( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout );
    }

    if ( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        /* Probe() has failed. */
        close( p_sys->i_fd );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if ( val.i_int == AOUT_VAR_SPDIF )
    {
        p_aout->format.i_format = VLC_CODEC_SPDIFL;
    }
    else if ( val.i_int == AOUT_VAR_5_1 )
    {
        p_aout->format.i_format = VLC_CODEC_S16N;
        p_aout->format.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
               | AOUT_CHAN_LFE;
    }
    else if ( val.i_int == AOUT_VAR_2F2R )
    {
        p_aout->format.i_format = VLC_CODEC_S16N;
        p_aout->format.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
    }
    else if ( val.i_int == AOUT_VAR_STEREO )
    {
        p_aout->format.i_format = VLC_CODEC_S16N;
        p_aout->format.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }
    else if ( val.i_int == AOUT_VAR_MONO )
    {
        p_aout->format.i_format = VLC_CODEC_S16N;
        p_aout->format.i_physical_channels = AOUT_CHAN_CENTER;
    }
    else
    {
        /* This should not happen ! */
        msg_Err( p_aout, "internal: can't find audio-device (%"PRId64")",
                 val.i_int );
        close( p_sys->i_fd );
        free( p_sys );
        return VLC_EGENERIC;
    }

    var_TriggerCallback( p_aout, "intf-change" );

    /* Reset the DSP device */
    if( ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL ) < 0 )
    {
        msg_Err( p_aout, "cannot reset OSS audio device" );
        close( p_sys->i_fd );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Set the output format */
    if ( AOUT_FMT_SPDIF( &p_aout->format ) )
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

        p_aout->format.i_format = VLC_CODEC_SPDIFL;
        p_aout->format.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->format.i_frame_length = A52_FRAME_NB;

        aout_PacketInit( p_aout, &p_sys->packet, A52_FRAME_NB );
        aout_VolumeNoneInit( p_aout );
    }
    else
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
            p_aout->format.i_format = VLC_CODEC_U8;
            break;
        case AFMT_S8:
            p_aout->format.i_format = VLC_CODEC_S8;
            break;
        case AFMT_U16_LE:
            p_aout->format.i_format = VLC_CODEC_U16L;
            break;
        case AFMT_S16_LE:
            p_aout->format.i_format = VLC_CODEC_S16L;
            break;
        case AFMT_U16_BE:
            p_aout->format.i_format = VLC_CODEC_U16B;
            break;
        case AFMT_S16_BE:
            p_aout->format.i_format = VLC_CODEC_S16B;
            break;
        default:
            msg_Err( p_aout, "OSS fell back to an unknown format (%d)",
                     i_format );
            close( p_sys->i_fd );
            free( p_sys );
            return VLC_EGENERIC;
        }

        i_nb_channels = aout_FormatNbChannels( &p_aout->format );

        /* Set the number of channels */
        if( ioctl( p_sys->i_fd, SNDCTL_DSP_CHANNELS, &i_nb_channels ) < 0 ||
            i_nb_channels != aout_FormatNbChannels( &p_aout->format ) )
        {
            msg_Err( p_aout, "cannot set number of audio channels (%s)",
                     aout_FormatPrintChannels( &p_aout->format) );
            close( p_sys->i_fd );
            free( p_sys );
            return VLC_EGENERIC;
        }

        /* Set the output rate */
        i_rate = p_aout->format.i_rate;
        if( ioctl( p_sys->i_fd, SNDCTL_DSP_SPEED, &i_rate ) < 0 )
        {
            msg_Err( p_aout, "cannot set audio output rate (%i)",
                             p_aout->format.i_rate );
            close( p_sys->i_fd );
            free( p_sys );
            return VLC_EGENERIC;
        }

        if( i_rate != p_aout->format.i_rate )
        {
            p_aout->format.i_rate = i_rate;
        }

        /* Set the fragment size */
        aout_FormatPrepare( &p_aout->format );

        /* i_fragment = xxxxyyyy where: xxxx        is fragtotal
         *                              1 << yyyy   is fragsize */
        i_frame_size = ((uint64_t)p_aout->format.i_bytes_per_frame * p_aout->format.i_rate * 65536) / (48000 * 2 * 2) / FRAME_COUNT;
        i_fragments = 4;
        while( i_fragments < 12 && (1U << i_fragments) < i_frame_size )
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

        /* Number of fragments actually allocated */
        p_aout->sys->i_fragstotal = audio_buf.fragstotal;

        /* Maximum duration the soundcard's buffer can hold */
        p_aout->sys->max_buffer_duration =
                (mtime_t)audio_buf.fragstotal * audio_buf.fragsize * 1000000
                / p_aout->format.i_bytes_per_frame
                / p_aout->format.i_rate
                * p_aout->format.i_frame_length;

        aout_PacketInit( p_aout, &p_sys->packet,
                         audio_buf.fragsize/p_aout->format.i_bytes_per_frame );
        aout_VolumeSoftInit( p_aout );
    }

    /* Create OSS thread and wait for its readiness. */
    if( vlc_clone( &p_sys->thread, OSSThread, p_aout,
                   VLC_THREAD_PRIORITY_OUTPUT ) )
    {
        msg_Err( p_aout, "cannot create OSS thread (%m)" );
        close( p_sys->i_fd );
        aout_PacketDestroy( p_aout );
        free( p_sys );
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the DSP audio device
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->sys;

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );
    p_aout->b_die = false;

    ioctl( p_sys->i_fd, SNDCTL_DSP_RESET, NULL );
    close( p_sys->i_fd );

    aout_PacketDestroy( p_aout );
    free( p_sys );
}

/*****************************************************************************
 * BufferDuration: buffer status query
 *****************************************************************************
 * This function returns the duration in microseconds of the current buffer.
 *****************************************************************************/
static mtime_t BufferDuration( audio_output_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->sys;
    audio_buf_info audio_buf;
    int i_bytes;

#ifdef SNDCTL_DSP_GETODELAY
    if ( ioctl( p_sys->i_fd, SNDCTL_DSP_GETODELAY, &i_bytes ) < 0 )
#endif
    {
        /* Fall back to GETOSPACE and approximate latency. */

        /* Fill the audio_buf_info structure:
         * - fragstotal: total number of fragments allocated
         * - fragsize: size of a fragment in bytes
         * - bytes: available space in bytes (includes partially used fragments)
         * Note! 'bytes' could be more than fragments*fragsize */
        ioctl( p_sys->i_fd, SNDCTL_DSP_GETOSPACE, &audio_buf );

        /* calculate number of available fragments (not partially used ones) */
        i_bytes = (audio_buf.fragstotal * audio_buf.fragsize) - audio_buf.bytes;
    }

    /* Return the fragment duration */
    return (mtime_t)i_bytes * 1000000
            / p_aout->format.i_bytes_per_frame
            / p_aout->format.i_rate
            * p_aout->format.i_frame_length;
}

typedef struct
{
    aout_buffer_t *p_buffer;
    void          *p_bytes;
} oss_thread_ctx_t;

static void OSSThreadCleanup( void *data )
{
    oss_thread_ctx_t *p_ctx = data;
    if( p_ctx->p_buffer )
        aout_BufferFree( p_ctx->p_buffer );
    else
        free( p_ctx->p_bytes );
}

/*****************************************************************************
 * OSSThread: asynchronous thread used to DMA the data to the device
 *****************************************************************************/
static void* OSSThread( void *obj )
{
    audio_output_t * p_aout = (audio_output_t*)obj;
    struct aout_sys_t * p_sys = p_aout->sys;
    mtime_t next_date = 0;

    for( ;; )
    {
        aout_buffer_t * p_buffer = NULL;

        int canc = vlc_savecancel ();
        if ( p_aout->format.i_format != VLC_CODEC_SPDIFL )
        {
            mtime_t buffered = BufferDuration( p_aout );

            /* Next buffer will be played at mdate() + buffered */
            p_buffer = aout_PacketNext( p_aout, mdate() + buffered );

            if( p_buffer == NULL &&
                buffered > ( p_aout->sys->max_buffer_duration
                             / p_aout->sys->i_fragstotal ) )
            {
                vlc_restorecancel (canc);
                /* If we have at least a fragment full, then we can wait a
                 * little and retry to get a new audio buffer instead of
                 * playing a blank sample */
                msleep( ( p_aout->sys->max_buffer_duration
                          / p_aout->sys->i_fragstotal / 2 ) );
                continue;
            }
        }
        else
        {
            vlc_restorecancel (canc);

            /* emu10k1 driver does not report Buffer Duration correctly in
             * passthrough mode so we have to cheat */
            if( !next_date )
            {
                next_date = mdate();
            }
            else
            {
                mtime_t delay = next_date - mdate();
                if( delay > AOUT_MAX_PTS_ADVANCE )
                {
                    msleep( delay / 2 );
                }
            }

            for( ;; )
            {
                canc = vlc_savecancel ();
                p_buffer = aout_PacketNext( p_aout, next_date );
                if ( p_buffer )
                    break;
                vlc_restorecancel (canc);

                msleep( VLC_HARD_MIN_SLEEP );
                next_date = mdate();
            }
        }

        uint8_t * p_bytes;
        int i_size;
        if ( p_buffer != NULL )
        {
            p_bytes = p_buffer->p_buffer;
            i_size = p_buffer->i_buffer;
            /* This is theoretical ... we'll see next iteration whether
             * we're drifting */
            next_date += p_buffer->i_length;
        }
        else
        {
            i_size = FRAME_SIZE / p_aout->format.i_frame_length
                      * p_aout->format.i_bytes_per_frame;
            p_bytes = malloc( i_size );
            memset( p_bytes, 0, i_size );
            next_date = 0;
        }

        oss_thread_ctx_t ctx = {
            .p_buffer = p_buffer,
            .p_bytes  = p_bytes,
        };

        vlc_cleanup_push( OSSThreadCleanup, &ctx );
        vlc_restorecancel( canc );

        int i_tmp = write( p_sys->i_fd, p_bytes, i_size );

        if( i_tmp < 0 )
        {
            msg_Err( p_aout, "write failed (%m)" );
        }
        vlc_cleanup_run();
    }

    return NULL;
}
