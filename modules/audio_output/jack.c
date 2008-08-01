/*****************************************************************************
 * jack : JACK audio output module
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
    jack_client_t  *p_jack_client;
    jack_port_t   **p_jack_ports;
    jack_sample_t **p_jack_buffers;
    unsigned int    i_channels;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Play         ( aout_instance_t * );
static int  Process      ( jack_nframes_t i_frames, void *p_arg );

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
vlc_module_begin();
    set_shortname( "JACK" );
    set_description( N_("JACK audio output") );
    set_capability( "audio output", 100 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    add_bool( AUTO_CONNECT_OPTION, 0, NULL, AUTO_CONNECT_TEXT,
              AUTO_CONNECT_LONGTEXT, true );
    add_string( CONNECT_REGEX_OPTION, NULL, NULL, CONNECT_REGEX_TEXT,
                CONNECT_REGEX_LONGTEXT, true );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: create a JACK client
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    char psz_name[32];
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t *p_sys = NULL;
    int status = VLC_SUCCESS;
    unsigned int i;
    int i_error;

    /* Allocate structure */
    p_sys = calloc( 1, sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        status = VLC_ENOMEM;
        goto error_out;
    }
    p_aout->output.p_sys = p_sys;

    /* Connect to the JACK server */
    snprintf( psz_name, sizeof(psz_name), "vlc_%d", getpid());
    psz_name[sizeof(psz_name) - 1] = '\0';
    p_sys->p_jack_client = jack_client_new( psz_name );
    if( p_sys->p_jack_client == NULL )
    {
        msg_Err( p_aout, "failed to connect to JACK server" );
        status = VLC_EGENERIC;
        goto error_out;
    }

    /* Set the process callback */
    jack_set_process_callback( p_sys->p_jack_client, Process, p_aout );

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    /* JACK only supports fl32 format */
    p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
    // TODO add buffer size callback
    p_aout->output.i_nb_samples = jack_get_buffer_size( p_sys->p_jack_client );
    p_aout->output.output.i_rate = jack_get_sample_rate( p_sys->p_jack_client );

    p_sys->i_channels = aout_FormatNbChannels( &p_aout->output.output );

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
    if( config_GetInt( p_aout, AUTO_CONNECT_OPTION ) )
    {
        unsigned int i_in_ports;
        char *psz_regex = config_GetPsz( p_aout, CONNECT_REGEX_OPTION );
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

    msg_Dbg( p_aout, "JACK audio output initialized (%d channels, buffer "
             "size=%d, rate=%d)", p_sys->i_channels,
             p_aout->output.i_nb_samples, p_aout->output.output.i_rate );

error_out:
    /* Clean up, if an error occurred */
    if( status != VLC_SUCCESS && p_sys != NULL)
    {
        if( p_sys->p_jack_client )
        {
            jack_deactivate( p_sys->p_jack_client );
            jack_client_close( p_sys->p_jack_client );
        }
        free( p_sys->p_jack_ports );
        free( p_sys->p_jack_buffers );
        free( p_sys );
    }
    return status;
}


/*****************************************************************************
 * Process: callback for JACK
 *****************************************************************************/
int Process( jack_nframes_t i_frames, void *p_arg )
{
    unsigned int i, j, i_nb_samples = 0;
    aout_instance_t *p_aout = (aout_instance_t*) p_arg;
    struct aout_sys_t *p_sys = p_aout->output.p_sys;
    jack_sample_t *p_src = NULL;

    /* Get the next audio data buffer */
    aout_buffer_t *p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
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
    int i_error;
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t *p_sys = p_aout->output.p_sys;

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
    free( p_sys );
}
