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
    input_dvb_t *       p_dvb;
    char *              psz_parser;
    char *              psz_next;
    int                 i_fd = 0;
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

    msg_Dbg(p_input, "method of access is %s", p_input->psz_access);

    /* Initialise structure */
    p_dvb = malloc( sizeof( input_dvb_t ) );

    if( p_dvb == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }

    p_input->p_access_data = (void *) p_dvb;

    /* Get adapter and device number to use for this dvb card */
    p_dvb->u_adapter = config_GetInt( p_input, "adapter" );
    p_dvb->u_device  = config_GetInt( p_input, "device" );
    p_dvb->b_probe   = config_GetInt( p_input, "probe" );
  
    /* Get antenna configuration options */
    p_dvb->b_diseqc   = config_GetInt( p_input, "diseqc" );
    p_dvb->u_lnb_lof1 = config_GetInt( p_input, "lnb-lof1" );
    p_dvb->u_lnb_lof2 = config_GetInt( p_input, "lnb-lof2" );
    p_dvb->u_lnb_slof = config_GetInt( p_input, "lnb-slof" );

    /* Get modulation parameters */
    p_dvb->i_bandwidth    = config_GetInt( p_input, "bandwidth");
    p_dvb->i_code_rate_HP = config_GetInt(p_input, "code-rate-hp");
    p_dvb->i_code_rate_LP = config_GetInt(p_input, "code-rate-lp");
    p_dvb->i_modulation   = config_GetInt(p_input, "modulation");
    p_dvb->i_transmission = config_GetInt(p_input, "transmission");
    p_dvb->i_guard        = config_GetInt(p_input, "guard");
    p_dvb->i_hierarchy    = config_GetInt(p_input, "hierarchy");

    /* Register Callback functions */
    p_input->pf_read = SatelliteRead;
    p_input->pf_set_program = SatelliteSetProgram;
    p_input->pf_set_area = SatelliteSetArea;
    p_input->pf_seek = SatelliteSeek;

    /* Parse commandline */
    i_test = strtol( psz_parser, &psz_next, 10 );
    if( psz_next == psz_parser )
    {
        for ( ;; )
        {
            if( !strncmp( psz_parser, "frequency=",
                               strlen( "frequency=" ) ) )
            {
                p_dvb->u_freq =
                (unsigned int)strtol( psz_parser + strlen( "frequency=" ),
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
                if( (!strncmp( psz_parser_init, "V" ,
                     psz_parser - psz_parser_init ) ) || 
                   (!strncmp( psz_parser_init, "V" ,
                     psz_parser - psz_parser_init ) ) )
                {
                    p_dvb->i_polarisation = VLC_FALSE;
                }
                else if( (!strncmp( psz_parser_init, "H" ,
                     psz_parser - psz_parser_init ) ) ||
                   (!strncmp( psz_parser_init, "h" ,
                     psz_parser - psz_parser_init ) ) )

                {
                    p_dvb->i_polarisation = VLC_TRUE;
                }
                else if( (!strncmp( psz_parser_init, "A" ,
                     psz_parser - psz_parser_init ) ) ||
                   (!strncmp( psz_parser_init, "a" ,
                     psz_parser - psz_parser_init ) ) )

                {
                    p_dvb->i_polarisation = 2;
                }
            }
            else if( !strncmp( psz_parser, "fec=",
                               strlen( "fec=" ) ) )
            {
                p_dvb->i_fec =
                (int)strtol( psz_parser + strlen( "fec=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "srate=",
                               strlen( "srate=" ) ) )
            {
                p_dvb->u_srate =
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
                p_dvb->b_diseqc = VLC_TRUE;
            }
            else if( !strncmp( psz_parser, "lnb-lof1=",
                               strlen( "lnb-lof1=" ) ) )
            {
                p_dvb->u_lnb_lof1 =
                (unsigned int)strtol( psz_parser + strlen( "lnb-lof1=" ),
                            &psz_parser, 0 );                
                frontend_info.frequency_min = p_dvb->u_lnb_lof1; /* lnb_lof1 */
            }
            else if( !strncmp( psz_parser, "lnb-lof2=",
                               strlen( "lnb-lof2=" ) ) )
            {
                p_dvb->u_lnb_lof2 =
                (unsigned int)strtol( psz_parser + strlen( "lnb-lof2=" ),
                            &psz_parser, 0 );
                frontend_info.frequency_max = p_dvb->u_lnb_lof2; /* in KHz, lnb_lof2 */
            }
            else if( !strncmp( psz_parser, "lnb-slof=",
                               strlen( "lnb-slof=" ) ) )
            {
                p_dvb->u_lnb_slof =
                (unsigned int)strtol( psz_parser + strlen( "lnb-slof=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "device=",
                               strlen( "device=" ) ) )
            {
                p_dvb->u_device =
                (unsigned int)strtol( psz_parser + strlen( "device=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "adapter=",
                               strlen( "adapter=" ) ) )
            {
                p_dvb->u_adapter =
                (unsigned int)strtol( psz_parser + strlen( "adapter=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "modulation=",
                               strlen( "modulation=" ) ) )
            {
                p_dvb->i_modulation = (int)strtol( psz_parser + strlen( "modulation=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "bandwidth=",
                               strlen( "bandwidth=" ) ) )
            {
                p_dvb->i_bandwidth = (int)strtol( psz_parser + strlen( "bandwidth=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "guard=",
                               strlen( "guard=" ) ) )
            {
                p_dvb->i_guard = (int)strtol( psz_parser + strlen( "guard=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "transmission=",
                               strlen( "transmission=" ) ) )
            {
                p_dvb->i_transmission = (int)strtol( psz_parser + 
                            strlen( "transmission=" ),&psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "hierarchy=",
                               strlen( "hierarchy=" ) ) )
            {
                p_dvb->i_hierarchy = (int)strtol( psz_parser + 
                                strlen( "hierarchy=" ),&psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "code-rate-HP=",
                               strlen( "code-rate-HP=" ) ) )
            {
                p_dvb->i_code_rate_HP = (int)strtol( psz_parser + 
                                strlen( "code-rate-HP=" ),&psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "code-rate-LP=",
                               strlen( "code-rate-LP=" ) ) )
            {
                p_dvb->i_code_rate_LP = (int)strtol( psz_parser +
                                strlen( "code-rate-LP=" ),&psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "probe",
                               strlen( "probe" ) ) )
            {
                psz_parser += strlen("probe");
                p_dvb->b_probe = VLC_TRUE;
            }
            if( *psz_parser )
                psz_parser++;
            else
                break;
        }
    }
    else
    {
        msg_Err(p_input, "DVB Input old syntax deprecreated");
        free( p_dvb );
        return -1;
    }

    /* Determine frontend device */
    i_len = sizeof(FRONTEND);
    if( snprintf( frontend, sizeof(FRONTEND), FRONTEND, p_dvb->u_adapter, p_dvb->u_device ) >= i_len )
    {
        msg_Err( p_input, "snprintf() truncated string for FRONTEND" );
        frontend[sizeof(FRONTEND)] = '\0';
    }

    msg_Dbg( p_input, "Opening device %s", frontend );
    if( ( p_dvb->i_frontend = open( frontend, O_RDWR )) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "Opening device failed (%s)", strerror( errno ));
#   else
        msg_Err( p_input, "Opening device failed");
#   endif
        close( p_dvb->i_frontend );
        free( p_dvb );
        return -1;
    }
    
    /* Determine frontend device information and capabilities */        
    if( p_dvb->b_probe )
    {
        if( ioctl_InfoFrontend(p_input, &frontend_info, p_dvb->u_adapter, p_dvb->u_device) < 0 )
        {
            msg_Err( p_input, "(access) cannot determine frontend info" );
            close( p_dvb->i_frontend );
            free( p_dvb );
            return -1;
        }
    }
    else /* no frontend probing is done so use default border values. */
    {
        msg_Dbg( p_input, "using default values for frontend info" );
        strncpy(frontend_info.name, frontend, 128);

        frontend_info.type = FE_QPSK;
        if( (strncmp( p_input->psz_access, "qpsk", 4) ==0) ||
             (strncmp( p_input->psz_access, "dvb-s", 5 ) == 0) ||
             (strncmp( p_input->psz_access, "satellite", 9 ) == 0) )
            frontend_info.type = FE_QPSK;
        else if( (strncmp( p_input->psz_access, "cable", 5) ==0) ||
                  (strncmp( p_input->psz_access, "dvb-c", 5 ) == 0) )
            frontend_info.type = FE_QAM;
        else if( (strncmp( p_input->psz_access, "terrestrial", 11) ==0) ||
                  (strncmp( p_input->psz_access, "dvb-t", 5 ) == 0) )
            frontend_info.type = FE_OFDM;

        frontend_info.frequency_max = p_dvb->u_lnb_lof2; /* in KHz, lnb_lof2 */
        frontend_info.frequency_min = p_dvb->u_lnb_lof1; /* lnb_lof1 */

        frontend_info.symbol_rate_max = 30000000;
        frontend_info.symbol_rate_min =  1000000;
    }

    /* Sanity check */
    if( ((strncmp( p_input->psz_access, "qpsk", 4 ) == 0) ||
         (strncmp( p_input->psz_access, "dvb-s", 5 ) == 0) ||
         (strncmp( p_input->psz_access, "satellite", 9 ) == 0) ) &&
         (frontend_info.type != FE_QPSK) )
    {
        if( frontend_info.type == FE_OFDM )
            msg_Err(p_input, "User expects DVB-S card but DVB-T card found.");
        else if( frontend_info.type == FE_QAM )
            msg_Err(p_input, "User expects DVB-S card but DVB-C card found.");
        else msg_Err(p_input, "User expects DVB-S card but unknown card found.");

        close( p_dvb->i_frontend );
        free( p_dvb );        
        return -1;
    }
    if( ((strncmp( p_input->psz_access, "cable", 5 ) == 0) ||
         (strncmp( p_input->psz_access, "dvb-c", 5 ) == 0) ) &&
         (frontend_info.type != FE_QAM) )
    {
        if( frontend_info.type == FE_OFDM )
            msg_Err( p_input, "User expects DVB-C card but DVB-T card found." );
        else if( frontend_info.type == FE_QPSK )
            msg_Err( p_input, "User expects DVB-C card but DVB-S card found." );
        else msg_Err( p_input, "User expects DVB-C card but unknown card found." );

        close( p_dvb->i_frontend );
        free( p_dvb );
        return -1;
    }
    if( ((strncmp( p_input->psz_access, "terrestrial", 11 ) == 0) ||
         (strncmp( p_input->psz_access, "dvb-t", 5 ) == 0) ) &&
         (frontend_info.type != FE_OFDM) )
    {
        if( frontend_info.type == FE_QAM )
            msg_Err(p_input, "User expects DVB-T card but DVB-C card found.");
        else if( frontend_info.type == FE_QPSK )
            msg_Err(p_input, "User expects DVB-T card but DVB-S card found.");
        else msg_Err(p_input, "User expects DVB-T card but unknown card found.");

        close( p_dvb->i_frontend );
        free( p_dvb );
        return -1;
    }
   
#if 0
    /* Validating input values (QPSK in KHz, OFDM/QAM in Hz) */
    if( ((p_dvb->u_freq) > frontend_info.frequency_max) ||
        ((p_dvb->u_freq) < frontend_info.frequency_min) )
    {
        if( (p_dvb->u_freq) > frontend_info.frequency_max )
            msg_Err( p_input, "given frequency %u (kHz) > %u (kHz) max. frequency",
                     p_dvb->u_freq, frontend_info.frequency_max );
        else
            msg_Err( p_input, "given frequency %u (kHz) < %u (kHz) min.frequency",
                     p_dvb->u_freq, frontend_info.frequency_min );
        msg_Err( p_input, "bailing out given frequency outside specification range for this frontend" );

        close( p_dvb->i_frontend );
        free( p_dvb );
        return -1;
    }
#endif

    /* Workaround for backwards compatibility */
    if( strncmp( p_input->psz_access, "satellite", 9 ) ==0 )
    {
        msg_Warn( p_input, "invalid symbol rate %d possibly specified in MHz, trying value *1000 KHz", p_dvb->u_srate );
        p_dvb->u_srate *= 1000UL;
    }
    
    if( ((p_dvb->u_srate) > frontend_info.symbol_rate_max) ||
        ((p_dvb->u_srate) < frontend_info.symbol_rate_min) )
    {
        msg_Warn( p_input, "invalid symbol rate, using default one" );
        p_dvb->u_srate = config_GetInt( p_input, "symbol-rate" );
        if( ((p_dvb->u_srate) > frontend_info.symbol_rate_max) ||
            ((p_dvb->u_srate) < frontend_info.symbol_rate_min) )
        {
            msg_Err( p_input, "invalid default symbol rate" );

            close( p_dvb->i_frontend );
            free( p_dvb );
            return -1;
        }
    }

    if( (p_dvb->i_fec > 9) || (p_dvb->i_fec < 1) )
    {
        msg_Warn( p_input, "invalid FEC, using default one" );
        p_dvb->i_fec = config_GetInt( p_input, "fec" );
        if( (p_dvb->i_fec > 9) || (p_dvb->i_fec < 1) )
        {
            msg_Err( p_input, "invalid default FEC" );
            close( p_dvb->i_frontend );
            free( p_dvb );
            return -1;
        }
    }

    /* Setting frontend parameters for tuning the hardware */      
    msg_Dbg( p_input, "Trying to tune to channel ...");
    switch( frontend_info.type )
    {
        /* DVB-S: satellite and budget cards (nova) */
        case FE_QPSK:
            fep.frequency = p_dvb->u_freq; /* KHz */
            fep.inversion = dvb_DecodeInversion( p_input, p_dvb->i_polarisation );
            fep.u.qpsk.symbol_rate = p_dvb->u_srate;
            fep.u.qpsk.fec_inner = dvb_DecodeFEC( p_input, p_dvb->i_fec ); 
            msg_Dbg( p_input, "DVB-S: satellite (QPSK) frontend %s found", frontend_info.name );

            if( ioctl_SetQPSKFrontend( p_input, fep, p_dvb->i_polarisation,
                               p_dvb->u_lnb_lof1, p_dvb->u_lnb_lof2, p_dvb->u_lnb_slof,
                               p_dvb->u_adapter, p_dvb->u_device ) < 0 )
            {
                msg_Err( p_input, "DVB-S: tuning failed" );
                close( p_dvb->i_frontend );
                free( p_dvb );
                return -1;
            }
            break;
            
        /* DVB-C */
        case FE_QAM:
            fep.frequency = p_dvb->u_freq; /* in Hz */
            fep.inversion = dvb_DecodeInversion( p_input, p_dvb->i_polarisation );
            fep.u.qam.symbol_rate = p_dvb->u_srate;
            fep.u.qam.fec_inner = dvb_DecodeFEC( p_input, p_dvb->i_fec ); 
            fep.u.qam.modulation = dvb_DecodeModulation( p_input, p_dvb->i_modulation ); 
            msg_Dbg( p_input, "DVB-C: cable (QAM) frontend %s found", frontend_info.name );
            if( ioctl_SetQAMFrontend( p_input, fep, p_dvb->u_adapter, p_dvb->u_device ) < 0 )
            {
                msg_Err( p_input, "DVB-C: tuning failed" );
                close( p_dvb->i_frontend );
                free( p_dvb );
                return -1;
            }
            break;

        /* DVB-T */
        case FE_OFDM:
            fep.frequency = p_dvb->u_freq; /* in Hz */
            fep.inversion = dvb_DecodeInversion( p_input, p_dvb->i_polarisation );
            fep.u.ofdm.bandwidth = dvb_DecodeBandwidth( p_input, p_dvb->i_bandwidth );
            fep.u.ofdm.code_rate_HP = dvb_DecodeFEC( p_input, p_dvb->i_code_rate_HP ); 
            fep.u.ofdm.code_rate_LP = dvb_DecodeFEC( p_input, p_dvb->i_code_rate_LP );
            fep.u.ofdm.constellation = dvb_DecodeModulation( p_input, p_dvb->i_modulation ); 
            fep.u.ofdm.transmission_mode = dvb_DecodeTransmission( p_input, p_dvb->i_transmission );
            fep.u.ofdm.guard_interval = dvb_DecodeGuardInterval( p_input, p_dvb->i_guard );
            fep.u.ofdm.hierarchy_information = dvb_DecodeHierarchy( p_input, p_dvb->i_hierarchy );
            msg_Dbg( p_input, "DVB-T: terrestrial (OFDM) frontend %s found", frontend_info.name );
            if( ioctl_SetOFDMFrontend( p_input, fep, p_dvb->u_adapter, p_dvb->u_device ) < 0 )
            {
                msg_Err( p_input, "DVB-T: tuning failed" );
                close( p_dvb->i_frontend );
                free( p_dvb );
                return -1;
            }
            break;
        default:
            msg_Err( p_input, "Could not determine frontend type on %s", frontend_info.name );
            close( p_dvb->i_frontend );
            free( p_dvb );
            return -1;
    }
    msg_Dbg( p_input, "Tuning done.");

    /* Initialise structure */
    p_dvb->p_satellite = malloc( sizeof( input_socket_t ) );

    if( p_dvb->p_satellite == NULL )
    {
        msg_Err( p_input, "out of memory" );
        close( p_dvb->i_frontend );
        free( p_dvb );
        return -1;
    }

    /* Open the DVR device */
    i_len = sizeof(DVR);
    if( snprintf( dvr, sizeof(DVR), DVR, p_dvb->u_adapter, p_dvb->u_device ) >= i_len )
    {
        msg_Err( p_input, "snprintf() truncated string for DVR" );
        dvr[sizeof(DVR)] = '\0';
    }
    msg_Dbg( p_input, "opening DVR device '%s'", dvr );

    if( (p_dvb->p_satellite->i_handle = open( dvr,
                                   /*O_NONBLOCK | O_LARGEFILE*/0 ) ) == (-1) )
    {
#   ifdef HAVE_ERRNO_H
        msg_Warn( p_input, "cannot open `%s' (%s)", dvr, strerror( errno ) );
#   else
        msg_Warn( p_input, "cannot open `%s'", dvr );
#   endif
        free( p_dvb->p_satellite );

        close( p_dvb->i_frontend );
        free( p_dvb );
        return -1;
    }

    msg_Dbg( p_input, "setting filter on PAT" );

    /* Set Filter on PAT packet */
    if( ioctl_SetDMXFilter(p_input, 0, &i_fd, 21, p_dvb->u_adapter, p_dvb->u_device ) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "an error occured when setting filter on PAT (%s)", strerror( errno ) );
#   else
        msg_Err( p_input, "an error occured when setting filter on PAT" );
#   endif        
        close( p_dvb->p_satellite->i_handle );
        free( p_dvb->p_satellite );

        close( p_dvb->i_frontend );
        free( p_dvb );
        return -1;
    }

    if( input_InitStream( p_input, sizeof( stream_ts_data_t ) ) == -1 )
    {
        msg_Err( p_input, "could not initialize stream structure" );
        close( p_dvb->p_satellite->i_handle );
        free( p_dvb->p_satellite );

        close( p_dvb->i_frontend );
        free( p_dvb );
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
    input_dvb_t *       p_dvb;
    unsigned int        i_es_index;
    
    if( p_input->stream.p_selected_program )
    {
        for( i_es_index = 1 ;
             i_es_index < p_input->stream.p_selected_program->i_es_number;
             i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if( p_es->p_dec )
            {
                ioctl_UnsetDMXFilter(p_input, p_es->i_demux_fd );
            }
#undef p_es
        }
    }

    p_dvb = (input_dvb_t *)p_input->p_access_data;

    close( p_dvb->p_satellite->i_handle );
    free( p_dvb->p_satellite );
    
    close( p_dvb->i_frontend );
    free( p_dvb );
}

/*****************************************************************************
 * SatelliteRead: reads data from the satellite card
 *****************************************************************************/
static ssize_t SatelliteRead( input_thread_t * p_input, byte_t * p_buffer,
                              size_t i_len )
{
    input_dvb_t * p_dvb = (input_dvb_t *)p_input->p_access_data;
    ssize_t i_ret;
    unsigned int i;

    /* if not set, set filters to the PMTs */
    for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
    {
        if( p_input->stream.pp_programs[i]->pp_es[0]->i_demux_fd == 0 )
        {
            ioctl_SetDMXFilter( p_input, p_input->stream.pp_programs[i]->pp_es[0]->i_id,
                       &p_input->stream.pp_programs[i]->pp_es[0]->i_demux_fd,
                       21, p_dvb->u_adapter, p_dvb->u_device );
        }
    }

    i_ret = read( p_dvb->p_satellite->i_handle, p_buffer, i_len );
 
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
    input_dvb_t * p_dvb = (input_dvb_t *)p_input->p_access_data;
    unsigned int i_es_index;
    vlc_value_t val;
    unsigned int u_video_type = 1; /* default video type */
    unsigned int u_audio_type = 2; /* default audio type */

    if( p_input->stream.p_selected_program )
    {
        for( i_es_index = 1 ; /* 0 should be the PMT */
                i_es_index < p_input->stream.p_selected_program->i_es_number ;
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if( p_es->p_dec )
            {
                input_UnselectES( p_input , p_es );
            }
            if( p_es->i_demux_fd > 0 )
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
                if( input_SelectES( p_input , p_es ) == 0 )
                {
                    ioctl_SetDMXFilter(p_input, p_es->i_id, &p_es->i_demux_fd, u_video_type,
                                       p_dvb->u_adapter, p_dvb->u_device);
                    u_video_type += 5;
                }
                break;
            case MPEG1_AUDIO_ES:
            case MPEG2_AUDIO_ES:
                if( input_SelectES( p_input , p_es ) == 0 )
                {
                    ioctl_SetDMXFilter(p_input, p_es->i_id, &p_es->i_demux_fd, u_audio_type,
                                       p_dvb->u_adapter, p_dvb->u_device);
                    input_SelectES( p_input , p_es );
                    u_audio_type += 5;
                }
                break;
            default:
                ioctl_SetDMXFilter(p_input, p_es->i_id, &p_es->i_demux_fd, 21, p_dvb->u_adapter, p_dvb->u_device);
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

