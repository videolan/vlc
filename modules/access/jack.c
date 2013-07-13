/*****************************************************************************
 * jack.c: JACK audio input module
 *****************************************************************************
 * Copyright (C) 2007-2008 VLC authors and VideoLAN
 * Copyright (C) 2007 Société des arts technologiques
 * Copyright (C) 2007 Savoir-faire Linux
 *
 * Authors: Arnaud Sala <arnaud.sala at savoirfairelinux.com>
 *          Julien Plissonneau Duquene <... at savoirfairelinux.com>
 *          Pierre-Luc Beaudoin <pierre-luc.beaudoin at savoirfairelinux.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * \file modules/access/jack.c
 * \brief JACK audio input functions
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_url.h>
#include <vlc_strings.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include <sys/types.h>
#include <unistd.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define PACE_TEXT N_( "Pace" )
#define PACE_LONGTEXT N_( \
    "Read the audio stream at VLC pace rather than Jack pace." )
#define AUTO_CONNECT_TEXT N_( "Auto connection" )
#define AUTO_CONNECT_LONGTEXT N_( \
    "Automatically connect VLC input ports to available output ports." )

vlc_module_begin ()
     set_description( N_("JACK audio input") )
     set_capability( "access_demux", 0 )
     set_shortname( N_( "JACK Input" ) )
     set_category( CAT_INPUT )
     set_subcategory( SUBCAT_INPUT_ACCESS )

     add_bool( "jack-input-use-vlc-pace", false,
         PACE_TEXT, PACE_LONGTEXT, true )
     add_bool( "jack-input-auto-connect", false,
         AUTO_CONNECT_TEXT, AUTO_CONNECT_LONGTEXT, false )

     add_shortcut( "jack" )
     set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct demux_sys_t
{
    /* Audio properties */
    vlc_fourcc_t                i_acodec_raw;
    unsigned int                i_channels;
    int                         i_sample_rate;
    int                         i_audio_max_frame_size;
    int                         i_frequency;
    block_t                     *p_block_audio;
    es_out_id_t                 *p_es_audio;
    date_t                      pts;

    /* Jack properties */
    jack_client_t               *p_jack_client;
    jack_port_t                 **pp_jack_port_input;
    jack_default_audio_sample_t **pp_jack_buffer;
    jack_ringbuffer_t           *p_jack_ringbuffer;
    jack_nframes_t              jack_buffer_size;
    jack_nframes_t              jack_sample_rate;
    size_t                      jack_sample_size;
    char                        *psz_ports;
    char                        **pp_jack_port_table;
    char                        i_match_ports;
};

static int Demux( demux_t * );
static int Control( demux_t *p_demux, int i_query, va_list args );

static void Parse ( demux_t * );
static void Port_finder( demux_t * );
static int Process( jack_nframes_t i_frames, void *p_arg );

static block_t *GrabJack( demux_t * );

