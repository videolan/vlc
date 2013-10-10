/*****************************************************************************
 * linux_dvb.c : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2010 VLC authors and VideoLAN
 *
 * Authors: Damien Lucas <nitrox@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *          Christopher Ross <chris@tebibyte.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          David Kaplan <david@of1.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_fs.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

/* DVB Card Drivers */
#include <linux/dvb/version.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "dvb.h"
#include "scan.h"

#define DMX      "/dev/dvb/adapter%d/demux%d"
#define FRONTEND "/dev/dvb/adapter%d/frontend%d"
#define DVR      "/dev/dvb/adapter%d/dvr%d"

/*
 * Frontends
 */
struct frontend_t
{
    fe_status_t i_last_status;
    struct dvb_frontend_info info;
};

#define FRONTEND_LOCK_TIMEOUT 10000000 /* 10 s */

/* Local prototypes */
static int FrontendInfo( access_t * );
static int FrontendSetQPSK( access_t * );
static int FrontendSetQAM( access_t * );
static int FrontendSetOFDM( access_t * );
static int FrontendSetATSC( access_t * );

/*****************************************************************************
 * FrontendOpen : Determine frontend device information and capabilities
 *****************************************************************************/
int FrontendOpen( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    frontend_t * p_frontend;
    unsigned int i_adapter, i_device;
    bool b_probe;
    char frontend[128];

    i_adapter = var_GetInteger( p_access, "dvb-adapter" );
    i_device = var_GetInteger( p_access, "dvb-device" );
    b_probe = var_GetBool( p_access, "dvb-probe" );

    if( snprintf( frontend, sizeof(frontend), FRONTEND, i_adapter, i_device ) >= (int)sizeof(frontend) )
    {
        msg_Err( p_access, "snprintf() truncated string for FRONTEND" );
        frontend[sizeof(frontend) - 1] = '\0';
    }

    p_sys->p_frontend = p_frontend = malloc( sizeof(frontend_t) );
    if( !p_frontend )
        return VLC_ENOMEM;

    msg_Dbg( p_access, "Opening device %s", frontend );
    if( (p_sys->i_frontend_handle = vlc_open(frontend, O_RDWR | O_NONBLOCK)) < 0 )
    {
        msg_Err( p_access, "FrontEndOpen: opening device failed (%m)" );
        free( p_frontend );
        return VLC_EGENERIC;
    }

    if( b_probe )
    {
        const char * psz_expected = NULL;
        const char * psz_real;

        if( FrontendInfo( p_access ) < 0 )
        {
            close( p_sys->i_frontend_handle );
            free( p_frontend );
            return VLC_EGENERIC;
        }

        switch( p_frontend->info.type )
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
        case FE_ATSC:
            psz_real = "ATSC";
            break;
        default:
            psz_real = "unknown";
        }

        /* Sanity checks */
        if( (!strncmp( p_access->psz_access, "qpsk", 4 ) ||
             !strncmp( p_access->psz_access, "dvb-s", 5 ) ||
             !strncmp( p_access->psz_access, "satellite", 9 ) ) &&
             (p_frontend->info.type != FE_QPSK) )
        {
            psz_expected = "DVB-S";
        }
        if( (!strncmp( p_access->psz_access, "cable", 5 ) ||
             !strncmp( p_access->psz_access, "dvb-c", 5 ) ) &&
             (p_frontend->info.type != FE_QAM) )
        {
            psz_expected = "DVB-C";
        }
        if( (!strncmp( p_access->psz_access, "terrestrial", 11 ) ||
             !strncmp( p_access->psz_access, "dvb-t", 5 ) ) &&
             (p_frontend->info.type != FE_OFDM) )
        {
            psz_expected = "DVB-T";
        }

        if( (!strncmp( p_access->psz_access, "usdigital", 9 ) ||
             !strncmp( p_access->psz_access, "atsc", 4 ) ) &&
             (p_frontend->info.type != FE_ATSC) )
        {
            psz_expected = "ATSC";
        }

        if( psz_expected != NULL )
        {
            msg_Err( p_access, "requested type %s not supported by %s tuner",
                     psz_expected, psz_real );
            close( p_sys->i_frontend_handle );
            free( p_frontend );
            return VLC_EGENERIC;
        }
    }
    else /* no frontend probing is done so use default border values. */
    {
        msg_Dbg( p_access, "using default values for frontend info" );

        msg_Dbg( p_access, "method of access is %s", p_access->psz_access );
        p_frontend->info.type = FE_QPSK;
        if( !strncmp( p_access->psz_access, "qpsk", 4 ) ||
            !strncmp( p_access->psz_access, "dvb-s", 5 ) )
            p_frontend->info.type = FE_QPSK;
        else if( !strncmp( p_access->psz_access, "cable", 5 ) ||
                 !strncmp( p_access->psz_access, "dvb-c", 5 ) )
            p_frontend->info.type = FE_QAM;
        else if( !strncmp( p_access->psz_access, "terrestrial", 11 ) ||
                 !strncmp( p_access->psz_access, "dvb-t", 5 ) )
            p_frontend->info.type = FE_OFDM;
        else if( !strncmp( p_access->psz_access, "usdigital", 9 ) ||
                 !strncmp( p_access->psz_access, "atsc", 4 ) )
            p_frontend->info.type = FE_ATSC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * FrontendClose : Close the frontend
 *****************************************************************************/
void FrontendClose( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_frontend )
    {
        close( p_sys->i_frontend_handle );
        free( p_sys->p_frontend );

        p_sys->p_frontend = NULL;
    }
}

