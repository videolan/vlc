/*****************************************************************************
 * dvb.c : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2004 VideoLAN
 *
 * Authors: Damien Lucas <nitrox@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman@wxs.nl>
 *          Christopher Ross <chris@tebibyte.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <sys/ioctl.h>
#include <stdio.h>
#ifdef HAVE_ERRNO_H
#    include <string.h>
#    include <errno.h>
#endif

#ifdef HAVE_INTTYPES_H
#   include <inttypes.h>                                       /* int16_t .. */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>

/* DVB Card Drivers */
#include <linux/dvb/version.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include <linux/errno.h>

#include "dvb.h"


/*
 * Frontends
 */

typedef struct frontend_t {
    int i_handle;
    struct dvb_frontend_info info;
} frontend_t;

/* Local prototypes */
static int FrontendInfo( input_thread_t * p_input );
static int FrontendSetQPSK( input_thread_t * p_input );
static int FrontendSetQAM( input_thread_t * p_input );
static int FrontendSetOFDM( input_thread_t * p_input );
static int FrontendCheck( input_thread_t * p_input );

/*****************************************************************************
 * FrontendOpen : Determine frontend device information and capabilities
 *****************************************************************************/
int E_(FrontendOpen)( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    frontend_t * p_frontend;
    unsigned int i_adapter, i_device;
    vlc_bool_t b_probe;
    char frontend[128];
    vlc_value_t val;

    var_Get( p_input, "dvb-adapter", &val );
    i_adapter = val.i_int;
    var_Get( p_input, "dvb-device", &val );
    i_device = val.i_int;
    var_Get( p_input, "dvb-probe", &val );
    b_probe = val.b_bool;

    if ( snprintf( frontend, sizeof(frontend), FRONTEND, i_adapter, i_device )
            >= (int)sizeof(frontend) )
    {
        msg_Err( p_input, "snprintf() truncated string for FRONTEND" );
        frontend[sizeof(frontend) - 1] = '\0';
    }

    p_frontend = (frontend_t *) malloc(sizeof(frontend_t));
    if( p_frontend == NULL )
    {
        msg_Err( p_input, "FrontEndOpen: out of memory" );
        return -1;
    }

    p_dvb->p_frontend = p_frontend;

    msg_Dbg( p_input, "Opening device %s", frontend );
    if ( (p_frontend->i_handle = open(frontend, O_RDWR | O_NONBLOCK)) < 0 )
    {
        msg_Err( p_input, "FrontEndOpen: opening device failed (%s)",
                 strerror(errno) );
        free( p_frontend );
        return -1;
    }

    if ( b_probe )
    {
        char * psz_expected = NULL;
        char * psz_real;

        if ( FrontendInfo( p_input ) < 0 )
        {
            close( p_frontend->i_handle );
            free( p_frontend );
            return -1;
        }

        switch ( p_frontend->info.type )
        {
        case FE_OFDM:
            psz_real = "DVB-T";
            break;
        case FE_QAM:
            psz_real = "DVB-C";
            break;
        case FE_QPSK:
            psz_real = "DVB-S";
            break;
        default:
            psz_real = "unknown";
        }
    
        /* Sanity checks */
        if( ((strncmp( p_input->psz_access, "qpsk", 4 ) == 0) ||
             (strncmp( p_input->psz_access, "dvb-s", 5 ) == 0) ||
             (strncmp( p_input->psz_access, "satellite", 9 ) == 0) ) &&
             (p_frontend->info.type != FE_QPSK) )
        {
            psz_expected = "DVB-S";
        }
        if( ((strncmp( p_input->psz_access, "cable", 5 ) == 0) ||
             (strncmp( p_input->psz_access, "dvb-c", 5 ) == 0) ) &&
             (p_frontend->info.type != FE_QAM) )
        {
            psz_expected = "DVB-C";
        }
        if( ((strncmp( p_input->psz_access, "terrestrial", 11 ) == 0) ||
             (strncmp( p_input->psz_access, "dvb-t", 5 ) == 0) ) &&
             (p_frontend->info.type != FE_OFDM) )
        {
            psz_expected = "DVB-T";
        }

        if ( psz_expected != NULL )
        {
            msg_Err( p_input, "the user asked for %s, and the tuner is %s",
                     psz_expected, psz_real );
            close( p_frontend->i_handle );
            free( p_frontend );
            return -1;
        }
    }
    else /* no frontend probing is done so use default border values. */
    {
        msg_Dbg( p_input, "using default values for frontend info" );

        msg_Dbg( p_input, "method of access is %s", p_input->psz_access );
        p_frontend->info.type = FE_QPSK;
        if ( !strncmp( p_input->psz_access, "qpsk", 4 ) ||
                !strncmp( p_input->psz_access, "dvb-s", 5 ) )
            p_frontend->info.type = FE_QPSK;
        else if ( !strncmp( p_input->psz_access, "cable", 5 ) ||
                    !strncmp( p_input->psz_access, "dvb-c", 5 ) )
            p_frontend->info.type = FE_QAM;
        else if ( !strncmp( p_input->psz_access, "terrestrial", 11 ) ||
                    !strncmp( p_input->psz_access, "dvb-t", 5 ) )
            p_frontend->info.type = FE_OFDM;
    }

    return 0;
}

