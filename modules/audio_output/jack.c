/*****************************************************************************
 * jack.c : JACK audio output module
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax _at_ videolan.org>
 *          Jon Griffiths <jon_p_griffiths _At_ yahoo _DOT_ com>
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
/**
 * \file modules/audio_output/jack.c
 * \brief JACK audio output functions
 */
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

#include <jack/jack.h>

typedef jack_default_audio_sample_t jack_sample_t;

/*****************************************************************************
 * aout_sys_t: JACK audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some JACK specific variables.
 *****************************************************************************/
struct aout_sys_t
{
    aout_packet_t   packet;
    jack_client_t  *p_jack_client;
    jack_port_t   **p_jack_ports;
    jack_sample_t **p_jack_buffers;
    unsigned int    i_channels;
    jack_nframes_t latency;
    float soft_gain;
    bool soft_mute;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static int  Process      ( jack_nframes_t i_frames, void *p_arg );
static int  GraphChange  ( void *p_arg );

#include "volume.h"

#define AUTO_CONNECT_OPTION "jack-auto-connect"
#define AUTO_CONNECT_TEXT N_("Automatically connect to writable clients")
#define AUTO_CONNECT_LONGTEXT N_( \
    "If enabled, this option will automatically connect sound output to the " \
    "first writable JACK clients found." )

