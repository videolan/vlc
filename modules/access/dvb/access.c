/*****************************************************************************
 * access.c: DVB card input v4l2 only
 *****************************************************************************
 * Copyright (C) 1998-2003 VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <saman@natlab.research.philips.com>
 *          Christopher Ross <ross@natlab.research.philips.com>
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

/* DVB Card Drivers */
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "dvb.h"

#define SATELLITE_READ_ONCE 3

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t SatelliteRead( input_thread_t * p_input, byte_t * p_buffer,
                              size_t i_len);
static int     SatelliteSetArea    ( input_thread_t *, input_area_t * );
static int     SatelliteSetProgram ( input_thread_t *, pgrm_descriptor_t * );
static void    SatelliteSeek       ( input_thread_t *, off_t );

/*****************************************************************************
 * Open: open the frontend device
 *****************************************************************************/
int E_(Open) ( vlc_object_t *p_this )
{
    struct dvb_frontend_info frontend_info;
    struct dvb_frontend_parameters fep;
    input_thread_t *    p_input = (input_thread_t *)p_this;
    input_socket_t *    p_satellite;
    char *              psz_parser;
    char *              psz_next;
    int                 i_fd = 0;
		unsigned int				u_adapter = 1;
		unsigned int				u_device = 0;
    unsigned int        u_freq = 0;
    unsigned int        u_srate = 0;
    vlc_bool_t          b_polarisation = 0;
    int                 i_fec = 0;
    fe_code_rate_t      fe_fec = FEC_NONE;
    vlc_bool_t          b_diseqc;
    vlc_bool_t					b_no_probe;
    int                 i_lnb_lof1;
    int                 i_lnb_lof2;
    int                 i_lnb_slof;
		char 								dvr[] = DVR;
		char 								frontend[] = FRONTEND;
		int									i_len = 0;

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

    // Get adapter and device number to use for this dvb card
    u_adapter = config_GetInt( p_input, "adapter" );
    u_device  = config_GetInt( p_input, "device" );

    /* Determine frontend device information and capabilities */
    b_no_probe = config_GetInt( p_input, "no-probe" );
    if (!b_no_probe)
	  {
        if ( ioctl_InfoFrontend(&frontend_info, u_adapter, u_device) < 0 )
        {
          	msg_Err( p_input, "(access) cannot determine frontend info" );
            return -1;
        }
        if (frontend_info.type != FE_QPSK)
        {
            msg_Err( p_input, "frontend not of type satellite" );
            return -1;
        }
    }
	  else /* no frontend probing is done so use default values. */
		{
			  int i_len;
			  
			  msg_Dbg( p_input, "Using default values for frontend info" );
	   	  i_len = sizeof(FRONTEND);
     		if (snprintf(frontend, sizeof(FRONTEND), FRONTEND, u_adapter, u_device) >= i_len)
     		{
     		  printf( "error: snprintf() truncated string for FRONTEND" );
     			frontend[sizeof(FRONTEND)] = '\0';
        }
			  frontend_info.name = frontend;
			  frontend_info.type = FE_QPSK;
    	  frontend_info.frequency_max = 12999;
        frontend_info.frequency_min = 10000;
				frontend_info.symbol_rate_max = 30000;
        frontend_info.symbol_rate_min = 1000;
				/* b_polarisation */
    }

    u_freq = (int)strtol( psz_parser, &psz_next, 10 );
    if( *psz_next )
    {
        psz_parser = psz_next + 1;
        b_polarisation = (vlc_bool_t)strtol( psz_parser, &psz_next, 10 );
        if( *psz_next )
        {
            psz_parser = psz_next + 1;
            i_fec = (int)strtol( psz_parser, &psz_next, 10 );
            if( *psz_next )
            {
                psz_parser = psz_next + 1;
                u_srate = (int)strtol( psz_parser, &psz_next, 10 );
            }
        }
    }

    if ( ((u_freq) > frontend_info.frequency_max) ||
         ((u_freq) < frontend_info.frequency_min) )
    {
        msg_Warn( p_input, "invalid frequency %d, using default one", u_freq );
        u_freq = config_GetInt( p_input, "frequency" );
        if ( ((u_freq) > frontend_info.frequency_max) ||
             ((u_freq) < frontend_info.frequency_min) )
        {
            msg_Err( p_input, "invalid default frequency" );
            return -1;
        }
    }

    if ( ((u_srate) > frontend_info.symbol_rate_max) ||
         ((u_srate) < frontend_info.symbol_rate_min) )
    {
        msg_Warn( p_input, "invalid symbol rate, using default one" );
        u_srate = config_GetInt( p_input, "symbol-rate" );
        if ( ((u_srate) > frontend_info.symbol_rate_max) ||
             ((u_srate) < frontend_info.symbol_rate_min) )
        {
            msg_Err( p_input, "invalid default symbol rate" );
            return -1;
        }
    }

    if( b_polarisation && b_polarisation != 1 )
    {
        msg_Warn( p_input, "invalid polarization, using default one" );
        b_polarisation = config_GetInt( p_input, "polarization" );
        if( b_polarisation && b_polarisation != 1 )
        {
            msg_Err( p_input, "invalid default polarization" );
            return -1;
        }
    }

    if( (i_fec > 7) || (i_fec < 1) )
    {
        msg_Warn( p_input, "invalid FEC, using default one" );
        i_fec = config_GetInt( p_input, "fec" );
        if( (i_fec > 7) || (i_fec < 1) )
        {
            msg_Err( p_input, "invalid default FEC" );
            return -1;
        }
    }

    switch( i_fec )
    {
        case 1:
            fe_fec = FEC_1_2;
            break;
        case 2:
            fe_fec = FEC_2_3;
            break;
        case 3:
            fe_fec = FEC_3_4;
            break;
        case 4:
            fe_fec = FEC_4_5;
            break;
        case 5:
            fe_fec = FEC_5_6;
            break;
        case 6:
            fe_fec = FEC_6_7;
            break;
        case 7:
            fe_fec = FEC_7_8;
            break;
        case 8:
            fe_fec = FEC_8_9;
            break;
        case 9:
            fe_fec = FEC_AUTO;
            break;
        default:
            /* cannot happen */
            fe_fec = FEC_NONE;
            break;
    }

    switch( frontend_info.type )
    {
       	case FE_QPSK:
            fep.frequency = u_freq * 1000;
            fep.inversion = INVERSION_AUTO;
       	    fep.u.qpsk.symbol_rate = u_srate * 1000;
       	    fep.u.qpsk.fec_inner = fe_fec;
       	    msg_Dbg( p_input, "satellite frontend found on %s", frontend_info.name );
            break;
       	case FE_QAM:
       	    msg_Dbg( p_input, "cable frontend found on %s", frontend_info.name );
            break;
        case FE_OFDM:
            msg_Dbg( p_input, "terrestrial frontend found on %s", frontend_info.name );
            break;
        default:
            msg_Err( p_input, "Could not determine frontend type on %s", frontend_info.name );
            return -1;
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
	  i_len = sizeof(DVR);
		if (snprintf(dvr, sizeof(DVR), DVR, u_adapter, u_device) >= i_len)
		{
		  msg_Err( p_input, "error: snprintf() truncated string for DVR" );
			dvr[sizeof(DVR)] = '\0';
    }
    msg_Dbg( p_input, "opening DVR device '%s'", dvr );

    if( (p_satellite->i_handle = open( dvr,
                                   /*O_NONBLOCK | O_LARGEFILE*/0 )) == (-1) )
    {
        msg_Warn( p_input, "cannot open `%s' (%s)", dvr, strerror(errno) );
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
                      "FEC: %d, Srate: %d", u_freq, b_polarisation, fe_fec, u_srate );

    msg_Dbg( p_input, "initializing frontend device" );
//    switch (ioctl_SetQPSKFrontend ( u_freq * 1000, i_srate* 1000, fe_fec,
//                i_lnb_lof1 * 1000, i_lnb_lof2 * 1000, i_lnb_slof * 1000))
    switch (ioctl_SetQPSKFrontend ( fep, b_polarisation, u_adapter, u_device ))
    {
        case -2:
            msg_Err( p_input, "frontend returned an unexpected event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
        case -3:
            msg_Err( p_input, "frontend returned no event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
        case -4:
            msg_Err( p_input, "frontend: timeout when polling for event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
        case -5:
            msg_Err( p_input, "an error occured when polling frontend device" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
        case -1:
            msg_Err( p_input, "frontend returned a failure event" );
            close( p_satellite->i_handle );
            free( p_satellite );
            return -1;
        default:
            break;
    }

    msg_Dbg( p_input, "setting filter on PAT\n" );

    if ( ioctl_SetDMXFilter( 0, &i_fd, 3, u_adapter, u_device ) < 0 )
    {
        msg_Err( p_input, "an error occured when setting filter on PAT" );
        close( p_satellite->i_handle );
        free( p_satellite );
        return -1;
    }

    msg_Dbg( p_input, "@@@ Initialising input stream\n" );

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
    p_input->stream.p_selected_area->i_tell = 0;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_input->i_mtu = SATELLITE_READ_ONCE * TS_PACKET_SIZE;
    p_input->stream.i_method = INPUT_METHOD_SATELLITE;

    msg_Dbg( p_input, "@@@ Leaving E_(Open)\n" );
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
                i_es_index < p_input->stream.p_selected_program->i_es_number;
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
		unsigned int				u_adapter = 1;
		unsigned int			  u_device = 0;
 
    unsigned int i;

    msg_Dbg( p_input, "@@@ SatelliteRead seeking for %d program\n", p_input->stream.i_pgrm_number );

    // Get adapter and device number to use for this dvb card
    u_adapter = config_GetInt( p_input, "adapter" );
    u_device  = config_GetInt( p_input, "device" );

    /* if not set, set filters to the PMTs */
    for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
    {
        msg_Dbg( p_input, "@@@ trying to set filter on pmt pid %d",
                     p_input->stream.pp_programs[i]->pp_es[0]->i_id );

        if ( p_input->stream.pp_programs[i]->pp_es[0]->i_demux_fd == 0 )
        {
            msg_Dbg( p_input, "setting filter on pmt pid %d",
                     p_input->stream.pp_programs[i]->pp_es[0]->i_id );

            ioctl_SetDMXFilter( p_input->stream.pp_programs[i]->pp_es[0]->i_id,
                       &p_input->stream.pp_programs[i]->pp_es[0]->i_demux_fd,
                       3, u_adapter, u_device );
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

    msg_Dbg( p_input, "@@@ Searched all, returning %d\n", i_ret );
 
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
		unsigned int u_adapter = 1;
		unsigned int u_device = 0;

    msg_Dbg( p_input, "@@@ SatelliteSetProgram enter\n" );

    // Get adapter and device number to use for this dvb card
    u_adapter = config_GetInt( p_input, "adapter" );
    u_device  = config_GetInt( p_input, "device" );

    if ( p_input->stream.p_selected_program )
    {
        for ( i_es_index = 1 ; /* 0 should be the PMT */
                i_es_index < p_input->stream.p_selected_program->i_es_number ;
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
                if ( input_SelectES( p_input , p_es ) == 0 )
                {
                    ioctl_SetDMXFilter( p_es->i_id, &p_es->i_demux_fd, 1, u_adapter, u_device);
                }
                break;
            case MPEG1_AUDIO_ES:
            case MPEG2_AUDIO_ES:
                if ( input_SelectES( p_input , p_es ) == 0 )
                {
                    ioctl_SetDMXFilter( p_es->i_id, &p_es->i_demux_fd, 2, u_adapter, u_device);
                    input_SelectES( p_input , p_es );
                }
                break;
            default:
                ioctl_SetDMXFilter( p_es->i_id, &p_es->i_demux_fd, 3, u_adapter, u_device);
                input_SelectES( p_input , p_es );
                break;
#undef p_es
        }
    }

    p_input->stream.p_selected_program = p_new_prg;

    msg_Dbg( p_input, "@@@ SatelliteSetProgram exit\n" );
    return 0;
}

/*****************************************************************************
 * SatelliteSeek: does nothing (not a seekable stream
 *****************************************************************************/
static void SatelliteSeek( input_thread_t * p_input, off_t i_off )
{
    ;
}

