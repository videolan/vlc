/*****************************************************************************
 * sdl.c : SDL audio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2002 the VideoLAN team
 * $Id$
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Sam Hocevar <sam@zoy.org>
 *          Pierre Baillet <oct@zoy.org>
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
#include <unistd.h>                                      /* write(), close() */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#include SDL_INCLUDE_FILE

#define FRAME_SIZE 2048

/*****************************************************************************
 * aout_sys_t: SDL audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    mtime_t next_date;
    mtime_t buffer_time;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );
static void Play        ( aout_instance_t * );
static void SDLCallback ( void *, uint8_t *, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( "SDL" );
    set_description( N_("Simple DirectMedia Layer audio output") );
    set_capability( "audio output", 40 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    add_shortcut( "sdl" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    SDL_AudioSpec desired, obtained;
    int i_nb_channels;
    vlc_value_t val, text;

    /* Check that no one uses the DSP. */
    uint32_t i_flags = SDL_INIT_AUDIO;
    if( SDL_WasInit( i_flags ) )
    {
        return VLC_EGENERIC;
    }

#ifndef WIN32
    /* Win32 SDL implementation doesn't support SDL_INIT_EVENTTHREAD yet */
    i_flags |= SDL_INIT_EVENTTHREAD;
#endif
#ifndef NDEBUG
    /* In debug mode you may want vlc to dump a core instead of staying
     * stuck */
    i_flags |= SDL_INIT_NOPARACHUTE;
#endif

    /* Initialize library */
    if( SDL_Init( i_flags ) < 0 )
    {
        msg_Err( p_aout, "cannot initialize SDL (%s)", SDL_GetError() );
        return VLC_EGENERIC;
    }

    if ( var_Type( p_aout, "audio-device" ) != 0 )
    {
        /* The user has selected an audio device. */
        vlc_value_t val;
        var_Get( p_aout, "audio-device", &val );
        if ( val.i_int == AOUT_VAR_STEREO )
        {
            p_aout->output.output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        }
        else if ( val.i_int == AOUT_VAR_MONO )
        {
            p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
        }
    }

    i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );
    if ( i_nb_channels > 2 )
    {
        /* SDL doesn't support more than two channels. */
        i_nb_channels = 2;
        p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }
    desired.freq       = p_aout->output.output.i_rate;
    desired.format     = AUDIO_S16SYS;
    desired.channels   = i_nb_channels;
    desired.callback   = SDLCallback;
    desired.userdata   = p_aout;
    desired.samples    = FRAME_SIZE;

    /* Open the sound device. */
    if( SDL_OpenAudio( &desired, &obtained ) < 0 )
    {
        return VLC_EGENERIC;
    }

    SDL_PauseAudio( 0 );

    /* Now have a look at what we got. */
    switch ( obtained.format )
    {
    case AUDIO_S16LSB:
        p_aout->output.output.i_format = VLC_FOURCC('s','1','6','l'); break;
    case AUDIO_S16MSB:
        p_aout->output.output.i_format = VLC_FOURCC('s','1','6','b'); break;
    case AUDIO_U16LSB:
        p_aout->output.output.i_format = VLC_FOURCC('u','1','6','l'); break;
    case AUDIO_U16MSB:
        p_aout->output.output.i_format = VLC_FOURCC('u','1','6','b'); break;
    case AUDIO_S8:
        p_aout->output.output.i_format = VLC_FOURCC('s','8',' ',' '); break;
    case AUDIO_U8:
        p_aout->output.output.i_format = VLC_FOURCC('u','8',' ',' '); break;
    }
    /* Volume is entirely done in software. */
    aout_VolumeSoftInit( p_aout );

    if ( obtained.channels != i_nb_channels )
    {
        p_aout->output.output.i_physical_channels = (obtained.channels == 2 ?
                                            AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT :
                                            AOUT_CHAN_CENTER);

        if ( var_Type( p_aout, "audio-device" ) == 0 )
        {
            var_Create( p_aout, "audio-device",
                        VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
            text.psz_string = _("Audio Device");
            var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

            val.i_int = (obtained.channels == 2) ? AOUT_VAR_STEREO :
                        AOUT_VAR_MONO;
            text.psz_string = (obtained.channels == 2) ? N_("Stereo") :
                              N_("Mono");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart,
                             NULL );
        }
    }
    else if ( var_Type( p_aout, "audio-device" ) == 0 )
    {
        /* First launch. */
        var_Create( p_aout, "audio-device",
                    VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
        text.psz_string = _("Audio Device");
        var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

        val.i_int = AOUT_VAR_STEREO;
        text.psz_string = N_("Stereo");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        val.i_int = AOUT_VAR_MONO;
        text.psz_string = N_("Mono");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        if ( i_nb_channels == 2 )
        {
            val.i_int = AOUT_VAR_STEREO;
        }
        else
        {
            val.i_int = AOUT_VAR_MONO;
        }
        var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
        var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart, NULL );
    }

    val.b_bool = true;
    var_Set( p_aout, "intf-change", val );

    p_aout->output.output.i_rate = obtained.freq;
    p_aout->output.i_nb_samples = obtained.samples;
    p_aout->output.pf_play = Play;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
    VLC_UNUSED(p_aout);
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this);
    SDL_PauseAudio( 1 );
    SDL_CloseAudio();
    SDL_QuitSubSystem( SDL_INIT_AUDIO );
}

/*****************************************************************************
 * SDLCallback: what to do once SDL has played sound samples
 *****************************************************************************/
static void SDLCallback( void * _p_aout, uint8_t * p_stream, int i_len )
{
    aout_instance_t * p_aout = (aout_instance_t *)_p_aout;
    aout_buffer_t *   p_buffer;

    /* SDL is unable to call us at regular times, or tell us its current
     * hardware latency, or the buffer state. So we just pop data and throw
     * it at SDL's face. Nah. */

    vlc_mutex_lock( &p_aout->output_fifo_lock );
    p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
    vlc_mutex_unlock( &p_aout->output_fifo_lock );

    if ( p_buffer != NULL )
    {
        vlc_memcpy( p_stream, p_buffer->p_buffer, i_len );
        aout_BufferFree( p_buffer );
    }
    else
    {
        vlc_memset( p_stream, 0, i_len );
    }
}