#define CONNECT_REGEX_OPTION "jack-connect-regex"
#define CONNECT_REGEX_TEXT N_("Connect to clients matching")
#define CONNECT_REGEX_LONGTEXT N_( \
    "If automatic connection is enabled, only JACK clients whose names " \
    "match this regular expression will be considered for connection." )

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( "JACK" )
    set_description( N_("JACK audio output") )
    set_capability( "audio output", 100 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_bool( AUTO_CONNECT_OPTION, true, AUTO_CONNECT_TEXT,
              AUTO_CONNECT_LONGTEXT, false )
    add_string( CONNECT_REGEX_OPTION, "system", CONNECT_REGEX_TEXT,
                CONNECT_REGEX_LONGTEXT, false )
    add_sw_gain( )
    set_callbacks( Open, Close )
vlc_module_end ()


static int Start( audio_output_t *p_aout, audio_sample_format_t *restrict fmt )
{
    char psz_name[32];
    struct aout_sys_t *p_sys = p_aout->sys;
    int status = VLC_SUCCESS;
    unsigned int i;
    int i_error;

    p_sys->latency = 0;

    /* Connect to the JACK server */
    snprintf( psz_name, sizeof(psz_name), "vlc_%d", getpid());
    psz_name[sizeof(psz_name) - 1] = '\0';
    p_sys->p_jack_client = jack_client_open( psz_name,
                                             JackNullOption | JackNoStartServer,
                                             NULL );
    if( p_sys->p_jack_client == NULL )
    {
        msg_Err( p_aout, "failed to connect to JACK server" );
        status = VLC_EGENERIC;
        goto error_out;
    }

    /* Set the process callback */
    jack_set_process_callback( p_sys->p_jack_client, Process, p_aout );
    jack_set_graph_order_callback ( p_sys->p_jack_client, GraphChange, p_aout );

    /* JACK only supports fl32 format */
    fmt->i_format = VLC_CODEC_FL32;
    // TODO add buffer size callback
    fmt->i_rate = jack_get_sample_rate( p_sys->p_jack_client );

    p_aout->time_get = aout_PacketTimeGet;
    p_aout->play = aout_PacketPlay;
    p_aout->pause = aout_PacketPause;
    p_aout->flush = aout_PacketFlush;
    aout_PacketInit( p_aout, &p_sys->packet,
                     jack_get_buffer_size( p_sys->p_jack_client ), fmt );
    aout_SoftVolumeStart( p_aout );

    p_sys->i_channels = aout_FormatNbChannels( fmt );

    p_sys->p_jack_ports = malloc( p_sys->i_channels *
                                  sizeof(jack_port_t *) );
    if( p_sys->p_jack_ports == NULL )
    {
        status = VLC_ENOMEM;
        goto error_out;
    }

    p_sys->p_jack_buffers = malloc( p_sys->i_channels *
                                    sizeof(jack_sample_t *) );
    if( p_sys->p_jack_buffers == NULL )
    {
        status = VLC_ENOMEM;
        goto error_out;
    }

    /* Create the output ports */
    for( i = 0; i < p_sys->i_channels; i++ )
    {
        snprintf( psz_name, sizeof(psz_name), "out_%d", i + 1);
        psz_name[sizeof(psz_name) - 1] = '\0';
        p_sys->p_jack_ports[i] = jack_port_register( p_sys->p_jack_client,
                psz_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );

        if( p_sys->p_jack_ports[i] == NULL )
        {
            msg_Err( p_aout, "failed to register a JACK port" );
            status = VLC_EGENERIC;
            goto error_out;
        }
    }

    /* Tell the JACK server we are ready */
    i_error = jack_activate( p_sys->p_jack_client );
    if( i_error )
    {
        msg_Err( p_aout, "failed to activate JACK client (error %d)", i_error );
        status = VLC_EGENERIC;
        goto error_out;
    }

    /* Auto connect ports if we were asked to */
    if( var_InheritBool( p_aout, AUTO_CONNECT_OPTION ) )
    {
        unsigned int i_in_ports;
        char *psz_regex = var_InheritString( p_aout, CONNECT_REGEX_OPTION );
        const char **pp_in_ports = jack_get_ports( p_sys->p_jack_client,
                                                   psz_regex, NULL,
                                                   JackPortIsInput );
        free( psz_regex );
        /* Count the number of returned ports */
        i_in_ports = 0;
        while( pp_in_ports && pp_in_ports[i_in_ports] )
        {
            i_in_ports++;
        }

        /* Tie the output ports to JACK input ports */
        for( i = 0; i_in_ports > 0 && i < p_sys->i_channels; i++ )
        {
            const char* psz_in = pp_in_ports[i % i_in_ports];
            const char* psz_out = jack_port_name( p_sys->p_jack_ports[i] );

            i_error = jack_connect( p_sys->p_jack_client, psz_out, psz_in );
            if( i_error )
            {
                msg_Err( p_aout, "failed to connect port %s to port %s (error %d)",
                         psz_out, psz_in, i_error );
            }
            else
            {
                msg_Dbg( p_aout, "connecting port %s to port %s",
                         psz_out, psz_in );
            }
        }
        free( pp_in_ports );
    }

    msg_Dbg( p_aout, "JACK audio output initialized (%d channels, rate=%d)",
             p_sys->i_channels, fmt->i_rate );

error_out:
    /* Clean up, if an error occurred */
    if( status != VLC_SUCCESS && p_sys != NULL)
    {
        if( p_sys->p_jack_client )
        {
            jack_deactivate( p_sys->p_jack_client );
            jack_client_close( p_sys->p_jack_client );
            aout_PacketDestroy( p_aout );
        }
        free( p_sys->p_jack_ports );
        free( p_sys->p_jack_buffers );
    }
    return status;
}


/*****************************************************************************
 * Process: callback for JACK
 *****************************************************************************/
int Process( jack_nframes_t i_frames, void *p_arg )
{
    unsigned int i, j, i_nb_samples = 0;
    audio_output_t *p_aout = (audio_output_t*) p_arg;
    struct aout_sys_t *p_sys = p_aout->sys;
    jack_sample_t *p_src = NULL;

    jack_nframes_t dframes = p_sys->latency
                             - jack_frames_since_cycle_start( p_sys->p_jack_client );

    jack_time_t dtime = dframes * 1000 * 1000 / jack_get_sample_rate( p_sys->p_jack_client );
    mtime_t play_date = mdate() + (mtime_t) ( dtime );

    /* Get the next audio data buffer */
    block_t *p_buffer = aout_PacketNext( p_aout, play_date );

    if( p_buffer != NULL )
    {
        p_src = (jack_sample_t *)p_buffer->p_buffer;
        i_nb_samples = p_buffer->i_nb_samples;
    }

    /* Get the JACK buffers to write to */
    for( i = 0; i < p_sys->i_channels; i++ )
    {
        p_sys->p_jack_buffers[i] = jack_port_get_buffer( p_sys->p_jack_ports[i],
                                                         i_frames );
    }

    /* Copy in the audio data */
    for( j = 0; j < i_nb_samples; j++ )
    {
        for( i = 0; i < p_sys->i_channels; i++ )
        {
            jack_sample_t *p_dst = p_sys->p_jack_buffers[i];
            p_dst[j] = *p_src;
            p_src++;
        }
    }

    /* Fill any remaining buffer with silence */
    if( i_nb_samples < i_frames )
    {
        for( i = 0; i < p_sys->i_channels; i++ )
        {
            memset( p_sys->p_jack_buffers[i] + i_nb_samples, 0,
                    sizeof( jack_sample_t ) * (i_frames - i_nb_samples) );
        }
    }

    if( p_buffer )
        block_Release( p_buffer );
    return 0;
}

/*****************************************************************************
 * GraphChange: callback when JACK reorders it's process graph.
                We update latency information.
 *****************************************************************************/

static int GraphChange( void *p_arg )
{
  audio_output_t *p_aout = (audio_output_t*) p_arg;
  struct aout_sys_t *p_sys = p_aout->sys;
  unsigned int i;
  jack_nframes_t port_latency;

  p_sys->latency = 0;

  for( i = 0; i < p_sys->i_channels; ++i )
  {
    port_latency = jack_port_get_total_latency( p_sys->p_jack_client,
                                                  p_sys->p_jack_ports[i] );
    p_sys->latency = __MAX( p_sys->latency, port_latency );
  }

  msg_Dbg(p_aout, "JACK graph reordered. Our maximum latency=%d.", p_sys->latency);

  return 0;
}

/*****************************************************************************
 * Close: close the JACK client
 *****************************************************************************/
static void Stop( audio_output_t *p_aout )
{
    int i_error;
    struct aout_sys_t *p_sys = p_aout->sys;

    i_error = jack_deactivate( p_sys->p_jack_client );
    if( i_error )
    {
        msg_Err( p_aout, "jack_deactivate failed (error %d)", i_error );
    }

    i_error = jack_client_close( p_sys->p_jack_client );
    if( i_error )
    {
        msg_Err( p_aout, "jack_client_close failed (error %d)", i_error );
    }
    free( p_sys->p_jack_ports );
    free( p_sys->p_jack_buffers );
    aout_PacketDestroy( p_aout );
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = calloc(1, sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout_SoftVolumeInit(aout);
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free(sys);
}