/*****************************************************************************
 * FrontendClose : Close the frontend
 *****************************************************************************/
void E_(FrontendClose)( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    frontend_t * p_frontend = (frontend_t *)p_dvb->p_frontend;

    if ( p_frontend != NULL )
    {
        close( p_frontend->i_handle );
        free( p_frontend );
    }
}

/*****************************************************************************
 * FrontendSet : Tune !
 *****************************************************************************/
int E_(FrontendSet)( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    frontend_t * p_frontend = (frontend_t *)p_dvb->p_frontend;

    switch ( p_frontend->info.type )
    {
    /* DVB-S: satellite and budget cards (nova) */
    case FE_QPSK:
        if ( FrontendSetQPSK( p_input ) < 0 )
        {
            msg_Err( p_input, "DVB-S: tuning failed" );
            return -1;
        }
        break;

    /* DVB-C */
    case FE_QAM:
        if ( FrontendSetQAM( p_input ) < 0 )
        {
            msg_Err( p_input, "DVB-C: tuning failed" );
            return -1;
        }
        break;

    /* DVB-T */
    case FE_OFDM:
        if ( FrontendSetOFDM( p_input ) < 0 )
        {
            msg_Err( p_input, "DVB-T: tuning failed" );
            return -1;
        }
        break;

    default:
        msg_Err( p_input, "Could not determine frontend type on %s",
                 p_frontend->info.name );
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * FrontendInfo : Return information about given frontend
 *****************************************************************************/
static int FrontendInfo( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    frontend_t * p_frontend = (frontend_t *)p_dvb->p_frontend;
    int i_ret;

    /* Determine type of frontend */
    if ( (i_ret = ioctl( p_frontend->i_handle, FE_GET_INFO,
                         &p_frontend->info )) < 0 )
    {
        msg_Err( p_input, "ioctl FE_GET_INFO failed (%d) %s", i_ret,
                 strerror(errno) );
        return -1;
    }

    /* Print out frontend capabilities. */
    msg_Dbg(p_input, "Frontend Info:" );
    msg_Dbg(p_input, "  name = %s", p_frontend->info.name);
    switch ( p_frontend->info.type )
    {
        case FE_QPSK:
            msg_Dbg( p_input, "  type = QPSK (DVB-S)" );
            break;
        case FE_QAM:
            msg_Dbg( p_input, "  type = QAM (DVB-C)" );
            break;
        case FE_OFDM:
            msg_Dbg( p_input, "  type = OFDM (DVB-T)" );
            break;
#if 0 /* DVB_API_VERSION == 3 */
        case FE_MEMORY:
            msg_Dbg(p_input, "  type = MEMORY" );
            break;
        case FE_NET:
            msg_Dbg(p_input, "  type = NETWORK" );
            break;
#endif
        default:
            msg_Err( p_input, "  unknown frontend type (%d)",
                     p_frontend->info.type );
            return -1;
    }
    msg_Dbg(p_input, "  frequency_min = %u (kHz)",
            p_frontend->info.frequency_min);
    msg_Dbg(p_input, "  frequency_max = %u (kHz)",
            p_frontend->info.frequency_max);
    msg_Dbg(p_input, "  frequency_stepsize = %u",
            p_frontend->info.frequency_stepsize);
    msg_Dbg(p_input, "  frequency_tolerance = %u",
            p_frontend->info.frequency_tolerance);
    msg_Dbg(p_input, "  symbol_rate_min = %u (kHz)",
            p_frontend->info.symbol_rate_min);
    msg_Dbg(p_input, "  symbol_rate_max = %u (kHz)",
            p_frontend->info.symbol_rate_max);
    msg_Dbg(p_input, "  symbol_rate_tolerance (ppm) = %u",
            p_frontend->info.symbol_rate_tolerance);
    msg_Dbg(p_input, "  notifier_delay (ms) = %u",
            p_frontend->info.notifier_delay );

    msg_Dbg(p_input, "Frontend Info capability list:");
    if (p_frontend->info.caps & FE_IS_STUPID)
        msg_Dbg(p_input, "  no capabilities - frontend is stupid!");
    if (p_frontend->info.caps & FE_CAN_INVERSION_AUTO)
        msg_Dbg(p_input, "  inversion auto");
    if (p_frontend->info.caps & FE_CAN_FEC_1_2)
        msg_Dbg(p_input, "  forward error correction 1/2");
    if (p_frontend->info.caps & FE_CAN_FEC_2_3)
        msg_Dbg(p_input, "  forward error correction 2/3");
    if (p_frontend->info.caps & FE_CAN_FEC_3_4)
        msg_Dbg(p_input, "  forward error correction 3/4");
    if (p_frontend->info.caps & FE_CAN_FEC_4_5)
        msg_Dbg(p_input, "  forward error correction 4/5");
    if (p_frontend->info.caps & FE_CAN_FEC_5_6)
        msg_Dbg(p_input, "  forward error correction 5/6");
    if (p_frontend->info.caps & FE_CAN_FEC_6_7)
        msg_Dbg(p_input, "  forward error correction 6/7");
    if (p_frontend->info.caps & FE_CAN_FEC_7_8)
        msg_Dbg(p_input, "  forward error correction 7/8");
    if (p_frontend->info.caps & FE_CAN_FEC_8_9)
        msg_Dbg(p_input, "  forward error correction 8/9");
    if (p_frontend->info.caps & FE_CAN_FEC_AUTO)
        msg_Dbg(p_input, "  forward error correction auto");
    if (p_frontend->info.caps & FE_CAN_QPSK)
        msg_Dbg(p_input, "  card can do QPSK");
    if (p_frontend->info.caps & FE_CAN_QAM_16)
        msg_Dbg(p_input, "  card can do QAM 16");
    if (p_frontend->info.caps & FE_CAN_QAM_32)
        msg_Dbg(p_input, "  card can do QAM 32");
    if (p_frontend->info.caps & FE_CAN_QAM_64)
        msg_Dbg(p_input, "  card can do QAM 64");
    if (p_frontend->info.caps & FE_CAN_QAM_128)
        msg_Dbg(p_input, "  card can do QAM 128");
    if (p_frontend->info.caps & FE_CAN_QAM_256)
        msg_Dbg(p_input, "  card can do QAM 256");
    if (p_frontend->info.caps & FE_CAN_QAM_AUTO)
        msg_Dbg(p_input, "  card can do QAM auto");
    if (p_frontend->info.caps & FE_CAN_TRANSMISSION_MODE_AUTO)
        msg_Dbg(p_input, "  transmission mode auto");
    if (p_frontend->info.caps & FE_CAN_BANDWIDTH_AUTO)
        msg_Dbg(p_input, "  bandwidth mode auto");
    if (p_frontend->info.caps & FE_CAN_GUARD_INTERVAL_AUTO)
        msg_Dbg(p_input, "  guard interval mode auto");
    if (p_frontend->info.caps & FE_CAN_HIERARCHY_AUTO)
        msg_Dbg(p_input, "  hierarchy mode auto");
    if (p_frontend->info.caps & FE_CAN_MUTE_TS)
        msg_Dbg(p_input, "  card can mute TS");
    if (p_frontend->info.caps & FE_CAN_CLEAN_SETUP)
        msg_Dbg(p_input, "  clean setup");
    msg_Dbg(p_input, "End of capability list");

    return 0;
}

/*****************************************************************************
 * Decoding the DVB parameters (common)
 *****************************************************************************/
static fe_spectral_inversion_t DecodeInversion( input_thread_t * p_input )
{
    vlc_value_t         val;
    fe_spectral_inversion_t fe_inversion = 0;

    var_Get( p_input, "dvb-inversion", &val );
    msg_Dbg( p_input, "using inversion=%d", val.i_int );

    switch ( val.i_int )
    {
        case 0: fe_inversion = INVERSION_OFF; break;
        case 1: fe_inversion = INVERSION_ON; break;
        case 2: fe_inversion = INVERSION_AUTO; break;
        default:
            msg_Dbg( p_input, "dvb has inversion not set, using auto");
            fe_inversion = INVERSION_AUTO;
            break;
    }
    return fe_inversion;
}

static fe_code_rate_t DecodeFEC( input_thread_t * p_input, int i_val )
{
    fe_code_rate_t      fe_fec = FEC_NONE;

    msg_Dbg( p_input, "using feq=%d", i_val );

    switch ( i_val )
    {
        case 1: fe_fec = FEC_1_2; break;
        case 2: fe_fec = FEC_2_3; break;
        case 3: fe_fec = FEC_3_4; break;
        case 4: fe_fec = FEC_4_5; break;
        case 5: fe_fec = FEC_5_6; break;
        case 6: fe_fec = FEC_6_7; break;
        case 7: fe_fec = FEC_7_8; break;
        case 8: fe_fec = FEC_8_9; break;
        case 9: fe_fec = FEC_AUTO; break;
        default:
            /* cannot happen */
            fe_fec = FEC_NONE;
            msg_Err( p_input, "argument has invalid FEC (%d)", i_val);
            break;
    }    
    return fe_fec;
}

static fe_modulation_t DecodeModulation( input_thread_t * p_input )
{
    vlc_value_t         val;
    fe_modulation_t     fe_modulation = 0;

    var_Get( p_input, "dvb-modulation", &val );

    switch ( val.i_int )
    {
        case -1: fe_modulation = QPSK; break;
        case 0: fe_modulation = QAM_AUTO; break;
        case 16: fe_modulation = QAM_16; break;
        case 32: fe_modulation = QAM_32; break;
        case 64: fe_modulation = QAM_64; break;
        case 128: fe_modulation = QAM_128; break;
        case 256: fe_modulation = QAM_256; break;
        default:
            msg_Dbg( p_input, "terrestrial/cable dvb has constellation/modulation not set, using auto");
            fe_modulation = QAM_AUTO;
            break;
    }    
    return fe_modulation;
}

/*****************************************************************************
 * FrontendSetQPSK : controls the FE device
 *****************************************************************************/
static fe_sec_voltage_t DecodeVoltage( input_thread_t * p_input )
{
    vlc_value_t         val;
    fe_sec_voltage_t    fe_voltage;

    var_Get( p_input, "dvb-voltage", &val );
    msg_Dbg( p_input, "using voltage=%d", val.i_int );

    switch ( val.i_int )
    {
        case 0: fe_voltage = SEC_VOLTAGE_OFF; break;
        case 13: fe_voltage = SEC_VOLTAGE_13; break;
        case 18: fe_voltage = SEC_VOLTAGE_18; break;
        default:
            fe_voltage = SEC_VOLTAGE_OFF;
            msg_Err( p_input, "argument has invalid voltage (%d)", val.i_int);
            break;
    }    
    return fe_voltage;
}

static fe_sec_tone_mode_t DecodeTone( input_thread_t * p_input )
{
    vlc_value_t         val;
    fe_sec_tone_mode_t  fe_tone;

    var_Get( p_input, "dvb-tone", &val );
    msg_Dbg( p_input, "using tone=%d", val.i_int );

    switch ( val.i_int )
    {
        case 0: fe_tone = SEC_TONE_OFF; break;
        case 1: fe_tone = SEC_TONE_ON; break;
        default:
            fe_tone = SEC_TONE_OFF;
            msg_Err( p_input, "argument has invalid tone mode (%d)", val.i_int);
            break;
    }    
    return fe_tone;
}

struct diseqc_cmd_t
{
    struct dvb_diseqc_master_cmd cmd;
    uint32_t wait;
};

static int DoDiseqc( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    frontend_t * p_frontend = (frontend_t *)p_dvb->p_frontend;
    vlc_value_t val;
    int i_frequency, i_lnb_slof;
    fe_sec_voltage_t fe_voltage;
    fe_sec_tone_mode_t fe_tone;
    int i_err;

    var_Get( p_input, "dvb-frequency", &val );
    i_frequency = val.i_int;
    var_Get( p_input, "dvb-lnb-slof", &val );
    i_lnb_slof = val.i_int;

    var_Get( p_input, "dvb-tone", &val );
    if ( val.i_int == -1 /* auto */ )
    {
        if ( i_frequency >= i_lnb_slof )
            val.i_int = 1;
        else
            val.i_int = 0;
        var_Set( p_input, "dvb-tone", val );
    }

    fe_voltage = DecodeVoltage( p_input );
    fe_tone = DecodeTone( p_input );

    if ( (i_err = ioctl( p_frontend->i_handle, FE_SET_VOLTAGE, fe_voltage )) < 0 )
    {
        msg_Err( p_input, "ioctl FE_SET_VOLTAGE failed, voltage=%d (%d) %s",
                 fe_voltage, i_err, strerror(errno) );
        return i_err;
    }

    var_Get( p_input, "dvb-satno", &val );
    if ( val.i_int != 0 )
    {
        /* digital satellite equipment control,
         * specification is available from http://www.eutelsat.com/ 
         */
        if ( (i_err = ioctl( p_frontend->i_handle, FE_SET_TONE,
                             SEC_TONE_OFF )) < 0 )
        {
            msg_Err( p_input, "ioctl FE_SET_TONE failed, tone=off (%d) %s",
                     i_err, strerror(errno) );
            return i_err;
        }

        msleep(15000);

        if ( val.i_int >= 1 && val.i_int <= 4 )
        {
            /* 1.x compatible equipment */
            struct diseqc_cmd_t cmd =  { {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4}, 0 };

            /* param: high nibble: reset bits, low nibble set bits,
             * bits are: option, position, polarization, band
             */
            cmd.cmd.msg[3] = 0xf0 /* reset bits */
                              | (((val.i_int - 1) * 4) & 0xc)
                              | (fe_voltage == SEC_VOLTAGE_13 ? 0 : 2)
                              | (fe_tone == SEC_TONE_ON ? 1 : 0);

            if ( (i_err = ioctl( p_frontend->i_handle, FE_DISEQC_SEND_MASTER_CMD,
                               &cmd.cmd )) < 0 )
            {
                msg_Err( p_input, "ioctl FE_SEND_MASTER_CMD failed (%d) %s",
                         i_err, strerror(errno) );
                return i_err;
            }

            msleep(cmd.wait * 1000);
        }
        else
        {
            /* A or B simple diseqc */
            if ( (i_err = ioctl( p_frontend->i_handle, FE_DISEQC_SEND_BURST,
                          val.i_int == -1 ? SEC_MINI_A : SEC_MINI_B )) < 0 )
            {
                msg_Err( p_input, "ioctl FE_SEND_BURST failed (%d) %s",
                         i_err, strerror(errno) );
                return i_err;
            }
        }

        msleep(15000);
    }

    if ( (i_err = ioctl( p_frontend->i_handle, FE_SET_TONE, fe_tone )) < 0 )
    {
        msg_Err( p_input, "ioctl FE_SET_TONE failed, tone=%s (%d) %s",
                 fe_tone == SEC_TONE_ON ? "on" : "off", i_err,
                 strerror(errno) );
        return i_err;
    }

    msleep(15000);
    return 0;
}

static int FrontendSetQPSK( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    frontend_t * p_frontend = (frontend_t *)p_dvb->p_frontend;
    struct dvb_frontend_parameters fep;
    int i_ret;
    vlc_value_t val;
    int i_frequency, i_lnb_slof;

    /* Prepare the fep structure */

    var_Get( p_input, "dvb-frequency", &val );
    i_frequency = val.i_int;
    var_Get( p_input, "dvb-lnb-slof", &val );
    i_lnb_slof = val.i_int;

    if ( i_frequency >= i_lnb_slof )
        var_Get( p_input, "dvb-lnb-lof2", &val );
    else
        var_Get( p_input, "dvb-lnb-lof1", &val );
    fep.frequency = i_frequency - val.i_int;

    fep.inversion = DecodeInversion( p_input );

    var_Get( p_input, "dvb-srate", &val );
    fep.u.qpsk.symbol_rate = val.i_int;

    var_Get( p_input, "dvb-fec", &val );
    fep.u.qpsk.fec_inner = DecodeFEC( p_input, val.i_int );

    if ( DoDiseqc( p_input ) < 0 )
    {
        return -1;
    }

    msleep(100000);

    /* Empty the event queue */
    for ( ; ; )
    {
        struct dvb_frontend_event event;
        if ( ioctl( p_frontend->i_handle, FE_GET_EVENT, &event ) < 0 )
            break;
    }

    /* Now send it all to the frontend device */
    if ( (i_ret = ioctl( p_frontend->i_handle, FE_SET_FRONTEND, &fep )) < 0 )
    {
        msg_Err( p_input, "DVB-S: setting frontend failed (%d) %s", i_ret,
                 strerror(errno) );
        return -1;
    }

    return FrontendCheck( p_input );
}

/*****************************************************************************
 * FrontendSetQAM : controls the FE device
 *****************************************************************************/
static int FrontendSetQAM( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    frontend_t * p_frontend = (frontend_t *)p_dvb->p_frontend;
    struct dvb_frontend_parameters fep;
    vlc_value_t val;
    int i_ret;

    /* Prepare the fep structure */

    var_Get( p_input, "dvb-frequency", &val );
    fep.frequency = val.i_int;

    fep.inversion = DecodeInversion( p_input );

    var_Get( p_input, "dvb-srate", &val );
    fep.u.qam.symbol_rate = val.i_int;

    var_Get( p_input, "dvb-fec", &val );
    fep.u.qam.fec_inner = DecodeFEC( p_input, val.i_int );

    fep.u.qam.modulation = DecodeModulation( p_input );

    /* Empty the event queue */
    for ( ; ; )
    {
        struct dvb_frontend_event event;
        if ( ioctl( p_frontend->i_handle, FE_GET_EVENT, &event ) < 0 )
            break;
    }

    /* Now send it all to the frontend device */
    if ( (i_ret = ioctl( p_frontend->i_handle, FE_SET_FRONTEND, &fep )) < 0 )
    {
        msg_Err( p_input, "DVB-C: setting frontend failed (%d) %s", i_ret,
                 strerror(errno) );
        return -1;
    }

    return FrontendCheck( p_input );
}

/*****************************************************************************
 * FrontendSetOFDM : controls the FE device
 *****************************************************************************/
static fe_bandwidth_t DecodeBandwidth( input_thread_t * p_input )
{
    vlc_value_t         val;
    fe_bandwidth_t      fe_bandwidth = 0;

    var_Get( p_input, "dvb-bandwidth", &val );
    msg_Dbg( p_input, "using bandwidth=%d", val.i_int );

    switch ( val.i_int )
    {
        case 0: fe_bandwidth = BANDWIDTH_AUTO; break;
        case 6: fe_bandwidth = BANDWIDTH_6_MHZ; break;
        case 7: fe_bandwidth = BANDWIDTH_7_MHZ; break;
        case 8: fe_bandwidth = BANDWIDTH_8_MHZ; break;
        default:
            msg_Dbg( p_input, "terrestrial dvb has bandwidth not set, using auto" );
            fe_bandwidth = BANDWIDTH_AUTO;
            break;
    }
    return fe_bandwidth;
}

static fe_transmit_mode_t DecodeTransmission( input_thread_t * p_input )
{
    vlc_value_t         val;
    fe_transmit_mode_t  fe_transmission = 0;

    var_Get( p_input, "dvb-transmission", &val );
    msg_Dbg( p_input, "using transmission=%d", val.i_int );

    switch ( val.i_int )
    {
        case 0: fe_transmission = TRANSMISSION_MODE_AUTO; break;
        case 2: fe_transmission = TRANSMISSION_MODE_2K; break;
        case 8: fe_transmission = TRANSMISSION_MODE_8K; break;
        default:
            msg_Dbg( p_input, "terrestrial dvb has transmission mode not set, using auto");
            fe_transmission = TRANSMISSION_MODE_AUTO;
            break;
    }    
    return fe_transmission;
}

static fe_guard_interval_t DecodeGuardInterval( input_thread_t * p_input )
{
    vlc_value_t         val;
    fe_guard_interval_t fe_guard = 0;

    var_Get( p_input, "dvb-guard", &val );
    msg_Dbg( p_input, "using guard=%d", val.i_int );

    switch ( val.i_int )
    {
        case 0: fe_guard = GUARD_INTERVAL_AUTO; break;
        case 4: fe_guard = GUARD_INTERVAL_1_4; break;
        case 8: fe_guard = GUARD_INTERVAL_1_8; break;
        case 16: fe_guard = GUARD_INTERVAL_1_16; break;
        case 32: fe_guard = GUARD_INTERVAL_1_32; break;
        default:
            msg_Dbg( p_input, "terrestrial dvb has guard interval not set, using auto");
            fe_guard = GUARD_INTERVAL_AUTO;
            break;
    }
    return fe_guard;
}

static fe_hierarchy_t DecodeHierarchy( input_thread_t * p_input )
{
    vlc_value_t         val;
    fe_hierarchy_t      fe_hierarchy = 0;

    var_Get( p_input, "dvb-hierarchy", &val );
    msg_Dbg( p_input, "using hierarchy=%d", val.i_int );

    switch ( val.i_int )
    {
        case -1: fe_hierarchy = HIERARCHY_NONE; break;
        case 0: fe_hierarchy = HIERARCHY_AUTO; break;
        case 1: fe_hierarchy = HIERARCHY_1; break;
        case 2: fe_hierarchy = HIERARCHY_2; break;
        case 4: fe_hierarchy = HIERARCHY_4; break;
        default:
            msg_Dbg( p_input, "terrestrial dvb has hierarchy not set, using auto");
            fe_hierarchy = HIERARCHY_AUTO;
            break;
    }
    return fe_hierarchy;
}

static int FrontendSetOFDM( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    frontend_t * p_frontend = (frontend_t *)p_dvb->p_frontend;
    struct dvb_frontend_parameters fep;
    vlc_value_t val;
    int ret;

    /* Prepare the fep structure */

    var_Get( p_input, "dvb-frequency", &val );
    fep.frequency = val.i_int;

    fep.inversion = DecodeInversion( p_input );

    fep.u.ofdm.bandwidth = DecodeBandwidth( p_input );
    var_Get( p_input, "dvb-code-rate-HP", &val );
    fep.u.ofdm.code_rate_HP = DecodeFEC( p_input, val.i_int );
    var_Get( p_input, "dvb-code-rate-LP", &val );
    fep.u.ofdm.code_rate_LP = DecodeFEC( p_input, val.i_int );
    fep.u.ofdm.constellation = DecodeModulation( p_input );
    fep.u.ofdm.transmission_mode = DecodeTransmission( p_input );
    fep.u.ofdm.guard_interval = DecodeGuardInterval( p_input );
    fep.u.ofdm.hierarchy_information = DecodeHierarchy( p_input );

    /* Empty the event queue */
    for ( ; ; )
    {
        struct dvb_frontend_event event;
        if ( ioctl( p_frontend->i_handle, FE_GET_EVENT, &event ) < 0 )
            break;
    }

    /* Now send it all to the frontend device */
    if ( (ret = ioctl( p_frontend->i_handle, FE_SET_FRONTEND, &fep )) < 0 )
    {
        msg_Err( p_input, "DVB-T: setting frontend failed (%d) %s", ret,
                 strerror(errno) );
        return -1;
    }

    return FrontendCheck( p_input );
}

/******************************************************************
 * FrontendCheck: Check completion of the frontend control sequence
 ******************************************************************/
static int FrontendCheck( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    frontend_t * p_frontend = (frontend_t *)p_dvb->p_frontend;
    int i_ret;

    while ( !p_input->b_die && !p_input->b_error )
    {
        fe_status_t status;
        if ( (i_ret = ioctl( p_frontend->i_handle, FE_READ_STATUS,
                             &status )) < 0 )
        {
            msg_Err( p_input, "reading frontend status failed (%d) %s",
                     i_ret, strerror(errno) );
            return i_ret;
        }

        if (status & FE_HAS_SIGNAL) /* found something above the noise level */
            msg_Dbg(p_input, "check frontend ... has signal");

        if (status & FE_HAS_CARRIER) /* found a DVB signal  */
            msg_Dbg(p_input, "check frontend ... has carrier");

        if (status & FE_HAS_VITERBI) /* FEC is stable  */
            msg_Dbg(p_input, "check frontend ... has stable forward error correction");

        if (status & FE_HAS_SYNC)    /* found sync bytes  */
            msg_Dbg(p_input, "check frontend ... has sync");

        if (status & FE_HAS_LOCK)    /* everything's working... */
        {
            int32_t value;
            msg_Dbg(p_input, "check frontend ... has lock");
            msg_Dbg(p_input, "tuning succeeded");

            /* Read some statistics */
            value = 0;
            if ( ioctl( p_frontend->i_handle, FE_READ_BER, &value ) >= 0 )
                msg_Dbg( p_input, "Bit error rate: %d", value );

            value = 0;
            if ( ioctl( p_frontend->i_handle, FE_READ_SIGNAL_STRENGTH, &value ) >= 0 )
                msg_Dbg( p_input, "Signal strength: %d", value );

            value = 0;
            if ( ioctl( p_frontend->i_handle, FE_READ_SNR, &value ) >= 0 )
                msg_Dbg( p_input, "SNR: %d", value );

            return 0;
        }

        if (status & FE_TIMEDOUT)    /*  no lock within the last ~2 seconds */
        {
            msg_Err(p_input, "tuning failed ... timed out");
            return -2;
        }

        if (status & FE_REINIT)
        {
            /*  frontend was reinitialized,  */
            /*  application is recommended to reset */
            /*  DiSEqC, tone and parameters */
            msg_Err(p_input, "tuning failed ... resend frontend parameters");
            return -3;
        }

        msleep(500000);
    }
    return -1;
}


/*
 * Demux
 */

/*****************************************************************************
 * DMXSetFilter : controls the demux to add a filter
 *****************************************************************************/
int E_(DMXSetFilter)( input_thread_t * p_input, int i_pid, int * pi_fd,
                      int i_type )
{
    struct dmx_pes_filter_params s_filter_params;
    int i_ret;
    unsigned int i_adapter, i_device;
    char dmx[128];
    vlc_value_t val;

    var_Get( p_input, "dvb-adapter", &val );
    i_adapter = val.i_int;
    var_Get( p_input, "dvb-device", &val );
    i_device = val.i_int;

    if ( snprintf( dmx, sizeof(dmx), DMX, i_adapter, i_device )
            >= (int)sizeof(dmx) )
    {
        msg_Err( p_input, "snprintf() truncated string for DMX" );
        dmx[sizeof(dmx) - 1] = '\0';
    }

    msg_Dbg( p_input, "Opening device %s", dmx );
    if ( (*pi_fd = open(dmx, O_RDWR)) < 0 )
    {
        msg_Err( p_input, "DMXSetFilter: opening device failed (%s)",
                 strerror(errno) );
        return -1;
    }

    /* We fill the DEMUX structure : */
    s_filter_params.pid     =   i_pid;
    s_filter_params.input   =   DMX_IN_FRONTEND;
    s_filter_params.output  =   DMX_OUT_TS_TAP;
    s_filter_params.flags   =   DMX_IMMEDIATE_START;

    switch ( i_type )
    {   /* First device */
        case 1:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_VIDEO0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_VIDEO0;
            break;
        case 2:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_AUDIO0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_AUDIO0;
            break;
        case 3:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_TELETEXT0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_TELETEXT0;
            break;
        case 4:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_SUBTITLE0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_SUBTITLE0;
            break;
        case 5:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_PCR0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_PCR0;
            break;
        /* Second device */
        case 6:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_VIDEO1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_VIDEO1;
            break;
        case 7:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_AUDIO1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_AUDIO1;
            break;
        case 8:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_TELETEXT1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_TELETEXT1;
            break;
        case 9:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_SUBTITLE1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_SUBTITLE1;
            break;
        case 10:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_PCR1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_PCR1;
            break;
        /* Third device */
        case 11:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_VIDEO2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_VIDEO2;
            break;
        case 12:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_AUDIO2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_AUDIO2;
            break;
        case 13:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_TELETEXT2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_TELETEXT2;
            break;
        case 14:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_SUBTITLE2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_SUBTITLE2;
            break;
        case 15:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_PCR2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_PCR2;
            break;
        /* Forth device */
        case 16:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_VIDEO3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_VIDEO3;
            break;
        case 17:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_AUDIO3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_AUDIO3;
            break;
        case 18:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_TELETEXT3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_TELETEXT3;
            break;
        case 19:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_SUBTITLE3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_SUBTITLE3;
            break;
        case 20:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_PCR3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_PCR3;
            break;
        /* Usually used by Nova cards */
        case 21:
        default:
            msg_Dbg(p_input, "DMXSetFilter: DMX_PES_OTHER for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_OTHER;
            break;
    }

    /* We then give the order to the device : */
    if ( (i_ret = ioctl( *pi_fd, DMX_SET_PES_FILTER, &s_filter_params )) < 0 )
    {
        msg_Err( p_input, "DMXSetFilter: failed with %d (%s)", i_ret,
                 strerror(errno) );
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * DMXUnsetFilter : removes a filter
 *****************************************************************************/
int E_(DMXUnsetFilter)( input_thread_t * p_input, int i_fd )
{
    int i_ret;

    if ( (i_ret = ioctl( i_fd, DMX_STOP )) < 0 )
    {
        msg_Err( p_input, "DMX_STOP failed for demux (%d) %s",
                 i_ret, strerror(errno) );
        return i_ret;
    }

    msg_Dbg( p_input, "DMXUnsetFilter: closing demux %d", i_fd);
    close( i_fd );
    return 0;
}


/*
 * DVR device
 */

/*****************************************************************************
 * DVROpen :
 *****************************************************************************/
int E_(DVROpen)( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;
    unsigned int i_adapter, i_device;
    char dvr[128];
    vlc_value_t val;

    var_Get( p_input, "dvb-adapter", &val );
    i_adapter = val.i_int;
    var_Get( p_input, "dvb-device", &val );
    i_device = val.i_int;

    if ( snprintf( dvr, sizeof(dvr), DVR, i_adapter, i_device )
            >= (int)sizeof(dvr) )
    {
        msg_Err( p_input, "snprintf() truncated string for DVR" );
        dvr[sizeof(dvr) - 1] = '\0';
    }

    msg_Dbg( p_input, "Opening device %s", dvr );
    if ( (p_dvb->i_handle = open(dvr, O_RDONLY)) < 0 )
    {
        msg_Err( p_input, "DVROpen: opening device failed (%s)",
                 strerror(errno) );
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * DVRClose :
 *****************************************************************************/
void E_(DVRClose)( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb
                = (thread_dvb_data_t *)p_input->p_access_data;

    close( p_dvb->i_handle );
}

