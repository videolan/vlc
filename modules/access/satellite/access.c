/*****************************************************************************
 * access.c: Satellite card input
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

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "../../demux/mpeg/system.h"

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

#include "dvb.h"

#define SATELLITE_READ_ONCE 3

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t SatelliteRead( input_thread_t * p_input, byte_t * p_buffer,
                                     size_t i_len );
static int     SatelliteSetArea    ( input_thread_t *, input_area_t * );
static int     SatelliteSetProgram ( input_thread_t *, pgrm_descriptor_t * );
static void    SatelliteSeek       ( input_thread_t *, off_t );

/*****************************************************************************
 * Open: open the dvr device
 *****************************************************************************/
int E_(Open) ( vlc_object_t *p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    input_socket_t *    p_satellite;
    char *              psz_parser;
    char *              psz_next;
    int                 i_fd = 0;
    int                 i_freq = 0;
    int                 i_srate = 0;
    vlc_bool_t          b_pol = 0;
    int                 i_fec = 0;
    float               f_fec = 1./2;
    vlc_bool_t          b_diseqc;
    int                 i_lnb_lof1;
    int                 i_lnb_lof2;
    int                 i_lnb_slof;

    /* parse the options passed in command line : */

    psz_parser = strdup( p_input->psz_name );

    if( !psz_parser )
    {
        return( -1 );
    }

    p_input->pf_read = SatelliteRead;
    p_input->pf_set_program = SatelliteSetProgram;
    p_input->pf_set_area = SatelliteSetArea;
    p_input->pf_seek = SatelliteSeek;

    i_freq = (int)strtol( psz_parser, &psz_next, 10 );

    if( *psz_next )
    {
        psz_parser = psz_next + 1;
        b_pol = (vlc_bool_t)strtol( psz_parser, &psz_next, 10 );
            if( *psz_next )
            {
                psz_parser = psz_next + 1;
                i_fec = (int)strtol( psz_parser, &psz_next, 10 );
                if( *psz_next )
                {
                    psz_parser = psz_next + 1;
                    i_srate = (int)strtol( psz_parser, &psz_next, 10 );
                }
            }

    }

    if( i_freq > 12999 || i_freq < 10000 )
    {
        msg_Warn( p_input, "invalid frequency, using default one" );
        i_freq = config_GetInt( p_input, "frequency" );
        if( i_freq > 12999 || i_freq < 10000 )
        {
            msg_Err( p_input, "invalid default frequency" );
            return -1;
        }
    }

    if( i_srate > 30000 || i_srate < 1000 )
    {
        msg_Warn( p_input, "invalid symbol rate, using default one" );
        i_srate = config_GetInt( p_input, "symbol-rate" );
        if( i_srate > 30000 || i_srate < 1000 )
        {
            msg_Err( p_input, "invalid default symbol rate" );
            return -1;
        }
    }

    if( b_pol && b_pol != 1 )
    {
        msg_Warn( p_input, "invalid polarization, using default one" );
        b_pol = config_GetInt( p_input, "polarization" );
        if( b_pol && b_pol != 1 )
        {
            msg_Err( p_input, "invalid default polarization" );
            return -1;
        }
    }

    if( i_fec > 7 || i_fec < 1 )
    {
        msg_Warn( p_input, "invalid FEC, using default one" );
        i_fec = config_GetInt( p_input, "fec" );
        if( i_fec > 7 || i_fec < 1 )
        {
            msg_Err( p_input, "invalid default FEC" );
            return -1;
        }
    }

    switch( i_fec )
    {
        case 1:
            f_fec = 1./2;
            break;
        case 2:
            f_fec = 2./3;
            break;
        case 3:
            f_fec = 3./4;
            break;
        case 4:
            f_fec = 4./5;
            break;
        case 5:
            f_fec = 5./6;
            break;
        case 6:
            f_fec = 6./7;
            break;
        case 7:
            f_fec = 7./8;
            break;
        default:
            /* cannot happen */
            break;
    }


    /* Initialise structure */
    p_satellite = malloc( sizeof( input_socket_t ) );

    if( p_satellite == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }

    p_input->p_access_data = (void *)p_satellite;

    /* Open the DVR device */
    msg_Dbg( p_input, "opening DVR device `%s'", DVR );

    if( (p_satellite->i_handle = open( DVR,
                                   /*O_NONBLOCK | O_LARGEFILE*/0 )) == (-1) )
    {
        msg_Warn( p_input, "cannot open `%s' (%s)", DVR, strerror(errno) );
        free( p_satellite );
        return -1;
    }


    /* Get antenna configuration options */
    b_diseqc = config_GetInt( p_input, "diseqc" );
    i_lnb_lof1 = config_GetInt( p_input, "lnb-lof1" );
    i_lnb_lof2 = config_GetInt( p_input, "lnb-lof2" );
    i_lnb_slof = config_GetInt( p_input, "lnb-slof" );

    /* Initialize the Satellite Card */

    msg_Dbg( p_input, "initializing Sat Card with Freq: %d, Pol: %d, "
                      "FEC: %03f, Srate: %d", i_freq, b_pol, f_fec, i_srate );

    if ( ioctl_SECControl( i_freq * 1000, b_pol, i_lnb_slof * 1000,
                b_diseqc ) < 0 )
    {
        msg_Err( p_input, "an error occured when controling SEC" );
        close( p_satellite->i_handle );
        free( p_satellite );
        return -1;
    }

    msg_Dbg( p_input, "initializing frontend device" );
    switch (ioctl_SetQPSKFrontend ( i_freq * 1000, i_srate* 1000, f_fec,
                i_lnb_lof1 * 1000, i_lnb_lof2 * 1000, i_lnb_slof * 1000))
    {
        case -2:
            msg_Err( p_input, "frontend returned an unexpected event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        case -3:
            msg_Err( p_input, "frontend returned no event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        case -4:
            msg_Err( p_input, "frontend: timeout when polling for event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        case -5:
            msg_Err( p_input, "an error occured when polling frontend device" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        case -1:
            msg_Err( p_input, "frontend returned a failure event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
            break;
        default:
            break;
    }

    msg_Dbg( p_input, "setting filter on PAT" );

    if ( ioctl_SetDMXFilter( 0, &i_fd, 3 ) < 0 )
    {
        msg_Err( p_input, "an error occured when setting filter on PAT" );
        close( p_satellite->i_handle );
        free( p_satellite );
        return -1;
    }

    if( input_InitStream( p_input, sizeof( stream_ts_data_t ) ) == -1 )
    {
        msg_Err( p_input, "could not initialize stream structure" );
        close( p_satellite->i_handle );
        free( p_satellite );
        return( -1 );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.b_pace_control = 1;
    p_input->stream.b_seekable = 0;
    p_input->stream.b_connected = 0;
    p_input->stream.p_selected_area->i_tell = 0;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_input->i_mtu = SATELLITE_READ_ONCE * TS_PACKET_SIZE;
    p_input->stream.i_method = INPUT_METHOD_SATELLITE;

    return 0;
}

/*****************************************************************************
 * Close : Close the device
 *****************************************************************************/
void E_(Close) ( vlc_object_t *p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    input_socket_t *    p_satellite;
    unsigned int        i_es_index;

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
                ioctl_UnsetDMXFilter( p_es->i_demux_fd );
            }
#undef p_es
        }
    }

    p_satellite = (input_socket_t *)p_input;
    close( p_satellite->i_handle );
}

/*****************************************************************************
 * SatelliteRead: reads data from the satellite card
 *****************************************************************************/
static ssize_t SatelliteRead( input_thread_t * p_input, byte_t * p_buffer,
                              size_t i_len )
{
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;
    ssize_t i_ret;
 
    unsigned int i;

    /* if not set, set filters to the PMTs */
    for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
    {
        if ( p_input->stream.pp_programs[i]->pp_es[0]->i_demux_fd == 0 )
        {
            msg_Dbg( p_input, "setting filter on pmt pid %d",
                     p_input->stream.pp_programs[i]->pp_es[0]->i_id );
            ioctl_SetDMXFilter( p_input->stream.pp_programs[i]->pp_es[0]->i_id,
                       &p_input->stream.pp_programs[i]->pp_es[0]->i_demux_fd,
                       3 );
        }
    }

    i_ret = read( p_access_data->i_handle, p_buffer, i_len );
 
    if( i_ret < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "read failed (%s)", strerror(errno) );
#   else
        msg_Err( p_input, "read failed" );
#   endif
    }
 
    return i_ret;
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
    unsigned int i_es_index;
    vlc_value_t val;

    if ( p_input->stream.p_selected_program )
    {
        for ( i_es_index = 1 ; /* 0 should be the PMT */
                i_es_index < p_input->stream.p_selected_program->
                    i_es_number ;
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if ( p_es->p_decoder_fifo )
            {
                input_UnselectES( p_input , p_es );
            }
            if ( p_es->i_demux_fd )
            {
                ioctl_UnsetDMXFilter( p_es->i_demux_fd );
                p_es->i_demux_fd = 0;
            }
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
            case MPEG2_MOTO_VIDEO_ES:
                if ( input_SelectES( p_input , p_es ) == 0 )
                {
                    ioctl_SetDMXFilter( p_es->i_id, &p_es->i_demux_fd, 1);
                }
                break;
            case MPEG1_AUDIO_ES:
            case MPEG2_AUDIO_ES:
                if ( input_SelectES( p_input , p_es ) == 0 )
                {
                    ioctl_SetDMXFilter( p_es->i_id, &p_es->i_demux_fd, 2);
                    input_SelectES( p_input , p_es );
                }
                break;
            default:
                ioctl_SetDMXFilter( p_es->i_id, &p_es->i_demux_fd, 3);
                input_SelectES( p_input , p_es );
                break;
#undef p_es
        }
    }

    p_input->stream.p_selected_program = p_new_prg;

    /* Update the navigation variables without triggering a callback */
    val.i_int = p_new_prg->i_number;
    var_Change( p_input, "program", VLC_VAR_SETVALUE, &val );

    return 0;
}

/*****************************************************************************
 * SatelliteSeek: does nothing (not a seekable stream
 *****************************************************************************/
static void SatelliteSeek( input_thread_t * p_input, off_t i_off )
{
    ;
}

