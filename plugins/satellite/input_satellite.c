/*****************************************************************************
 * input_satellite.c: Satellite card input
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
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
#include <stdio.h>
#include <stdlib.h>

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "debug.h"

#include "satellite_tools.h"

#define DISEQC 0                            /* Wether you should use Diseqc*/
#define FEC 2                                                      /* FEC */
#define LNB_LOF_1 9750000
#define LNB_LOF_2 10600000
#define LNB_SLOF 11700000

#define SATELLITE_READ_ONCE 3

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  SatelliteOpen       ( input_thread_t * );
static void SatelliteClose      ( input_thread_t * );
static int  SatelliteSetArea    ( input_thread_t *, input_area_t * );
static int  SatelliteSetProgram ( input_thread_t *, pgrm_descriptor_t * );
static void SatelliteSeek       ( input_thread_t *, off_t );

static int  SatelliteInit       ( input_thread_t * );
static void SatelliteEnd        ( input_thread_t * );
static int  SatelliteDemux      ( input_thread_t * );
static int  SatelliteRewind     ( input_thread_t * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( access_getfunctions )( function_list_t * p_function_list )
{
#define access p_function_list->functions.access
    access.pf_open             = SatelliteOpen;
    access.pf_close            = SatelliteClose;
    access.pf_read             = input_FDRead;
    access.pf_set_area         = SatelliteSetArea;
    access.pf_set_program      = SatelliteSetProgram;
    access.pf_seek             = SatelliteSeek;
#undef access
}


void _M( demux_getfunctions )( function_list_t * p_function_list )
{
#define demux p_function_list->functions.demux
    demux.pf_init             = SatelliteInit;
    demux.pf_end              = SatelliteEnd;
    demux.pf_demux            = SatelliteDemux;
    demux.pf_rewind           = SatelliteRewind;
#undef demux
}




/*****************************************************************************
 * SatelliteOpen : open the dvr device
 *****************************************************************************/
static int SatelliteOpen( input_thread_t * p_input )
{
    input_socket_t *   p_satellite;
    char *                      psz_parser; 
    char *                      psz_next;
    int                         i_fd = 0;
    int                         i_freq = 0;
    int                         i_srate = 0;
    boolean_t                   b_pol = 0;

    /* parse the options passed in command line : */
    
    psz_parser = strdup( p_input->psz_name );
    
    if( !psz_parser )
    {
        return( -1 );
    }
 
    i_freq = (int)strtol( psz_parser, &psz_next, 10 );
    
    if ( *psz_next )
    {
        psz_parser = psz_next + 1;
        b_pol = (boolean_t)strtol( psz_parser, &psz_next, 10 );
            if ( *psz_next )
            {
                psz_parser = psz_next + 1;
                i_srate = (boolean_t)strtol( psz_parser, &psz_next, 10 );
            }

    }

    
    /* Initialise structure */
    p_satellite = malloc( sizeof( input_socket_t ) );
    
    if( p_satellite == NULL )
    {
        intf_ErrMsg( "input: satellite: Out of memory" );
        return -1;
    }

    p_input->p_access_data = (void *)p_satellite;
    
    /* Open the DVR device */
    
    intf_WarnMsg( 2, "input: opening file `%s'", DVR);
    
    if( (p_satellite->i_handle = open( DVR,
                                   /*O_NONBLOCK | O_LARGEFILE*/0 )) == (-1) )
    {
        intf_ErrMsg( "input error: cannot open file (%s)", strerror(errno) );
        return -1;
    }

    
    /* Initialize the Satellite Card */

    intf_WarnMsg( 2, "Initializing Sat Card with Freq: %d, Pol: %d, Srate: %d",
                        i_freq, b_pol, i_srate );
    
    if ( ioctl_SECControl( i_freq * 1000, b_pol, LNB_SLOF, DISEQC ) < 0 )
    {
        intf_ErrMsg("input: satellite: An error occured when controling SEC");
        return -1;
    }
    
    intf_WarnMsg( 3, "Initializing Frontend device" );
    switch (ioctl_SetQPSKFrontend ( i_freq * 1000, i_srate* 1000, FEC,
                         LNB_LOF_1, LNB_LOF_2, LNB_SLOF))
    {
        case -2:
            intf_ErrMsg( "input: satellite: Frontend returned 
                    an unexpected event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        case -3:
            intf_ErrMsg( "input: satellite: Frontend returned 
                    no event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        case -4:
            intf_ErrMsg( "input: satellite: Frontend: time out 
                    when polling for event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        case -5:
             intf_ErrMsg( "input: satellite: An error occured when polling 
                    Frontend device" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        case -1:
             intf_ErrMsg( "input: satellite: Frontend returned 
                    an failure event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        default:
            break;
    }

    intf_WarnMsg( 3, " Setting filter on PAT " );
    
    if ( ioctl_SetDMXFilter( 0, &i_fd, 3 ) < 0 )
    {
        intf_ErrMsg( "input: satellite: An error occured when setting 
                filter on PAT" );
        return -1;
    }

    if( input_InitStream( p_input, sizeof( stream_ts_data_t ) ) == -1 )
    {
        intf_ErrMsg( "input: satellite: Not enough memory to allow stream
                        structure" );
        return( -1 );
    }
    
    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.b_pace_control = 1;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_input->i_mtu = SATELLITE_READ_ONCE * TS_PACKET_SIZE;
    p_input->stream.i_method = INPUT_METHOD_SATELLITE;
    p_input->psz_demux = "satellite";
    
    return 0;
    
   }

/*****************************************************************************
 * SatelliteClose : Closes the device
 *****************************************************************************/
static void SatelliteClose( input_thread_t * p_input )
{
    input_socket_t *    p_satellite;
    int                 i_es_index;

    if ( p_input->stream.p_selected_program )
    {
        for ( i_es_index = 1 ;
                i_es_index < p_input->stream.p_selected_program->
                    i_es_number ; 
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if ( p_es->p_decoder_fifo )
            {
                ioctl_UnsetDMXFilter( p_es->i_dmx_fd );
            }
#undef p_es
        }
    }
    
    p_satellite = (input_socket_t *)p_input;
    close( p_satellite->i_handle );
}
    
/*****************************************************************************
 * SatelliteSetArea : Does nothing
 *****************************************************************************/
static int SatelliteSetArea( input_thread_t * p_input, input_area_t * p_area )
{
    return -1;
}

/*****************************************************************************
 * SatelliteSetProgram : Sets the card filters according to the 
 *                 selected program,
 *                 and makes the appropriate changes to stream structure.
 *****************************************************************************/
int SatelliteSetProgram( input_thread_t    * p_input, 
                         pgrm_descriptor_t * p_new_prg )
{
    int                 i_es_index;

    if ( p_input->stream.p_selected_program )
    {
        for ( i_es_index = 1 ;
                i_es_index < p_input->stream.p_selected_program->
                    i_es_number ; 
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if ( p_es->p_decoder_fifo )
            {
                input_UnselectES( p_input , p_es );
            }
            ioctl_UnsetDMXFilter( p_es->i_dmx_fd );
#undef p_es
        }
    }
    
    for (i_es_index = 1 ; i_es_index < p_new_prg->i_es_number ; i_es_index ++ )
    {
#define p_es p_new_prg->pp_es[i_es_index]
        switch( p_es->i_cat )
        {
            case MPEG1_VIDEO_ES:
            case MPEG2_VIDEO_ES:
                if ( p_main->b_video )
                {
                    ioctl_SetDMXFilter( p_es->i_id, &p_es->i_dmx_fd, 1);
                    input_SelectES( p_input , p_es );
                }
                break;
            case MPEG1_AUDIO_ES:
            case MPEG2_AUDIO_ES:
                if ( p_main->b_audio )
                {
                    ioctl_SetDMXFilter( p_es->i_id, &p_es->i_dmx_fd, 2);
                    input_SelectES( p_input , p_es );
                }
                break;
            default:
                ioctl_SetDMXFilter( p_es->i_id, &p_es->i_dmx_fd, 3);
                input_SelectES( p_input , p_es );
                break;
#undef p_es
        }
    }

    p_input->stream.p_selected_program = p_new_prg;

    return( 0 );
}

/*****************************************************************************
 * SatelliteSeek: does nothing (not a seekable stream
 *****************************************************************************/
static void SatelliteSeek( input_thread_t * p_input, off_t i_off )
{
    return;
}

/*****************************************************************************
 * SatelliteInit: initializes TS structures
 *****************************************************************************/
static int SatelliteInit( input_thread_t * p_input )
{
    es_descriptor_t     * p_pat_es;
    es_ts_data_t        * p_demux_data;
    stream_ts_data_t    * p_stream_data;

    /* Initialize the stream */
    input_InitStream( p_input, sizeof( stream_ts_data_t ) );


    /* Init */
    p_stream_data = (stream_ts_data_t *)p_input->stream.p_demux_data;
    p_stream_data->i_pat_version = PAT_UNINITIALIZED ;

    /* We'll have to catch the PAT in order to continue
     * Then the input will catch the PMT and then the others ES
     * The PAT es is indepedent of any program. */
    p_pat_es = input_AddES( p_input, NULL,
                           0x00, sizeof( es_ts_data_t ) );
    p_demux_data=(es_ts_data_t *)p_pat_es->p_demux_data;
    p_demux_data->b_psi = 1;
    p_demux_data->i_psi_type = PSI_IS_PAT;
    p_demux_data->p_psi_section = malloc(sizeof(psi_section_t));
    p_demux_data->p_psi_section->b_is_complete = 1;

    return 0;

}

/*****************************************************************************
 * SatelliteEnd: frees unused data
 *****************************************************************************/
static void SatelliteEnd( input_thread_t * p_input )
{
}

/*****************************************************************************
 * SatelliteDemux
 *****************************************************************************/
static int SatelliteDemux( input_thread_t * p_input )
{
    int             i_read_once = (p_input->i_mtu ?
                                   p_input->i_bufsize / TS_PACKET_SIZE :
                                   SATELLITE_READ_ONCE);
    int             i;

    /* if not set, set filters to the PMTs */

    for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
    {
        if ( p_input->stream.pp_programs[i]->pp_es[0]->i_dmx_fd == 0 )
        {
            intf_WarnMsg( 2, "input: satellite: setting filter on pmt pid %d",
                        p_input->stream.pp_programs[i]->pp_es[0]->i_id);
            ioctl_SetDMXFilter( p_input->stream.pp_programs[i]->pp_es[0]->i_id,
                       &p_input->stream.pp_programs[i]->pp_es[0]->i_dmx_fd,
                       3 );
        }
    }
            
        
    for( i = 0; i < SATELLITE_READ_ONCE; i++ )
    {
        data_packet_t *     p_data;
        ssize_t             i_result;

        i_result = input_ReadTS( p_input, &p_data );

        if( i_result <= 0 )
        {
            return( i_result );
        }

        input_DemuxTS( p_input, p_data );
    }

    return( i_read_once );
}

/*****************************************************************************
 * SatelliteRewind: Does nothing
 *****************************************************************************/
static int SatelliteRewind( input_thread_t * p_input )
{
    return -1;
}