/*****************************************************************************
 * Open: Connect to the JACK server
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    unsigned int i;
    demux_t             *p_demux = ( demux_t* )p_this;
    demux_sys_t         *p_sys;
    es_format_t         fmt;
    int i_out_ports = 0;

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    /* Allocate structure */
    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Parse MRL */
    Parse( p_demux );

    /* Create var */
    var_Create( p_demux, "jack-input-use-vlc-pace",
        VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_demux, "jack-input-auto-connect",
        VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* JACK connexions */
    /* define name and connect to jack server */
    char p_vlc_client_name[32];
    sprintf( p_vlc_client_name, "vlc-input-%d", getpid() );
    p_sys->p_jack_client = jack_client_open( p_vlc_client_name, JackNullOption, NULL );
    if( p_sys->p_jack_client == NULL )
    {
        msg_Err( p_demux, "failed to connect to JACK server" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* find some specifics ports if user entered a regexp */
    if( p_sys->psz_ports )
    {
        Port_finder( p_demux );
        if( p_sys->i_channels == 0 )
        {
            p_sys->i_channels = p_sys->i_match_ports;
        }
    }

    /* allocate input ports */
    if( p_sys->i_channels == 0 ) p_sys->i_channels = 2 ; /* default number of port */
    p_sys->pp_jack_port_input = malloc(
        p_sys->i_channels * sizeof( jack_port_t* ) );
    if( p_sys->pp_jack_port_input == NULL )
    {
        jack_client_close( p_sys->p_jack_client );
        free( p_sys );
        return VLC_ENOMEM;
    }

    /* allocate ringbuffer */
    /* The length of the ringbuffer is critical, it must be large enought
       to keep all data between 2 GrabJack() calls.  We assume 1 sec is ok */
    p_sys->p_jack_ringbuffer = jack_ringbuffer_create( p_sys->i_channels
         * jack_get_sample_rate( p_sys->p_jack_client )
         * sizeof( jack_default_audio_sample_t ) );
    if( p_sys->p_jack_ringbuffer == NULL )
    {
        free( p_sys->pp_jack_port_input );
        jack_client_close( p_sys->p_jack_client );
        free( p_sys );
        return VLC_ENOMEM;
    }

    /* register input ports */
    for( i = 0; i <  p_sys->i_channels; i++ )
    {
        char p_input_name[32];
        snprintf( p_input_name, 32, "vlc_in_%d", i+1 );
        p_sys->pp_jack_port_input[i] = jack_port_register(
            p_sys->p_jack_client, p_input_name, JACK_DEFAULT_AUDIO_TYPE,
            JackPortIsInput, 0 );
        if( p_sys->pp_jack_port_input[i] == NULL )
        {
            msg_Err( p_demux, "failed to register a JACK port" );
            jack_ringbuffer_free( p_sys->p_jack_ringbuffer );
            free( p_sys->pp_jack_port_input );
            jack_client_close( p_sys->p_jack_client );
            free( p_sys );
            return VLC_EGENERIC;
        }
    }

    /* allocate buffer for input ports */
    p_sys->pp_jack_buffer = malloc ( p_sys->i_channels
        * sizeof( jack_default_audio_sample_t * ) );
    if( p_sys->pp_jack_buffer == NULL )
    {
        for( i = 0; i < p_sys->i_channels; i++ )
            jack_port_unregister( p_sys->p_jack_client, p_sys->pp_jack_port_input[i] );
        jack_ringbuffer_free( p_sys->p_jack_ringbuffer );
        free( p_sys->pp_jack_port_input );
        jack_client_close( p_sys->p_jack_client );
        free( p_sys );
        return VLC_ENOMEM;
    }

    /* set process callback */
    jack_set_process_callback( p_sys->p_jack_client, Process, p_demux );

    /* tell jack server we are ready */
    if ( jack_activate( p_sys->p_jack_client ) )
    {
        msg_Err( p_demux, "failed to activate JACK client" );
        free( p_sys->pp_jack_buffer );
        for( i = 0; i < p_sys->i_channels; i++ )
            jack_port_unregister( p_sys->p_jack_client, p_sys->pp_jack_port_input[i] );
        jack_ringbuffer_free( p_sys->p_jack_ringbuffer );
        free( p_sys->pp_jack_port_input );
        jack_client_close( p_sys->p_jack_client );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* connect vlc input to specifics jack output ports if requested */
   /*  if( var_GetBool( p_demux, "jack-input-auto-connect" ) && p_sys->psz_ports ) */
    if( p_sys->psz_ports )
    {
        int        i_input_ports;
        int        j;
 
    if( p_sys->i_match_ports > 0 )
        {
            for( j = 0; j < p_sys->i_match_ports; j++ )
            {
                i_input_ports = j % p_sys->i_channels;
                jack_connect( p_sys->p_jack_client, p_sys->pp_jack_port_table[j],
                    jack_port_name( p_sys->pp_jack_port_input[i_input_ports] ) );
            }
        }
    }

    /* connect vlc input to all jack output ports if requested */
    if( var_GetBool( p_demux, "jack-input-auto-connect" ) && !p_sys->psz_ports )
    {
        int        i_input_ports;
        int        j;
        const char **pp_jack_port_output;

        pp_jack_port_output = jack_get_ports( p_sys->p_jack_client, NULL, NULL, JackPortIsOutput );

        while( pp_jack_port_output && pp_jack_port_output[i_out_ports] )
        {
            i_out_ports++;
        }
        if( i_out_ports > 0 )
        {
            for( j = 0; j < i_out_ports; j++ )
            {
                i_input_ports = j % p_sys->i_channels;
                jack_connect( p_sys->p_jack_client, pp_jack_port_output[j],
                    jack_port_name( p_sys->pp_jack_port_input[i_input_ports] ) );
            }
        }
        free( pp_jack_port_output );
    }

    /* info about jack server */
    /* get buffers size */
    p_sys->jack_buffer_size = jack_get_buffer_size( p_sys->p_jack_client );
    /* get sample rate */
    p_sys->jack_sample_rate = jack_get_sample_rate( p_sys->p_jack_client );
    /* get sample size */
    p_sys->jack_sample_size = sizeof( jack_default_audio_sample_t );

    /* Define output format */
    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_FL32 );
    fmt.audio.i_channels =  p_sys->i_channels;
    fmt.audio.i_rate =  p_sys->jack_sample_rate;
    fmt.audio.i_bitspersample =  p_sys->jack_sample_size * 8;
    fmt.audio.i_blockalign = fmt.audio.i_bitspersample / 8;
    fmt.i_bitrate = fmt.audio.i_rate * fmt.audio.i_bitspersample
        * fmt.audio.i_channels;

    p_sys->p_es_audio = es_out_Add( p_demux->out, &fmt );
    date_Init( &p_sys->pts, fmt.audio.i_rate, 1 );
    date_Set( &p_sys->pts, 1 );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Close: Disconnect from jack server and release associated resources
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t    *p_demux = ( demux_t* )p_this;
    demux_sys_t    *p_sys = p_demux->p_sys;

    msg_Dbg( p_demux,"Module unloaded" );
    if( p_sys->p_block_audio ) block_Release( p_sys->p_block_audio );
    if( p_sys->p_jack_client ) jack_client_close( p_sys->p_jack_client );
    if( p_sys->p_jack_ringbuffer ) jack_ringbuffer_free( p_sys->p_jack_ringbuffer );
    free( p_sys->pp_jack_port_input );
    free( p_sys->pp_jack_buffer );
    free( p_sys->pp_jack_port_table );
    free( p_sys );
}


/*****************************************************************************
 * Control
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    bool  *pb;
    int64_t     *pi64;
    demux_sys_t *p_sys = p_demux->p_sys;

    switch( i_query )
    {
    /* Special for access_demux */
    case DEMUX_CAN_PAUSE:
    case DEMUX_CAN_SEEK:
        pb = (bool *)va_arg( args, bool * );
        *pb = true;
        return VLC_SUCCESS;

    case DEMUX_SET_PAUSE_STATE:
        return VLC_SUCCESS;
    case DEMUX_CAN_CONTROL_PACE:
        pb = ( bool* )va_arg( args, bool * );
        *pb = var_GetBool( p_demux, "jack-input-use-vlc-pace" );
        return VLC_SUCCESS;

    case DEMUX_GET_PTS_DELAY:
        pi64 = ( int64_t* )va_arg( args, int64_t * );
        *pi64 = INT64_C(1000) * var_InheritInteger( p_demux, "live-caching" );
        return VLC_SUCCESS;

    case DEMUX_GET_TIME:
        pi64 = ( int64_t* )va_arg( args, int64_t * );
        *pi64 =  date_Get(&p_sys->pts);
            return VLC_SUCCESS;

    /* TODO implement others */
    default:
        return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}


/*****************************************************************************
 * Demux
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{

    demux_sys_t *p_sys;
    es_out_id_t  *p_es;
    block_t *p_block;

    p_sys = p_demux->p_sys;
    p_es = p_sys->p_es_audio;
    p_block = GrabJack( p_demux );

    if( p_block )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
        es_out_Send( p_demux->out, p_es, p_block );
    }

    return 1;
}


/*****************************************************************************
 * Process Callback : fill ringbuffer with Jack audio data
 *****************************************************************************/
int Process( jack_nframes_t i_frames, void *p_arg )
{
    demux_t            *p_demux = ( demux_t* )p_arg;
    demux_sys_t        *p_sys = p_demux->p_sys;
    unsigned int        i, j;
    size_t              i_write;
 
    /* Get and interlace buffers */
    for ( i = 0; i < p_sys->i_channels ; i++ )
    {
        p_sys->pp_jack_buffer[i] = jack_port_get_buffer(
            p_sys->pp_jack_port_input[i], i_frames );
    }

    /* fill ring buffer with signal */
    for( j = 0; j < i_frames; j++ )
    {
        for( i = 0; i <p_sys->i_channels; i++ )
        {
            if( jack_ringbuffer_write_space( p_sys->p_jack_ringbuffer ) <
                p_sys->jack_sample_size ) {
                msg_Err( p_demux, "buffer overflow");
                return 0; // buffer overflow
            }
            i_write = jack_ringbuffer_write( p_sys->p_jack_ringbuffer,
                                             ( char * ) (p_sys->pp_jack_buffer[i]+j),
                                             p_sys->jack_sample_size );
            if (i_write != p_sys->jack_sample_size ) {
                msg_Warn( p_demux, "error writing on ring buffer");
            }
        }
    }

    return 0;
}


/*****************************************************************************
 * GrabJack: grab audio data in the Jack buffer
 *****************************************************************************/
static block_t *GrabJack( demux_t *p_demux )
{
    size_t      i_read;
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    /* read signal from ring buffer */
    i_read = jack_ringbuffer_read_space( p_sys->p_jack_ringbuffer );

    if( i_read < 100 ) /* avoid small read */
    {   /* vlc has too much free time on its hands? */
#undef msleep
#warning Hmm.... looks wrong
        msleep(1000);
        return NULL;
    }

    if( p_sys->p_block_audio )
    {
        p_block = p_sys->p_block_audio;
    }
    else
    {
        p_block = block_Alloc( i_read );
    }
    if( !p_block )
    {
        msg_Warn( p_demux, "cannot get block" );
        return 0;
    }
 
    //Find the previous power of 2, this algo assumes size_t has the same size on all arch
    i_read >>= 1;
    i_read--;
    i_read |= i_read >> 1;
    i_read |= i_read >> 2;
    i_read |= i_read >> 4;
    i_read |= i_read >> 8;
    i_read |= i_read >> 16;
    i_read++;
 
    i_read = jack_ringbuffer_read( p_sys->p_jack_ringbuffer, ( char * ) p_block->p_buffer, i_read );
 
    p_block->i_dts = p_block->i_pts =    date_Increment( &p_sys->pts,
         i_read/(p_sys->i_channels * p_sys->jack_sample_size) );

    p_sys->p_block_audio = p_block;
    p_block->i_buffer = i_read;
    p_sys->p_block_audio = 0;
 
    return p_block;
}


/*****************************************************************************
 * Port_finder: compare ports with the regexp entered
 *****************************************************************************/
static void Port_finder( demux_t *p_demux )
{

    demux_sys_t *p_sys = p_demux->p_sys;
    char *psz_expr = p_sys->psz_ports;
    char *token = NULL;
    char *state = NULL;
    char *psz_uri = NULL;
    const char **pp_jack_port_output = NULL;
    int i_out_ports = 0;
    int i_total_out_ports =0;
    p_sys->pp_jack_port_table = NULL;

    /* parse the ports part of the MRL */
    for( token = strtok_r( psz_expr, ",", &state ); token;
            token = strtok_r( NULL, ",", &state ) )
    {
        psz_uri = decode_URI_duplicate( token );
        /* get the ports which match the regexp */
        pp_jack_port_output = jack_get_ports( p_sys->p_jack_client,
           psz_uri, NULL, JackPortIsOutput );
        if( pp_jack_port_output == NULL )
            msg_Err( p_demux, "port(s) asked not found:%s", psz_uri );
        else
        {
            while( pp_jack_port_output[i_out_ports] )
                i_out_ports++;
            /* alloc an array to store all the matched ports */
            p_sys->pp_jack_port_table = xrealloc( p_sys->pp_jack_port_table,
                (i_out_ports * sizeof( char * ) + i_total_out_ports * sizeof( char * ) ) );

            for(int i=0; i<i_out_ports;i++)
                p_sys->pp_jack_port_table[i_total_out_ports+i] = ( char * ) pp_jack_port_output[i];

            i_total_out_ports += i_out_ports;

            free( pp_jack_port_output );
        }
    }

    p_sys->i_match_ports = i_total_out_ports;
}


/*****************************************************************************
 * Parse: Parse the MRL
 *****************************************************************************/
static void Parse( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char *psz_dup = strdup( p_demux->psz_location );
    char *psz_parser = psz_dup;

    if( !strncmp( psz_parser, "channels=", strlen( "channels=" ) ) )
    {
        p_sys->i_channels = abs( strtol( psz_parser + strlen( "channels=" ),
            &psz_parser, 0 ) );
    }
    else if( !strncmp( psz_parser, "ports=", strlen( "ports=" ) ) )
    {
        int i_len;
        psz_parser += strlen( "ports=" );
        if( strchr( psz_parser, ':' ) )
        {
            i_len = strchr( psz_parser, ':' ) - psz_parser;
        }
        else
        {
            i_len = strlen( psz_parser );
        }
        p_sys->psz_ports = strndup( psz_parser, i_len );
        psz_parser += i_len;
    }
    else
    {
        msg_Warn( p_demux, "unknown option" );
    }

    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }

    if( *psz_parser == ':' )
    {
        for( ;; )
        {
            *psz_parser++ = '\0';
            if( !strncmp( psz_parser, "channels=", strlen( "channels=" ) ) )
            {
                p_sys->i_channels = abs( strtol(
                    psz_parser + strlen( "channels=" ), &psz_parser, 0 ) );
            }
            else if( !strncmp( psz_parser, "ports=", strlen( "ports=" ) ) )
            {
                int i_len;
                psz_parser += strlen( "ports=" );
                if( strchr( psz_parser, ':' ) )
                {
                    i_len = strchr( psz_parser, ':' ) - psz_parser;
                }
                else
                {
                    i_len = strlen( psz_parser );
                }
                p_sys->psz_ports = strndup( psz_parser, i_len );
                psz_parser += i_len;
            }
            else
            {
                msg_Warn( p_demux, "unknown option" );
            }
            while( *psz_parser && *psz_parser != ':' )
            {
                psz_parser++;
            }

            if( *psz_parser == '\0' )
            {
                break;
            }
        }
    }
}

