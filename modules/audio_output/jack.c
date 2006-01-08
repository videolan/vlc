/*****************************************************************************
 * jack : JACK audio output module
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
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
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>

#include "aout_internal.h"

#include <jack/jack.h>

/*****************************************************************************
 * aout_sys_t: JACK audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some JACK specific variables.
 *****************************************************************************/
struct aout_sys_t
{
    jack_client_t *p_jack_client;
    jack_port_t   *p_jack_port[2];
    unsigned int  i_channels;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Play         ( aout_instance_t * );
static int Process       ( jack_nframes_t i_frames, void *p_arg );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
   set_shortname( "JACK" );
   set_description( _("JACK audio output") );
   set_capability( "audio output", 100 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
   set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: create a JACK client
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    unsigned int i, i_in_ports;
    const char **pp_in_ports;
    struct aout_sys_t * p_sys;

    /* Allocate structure */
    p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_ENOMEM;
    }
    p_aout->output.p_sys = p_sys;

    /* Connect to the JACK server */
    p_sys->p_jack_client = jack_client_new( "vlc" );
    if( p_sys->p_jack_client == NULL )
    {
        msg_Err( p_aout, "Failed to connect to JACK server" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Set the process callback */
    jack_set_process_callback( p_sys->p_jack_client, Process, p_aout );

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    /* JACK only support fl32 format */
    p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
    // TODO add buffer size callback
    p_aout->output.i_nb_samples = jack_get_buffer_size( p_sys->p_jack_client );
    p_aout->output.output.i_rate = jack_get_sample_rate( p_sys->p_jack_client );

    p_sys->i_channels = aout_FormatNbChannels( &p_aout->output.output );

    /* Create the output ports */
    for( i = 0; i < p_sys->i_channels; i++ )
    {
        char p_name[32];
        snprintf( p_name, 32, "channel_%d", i + 1);
        p_sys->p_jack_port[i] = jack_port_register( p_sys->p_jack_client,
                p_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );

        if( p_sys->p_jack_port[i] == NULL )
        {
            msg_Err( p_aout, "Failed to register a JACK port" );
            jack_client_close( p_sys->p_jack_client );
            free( p_sys );
            return VLC_EGENERIC;
        }
    }

    /* Tell the JACK server we are ready */
    if( jack_activate( p_sys->p_jack_client ) )
    {
        msg_Err( p_aout, "Failed to activate JACK client" );
        jack_client_close( p_sys->p_jack_client );
        free( p_sys );
        return VLC_EGENERIC;
    }


    /* Find input ports to connect to */
    pp_in_ports = jack_get_ports( p_sys->p_jack_client, NULL, NULL,
                                  JackPortIsInput );
    i_in_ports = 0;
    while( pp_in_ports && pp_in_ports[i_in_ports] )
    {
        i_in_ports++;
    }

    /* Connect the output ports to input ports */
    if( i_in_ports > 0 )
    {
        for( i = 0; i < p_sys->i_channels; i++ )
        {
            int i_in = i % i_in_ports;
            if( jack_connect( p_sys->p_jack_client,
                              jack_port_name( p_sys->p_jack_port[i] ),
                              pp_in_ports[i_in]) )
            {
                msg_Err( p_aout, "Failed to connect port %s to port %s",
                         jack_port_name( p_sys->p_jack_port[i] ),
                         pp_in_ports[i_in] );

            }
            else
            {
                msg_Dbg( p_aout, "Connecting port %s to port %s",
                         jack_port_name( p_sys->p_jack_port[i] ),
                         pp_in_ports[i_in] );
            }
        }
    }

    msg_Dbg( p_aout, "JACK audio output initialized (%d channels, buffer "
             "size=%d, rate=%d)", p_sys->i_channels,
             p_aout->output.i_nb_samples, p_aout->output.output.i_rate );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Process: callback for JACK
 *****************************************************************************/
int Process( jack_nframes_t i_frames, void *p_arg )
{
    aout_buffer_t *p_buffer;
    jack_default_audio_sample_t *p_jack_buffer;
    unsigned int i, j, i_nb_samples = 0;
    aout_instance_t *p_aout = (aout_instance_t*) p_arg;

    /* Get the next audio data buffer */
    p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );

    if( p_buffer )
    {
        i_nb_samples = p_buffer->i_nb_samples;
    }

    for( i = 0; i < p_aout->output.p_sys->i_channels; i++ )
    {
        /* Get an output buffer from JACK */
        p_jack_buffer = jack_port_get_buffer(
            p_aout->output.p_sys->p_jack_port[i], i_frames );

        /* Fill the buffer with audio data */
        for( j = 0; j < i_nb_samples; j++ )
        {
            p_jack_buffer[j] = ((float*)p_buffer->p_buffer)[2*j+i];
        }
        if (i_nb_samples < i_frames)
        {
            memset( p_jack_buffer + i_nb_samples, 0,
                    sizeof( jack_default_audio_sample_t ) *
                    (i_frames - i_nb_samples) );
        }
    }

    if( p_buffer )
    {
        aout_BufferFree( p_buffer );
    }

    return 0;
}


/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
    aout_FifoFirstDate( p_aout, &p_aout->output.fifo );
}

/*****************************************************************************
 * Close: close the JACK client
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    jack_client_close( p_sys->p_jack_client );
    free( p_sys );
}

