/*****************************************************************************
 * jack : JACK audio output module
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
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
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc_aout.h>

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
    jack_port_t  **p_jack_ports;
    unsigned int  i_channels;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Play         ( aout_instance_t * );
static int Process       ( jack_nframes_t i_frames, void *p_arg );

#define AUTO_CONNECT_OPTION "jack-auto-connect"
#define AUTO_CONNECT_TEXT N_("Automatically connect to input devices")
#define AUTO_CONNECT_LONGTEXT N_( \
    "If enabled, this option will automatically connect output to the " \
    "first JACK inputs found." )

#define CONNECT_MATCH_OPTION "jack-connect-match"
#define CONNECT_MATCH_TEXT N_("Connect to outputs beginning with")
#define CONNECT_MATCH_LONGTEXT N_( \
    "If automatic connection is enabled, only JACK inputs whose names " \
    "begin with this prefix will be considered for connection." )

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( "JACK" );
    set_description( _("JACK audio output") );
    set_capability( "audio output", 100 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    add_bool( AUTO_CONNECT_OPTION, 0, NULL, AUTO_CONNECT_TEXT,
              AUTO_CONNECT_LONGTEXT, VLC_TRUE );
    add_string( CONNECT_MATCH_OPTION, NULL, NULL, CONNECT_MATCH_TEXT,
                CONNECT_MATCH_LONGTEXT, VLC_TRUE );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: create a JACK client
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t *p_sys = NULL;
    char **pp_match_ports = NULL;
    char *psz_prefix = NULL;
    int status = VLC_SUCCESS;
    unsigned int i;

    /* Allocate structure */
    p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        status = VLC_ENOMEM;
        goto error_out;
    }
    p_aout->output.p_sys = p_sys;

    /* Connect to the JACK server */
    p_sys->p_jack_client = jack_client_new( "vlc" );
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

    p_sys->p_jack_ports = malloc( p_sys->i_channels * sizeof(jack_port_t  *) );
    if( p_sys->p_jack_ports == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        status = VLC_ENOMEM;
        goto error_out;
    }

    /* Create the output ports */
    for( i = 0; i < p_sys->i_channels; i++ )
    {
        char p_name[32];
        snprintf( p_name, 32, "out_%d", i + 1);
        p_sys->p_jack_ports[i] = jack_port_register( p_sys->p_jack_client,
                p_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );

        if( p_sys->p_jack_ports[i] == NULL )
        {
            msg_Err( p_aout, "failed to register a JACK port" );
            status = VLC_EGENERIC;
            goto error_out;
        }
    }

    /* Tell the JACK server we are ready */
    if( jack_activate( p_sys->p_jack_client ) )
    {
        msg_Err( p_aout, "failed to activate JACK client" );
        jack_client_close( p_sys->p_jack_client );
        status = VLC_EGENERIC;
        goto error_out;
    }

    /* Auto connect ports if we were asked to */
    if( config_GetInt( p_aout, AUTO_CONNECT_OPTION ) )
    {
        unsigned int i_in_ports, i_prefix_len;
        const char **pp_in_ports;

        pp_in_ports = jack_get_ports( p_sys->p_jack_client, NULL, NULL,
                                      JackPortIsInput );
        psz_prefix = config_GetPsz( p_aout, CONNECT_MATCH_OPTION );
        i_prefix_len = psz_prefix ? strlen(psz_prefix) : 0;

        /* Find JACK input ports to connect to */
        i = 0;
        i_in_ports = 0;
        while( pp_in_ports && pp_in_ports[i] )
        {
            if( !psz_prefix ||
                !strncmp(psz_prefix, pp_in_ports[i], i_prefix_len) )
            {
                i_in_ports++; /* Found one */
            }
            i++;
        }

        /* Connect the output ports to input ports */
        if( i_in_ports > 0 )
        {
            pp_match_ports = malloc( i_in_ports * sizeof(char*) );
            if( pp_match_ports == NULL )
            {
                msg_Err( p_aout, "out of memory" );
                status = VLC_ENOMEM;
                goto error_out;
            }

            /* populate list of matching ports */
            i = 0;
            i_in_ports = 0;
            while( pp_in_ports[i] )
            {
                if( !psz_prefix ||
                     !strncmp(psz_prefix, pp_in_ports[i], i_prefix_len) )
                {
                    pp_match_ports[i_in_ports] = pp_in_ports[i];
                    i_in_ports++;  /* Found one */
                }
                i++;
            }

            /* Tie the output ports to JACK input ports */
            for( i = 0; i < p_sys->i_channels; i++ )
            {
                const char* psz_in = pp_match_ports[i % i_in_ports];
                const char* psz_out = jack_port_name( p_sys->p_jack_ports[i] );

                if( jack_connect( p_sys->p_jack_client, psz_out, psz_in) )
                {
                    msg_Err( p_aout, "failed to connect port %s to port %s",
                             psz_out, psz_in );
                }
                else
                {
                    msg_Dbg( p_aout, "connecting port %s to port %s",
                             psz_out, psz_in );
                }
            }
        }
    }

    msg_Dbg( p_aout, "JACK audio output initialized (%d channels, buffer "
             "size=%d, rate=%d)", p_sys->i_channels,
             p_aout->output.i_nb_samples, p_aout->output.output.i_rate );

error_out:
    /* Clean up */
    if( psz_prefix )
        free( psz_prefix );

    if( pp_match_ports )
        free( pp_match_ports );

    if( status != VLC_SUCCESS && p_sys != NULL)
    {
        if( p_sys->p_jack_ports )
            free( p_sys->p_jack_ports );
        if( p_sys->p_jack_client )
            jack_client_close( p_sys->p_jack_client );
        free( p_sys );
    }
    return status;
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
    unsigned int i_nb_channels = p_aout->output.p_sys->i_channels;

    /* Get the next audio data buffer */
    p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );

    if( p_buffer )
    {
        i_nb_samples = p_buffer->i_nb_samples;
    }

    for( i = 0; i < i_nb_channels; i++ )
    {
        /* Get an output buffer from JACK */
        p_jack_buffer = jack_port_get_buffer(
            p_aout->output.p_sys->p_jack_ports[i], i_frames );

        /* Fill the buffer with audio data */
        for( j = 0; j < i_nb_samples; j++ )
        {
            p_jack_buffer[j] = ((float*)p_buffer->p_buffer)[i_nb_channels*j+i];
        }
        if( i_nb_samples < i_frames )
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
    struct aout_sys_t *p_sys = p_aout->output.p_sys;

    free( p_sys->p_jack_ports );
    jack_client_close( p_sys->p_jack_client );
    free( p_sys );
}

