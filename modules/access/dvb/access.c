/*****************************************************************************
 * access.c: DVB card input v4l2 only
 *****************************************************************************
 * Copyright (C) 1998-2003 VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman@wxs.nl>
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

#ifdef HAVE_ERRNO_H
#    include <string.h>
#    include <errno.h>
#endif

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
    unsigned int        u_adapter = 1;
    unsigned int        u_device = 0;
    unsigned int        u_freq = 0;
    unsigned int        u_srate = 0;
    unsigned int        u_lnb_lof1;
    unsigned int        u_lnb_lof2;
    unsigned int        u_lnb_slof;
    int                 i_bandwidth = 0;
    int                 i_modulation = 0;
    int                 i_guard = 0;
    int                 i_transmission = 0;
    int                 i_hierarchy = 0;
    int                 i_polarisation = 0;
    int                 i_fec = 0;
    int                 i_code_rate_HP = 0;
    int                 i_code_rate_LP = 0;
    vlc_bool_t          b_diseqc;
    vlc_bool_t          b_probe;
    char                dvr[] = DVR;
    char                frontend[] = FRONTEND;
    int                 i_len = 0;
    vlc_value_t         val;
    int                 i_test;

    
    /* parse the options passed in command line : */
    psz_parser = strdup( p_input->psz_name );

    if( !psz_parser )
    {
        return( -1 );
    }

    // Get adapter and device number to use for this dvb card
    u_adapter = config_GetInt( p_input, "adapter" );
    u_device  = config_GetInt( p_input, "device" );
  
    /* Get antenna configuration options */
    b_diseqc = config_GetInt( p_input, "diseqc" );
    u_lnb_lof1 = config_GetInt( p_input, "lnb-lof1" );
    u_lnb_lof2 = config_GetInt( p_input, "lnb-lof2" );
    u_lnb_slof = config_GetInt( p_input, "lnb-slof" );

    /*Get modulation parameters*/
    i_bandwidth = config_GetInt( p_input, "bandwidth");
    i_code_rate_HP = config_GetInt(p_input, "code-rate-hp");
    i_code_rate_LP = config_GetInt(p_input, "code-rate-lp");
    i_modulation  = config_GetInt(p_input, "modulation");
    i_transmission = config_GetInt(p_input, "transmission");
    i_guard = config_GetInt(p_input, "guard");
    i_hierarchy = config_GetInt(p_input, "hierarchy");

    /* Determine frontend device information and capabilities */
    b_probe = config_GetInt( p_input, "probe" );
    if (b_probe)
    {
        if ( ioctl_InfoFrontend(p_input, &frontend_info, u_adapter, u_device) < 0 )
        {
            msg_Err( p_input, "(access) cannot determine frontend info" );
            return -1;
        }
    }
    else /* no frontend probing is done so use default border values. */
    {
        msg_Dbg( p_input, "using default values for frontend info" );
        i_len = sizeof(FRONTEND);
        if (snprintf(frontend, sizeof(FRONTEND), FRONTEND, u_adapter, u_device) >= i_len)
        {
            msg_Err( p_input, "snprintf() truncated string for FRONTEND" );
            frontend[sizeof(FRONTEND)] = '\0';
        }
        strncpy(frontend_info.name, frontend, 128);

        msg_Dbg(p_input, "method of access is %s", p_input->psz_access);
        
        frontend_info.type = FE_QPSK;
        if (strncmp( p_input->psz_access, "qpsk",4 ) ==0)
            frontend_info.type = FE_QPSK;
        else if (strncmp( p_input->psz_access, "cable",5 ) ==0)
            frontend_info.type = FE_QAM;
        else if (strncmp( p_input->psz_access, "terrestrial",11) ==0)
            frontend_info.type = FE_OFDM;

        frontend_info.frequency_max = u_lnb_lof2; /* in KHz, lnb_lof2 */
        frontend_info.frequency_min = u_lnb_lof1; /* lnb_lof1 */

        frontend_info.symbol_rate_max = 30000000;
        frontend_info.symbol_rate_min =  1000000;
    }
    
    /* Register Callback functions */
    p_input->pf_read = SatelliteRead;
    p_input->pf_set_program = SatelliteSetProgram;
    p_input->pf_set_area = SatelliteSetArea;
    p_input->pf_seek = SatelliteSeek;

    i_test = strtol( psz_parser, &psz_next, 10 );
    if ( psz_next == psz_parser )
    {
        for ( ;; )
        {
            if( !strncmp( psz_parser, "frequency=",
                               strlen( "frequency=" ) ) )
            {
                u_freq =
                  strtol( psz_parser + strlen( "frequency=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "polarization=",
                               strlen( "polarization=" ) ) )
            {
                char *psz_parser_init;
                psz_parser += strlen( "polarization=" );
                psz_parser_init = psz_parser;
                while ( *psz_parser != ':')
                {
                psz_parser++;
                }
                if ((!strncmp( psz_parser_init, "V" ,
                     psz_parser - psz_parser_init ) ) || 
                   (!strncmp( psz_parser_init, "V" ,
                     psz_parser - psz_parser_init ) ) )
                {
                    i_polarisation = 0; //VLC_FALSE;
                }
                else if ((!strncmp( psz_parser_init, "H" ,
                     psz_parser - psz_parser_init ) ) ||
                   (!strncmp( psz_parser_init, "h" ,
                     psz_parser - psz_parser_init ) ) )

                {
                    i_polarisation = 1; //VLC_TRUE;
                }
                else if ((!strncmp( psz_parser_init, "A" ,
                     psz_parser - psz_parser_init ) ) ||
                   (!strncmp( psz_parser_init, "a" ,
                     psz_parser - psz_parser_init ) ) )

                {
                    i_polarisation = 2;
                }
            }
            else if( !strncmp( psz_parser, "fec=",
                               strlen( "fec=" ) ) )
            {
                i_fec =
                (int)strtol( psz_parser + strlen( "fec=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "srate=",
                               strlen( "srate=" ) ) )
            {
                u_srate =
                (unsigned int)strtol( psz_parser + strlen( "srate=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "program=",
                               strlen( "program=" ) ) )
            {
                val.i_int = (int)strtol( psz_parser + strlen( "program=" ),
                            &psz_parser, 0 );
                var_Set( p_input, "program", val );
            }
            else if( !strncmp( psz_parser, "diseqc",
                               strlen( "disecq" ) ) )
            {
                psz_parser += strlen("disecq");
                b_diseqc = VLC_TRUE;
            }
            else if( !strncmp( psz_parser, "lnb-lof1=",
                               strlen( "lnb-lof1=" ) ) )
            {
                u_lnb_lof1 =
                (unsigned int)strtol( psz_parser + strlen( "lnb-lof1=" ),
                            &psz_parser, 0 );                
                frontend_info.frequency_min = u_lnb_lof1; /* lnb_lof1 */
            }
            else if( !strncmp( psz_parser, "lnb-lof2=",
                               strlen( "lnb-lof2=" ) ) )
            {
                u_lnb_lof2 =
                (unsigned int)strtol( psz_parser + strlen( "lnb-lof2=" ),
                            &psz_parser, 0 );
                frontend_info.frequency_max = u_lnb_lof2; /* in KHz, lnb_lof2 */
            }
            else if( !strncmp( psz_parser, "lnb-slof=",
                               strlen( "lnb-slof=" ) ) )
            {
                u_lnb_slof =
                (unsigned int)strtol( psz_parser + strlen( "lnb-slof=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "device=",
                               strlen( "device=" ) ) )
            {
                u_device =
                (unsigned int)strtol( psz_parser + strlen( "device=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "adapter=",
                               strlen( "adapter=" ) ) )
            {
                u_adapter =
                (unsigned int)strtol( psz_parser + strlen( "adapter=" ),
                            &psz_parser, 0 );
            }
            else if (!strncmp( psz_parser, "modulation=",
                               strlen( "modulation=" ) ) )
            {
                i_modulation = (int)strtol( psz_parser + strlen( "modulation=" ),
                            &psz_parser, 0 );
            }
            else if (!strncmp( psz_parser, "bandwidth=",
                               strlen( "bandwidth=" ) ) )
            {
                i_bandwidth = (int)strtol( psz_parser + strlen( "bandwidth=" ),
                            &psz_parser, 0 );
            }
            else if (!strncmp( psz_parser, "guard=",
                               strlen( "guard=" ) ) )
            {
                i_guard = (int)strtol( psz_parser + strlen( "guard=" ),
                            &psz_parser, 0 );
            }
            else if (!strncmp( psz_parser, "transmission=",
                               strlen( "transmission=" ) ) )
            {
                i_transmission = (int)strtol( psz_parser + 
                            strlen( "transmission=" ),&psz_parser, 0 );
            }
            else if (!strncmp( psz_parser, "hierarchy=",
                               strlen( "hierarchy=" ) ) )
            {
                i_hierarchy = (int)strtol( psz_parser + 
                                strlen( "hierarchy=" ),&psz_parser, 0 );
            }
            else if (!strncmp( psz_parser, "code-rate-HP=",
                               strlen( "code-rate-HP=" ) ) )
            {
                i_code_rate_HP = (int)strtol( psz_parser + 
                                strlen( "code-rate-HP=" ),&psz_parser, 0 );
            }
            else if (!strncmp( psz_parser, "code-rate-LP=",
                               strlen( "code-rate-LP=" ) ) )
            {
                i_code_rate_LP = (int)strtol( psz_parser +
                                strlen( "code-rate-LP=" ),&psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "probe",
                               strlen( "probe" ) ) )
            {
                psz_parser += strlen("probe");
                b_probe = VLC_TRUE;
            }
            if( *psz_parser )
                psz_parser++;
            else
            break;
        }
    }
    else
    {
        msg_Warn(p_input,"DVB Input syntax has changed, please see documentation for further informations");
        u_freq = (unsigned int)i_test;
        if( *psz_next )
        {
            psz_parser = psz_next + 1;
            i_polarisation = strtol( psz_parser, &psz_next, 10 );
            if( *psz_next )
            {
                psz_parser = psz_next + 1;
                i_fec = (int)strtol( psz_parser, &psz_next, 10 );
                if( *psz_next )
                {
                    psz_parser = psz_next + 1;
                    u_srate = (unsigned int)strtol( psz_parser, &psz_next, 10 );
                }
            }
        }
    }
   
    /* Validating input values */
    if ( ((u_freq) > frontend_info.frequency_max) ||
         ((u_freq) < frontend_info.frequency_min) )
    {
        msg_Warn( p_input, "invalid frequency %d (KHz), using default one", u_freq );
        u_freq = config_GetInt( p_input, "frequency" );
        if ( ((u_freq) > frontend_info.frequency_max) ||
             ((u_freq) < frontend_info.frequency_min) )
        {
            msg_Err( p_input, "invalid default frequency" );
            return -1;
        }
    }

    /* Workaround for backwards compatibility */
    if (strncmp( p_input->psz_access, "satellite", 9 ) ==0)
    {
        msg_Warn( p_input, "invalid symbol rate %d possibly specified in MHz, trying value *1000 KHz", u_freq );
        u_srate *= 1000;
    }
    
    if ( ((u_srate) > frontend_info.symbol_rate_max) ||
         ((u_srate) < frontend_info.symbol_rate_min) )
    {
        msg_Warn( p_input, "invalid symbol rate, using default one" );
        u_srate = config_GetInt( p_input, "symbol-rate"s );
        if ( ((u_srate) > frontend_info.symbol_rate_max) ||
             ((u_srate) < frontend_info.symbol_rate_min) )
        {
            msg_Err( p_input, "invalid default symbol rate" );
            return -1;
        }
    }

    if( (i_fec > 9) || (i_fec < 1) )
    {
        msg_Warn( p_input, "invalid FEC, using default one" );
        i_fec = config_GetInt( p_input, "fec" );
        if( (i_fec > 9) || (i_fec < 1) )
        {
            msg_Err( p_input, "invalid default FEC" );
            return -1;
        }
    }

    /* Setting frontend parameters for tuning the hardware */      
    msg_Dbg( p_input, "Trying to tune to channel ...");
    switch( frontend_info.type )
    {
        /* DVB-S: satellite and budget cards (nova) */
        case FE_QPSK:
            fep.frequency = u_freq; /* KHz */
            fep.inversion = dvb_DecodeInversion(p_input, i_polarisation);
            fep.u.qpsk.symbol_rate = u_srate;
            fep.u.qpsk.fec_inner = dvb_DecodeFEC(p_input, i_fec); 
            msg_Dbg( p_input, "DVB-S: satellite (QPSK) frontend %s found", frontend_info.name );

            if (ioctl_SetQPSKFrontend (p_input, fep, i_polarisation,
                               u_lnb_lof1, u_lnb_lof2, u_lnb_slof,
                               u_adapter, u_device )<0)
            {
                msg_Err( p_input, "DVB-S: tuning failed" );
                return -1;
            }
            break;
            
        /* DVB-C */
        case FE_QAM:
            fep.frequency = u_freq; /* KHz */
            fep.inversion = dvb_DecodeInversion(p_input, i_polarisation);
            fep.u.qam.symbol_rate = u_srate;
            fep.u.qam.fec_inner = dvb_DecodeFEC(p_input, i_fec); 
            fep.u.qam.modulation = dvb_DecodeModulation(p_input, i_modulation); 
            msg_Dbg( p_input, "DVB-C: cable (QAM) frontend %s found", frontend_info.name );
            if (ioctl_SetQAMFrontend (p_input, fep, u_adapter, u_device )<0)
            {
                msg_Err( p_input, "DVB-C: tuning failed" );
                return -1;
            }
            break;

        /* DVB-T */
        case FE_OFDM:
            fep.frequency = u_freq; /* KHz */
            fep.inversion = dvb_DecodeInversion(p_input, i_polarisation);
            fep.u.ofdm.bandwidth = dvb_DecodeBandwidth(p_input, i_bandwidth);
            fep.u.ofdm.code_rate_HP = dvb_DecodeFEC(p_input, i_code_rate_HP); 
            fep.u.ofdm.code_rate_LP = dvb_DecodeFEC(p_input, i_code_rate_LP);
            fep.u.ofdm.constellation = dvb_DecodeModulation(p_input, i_modulation); 
            fep.u.ofdm.transmission_mode = dvb_DecodeTransmission(p_input, i_transmission);
            fep.u.ofdm.guard_interval = dvb_DecodeGuardInterval(p_input, i_guard);
            fep.u.ofdm.hierarchy_information = dvb_DecodeHierarchy(p_input, i_hierarchy);
            msg_Dbg( p_input, "DVB-T: terrestrial (OFDM) frontend %s found", frontend_info.name );
            if (ioctl_SetOFDMFrontend (p_input, fep,u_adapter, u_device )<0)
            {
                msg_Err( p_input, "DVB-T: tuning failed" );
                return -1;
            }
            break;
        default:
            msg_Err( p_input, "Could not determine frontend type on %s", frontend_info.name );
            return -1;
    }
    msg_Dbg( p_input, "Tuning done.");

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
        msg_Err( p_input, "snprintf() truncated string for DVR" );
        dvr[sizeof(DVR)] = '\0';
    }
    msg_Dbg( p_input, "opening DVR device '%s'", dvr );

    if( (p_satellite->i_handle = open( dvr,
                                   /*O_NONBLOCK | O_LARGEFILE*/0 )) == (-1) )
    {
#   ifdef HAVE_ERRNO_H
        msg_Warn( p_input, "cannot open `%s' (%s)", dvr, strerror(errno) );
#   else
        msg_Warn( p_input, "cannot open `%s'", dvr );
#   endif
        free( p_satellite );
        return -1;
    }

    msg_Dbg( p_input, "setting filter on PAT" );

    /* Set Filter on PAT packet */
    if ( ioctl_SetDMXFilter(p_input, 0, &i_fd, 21, u_adapter, u_device ) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "an error occured when setting filter on PAT (%s)", strerror(errno) );
#   else
        msg_Err( p_input, "an error occured when setting filter on PAT" );
#   endif        
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
                i_es_index < p_input->stream.p_selected_program->i_es_number;
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if ( p_es->p_dec )
            {
                ioctl_UnsetDMXFilter(p_input, p_es->i_demux_fd );
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
    unsigned int u_adapter = 1;
    unsigned int u_device = 0;
    unsigned int i;

    // Get adapter and device number to use for this dvb card
    u_adapter = config_GetInt( p_input, "adapter" );
    u_device  = config_GetInt( p_input, "device" );

    /* if not set, set filters to the PMTs */
    for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
    {
        if ( p_input->stream.pp_programs[i]->pp_es[0]->i_demux_fd == 0 )
        {
            ioctl_SetDMXFilter(p_input, p_input->stream.pp_programs[i]->pp_es[0]->i_id,
                       &p_input->stream.pp_programs[i]->pp_es[0]->i_demux_fd,
                       21, u_adapter, u_device );
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
    unsigned int u_adapter = 1;
    unsigned int u_device = 0;
    unsigned int u_video_type = 1; /* default video type */
    unsigned int u_audio_type = 2; /* default audio type */

    /* Get adapter and device number to use for this dvb card */
    u_adapter = config_GetInt( p_input, "adapter" );
    u_device  = config_GetInt( p_input, "device" );

    if ( p_input->stream.p_selected_program )
    {
        for ( i_es_index = 1 ; /* 0 should be the PMT */
                i_es_index < p_input->stream.p_selected_program->i_es_number ;
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if ( p_es->p_dec )
            {
                input_UnselectES( p_input , p_es );
            }
            if ( p_es->i_demux_fd > 0)
            {
                ioctl_UnsetDMXFilter(p_input, p_es->i_demux_fd );
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
                    ioctl_SetDMXFilter(p_input, p_es->i_id, &p_es->i_demux_fd, u_video_type,
                                       u_adapter, u_device);
                    u_video_type += 5;
                }
                break;
            case MPEG1_AUDIO_ES:
            case MPEG2_AUDIO_ES:
                if ( input_SelectES( p_input , p_es ) == 0 )
                {
                    ioctl_SetDMXFilter(p_input, p_es->i_id, &p_es->i_demux_fd, u_audio_type,
                                       u_adapter, u_device);
                    input_SelectES( p_input , p_es );
                    u_audio_type += 5;
                }
                break;
            default:
                ioctl_SetDMXFilter(p_input, p_es->i_id, &p_es->i_demux_fd, 21, u_adapter, u_device);
                input_SelectES( p_input , p_es );
                msg_Warn(p_input, "ES streamtype 0x%d found used as DMX_PES_OTHER !!",(int) p_es->i_cat);
                break;
#undef p_es
        }
    }

    p_input->stream.p_selected_program = p_new_prg;

    /* Update the navigation variables without triggering a callback */
    val.i_int = p_new_prg->i_number;
    var_Change( p_input, "program", VLC_VAR_SETVALUE, &val, NULL );

    return 0;
}

/*****************************************************************************
 * SatelliteSeek: does nothing (not a seekable stream
 *****************************************************************************/
static void SatelliteSeek( input_thread_t * p_input, off_t i_off )
{
    ;
}