/*****************************************************************************
 * FrontendSet : Tune !
 *****************************************************************************/
int FrontendSet( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    switch( p_sys->p_frontend->info.type )
    {
    /* DVB-S */
    case FE_QPSK:
        if( FrontendSetQPSK( p_access ) )
        {
            msg_Err( p_access, "DVB-S tuning error" );
            return VLC_EGENERIC;
        }
        break;

    /* DVB-C */
    case FE_QAM:
        if( FrontendSetQAM( p_access ) )
        {
            msg_Err( p_access, "DVB-C tuning error" );
            return VLC_EGENERIC;
        }
        break;

    /* DVB-T */
    case FE_OFDM:
        if( FrontendSetOFDM( p_access ) )
        {
            msg_Err( p_access, "DVB-T tuning error" );
            return VLC_EGENERIC;
        }
        break;

    /* ATSC */
    case FE_ATSC:
        if( FrontendSetATSC( p_access ) )
        {
            msg_Err( p_access, "ATSC tuning error" );
            return VLC_EGENERIC;
        }
        break;

    default:
        msg_Err( p_access, "tuner type %s not supported",
                 p_sys->p_frontend->info.name );
        return VLC_EGENERIC;
    }
    p_sys->p_frontend->i_last_status = 0;
    p_sys->i_frontend_timeout = mdate() + FRONTEND_LOCK_TIMEOUT;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * FrontendPoll : Poll for frontend events
 *****************************************************************************/
void FrontendPoll( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    frontend_t * p_frontend = p_sys->p_frontend;
    struct dvb_frontend_event event;
    fe_status_t i_status, i_diff;

    for( ;; )
    {
        if( ioctl( p_sys->i_frontend_handle, FE_GET_EVENT, &event ) < 0 )
        {
            if( errno != EWOULDBLOCK )
                msg_Err( p_access, "frontend event error: %m" );
            return;
        }

        i_status = event.status;
        i_diff = i_status ^ p_frontend->i_last_status;
        p_frontend->i_last_status = i_status;

        {
#define IF_UP( x )                                                          \
        }                                                                   \
        if ( i_diff & (x) )                                                 \
        {                                                                   \
            if ( i_status & (x) )

            IF_UP( FE_HAS_SIGNAL )
                msg_Dbg( p_access, "frontend has acquired signal" );
            else
                msg_Dbg( p_access, "frontend has lost signal" );

            IF_UP( FE_HAS_CARRIER )
                msg_Dbg( p_access, "frontend has acquired carrier" );
            else
                msg_Dbg( p_access, "frontend has lost carrier" );

            IF_UP( FE_HAS_VITERBI )
                msg_Dbg( p_access, "frontend has acquired stable FEC" );
            else
                msg_Dbg( p_access, "frontend has lost FEC" );

            IF_UP( FE_HAS_SYNC )
                msg_Dbg( p_access, "frontend has acquired sync" );
            else
                msg_Dbg( p_access, "frontend has lost sync" );

            IF_UP( FE_HAS_LOCK )
            {
                frontend_statistic_t stat;

                msg_Dbg( p_access, "frontend has acquired lock" );
                p_sys->i_frontend_timeout = 0;

                /* Read some statistics */
                if( !FrontendGetStatistic( p_access, &stat ) )
                {
                    if( stat.i_ber >= 0 )
                        msg_Dbg( p_access, "- Bit error rate: %d", stat.i_ber );
                    if( stat.i_signal_strenth >= 0 )
                        msg_Dbg( p_access, "- Signal strength: %d", stat.i_signal_strenth );
                    if( stat.i_snr >= 0 )
                        msg_Dbg( p_access, "- SNR: %d", stat.i_snr );
                }
            }
            else
            {
                msg_Dbg( p_access, "frontend has lost lock" );
                p_sys->i_frontend_timeout = mdate() + FRONTEND_LOCK_TIMEOUT;
            }

            IF_UP( FE_REINIT )
            {
                /* The frontend was reinited. */
                msg_Warn( p_access, "reiniting frontend");
                FrontendSet( p_access );
            }
        }
#undef IF_UP
    }
}

int FrontendGetStatistic( access_t *p_access, frontend_statistic_t *p_stat )
{
    access_sys_t *p_sys = p_access->p_sys;
    frontend_t * p_frontend = p_sys->p_frontend;

    if( (p_frontend->i_last_status & FE_HAS_LOCK) == 0 )
        return VLC_EGENERIC;

    memset( p_stat, 0, sizeof(*p_stat) );
    if( ioctl( p_sys->i_frontend_handle, FE_READ_BER, &p_stat->i_ber ) < 0 )
        p_stat->i_ber = -1;
    if( ioctl( p_sys->i_frontend_handle, FE_READ_SIGNAL_STRENGTH, &p_stat->i_signal_strenth ) < 0 )
        p_stat->i_signal_strenth = -1;
    if( ioctl( p_sys->i_frontend_handle, FE_READ_SNR, &p_stat->i_snr ) < 0 )
        p_stat->i_snr = -1;

    return VLC_SUCCESS;
}

void FrontendGetStatus( access_t *p_access, frontend_status_t *p_status )
{
    access_sys_t *p_sys = p_access->p_sys;
    frontend_t * p_frontend = p_sys->p_frontend;

    p_status->b_has_signal = (p_frontend->i_last_status & FE_HAS_SIGNAL) != 0;
    p_status->b_has_carrier = (p_frontend->i_last_status & FE_HAS_CARRIER) != 0;
    p_status->b_has_lock = (p_frontend->i_last_status & FE_HAS_LOCK) != 0;
}

static int ScanParametersDvbS( access_t *p_access, scan_parameter_t *p_scan )
{
    const frontend_t *p_frontend = p_access->p_sys->p_frontend;

    memset( p_scan, 0, sizeof(*p_scan) );
    p_scan->type = SCAN_DVB_S;

    p_scan->frequency.i_min = p_frontend->info.frequency_min;
    p_scan->frequency.i_max = p_frontend->info.frequency_max;
    /* set satellite config file path */
    p_scan->sat_info.psz_name = var_InheritString( p_access, "dvb-satellite" );

    return VLC_SUCCESS;
}

static int ScanParametersDvbC( access_t *p_access, scan_parameter_t *p_scan )
{
    const frontend_t *p_frontend = p_access->p_sys->p_frontend;

    memset( p_scan, 0, sizeof(*p_scan) );
    p_scan->type = SCAN_DVB_C;
    p_scan->b_exhaustive = false;

    /* */
    p_scan->frequency.i_min = p_frontend->info.frequency_min;
    p_scan->frequency.i_max = p_frontend->info.frequency_max;
    p_scan->frequency.i_step = p_frontend->info.frequency_stepsize
        ? p_frontend->info.frequency_stepsize : 166667;
    p_scan->frequency.i_count = (p_scan->frequency.i_max-p_scan->frequency.i_min)/p_scan->frequency.i_step;

    /* if frontend can do auto, don't scan them */
    if( p_frontend->info.caps & FE_CAN_QAM_AUTO )
    {
        p_scan->b_modulation_set = true;
    } else {
        p_scan->b_modulation_set = false;
        /* our scanning code flips modulation from 16..256 automaticly*/
        p_scan->i_modulation = 0;
    }

    /* if user supplies symbolrate, don't scan those */
    if( var_GetInteger( p_access, "dvb-srate" ) )
        p_scan->b_symbolrate_set = true;
    else
        p_scan->b_symbolrate_set = false;

    /* */
    p_scan->bandwidth.i_min  = 6;
    p_scan->bandwidth.i_max  = 8;
    p_scan->bandwidth.i_step = 1;
    p_scan->bandwidth.i_count = 3;
    return VLC_SUCCESS;
}

static int ScanParametersDvbT( access_t *p_access, scan_parameter_t *p_scan )
{
    const frontend_t *p_frontend = p_access->p_sys->p_frontend;

    memset( p_scan, 0, sizeof(*p_scan) );
    p_scan->type = SCAN_DVB_T;
    p_scan->b_exhaustive = false;

    /* */
    p_scan->frequency.i_min = p_frontend->info.frequency_min;
    p_scan->frequency.i_max = p_frontend->info.frequency_max;
    p_scan->frequency.i_step = p_frontend->info.frequency_stepsize
        ? p_frontend->info.frequency_stepsize : 166667;
    p_scan->frequency.i_count = (p_scan->frequency.i_max-p_scan->frequency.i_min)/p_scan->frequency.i_step;

    /* */
    p_scan->bandwidth.i_min  = 6;
    p_scan->bandwidth.i_max  = 8;
    p_scan->bandwidth.i_step = 1;
    p_scan->bandwidth.i_count = 3;
    return VLC_SUCCESS;
}

int  FrontendGetScanParameter( access_t *p_access, scan_parameter_t *p_scan )
{
    access_sys_t *p_sys = p_access->p_sys;
    const frontend_t *p_frontend = p_sys->p_frontend;

    if( p_frontend->info.type == FE_OFDM )              /* DVB-T */
        return ScanParametersDvbT( p_access, p_scan );
    else if( p_frontend->info.type == FE_QAM )          /* DVB-C */
        return ScanParametersDvbC( p_access, p_scan );
    else if( p_frontend->info.type == FE_QPSK )
        return ScanParametersDvbS( p_access, p_scan );  /* DVB-S */

    msg_Err( p_access, "frontend scanning not supported" );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * FrontendInfo : Return information about given frontend
 *****************************************************************************/
static int FrontendInfo( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    frontend_t *p_frontend = p_sys->p_frontend;

    /* Determine type of frontend */
    if( ioctl( p_sys->i_frontend_handle, FE_GET_INFO, &p_frontend->info ) < 0 )
    {
        msg_Err( p_access, "frontend info request error: %m" );
        return VLC_EGENERIC;
    }

    /* Print out frontend capabilities. */
    msg_Dbg(p_access, "Frontend Info:" );
    msg_Dbg(p_access, "  name = %s", p_frontend->info.name );
    switch( p_frontend->info.type )
    {
        case FE_QPSK:
            msg_Dbg( p_access, "  type = QPSK (DVB-S)" );
            break;
        case FE_QAM:
            msg_Dbg( p_access, "  type = QAM (DVB-C)" );
            break;
        case FE_OFDM:
            msg_Dbg( p_access, "  type = OFDM (DVB-T)" );
            break;
        case FE_ATSC:
            msg_Dbg( p_access, "  type = ATSC (USA)" );
            break;
#if 0 /* DVB_API_VERSION == 3 */
        case FE_MEMORY:
            msg_Dbg(p_access, "  type = MEMORY" );
            break;
        case FE_NET:
            msg_Dbg(p_access, "  type = NETWORK" );
            break;
#endif
        default:
            msg_Err( p_access, "  unknown frontend type (%d)",
                     p_frontend->info.type );
            return VLC_EGENERIC;
    }
    msg_Dbg(p_access, "  frequency_min = %u (kHz)",
            p_frontend->info.frequency_min);
    msg_Dbg(p_access, "  frequency_max = %u (kHz)",
            p_frontend->info.frequency_max);
    msg_Dbg(p_access, "  frequency_stepsize = %u",
            p_frontend->info.frequency_stepsize);
    msg_Dbg(p_access, "  frequency_tolerance = %u",
            p_frontend->info.frequency_tolerance);
    msg_Dbg(p_access, "  symbol_rate_min = %u (kHz)",
            p_frontend->info.symbol_rate_min);
    msg_Dbg(p_access, "  symbol_rate_max = %u (kHz)",
            p_frontend->info.symbol_rate_max);
    msg_Dbg(p_access, "  symbol_rate_tolerance (ppm) = %u",
            p_frontend->info.symbol_rate_tolerance);
    msg_Dbg(p_access, "  notifier_delay (ms) = %u",
            p_frontend->info.notifier_delay );

    msg_Dbg(p_access, "Frontend Info capability list:");
    if( p_frontend->info.caps == FE_IS_STUPID)
        msg_Dbg(p_access, "  no capabilities - frontend is stupid!");
    if( p_frontend->info.caps & FE_CAN_INVERSION_AUTO)
        msg_Dbg(p_access, "  inversion auto");
    if( p_frontend->info.caps & FE_CAN_FEC_1_2)
        msg_Dbg(p_access, "  forward error correction 1/2");
    if( p_frontend->info.caps & FE_CAN_FEC_2_3)
        msg_Dbg(p_access, "  forward error correction 2/3");
    if( p_frontend->info.caps & FE_CAN_FEC_3_4)
        msg_Dbg(p_access, "  forward error correction 3/4");
    if( p_frontend->info.caps & FE_CAN_FEC_4_5)
        msg_Dbg(p_access, "  forward error correction 4/5");
    if( p_frontend->info.caps & FE_CAN_FEC_5_6)
        msg_Dbg(p_access, "  forward error correction 5/6");
    if( p_frontend->info.caps & FE_CAN_FEC_6_7)
        msg_Dbg(p_access, "  forward error correction 6/7");
    if( p_frontend->info.caps & FE_CAN_FEC_7_8)
        msg_Dbg(p_access, "  forward error correction 7/8");
    if( p_frontend->info.caps & FE_CAN_FEC_8_9)
        msg_Dbg(p_access, "  forward error correction 8/9");
    if( p_frontend->info.caps & FE_CAN_FEC_AUTO)
        msg_Dbg(p_access, "  forward error correction auto");
    if( p_frontend->info.caps & FE_CAN_QPSK)
        msg_Dbg(p_access, "  QPSK modulation");
    if( p_frontend->info.caps & FE_CAN_QAM_16)
        msg_Dbg(p_access, "  QAM 16 modulation");
    if( p_frontend->info.caps & FE_CAN_QAM_32)
        msg_Dbg(p_access, "  QAM 32 modulation");
    if( p_frontend->info.caps & FE_CAN_QAM_64)
        msg_Dbg(p_access, "  QAM 64 modulation");
    if( p_frontend->info.caps & FE_CAN_QAM_128)
        msg_Dbg(p_access, "  QAM 128 modulation");
    if( p_frontend->info.caps & FE_CAN_QAM_256)
        msg_Dbg(p_access, "  QAM 256 modulation");
    if( p_frontend->info.caps & FE_CAN_QAM_AUTO)
        msg_Dbg(p_access, "  QAM auto modulation");
    if( p_frontend->info.caps & FE_CAN_TRANSMISSION_MODE_AUTO)
        msg_Dbg(p_access, "  transmission mode auto");
    if( p_frontend->info.caps & FE_CAN_BANDWIDTH_AUTO)
        msg_Dbg(p_access, "  bandwidth mode auto");
    if( p_frontend->info.caps & FE_CAN_GUARD_INTERVAL_AUTO)
        msg_Dbg(p_access, "  guard interval mode auto");
    if( p_frontend->info.caps & FE_CAN_HIERARCHY_AUTO)
        msg_Dbg(p_access, "  hierarchy mode auto");
    if( p_frontend->info.caps & FE_CAN_8VSB)
        msg_Dbg(p_access, "  8-level VSB modulation");
    if( p_frontend->info.caps & FE_CAN_16VSB)
        msg_Dbg(p_access, "  16-level VSB modulation");
    if( p_frontend->info.caps & FE_HAS_EXTENDED_CAPS)
        msg_Dbg(p_access, "  extended capabilities");
    /* 3 capabilities that don't exist yet HERE */
#if (DVB_API_VERSION > 5) \
 || ((DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 2))
    if( p_frontend->info.caps & FE_CAN_TURBO_FEC)
        msg_Dbg(p_access, "  Turbo FEC modulation");
#else
# warning Please update your Linux kernel headers!
#endif
    if( p_frontend->info.caps & FE_CAN_2G_MODULATION)
        msg_Dbg(p_access, "  2nd generation modulation (DVB-S2)");
    /* FE_NEEDS_BENDING is deprecated */
    if( p_frontend->info.caps & FE_CAN_RECOVER)
        msg_Dbg(p_access, "  cable unplug recovery");
    if( p_frontend->info.caps & FE_CAN_MUTE_TS)
        msg_Dbg(p_access, "  spurious TS muting");
   msg_Dbg(p_access, "End of capability list");

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Decoding the DVB parameters (common)
 *****************************************************************************/
static fe_spectral_inversion_t DecodeInversion( access_t *p_access )
{
    int i_val;
    fe_spectral_inversion_t fe_inversion = 0;

    i_val = var_GetInteger( p_access, "dvb-inversion" );
    msg_Dbg( p_access, "using inversion=%d", i_val );

    switch( i_val )
    {
        case 0: fe_inversion = INVERSION_OFF; break;
        case 1: fe_inversion = INVERSION_ON; break;
        case 2: fe_inversion = INVERSION_AUTO; break;
        default:
            msg_Dbg( p_access, "dvb has inversion not set, using auto");
            fe_inversion = INVERSION_AUTO;
            break;
    }
    return fe_inversion;
}

/*****************************************************************************
 * FrontendSetQPSK : controls the FE device
 *****************************************************************************/
static fe_sec_voltage_t DecodeVoltage( access_t *p_access )
{
    switch( var_GetInteger( p_access, "dvb-voltage" ) )
    {
        case 0:  return SEC_VOLTAGE_OFF;
        case 13: return SEC_VOLTAGE_13;
        case 18: return SEC_VOLTAGE_18;
        default: return SEC_VOLTAGE_OFF;
    }
}

static fe_sec_tone_mode_t DecodeTone( access_t *p_access )
{
    switch( var_GetInteger( p_access, "dvb-tone" ) )
    {
        case 0:  return SEC_TONE_OFF;
        case 1:  return SEC_TONE_ON;
        default: return SEC_TONE_OFF;
    }
}

struct diseqc_cmd_t
{
    struct dvb_diseqc_master_cmd cmd;
    uint32_t wait;
};

static int DoDiseqc( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_val;
    bool b_val;
    int i_frequency, i_lnb_slof;
    fe_sec_voltage_t fe_voltage;
    fe_sec_tone_mode_t fe_tone;

    i_frequency = var_GetInteger( p_access, "dvb-frequency" );
    i_lnb_slof = var_GetInteger( p_access, "dvb-lnb-slof" );

    i_val = var_GetInteger( p_access, "dvb-tone" );
    if( i_val == -1 /* auto */ )
    {
        if( i_frequency >= i_lnb_slof )
            i_val = 1;
        else
            i_val = 0;
        var_SetInteger( p_access, "dvb-tone", i_val );
    }

    fe_voltage = DecodeVoltage( p_access );
    fe_tone = DecodeTone( p_access );

    /* Switch off continuous tone. */
    if( ioctl( p_sys->i_frontend_handle, FE_SET_TONE, SEC_TONE_OFF ) < 0 )
    {
        msg_Err( p_access, "switching tone %s error: %m", "off" );
        return VLC_EGENERIC;
    }

    /* Configure LNB voltage. */
    if( ioctl( p_sys->i_frontend_handle, FE_SET_VOLTAGE, fe_voltage ) < 0 )
    {
        msg_Err( p_access, "voltage error: %m" );
        return VLC_EGENERIC;
    }

    b_val = var_GetBool( p_access, "dvb-high-voltage" );
    if( ioctl( p_sys->i_frontend_handle,
               FE_ENABLE_HIGH_LNB_VOLTAGE, b_val ) < 0 && b_val )
    {
        msg_Err( p_access, "high LNB voltage error: %m" );
    }

    /* Wait for at least 15 ms. */
    msleep(15000);

    i_val = var_GetInteger( p_access, "dvb-satno" );
    if( i_val > 0 && i_val < 5 )
    {
        /* digital satellite equipment control,
         * specification is available from http://www.eutelsat.com/
         */

        /* 1.x compatible equipment */
        struct diseqc_cmd_t cmd =  { {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4}, 0 };

        /* param: high nibble: reset bits, low nibble set bits,
         * bits are: option, position, polarization, band
         */
        cmd.cmd.msg[3] = 0xf0 /* reset bits */
                          | (((i_val - 1) * 4) & 0xc)
                          | (fe_voltage == SEC_VOLTAGE_13 ? 0 : 2)
                          | (fe_tone == SEC_TONE_ON ? 1 : 0);

        if( ioctl( p_sys->i_frontend_handle, FE_DISEQC_SEND_MASTER_CMD,
                   &cmd.cmd ) )
        {
            msg_Err( p_access, "master command sending error: %m" );
            return VLC_EGENERIC;
        }

        msleep(15000 + cmd.wait * 1000);

        /* A or B simple diseqc ("diseqc-compatible") */
        if( ioctl( p_sys->i_frontend_handle, FE_DISEQC_SEND_BURST,
                  ((i_val - 1) % 2) ? SEC_MINI_B : SEC_MINI_A ) )
        {
            msg_Err( p_access, "burst sending error: %m" );
            return VLC_EGENERIC;
        }

        msleep(15000);
    }

    if( ioctl( p_sys->i_frontend_handle, FE_SET_TONE, fe_tone ) )
    {
        msg_Err( p_access, "switching tone %s error: %m",
                 (fe_tone == SEC_TONE_ON) ? "on" : "off" );
        return VLC_EGENERIC;
    }

    msleep(50000);
    return 0;
}

static int FrontendSetQPSK( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    struct dvb_frontend_parameters fep;
    int i_val;
    int i_frequency, i_lnb_slof = 0, i_lnb_lof1, i_lnb_lof2 = 0;

    /* Prepare the fep structure */
    i_frequency = var_GetInteger( p_access, "dvb-frequency" );

    i_val = var_GetInteger( p_access, "dvb-lnb-lof1" );
    if( i_val == 0 )
    {
        /* Automatic mode. */
        if ( i_frequency >= 950000 && i_frequency <= 2150000 )
        {
            msg_Dbg( p_access, "frequency %d is in IF-band", i_frequency );
            i_lnb_lof1 = 0;
        }
        else if ( i_frequency >= 2500000 && i_frequency <= 2700000 )
        {
            msg_Dbg( p_access, "frequency %d is in S-band", i_frequency );
            i_lnb_lof1 = 3650000;
        }
        else if ( i_frequency >= 3400000 && i_frequency <= 4200000 )
        {
            msg_Dbg( p_access, "frequency %d is in C-band (lower)",
                     i_frequency );
            i_lnb_lof1 = 5150000;
        }
        else if ( i_frequency >= 4500000 && i_frequency <= 4800000 )
        {
            msg_Dbg( p_access, "frequency %d is in C-band (higher)",
                     i_frequency );
            i_lnb_lof1 = 5950000;
        }
        else if ( i_frequency >= 10700000 && i_frequency <= 13250000 )
        {
            msg_Dbg( p_access, "frequency %d is in Ku-band",
                     i_frequency );
            i_lnb_lof1 = 9750000;
            i_lnb_lof2 = 10600000;
            i_lnb_slof = 11700000;
        }
        else
        {
            msg_Err( p_access, "frequency %d is out of any known band",
                     i_frequency );
            msg_Err( p_access, "specify dvb-lnb-lof1 manually for the local "
                     "oscillator frequency" );
            return VLC_EGENERIC;
        }
        var_SetInteger( p_access, "dvb-lnb-lof1", i_lnb_lof1 );
        var_SetInteger( p_access, "dvb-lnb-lof2", i_lnb_lof2 );
        var_SetInteger( p_access, "dvb-lnb-slof", i_lnb_slof );
    }
    else
    {
        i_lnb_lof1 = i_val;
        i_lnb_lof2 = var_GetInteger( p_access, "dvb-lnb-lof2" );
        i_lnb_slof = var_GetInteger( p_access, "dvb-lnb-slof" );
    }

    if( i_lnb_slof && i_frequency >= i_lnb_slof )
    {
        i_frequency -= i_lnb_lof2;
    }
    else
    {
        i_frequency -= i_lnb_lof1;
    }
    fep.frequency = i_frequency >= 0 ? i_frequency : -i_frequency;

    fep.inversion = DecodeInversion( p_access );

    fep.u.qpsk.symbol_rate = var_GetInteger( p_access, "dvb-srate" );

    fep.u.qpsk.fec_inner = FEC_NONE;

    if( DoDiseqc( p_access ) < 0 )
    {
        return VLC_EGENERIC;
    }

    /* Empty the event queue */
    for( ; ; )
    {
        struct dvb_frontend_event event;
        if ( ioctl( p_sys->i_frontend_handle, FE_GET_EVENT, &event ) < 0
              && errno == EWOULDBLOCK )
            break;
    }

    /* Now send it all to the frontend device */
    if( ioctl( p_sys->i_frontend_handle, FE_SET_FRONTEND, &fep ) < 0 )
    {
        msg_Err( p_access, "frontend error: %m" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * FrontendSetQAM : controls the FE device
 *****************************************************************************/
static int FrontendSetQAM( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    frontend_t *p_frontend = p_sys->p_frontend;
    struct dvb_frontend_parameters fep;
    unsigned int i_val;

    /* Prepare the fep structure */

    fep.frequency = var_GetInteger( p_access, "dvb-frequency" );

    fep.inversion = DecodeInversion( p_access );

    /* Default symbol-rate is for dvb-s, and doesn't fit
     * for dvb-c, so if it's over the limit of frontend, default to
     * somewhat common value
     */
    i_val = var_GetInteger( p_access, "dvb-srate" );
    if( i_val < p_frontend->info.symbol_rate_max &&
        i_val > p_frontend->info.symbol_rate_min )
        fep.u.qam.symbol_rate = i_val;

    fep.u.qam.fec_inner = FEC_NONE;

    fep.u.qam.modulation = QAM_AUTO;

    /* Empty the event queue */
    for( ; ; )
    {
        struct dvb_frontend_event event;
        if ( ioctl( p_sys->i_frontend_handle, FE_GET_EVENT, &event ) < 0
              && errno == EWOULDBLOCK )
            break;
    }

    /* Now send it all to the frontend device */
    if( ioctl( p_sys->i_frontend_handle, FE_SET_FRONTEND, &fep ) < 0 )
    {
        msg_Err( p_access, "frontend error: %m" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * FrontendSetOFDM : controls the FE device
 *****************************************************************************/
static fe_bandwidth_t DecodeBandwidth( access_t *p_access )
{
    fe_bandwidth_t      fe_bandwidth = 0;
    int i_bandwidth = var_GetInteger( p_access, "dvb-bandwidth" );

    msg_Dbg( p_access, "using bandwidth=%d", i_bandwidth );

    switch( i_bandwidth )
    {
        case 0: fe_bandwidth = BANDWIDTH_AUTO; break;
        case 6: fe_bandwidth = BANDWIDTH_6_MHZ; break;
        case 7: fe_bandwidth = BANDWIDTH_7_MHZ; break;
        case 8: fe_bandwidth = BANDWIDTH_8_MHZ; break;
        default:
            msg_Dbg( p_access, "terrestrial dvb has bandwidth not set, using auto" );
            fe_bandwidth = BANDWIDTH_AUTO;
            break;
    }
    return fe_bandwidth;
}

static fe_transmit_mode_t DecodeTransmission( access_t *p_access )
{
    fe_transmit_mode_t  fe_transmission = 0;
    int i_transmission = var_GetInteger( p_access, "dvb-transmission" );

    msg_Dbg( p_access, "using transmission=%d", i_transmission );

    switch( i_transmission )
    {
        case 0: fe_transmission = TRANSMISSION_MODE_AUTO; break;
        case 2: fe_transmission = TRANSMISSION_MODE_2K; break;
        case 8: fe_transmission = TRANSMISSION_MODE_8K; break;
        default:
            msg_Dbg( p_access, "terrestrial dvb has transmission mode not set, using auto");
            fe_transmission = TRANSMISSION_MODE_AUTO;
            break;
    }
    return fe_transmission;
}

static fe_hierarchy_t DecodeHierarchy( access_t *p_access )
{
    fe_hierarchy_t      fe_hierarchy = 0;
    int i_hierarchy = var_GetInteger( p_access, "dvb-hierarchy" );

    msg_Dbg( p_access, "using hierarchy=%d", i_hierarchy );

    switch( i_hierarchy )
    {
        case -1: fe_hierarchy = HIERARCHY_NONE; break;
        case 0: fe_hierarchy = HIERARCHY_AUTO; break;
        case 1: fe_hierarchy = HIERARCHY_1; break;
        case 2: fe_hierarchy = HIERARCHY_2; break;
        case 4: fe_hierarchy = HIERARCHY_4; break;
        default:
            msg_Dbg( p_access, "terrestrial dvb has hierarchy not set, using auto");
            fe_hierarchy = HIERARCHY_AUTO;
            break;
    }
    return fe_hierarchy;
}

static int FrontendSetOFDM( access_t * p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    struct dvb_frontend_parameters fep;

    /* Prepare the fep structure */

    fep.frequency = var_GetInteger( p_access, "dvb-frequency" );

    fep.inversion = DecodeInversion( p_access );

    fep.u.ofdm.bandwidth = DecodeBandwidth( p_access );
    fep.u.ofdm.code_rate_HP = FEC_NONE;
    fep.u.ofdm.code_rate_LP = FEC_NONE;
    fep.u.ofdm.constellation = QAM_AUTO;
    fep.u.ofdm.transmission_mode = DecodeTransmission( p_access );
    fep.u.ofdm.guard_interval = GUARD_INTERVAL_AUTO;
    fep.u.ofdm.hierarchy_information = DecodeHierarchy( p_access );

    /* Empty the event queue */
    for( ; ; )
    {
        struct dvb_frontend_event event;
        if ( ioctl( p_sys->i_frontend_handle, FE_GET_EVENT, &event ) < 0
              && errno == EWOULDBLOCK )
            break;
    }

    /* Now send it all to the frontend device */
    if( ioctl( p_sys->i_frontend_handle, FE_SET_FRONTEND, &fep ) < 0 )
    {
        msg_Err( p_access, "frontend error: %m" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * FrontendSetATSC : controls the FE device
 *****************************************************************************/
static int FrontendSetATSC( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    struct dvb_frontend_parameters fep;

    /* Prepare the fep structure */

    fep.frequency = var_GetInteger( p_access, "dvb-frequency" );
    fep.u.vsb.modulation = VSB_8;

    /* Empty the event queue */
    for( ; ; )
    {
        struct dvb_frontend_event event;
        if ( ioctl( p_sys->i_frontend_handle, FE_GET_EVENT, &event ) < 0
              && errno == EWOULDBLOCK )
            break;
    }

    /* Now send it all to the frontend device */
    if( ioctl( p_sys->i_frontend_handle, FE_SET_FRONTEND, &fep ) < 0 )
    {
        msg_Err( p_access, "frontend error: %m" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/*
 * Demux
 */

/*****************************************************************************
 * DMXSetFilter : controls the demux to add a filter
 *****************************************************************************/
int DMXSetFilter( access_t * p_access, int i_pid, int * pi_fd, int i_type )
{
    struct dmx_pes_filter_params s_filter_params;
    unsigned int i_adapter, i_device;
    char dmx[128];

    i_adapter = var_GetInteger( p_access, "dvb-adapter" );
    i_device = var_GetInteger( p_access, "dvb-device" );

    if( snprintf( dmx, sizeof(dmx), DMX, i_adapter, i_device )
            >= (int)sizeof(dmx) )
    {
        msg_Err( p_access, "snprintf() truncated string for DMX" );
        dmx[sizeof(dmx) - 1] = '\0';
    }

    msg_Dbg( p_access, "Opening device %s", dmx );
    if( (*pi_fd = vlc_open(dmx, O_RDWR)) < 0 )
    {
        msg_Err( p_access, "DMXSetFilter: opening device failed (%m)" );
        return VLC_EGENERIC;
    }

    /* We fill the DEMUX structure : */
    s_filter_params.pid     =   i_pid;
    s_filter_params.input   =   DMX_IN_FRONTEND;
    s_filter_params.output  =   DMX_OUT_TS_TAP;
    s_filter_params.flags   =   DMX_IMMEDIATE_START;

    switch ( i_type )
    {   /* First device */
        case 1:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_VIDEO0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_VIDEO0;
            break;
        case 2:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_AUDIO0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_AUDIO0;
            break;
        case 3:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_TELETEXT0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_TELETEXT0;
            break;
        case 4:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_SUBTITLE0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_SUBTITLE0;
            break;
        case 5:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_PCR0 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_PCR0;
            break;
        /* Second device */
        case 6:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_VIDEO1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_VIDEO1;
            break;
        case 7:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_AUDIO1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_AUDIO1;
            break;
        case 8:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_TELETEXT1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_TELETEXT1;
            break;
        case 9:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_SUBTITLE1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_SUBTITLE1;
            break;
        case 10:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_PCR1 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_PCR1;
            break;
        /* Third device */
        case 11:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_VIDEO2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_VIDEO2;
            break;
        case 12:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_AUDIO2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_AUDIO2;
            break;
        case 13:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_TELETEXT2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_TELETEXT2;
            break;
        case 14:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_SUBTITLE2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_SUBTITLE2;
            break;
        case 15:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_PCR2 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_PCR2;
            break;
        /* Forth device */
        case 16:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_VIDEO3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_VIDEO3;
            break;
        case 17:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_AUDIO3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_AUDIO3;
            break;
        case 18:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_TELETEXT3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_TELETEXT3;
            break;
        case 19:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_SUBTITLE3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_SUBTITLE3;
            break;
        case 20:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_PCR3 for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_PCR3;
            break;
        /* Usually used by Nova cards */
        case 21:
        default:
            msg_Dbg(p_access, "DMXSetFilter: DMX_PES_OTHER for PID %d", i_pid);
            s_filter_params.pes_type = DMX_PES_OTHER;
            break;
    }

    /* We then give the order to the device : */
    if( ioctl( *pi_fd, DMX_SET_PES_FILTER, &s_filter_params ) )
    {
        msg_Err( p_access, "setting demux PES filter failed: %m" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DMXUnsetFilter : removes a filter
 *****************************************************************************/
int DMXUnsetFilter( access_t * p_access, int i_fd )
{
    if( ioctl( i_fd, DMX_STOP ) < 0 )
    {
        msg_Err( p_access, "stopping demux failed: %m" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "DMXUnsetFilter: closing demux %d", i_fd );
    close( i_fd );
    return VLC_SUCCESS;
}


/*
 * DVR device
 */

/*****************************************************************************
 * DVROpen :
 *****************************************************************************/
int DVROpen( access_t * p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    unsigned int i_adapter, i_device;
    char dvr[128];

    i_adapter = var_GetInteger( p_access, "dvb-adapter" );
    i_device = var_GetInteger( p_access, "dvb-device" );

    if( snprintf( dvr, sizeof(dvr), DVR, i_adapter, i_device )
            >= (int)sizeof(dvr) )
    {
        msg_Err( p_access, "snprintf() truncated string for DVR" );
        dvr[sizeof(dvr) - 1] = '\0';
    }

    msg_Dbg( p_access, "Opening device %s", dvr );
    if( (p_sys->i_handle = vlc_open(dvr, O_RDONLY)) < 0 )
    {
        msg_Err( p_access, "DVROpen: opening device failed (%m)" );
        return VLC_EGENERIC;
    }

    if( fcntl( p_sys->i_handle, F_SETFL, O_NONBLOCK ) == -1 )
    {
        msg_Warn( p_access, "DVROpen: couldn't set non-blocking mode (%m)" );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DVRClose :
 *****************************************************************************/
void DVRClose( access_t * p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    close( p_sys->i_handle );
}
