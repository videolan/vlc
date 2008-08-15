/*****************************************************************************
 * linux_dvb.c : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 *
 * Authors: Damien Lucas <nitrox@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/poll.h>

/* DVB Card Drivers */
#include <linux/dvb/version.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/ca.h>

/* Include dvbpsi headers */
#ifdef HAVE_DVBPSI_DR_H
#   include <dvbpsi/dvbpsi.h>
#   include <dvbpsi/descriptor.h>
#   include <dvbpsi/pat.h>
#   include <dvbpsi/pmt.h>
#   include <dvbpsi/dr.h>
#   include <dvbpsi/psi.h>
#else
#   include "dvbpsi.h"
#   include "descriptor.h"
#   include "tables/pat.h"
#   include "tables/pmt.h"
#   include "descriptors/dr.h"
#   include "psi.h"
#endif

#ifdef ENABLE_HTTPD
#   include "vlc_httpd.h"
#endif

#include "dvb.h"

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
    if( (p_sys->i_frontend_handle = open(frontend, O_RDWR | O_NONBLOCK)) < 0 )
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
            msg_Err( p_access, "the user asked for %s, and the tuner is %s",
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
        if( FrontendSetQPSK( p_access ) < 0 )
        {
            msg_Err( p_access, "DVB-S: tuning failed" );
            return VLC_EGENERIC;
        }
        break;

    /* DVB-C */
    case FE_QAM:
        if( FrontendSetQAM( p_access ) < 0 )
        {
            msg_Err( p_access, "DVB-C: tuning failed" );
            return VLC_EGENERIC;
        }
        break;

    /* DVB-T */
    case FE_OFDM:
        if( FrontendSetOFDM( p_access ) < 0 )
        {
            msg_Err( p_access, "DVB-T: tuning failed" );
            return VLC_EGENERIC;
        }
        break;

    /* ATSC */
    case FE_ATSC:
        if( FrontendSetATSC( p_access ) < 0 )
        {
            msg_Err( p_access, "ATSC: tuning failed" );
            return VLC_EGENERIC;
        }
        break;

    default:
        msg_Err( p_access, "Could not determine frontend type on %s",
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
        int i_ret = ioctl( p_sys->i_frontend_handle, FE_GET_EVENT, &event );

        if( i_ret < 0 )
        {
            if( errno == EWOULDBLOCK )
                return; /* no more events */

            msg_Err( p_access, "reading frontend event failed (%d): %m",
                     i_ret );
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
                int32_t i_value = 0;
                msg_Dbg( p_access, "frontend has acquired lock" );
                p_sys->i_frontend_timeout = 0;

                /* Read some statistics */
                if( ioctl( p_sys->i_frontend_handle, FE_READ_BER, &i_value ) >= 0 )
                    msg_Dbg( p_access, "- Bit error rate: %d", i_value );
                if( ioctl( p_sys->i_frontend_handle, FE_READ_SIGNAL_STRENGTH, &i_value ) >= 0 )
                    msg_Dbg( p_access, "- Signal strength: %d", i_value );
                if( ioctl( p_sys->i_frontend_handle, FE_READ_SNR, &i_value ) >= 0 )
                    msg_Dbg( p_access, "- SNR: %d", i_value );
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

#ifdef ENABLE_HTTPD
/*****************************************************************************
 * FrontendStatus : Read frontend status
 *****************************************************************************/
void FrontendStatus( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    frontend_t *p_frontend = p_sys->p_frontend;
    char *p = p_sys->psz_frontend_info = malloc( 10000 );
    fe_status_t i_status;
    int i_ret;

    /* Determine type of frontend */
    if( (i_ret = ioctl( p_sys->i_frontend_handle, FE_GET_INFO,
                        &p_frontend->info )) < 0 )
    {
        char buf[1000];
        strerror_r( errno, buf, sizeof( buf ) );
        p += sprintf( p, "ioctl FE_GET_INFO failed (%d) %s\n", i_ret, buf );
        goto out;
    }

    /* Print out frontend capabilities. */
    p += sprintf( p, "<table border=1><tr><th>name</th><td>%s</td></tr>\n",
                  p_frontend->info.name );
    switch( p_frontend->info.type )
    {
        case FE_QPSK:
            p += sprintf( p, "<tr><th>type</th><td>QPSK (DVB-S)</td></tr>\n" );
            break;
        case FE_QAM:
            p += sprintf( p, "<tr><th>type</th><td>QAM (DVB-C)</td></tr>\n" );
            break;
        case FE_OFDM:
            p += sprintf( p, "<tr><th>type</th><td>OFDM (DVB-T)</td></tr>\n" );
            break;
#if 0 /* DVB_API_VERSION == 3 */
        case FE_MEMORY:
            p += sprintf( p, "<tr><th>type</th><td>MEMORY</td></tr>\n" );
            break;
        case FE_NET:
            p += sprintf( p, "<tr><th>type</th><td>NETWORK</td></tr>\n" );
            break;
#endif
        default:
            p += sprintf( p, "<tr><th>type</th><td>UNKNOWN (%d)</td></tr>\n",
                          p_frontend->info.type );
            goto out;
    }
#define CHECK_INFO( x )                                                     \
    p += sprintf( p,                                                        \
                  "<tr><th>" STRINGIFY(x) "</th><td>%u</td></tr>\n",        \
                  p_frontend->info.x );

    CHECK_INFO( frequency_min );
    CHECK_INFO( frequency_max );
    CHECK_INFO( frequency_stepsize );
    CHECK_INFO( frequency_tolerance );
    CHECK_INFO( symbol_rate_min );
    CHECK_INFO( symbol_rate_max );
    CHECK_INFO( symbol_rate_tolerance );
    CHECK_INFO( notifier_delay );
#undef CHECK_INFO

    p += sprintf( p, "</table><p>Frontend capability list:\n<table border=1>" );

#define CHECK_CAPS( x )                                                     \
    if ( p_frontend->info.caps & (FE_##x) )                                 \
        p += sprintf( p, "<tr><td>" STRINGIFY(x) "</td></tr>\n" );

    CHECK_CAPS( IS_STUPID );
    CHECK_CAPS( CAN_INVERSION_AUTO );
    CHECK_CAPS( CAN_FEC_1_2 );
    CHECK_CAPS( CAN_FEC_2_3 );
    CHECK_CAPS( CAN_FEC_3_4 );
    CHECK_CAPS( CAN_FEC_4_5 );
    CHECK_CAPS( CAN_FEC_5_6 );
    CHECK_CAPS( CAN_FEC_6_7 );
    CHECK_CAPS( CAN_FEC_7_8 );
    CHECK_CAPS( CAN_FEC_8_9 );
    CHECK_CAPS( CAN_FEC_AUTO );
    CHECK_CAPS( CAN_QPSK );
    CHECK_CAPS( CAN_QAM_16 );
    CHECK_CAPS( CAN_QAM_32 );
    CHECK_CAPS( CAN_QAM_64 );
    CHECK_CAPS( CAN_QAM_128 );
    CHECK_CAPS( CAN_QAM_256 );
    CHECK_CAPS( CAN_QAM_AUTO );
    CHECK_CAPS( CAN_TRANSMISSION_MODE_AUTO );
    CHECK_CAPS( CAN_BANDWIDTH_AUTO );
    CHECK_CAPS( CAN_GUARD_INTERVAL_AUTO );
    CHECK_CAPS( CAN_HIERARCHY_AUTO );
    CHECK_CAPS( CAN_MUTE_TS );
    CHECK_CAPS( CAN_RECOVER );
#if 0 /* Disabled because of older distributions */
    CHECK_CAPS( CAN_CLEAN_SETUP );
#endif
#undef CHECK_CAPS

    p += sprintf( p, "</table><p>Current frontend status:\n<table border=1>" );

    if( (i_ret = ioctl( p_sys->i_frontend_handle, FE_READ_STATUS, &i_status ))
           < 0 )
    {
        char buf[1000];
        strerror_r( errno, buf, sizeof( buf ) );
        p += sprintf( p, "</table>ioctl FE_READ_STATUS failed (%d) %s\n",
                      i_ret, buf );
        goto out;
    }

#define CHECK_STATUS( x )                                                   \
    if ( i_status & (FE_##x) )                                              \
        p += sprintf( p, "<tr><td>" STRINGIFY(x) "</td></tr>\n" );

    CHECK_STATUS( HAS_SIGNAL );
    CHECK_STATUS( HAS_CARRIER );
    CHECK_STATUS( HAS_VITERBI );
    CHECK_STATUS( HAS_SYNC );
    CHECK_STATUS( HAS_LOCK );
    CHECK_STATUS( REINIT );
    if( i_status == 0 )
        p += sprintf( p, "<tr><td>Tuning failed</td></tr>\n" );
#undef CHECK_STATUS

    if ( i_status & FE_HAS_LOCK )
    {
        int32_t i_value;
        p += sprintf( p, "</table><p>Signal status:\n<table border=1>" );
        if( ioctl( p_sys->i_frontend_handle, FE_READ_BER, &i_value ) >= 0 )
            p += sprintf( p, "<tr><th>Bit error rate</th><td>%d</td></tr>\n",
                          i_value );
        if( ioctl( p_sys->i_frontend_handle, FE_READ_SIGNAL_STRENGTH,
                   &i_value ) >= 0 )
            p += sprintf( p, "<tr><th>Signal strength</th><td>%d</td></tr>\n",
                          i_value );
        if( ioctl( p_sys->i_frontend_handle, FE_READ_SNR, &i_value ) >= 0 )
            p += sprintf( p, "<tr><th>SNR</th><td>%d</td></tr>\n",
                          i_value );
    }
    p += sprintf( p, "</table>" );

out:
    vlc_mutex_lock( &p_sys->httpd_mutex );
    p_sys->b_request_frontend_info = false;
    vlc_cond_signal( &p_sys->httpd_cond );
    vlc_mutex_unlock( &p_sys->httpd_mutex );
}
#endif

/*****************************************************************************
 * FrontendInfo : Return information about given frontend
 *****************************************************************************/
static int FrontendInfo( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    frontend_t *p_frontend = p_sys->p_frontend;
    int i_ret;

    /* Determine type of frontend */
    if( (i_ret = ioctl( p_sys->i_frontend_handle, FE_GET_INFO,
                        &p_frontend->info )) < 0 )
    {
        msg_Err( p_access, "ioctl FE_GET_INFO failed (%d): %m", i_ret );
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
    if( p_frontend->info.caps & FE_IS_STUPID)
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
        msg_Dbg(p_access, "  card can do QPSK");
    if( p_frontend->info.caps & FE_CAN_QAM_16)
        msg_Dbg(p_access, "  card can do QAM 16");
    if( p_frontend->info.caps & FE_CAN_QAM_32)
        msg_Dbg(p_access, "  card can do QAM 32");
    if( p_frontend->info.caps & FE_CAN_QAM_64)
        msg_Dbg(p_access, "  card can do QAM 64");
    if( p_frontend->info.caps & FE_CAN_QAM_128)
        msg_Dbg(p_access, "  card can do QAM 128");
    if( p_frontend->info.caps & FE_CAN_QAM_256)
        msg_Dbg(p_access, "  card can do QAM 256");
    if( p_frontend->info.caps & FE_CAN_QAM_AUTO)
        msg_Dbg(p_access, "  card can do QAM auto");
    if( p_frontend->info.caps & FE_CAN_TRANSMISSION_MODE_AUTO)
        msg_Dbg(p_access, "  transmission mode auto");
    if( p_frontend->info.caps & FE_CAN_BANDWIDTH_AUTO)
        msg_Dbg(p_access, "  bandwidth mode auto");
    if( p_frontend->info.caps & FE_CAN_GUARD_INTERVAL_AUTO)
        msg_Dbg(p_access, "  guard interval mode auto");
    if( p_frontend->info.caps & FE_CAN_HIERARCHY_AUTO)
        msg_Dbg(p_access, "  hierarchy mode auto");
    if( p_frontend->info.caps & FE_CAN_MUTE_TS)
        msg_Dbg(p_access, "  card can mute TS");
    if( p_frontend->info.caps & FE_CAN_RECOVER)
        msg_Dbg(p_access, "  card can recover from a cable unplug");
    if( p_frontend->info.caps & FE_CAN_8VSB)
        msg_Dbg(p_access, "  card can do 8vsb");
    if( p_frontend->info.caps & FE_CAN_16VSB)
        msg_Dbg(p_access, "  card can do 16vsb");
    msg_Dbg(p_access, "End of capability list");

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Decoding the DVB parameters (common)
 *****************************************************************************/
static fe_spectral_inversion_t DecodeInversion( access_t *p_access )
{
    vlc_value_t         val;
    fe_spectral_inversion_t fe_inversion = 0;

    var_Get( p_access, "dvb-inversion", &val );
    msg_Dbg( p_access, "using inversion=%d", val.i_int );

    switch( val.i_int )
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

static fe_code_rate_t DecodeFEC( access_t *p_access, int i_val )
{
    fe_code_rate_t      fe_fec = FEC_NONE;

    msg_Dbg( p_access, "using fec=%d", i_val );

    switch( i_val )
    {
        case 0: fe_fec = FEC_NONE; break;
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
            msg_Err( p_access, "argument has invalid FEC (%d)", i_val);
            break;
    }
    return fe_fec;
}

static fe_modulation_t DecodeModulationQAM( access_t *p_access )
{
    switch( var_GetInteger( p_access, "dvb-modulation" ) )
    {
        case 0:     return QAM_AUTO;
        case 16:    return QAM_16;
        case 32:    return QAM_32;
        case 64:    return QAM_64;
        case 128:   return QAM_128;
        case 256:   return QAM_256;
        default:
            msg_Dbg( p_access, "QAM modulation not set, using auto");
            return QAM_AUTO;
    }
}
static fe_modulation_t DecodeModulationOFDM( access_t *p_access )
{
    switch( var_GetInteger( p_access, "dvb-modulation" ) )
    {
        case -1:    return QPSK;
        case 0:     return QAM_AUTO;
        case 16:    return QAM_16;
        case 32:    return QAM_32;
        case 64:    return QAM_64;
        case 128:   return QAM_128;
        case 256:   return QAM_256;
        default:
            msg_Dbg( p_access, "OFDM modulation not set, using QAM auto");
            return QAM_AUTO;
    }
}
static fe_modulation_t DecodeModulationATSC( access_t *p_access )
{
    switch( var_GetInteger( p_access, "dvb-modulation" ) )
    {
        case 8:     return VSB_8;
        case 16:    return VSB_16;
        default:
            msg_Dbg( p_access, "ATSC modulation not set, using VSB 8");
            return VSB_8;
    }
}

/*****************************************************************************
 * FrontendSetQPSK : controls the FE device
 *****************************************************************************/
static fe_sec_voltage_t DecodeVoltage( access_t *p_access )
{
    vlc_value_t         val;
    fe_sec_voltage_t    fe_voltage;

    var_Get( p_access, "dvb-voltage", &val );
    msg_Dbg( p_access, "using voltage=%d", val.i_int );

    switch( val.i_int )
    {
        case 0: fe_voltage = SEC_VOLTAGE_OFF; break;
        case 13: fe_voltage = SEC_VOLTAGE_13; break;
        case 18: fe_voltage = SEC_VOLTAGE_18; break;
        default:
            fe_voltage = SEC_VOLTAGE_OFF;
            msg_Err( p_access, "argument has invalid voltage (%d)", val.i_int );
            break;
    }
    return fe_voltage;
}

static fe_sec_tone_mode_t DecodeTone( access_t *p_access )
{
    vlc_value_t         val;
    fe_sec_tone_mode_t  fe_tone;

    var_Get( p_access, "dvb-tone", &val );
    msg_Dbg( p_access, "using tone=%d", val.i_int );

    switch( val.i_int )
    {
        case 0: fe_tone = SEC_TONE_OFF; break;
        case 1: fe_tone = SEC_TONE_ON; break;
        default:
            fe_tone = SEC_TONE_OFF;
            msg_Err( p_access, "argument has invalid tone mode (%d)", val.i_int);
            break;
    }
    return fe_tone;
}

struct diseqc_cmd_t
{
    struct dvb_diseqc_master_cmd cmd;
    uint32_t wait;
};

static int DoDiseqc( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    vlc_value_t val;
    int i_frequency, i_lnb_slof;
    fe_sec_voltage_t fe_voltage;
    fe_sec_tone_mode_t fe_tone;
    int i_err;

    var_Get( p_access, "dvb-frequency", &val );
    i_frequency = val.i_int;
    var_Get( p_access, "dvb-lnb-slof", &val );
    i_lnb_slof = val.i_int;

    var_Get( p_access, "dvb-tone", &val );
    if( val.i_int == -1 /* auto */ )
    {
        if( i_frequency >= i_lnb_slof )
            val.i_int = 1;
        else
            val.i_int = 0;
        var_Set( p_access, "dvb-tone", val );
    }

    fe_voltage = DecodeVoltage( p_access );
    fe_tone = DecodeTone( p_access );

    /* Switch off continuous tone. */
    if( (i_err = ioctl( p_sys->i_frontend_handle, FE_SET_TONE, SEC_TONE_OFF )) < 0 )
    {
        msg_Err( p_access, "ioctl FE_SET_TONE failed, tone=%s (%d) %m",
                 fe_tone == SEC_TONE_ON ? "on" : "off", i_err );
        return i_err;
    }

    /* Configure LNB voltage. */
    if( (i_err = ioctl( p_sys->i_frontend_handle, FE_SET_VOLTAGE, fe_voltage )) < 0 )
    {
        msg_Err( p_access, "ioctl FE_SET_VOLTAGE failed, voltage=%d (%d) %m",
                 fe_voltage, i_err );
        return i_err;
    }

    var_Get( p_access, "dvb-high-voltage", &val );
    if( (i_err = ioctl( p_sys->i_frontend_handle, FE_ENABLE_HIGH_LNB_VOLTAGE,
                        val.b_bool )) < 0 && val.b_bool )
    {
        msg_Err( p_access,
                 "ioctl FE_ENABLE_HIGH_LNB_VOLTAGE failed, val=%d (%d) %m",
                 val.b_bool, i_err );
    }

    /* Wait for at least 15 ms. */
    msleep(15000);

    var_Get( p_access, "dvb-satno", &val );
    if( val.i_int > 0 && val.i_int < 5 )
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
                          | (((val.i_int - 1) * 4) & 0xc)
                          | (fe_voltage == SEC_VOLTAGE_13 ? 0 : 2)
                          | (fe_tone == SEC_TONE_ON ? 1 : 0);

        if( (i_err = ioctl( p_sys->i_frontend_handle, FE_DISEQC_SEND_MASTER_CMD,
                           &cmd.cmd )) < 0 )
        {
            msg_Err( p_access, "ioctl FE_SEND_MASTER_CMD failed (%d) %m",
                     i_err );
            return i_err;
        }

        msleep(15000 + cmd.wait * 1000);

        /* A or B simple diseqc ("diseqc-compatible") */
        if( (i_err = ioctl( p_sys->i_frontend_handle, FE_DISEQC_SEND_BURST,
                      ((val.i_int - 1) % 2) ? SEC_MINI_B : SEC_MINI_A )) < 0 )
        {
            msg_Err( p_access, "ioctl FE_SEND_BURST failed (%d) %m",
                     i_err );
            return i_err;
        }

        msleep(15000);
    }

    if( (i_err = ioctl( p_sys->i_frontend_handle, FE_SET_TONE, fe_tone )) < 0 )
    {
        msg_Err( p_access, "ioctl FE_SET_TONE failed, tone=%s (%d) %m",
                 fe_tone == SEC_TONE_ON ? "on" : "off", i_err );
        return i_err;
    }

    msleep(50000);
    return 0;
}

static int FrontendSetQPSK( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    struct dvb_frontend_parameters fep;
    int i_ret;
    vlc_value_t val;
    int i_frequency, i_lnb_slof = 0, i_lnb_lof1, i_lnb_lof2 = 0;

    /* Prepare the fep structure */
    var_Get( p_access, "dvb-frequency", &val );
    i_frequency = val.i_int;

    var_Get( p_access, "dvb-lnb-lof1", &val );
    if ( val.i_int == 0 )
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
        val.i_int = i_lnb_lof1;
        var_Set( p_access, "dvb-lnb-lof1", val );
        val.i_int = i_lnb_lof2;
        var_Set( p_access, "dvb-lnb-lof2", val );
        val.i_int = i_lnb_slof;
        var_Set( p_access, "dvb-lnb-slof", val );
    }
    else
    {
        i_lnb_lof1 = val.i_int;
        var_Get( p_access, "dvb-lnb-lof2", &val );
        i_lnb_lof2 = val.i_int;
        var_Get( p_access, "dvb-lnb-slof", &val );
        i_lnb_slof = val.i_int;
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

    var_Get( p_access, "dvb-srate", &val );
    fep.u.qpsk.symbol_rate = val.i_int;

    var_Get( p_access, "dvb-fec", &val );
    fep.u.qpsk.fec_inner = DecodeFEC( p_access, val.i_int );

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
    if( (i_ret = ioctl( p_sys->i_frontend_handle, FE_SET_FRONTEND, &fep )) < 0 )
    {
        msg_Err( p_access, "DVB-S: setting frontend failed (%d) %m", i_ret );
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
    struct dvb_frontend_parameters fep;
    vlc_value_t val;
    int i_ret;

    /* Prepare the fep structure */

    var_Get( p_access, "dvb-frequency", &val );
    fep.frequency = val.i_int;

    fep.inversion = DecodeInversion( p_access );

    var_Get( p_access, "dvb-srate", &val );
    fep.u.qam.symbol_rate = val.i_int;

    var_Get( p_access, "dvb-fec", &val );
    fep.u.qam.fec_inner = DecodeFEC( p_access, val.i_int );

    fep.u.qam.modulation = DecodeModulationQAM( p_access );

    /* Empty the event queue */
    for( ; ; )
    {
        struct dvb_frontend_event event;
        if ( ioctl( p_sys->i_frontend_handle, FE_GET_EVENT, &event ) < 0
              && errno == EWOULDBLOCK )
            break;
    }

    /* Now send it all to the frontend device */
    if( (i_ret = ioctl( p_sys->i_frontend_handle, FE_SET_FRONTEND, &fep )) < 0 )
    {
        msg_Err( p_access, "DVB-C: setting frontend failed (%d): %m", i_ret );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * FrontendSetOFDM : controls the FE device
 *****************************************************************************/
static fe_bandwidth_t DecodeBandwidth( access_t *p_access )
{
    vlc_value_t         val;
    fe_bandwidth_t      fe_bandwidth = 0;

    var_Get( p_access, "dvb-bandwidth", &val );
    msg_Dbg( p_access, "using bandwidth=%d", val.i_int );

    switch( val.i_int )
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
    vlc_value_t         val;
    fe_transmit_mode_t  fe_transmission = 0;

    var_Get( p_access, "dvb-transmission", &val );
    msg_Dbg( p_access, "using transmission=%d", val.i_int );

    switch( val.i_int )
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

static fe_guard_interval_t DecodeGuardInterval( access_t *p_access )
{
    vlc_value_t         val;
    fe_guard_interval_t fe_guard = 0;

    var_Get( p_access, "dvb-guard", &val );
    msg_Dbg( p_access, "using guard=%d", val.i_int );

    switch( val.i_int )
    {
        case 0: fe_guard = GUARD_INTERVAL_AUTO; break;
        case 4: fe_guard = GUARD_INTERVAL_1_4; break;
        case 8: fe_guard = GUARD_INTERVAL_1_8; break;
        case 16: fe_guard = GUARD_INTERVAL_1_16; break;
        case 32: fe_guard = GUARD_INTERVAL_1_32; break;
        default:
            msg_Dbg( p_access, "terrestrial dvb has guard interval not set, using auto");
            fe_guard = GUARD_INTERVAL_AUTO;
            break;
    }
    return fe_guard;
}

static fe_hierarchy_t DecodeHierarchy( access_t *p_access )
{
    vlc_value_t         val;
    fe_hierarchy_t      fe_hierarchy = 0;

    var_Get( p_access, "dvb-hierarchy", &val );
    msg_Dbg( p_access, "using hierarchy=%d", val.i_int );

    switch( val.i_int )
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
    vlc_value_t val;
    int ret;

    /* Prepare the fep structure */

    var_Get( p_access, "dvb-frequency", &val );
    fep.frequency = val.i_int;

    fep.inversion = DecodeInversion( p_access );

    fep.u.ofdm.bandwidth = DecodeBandwidth( p_access );
    var_Get( p_access, "dvb-code-rate-hp", &val );
    fep.u.ofdm.code_rate_HP = DecodeFEC( p_access, val.i_int );
    var_Get( p_access, "dvb-code-rate-lp", &val );
    fep.u.ofdm.code_rate_LP = DecodeFEC( p_access, val.i_int );
    fep.u.ofdm.constellation = DecodeModulationOFDM( p_access );
    fep.u.ofdm.transmission_mode = DecodeTransmission( p_access );
    fep.u.ofdm.guard_interval = DecodeGuardInterval( p_access );
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
    if( (ret = ioctl( p_sys->i_frontend_handle, FE_SET_FRONTEND, &fep )) < 0 )
    {
        msg_Err( p_access, "DVB-T: setting frontend failed (%d): %m", ret );
        return -1;
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
    vlc_value_t val;
    int i_ret;

    /* Prepare the fep structure */

    var_Get( p_access, "dvb-frequency", &val );
    fep.frequency = val.i_int;

    fep.u.vsb.modulation = DecodeModulationATSC( p_access );

    /* Empty the event queue */
    for( ; ; )
    {
        struct dvb_frontend_event event;
        if ( ioctl( p_sys->i_frontend_handle, FE_GET_EVENT, &event ) < 0
              && errno == EWOULDBLOCK )
            break;
    }

    /* Now send it all to the frontend device */
    if( (i_ret = ioctl( p_sys->i_frontend_handle, FE_SET_FRONTEND, &fep )) < 0 )
    {
        msg_Err( p_access, "ATSC: setting frontend failed (%d): %m", i_ret );
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
    int i_ret;
    unsigned int i_adapter, i_device;
    char dmx[128];
    vlc_value_t val;

    var_Get( p_access, "dvb-adapter", &val );
    i_adapter = val.i_int;
    var_Get( p_access, "dvb-device", &val );
    i_device = val.i_int;

    if( snprintf( dmx, sizeof(dmx), DMX, i_adapter, i_device )
            >= (int)sizeof(dmx) )
    {
        msg_Err( p_access, "snprintf() truncated string for DMX" );
        dmx[sizeof(dmx) - 1] = '\0';
    }

    msg_Dbg( p_access, "Opening device %s", dmx );
    if( (*pi_fd = open(dmx, O_RDWR)) < 0 )
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
    if( (i_ret = ioctl( *pi_fd, DMX_SET_PES_FILTER, &s_filter_params )) < 0 )
    {
        msg_Err( p_access, "DMXSetFilter: failed with %d (%m)", i_ret );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DMXUnsetFilter : removes a filter
 *****************************************************************************/
int DMXUnsetFilter( access_t * p_access, int i_fd )
{
    int i_ret;

    if( (i_ret = ioctl( i_fd, DMX_STOP )) < 0 )
    {
        msg_Err( p_access, "DMX_STOP failed for demux (%d): %m", i_ret );
        return i_ret;
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
    vlc_value_t val;

    var_Get( p_access, "dvb-adapter", &val );
    i_adapter = val.i_int;
    var_Get( p_access, "dvb-device", &val );
    i_device = val.i_int;

    if( snprintf( dvr, sizeof(dvr), DVR, i_adapter, i_device )
            >= (int)sizeof(dvr) )
    {
        msg_Err( p_access, "snprintf() truncated string for DVR" );
        dvr[sizeof(dvr) - 1] = '\0';
    }

    msg_Dbg( p_access, "Opening device %s", dvr );
    if( (p_sys->i_handle = open(dvr, O_RDONLY)) < 0 )
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


/*
 * CAM device
 */

/*****************************************************************************
 * CAMOpen :
 *****************************************************************************/
int CAMOpen( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    char ca[128];
    int i_adapter, i_device;
    ca_caps_t caps;

    i_adapter = var_GetInteger( p_access, "dvb-adapter" );
    i_device = var_GetInteger( p_access, "dvb-device" );

    if( snprintf( ca, sizeof(ca), CA, i_adapter, i_device ) >= (int)sizeof(ca) )
    {
        msg_Err( p_access, "snprintf() truncated string for CA" );
        ca[sizeof(ca) - 1] = '\0';
    }
    memset( &caps, 0, sizeof( ca_caps_t ));

    msg_Dbg( p_access, "Opening device %s", ca );
    if( (p_sys->i_ca_handle = open(ca, O_RDWR | O_NONBLOCK)) < 0 )
    {
        msg_Warn( p_access, "CAMInit: opening CAM device failed (%m)" );
        p_sys->i_ca_handle = 0;
        return VLC_EGENERIC;
    }

    if ( ioctl( p_sys->i_ca_handle, CA_GET_CAP, &caps ) != 0 )
    {
        msg_Err( p_access, "CAMInit: ioctl() error getting CAM capabilities" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return VLC_EGENERIC;
    }

    /* Output CA capabilities */
    msg_Dbg( p_access, "CAMInit: CA interface with %d %s", caps.slot_num,
        caps.slot_num == 1 ? "slot" : "slots" );
    if ( caps.slot_type & CA_CI )
        msg_Dbg( p_access, "CAMInit: CI high level interface type" );
    if ( caps.slot_type & CA_CI_LINK )
        msg_Dbg( p_access, "CAMInit: CI link layer level interface type" );
    if ( caps.slot_type & CA_CI_PHYS )
        msg_Dbg( p_access, "CAMInit: CI physical layer level interface type (not supported) " );
    if ( caps.slot_type & CA_DESCR )
        msg_Dbg( p_access, "CAMInit: built-in descrambler detected" );
    if ( caps.slot_type & CA_SC )
        msg_Dbg( p_access, "CAMInit: simple smart card interface" );

    msg_Dbg( p_access, "CAMInit: %d available %s", caps.descr_num,
        caps.descr_num == 1 ? "descrambler (key)" : "descramblers (keys)" );
    if ( caps.descr_type & CA_ECD )
        msg_Dbg( p_access, "CAMInit: ECD scrambling system supported" );
    if ( caps.descr_type & CA_NDS )
        msg_Dbg( p_access, "CAMInit: NDS scrambling system supported" );
    if ( caps.descr_type & CA_DSS )
        msg_Dbg( p_access, "CAMInit: DSS scrambling system supported" );
 
    if ( caps.slot_num == 0 )
    {
        msg_Err( p_access, "CAMInit: CAM module with no slots" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return VLC_EGENERIC;
    }

    if( caps.slot_type & CA_CI_LINK )
    {
        p_sys->i_ca_type = CA_CI_LINK;
    }
    else if( caps.slot_type & CA_CI )
    {
        p_sys->i_ca_type = CA_CI;
    }
    else
    {
        p_sys->i_ca_type = -1;
        msg_Err( p_access, "CAMInit: incompatible CAM interface" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return VLC_EGENERIC;
    }

    p_sys->i_nb_slots = caps.slot_num;
    memset( p_sys->pb_active_slot, 0, sizeof(bool) * MAX_CI_SLOTS );
    memset( p_sys->pb_slot_mmi_expected, 0, sizeof(bool) * MAX_CI_SLOTS );
    memset( p_sys->pb_slot_mmi_undisplayed, 0,
            sizeof(bool) * MAX_CI_SLOTS );

    return en50221_Init( p_access );
}

/*****************************************************************************
 * CAMPoll :
 *****************************************************************************/
int CAMPoll( access_t * p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret = VLC_EGENERIC;

    if ( p_sys->i_ca_handle == 0 )
    {
        return VLC_EGENERIC;
    }

    switch( p_sys->i_ca_type )
    {
    case CA_CI_LINK:
        i_ret = en50221_Poll( p_access );
        break;
    case CA_CI:
        i_ret = VLC_SUCCESS;
        break;
    default:
        msg_Err( p_access, "CAMPoll: This should not happen" );
        break;
    }

    return i_ret;
}

#ifdef ENABLE_HTTPD
/*****************************************************************************
 * CAMStatus :
 *****************************************************************************/
void CAMStatus( access_t * p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    char *p;
    ca_caps_t caps;
    int i_slot, i;

    if ( p_sys->psz_request != NULL && *p_sys->psz_request )
    {
        /* Check if we have an undisplayed MMI message : in that case we ignore
         * the user input to avoid confusing the CAM. */
        for ( i_slot = 0; i_slot < p_sys->i_nb_slots; i_slot++ )
        {
            if ( p_sys->pb_slot_mmi_undisplayed[i_slot] == true )
            {
                p_sys->psz_request = NULL;
                msg_Dbg( p_access,
                         "ignoring user request because of a new MMI object" );
                break;
            }
        }
    }

    if ( p_sys->psz_request != NULL && *p_sys->psz_request )
    {
        /* We have a mission to accomplish. */
        en50221_mmi_object_t mmi_object;
        char *psz_request = p_sys->psz_request;
        char psz_value[255];
        int i_slot;
        bool b_ok = false;

        p_sys->psz_request = NULL;

        if ( HTTPExtractValue( psz_request, "slot", psz_value,
                                   sizeof(psz_value) ) == NULL )
        {
            p_sys->psz_mmi_info = strdup( "invalid request parameter\n" );
            goto out;
        }
        i_slot = atoi(psz_value);

        if ( HTTPExtractValue( psz_request, "open", psz_value,
                                   sizeof(psz_value) ) != NULL )
        {
            en50221_OpenMMI( p_access, i_slot );
            return;
        }

        if ( HTTPExtractValue( psz_request, "close", psz_value,
                                   sizeof(psz_value) ) != NULL )
        {
            en50221_CloseMMI( p_access, i_slot );
            return;
        }

        if ( HTTPExtractValue( psz_request, "cancel", psz_value,
                                   sizeof(psz_value) ) == NULL )
        {
            b_ok = true;
        }

        if ( HTTPExtractValue( psz_request, "type", psz_value,
                                   sizeof(psz_value) ) == NULL )
        {
            p_sys->psz_mmi_info = strdup( "invalid request parameter\n" );
            goto out;
        }

        if ( !strcmp( psz_value, "enq" ) )
        {
            mmi_object.i_object_type = EN50221_MMI_ANSW;
            mmi_object.u.answ.b_ok = b_ok;
            if ( b_ok == false )
            {
                mmi_object.u.answ.psz_answ = strdup("");
            }
            else
            {
                if ( HTTPExtractValue( psz_request, "answ", psz_value,
                                           sizeof(psz_value) ) == NULL )
                {
                    p_sys->psz_mmi_info = strdup( "invalid request parameter\n" );
                    goto out;
                }

                mmi_object.u.answ.psz_answ = strdup(psz_value);
            }
        }
        else
        {
            mmi_object.i_object_type = EN50221_MMI_MENU_ANSW;
            if ( b_ok == false )
            {
                mmi_object.u.menu_answ.i_choice = 0;
            }
            else
            {
                if ( HTTPExtractValue( psz_request, "choice", psz_value,
                                           sizeof(psz_value) ) == NULL )
                    mmi_object.u.menu_answ.i_choice = 0;
                else
                    mmi_object.u.menu_answ.i_choice = atoi(psz_value);
            }
        }

        en50221_SendMMIObject( p_access, i_slot, &mmi_object );
        return;
    }

    /* Check that we have all necessary MMI information. */
    for ( i_slot = 0; i_slot < p_sys->i_nb_slots; i_slot++ )
    {
        if ( p_sys->pb_slot_mmi_expected[i_slot] == true )
            return;
    }

    p = p_sys->psz_mmi_info = malloc( 10000 );

    if ( ioctl( p_sys->i_ca_handle, CA_GET_CAP, &caps ) != 0 )
    {
        char buf[1000];
        strerror_r( errno, buf, sizeof( buf ) );
        p += sprintf( p, "ioctl CA_GET_CAP failed (%s)\n", buf );
        goto out;
    }

    /* Output CA capabilities */
    p += sprintf( p, "CA interface with %d %s, type:\n<table>", caps.slot_num,
                  caps.slot_num == 1 ? "slot" : "slots" );
#define CHECK_CAPS( x, s )                                                  \
    if ( caps.slot_type & (CA_##x) )                                        \
        p += sprintf( p, "<tr><td>" s "</td></tr>\n" );

    CHECK_CAPS( CI, "CI high level interface" );
    CHECK_CAPS( CI_LINK, "CI link layer level interface" );
    CHECK_CAPS( CI_PHYS, "CI physical layer level interface (not supported)" );
    CHECK_CAPS( DESCR, "built-in descrambler" );
    CHECK_CAPS( SC, "simple smartcard interface" );
#undef CHECK_CAPS

    p += sprintf( p, "</table>%d available %s\n<table>", caps.descr_num,
        caps.descr_num == 1 ? "descrambler (key)" : "descramblers (keys)" );
#define CHECK_DESC( x )                                                     \
    if ( caps.descr_type & (CA_##x) )                                       \
        p += sprintf( p, "<tr><td>" STRINGIFY(x) "</td></tr>\n" );

    CHECK_DESC( ECD );
    CHECK_DESC( NDS );
    CHECK_DESC( DSS );
#undef CHECK_DESC

    p += sprintf( p, "</table>" );

    for ( i_slot = 0; i_slot < p_sys->i_nb_slots; i_slot++ )
    {
        ca_slot_info_t sinfo;

        p_sys->pb_slot_mmi_undisplayed[i_slot] = false;
        p += sprintf( p, "<p>CA slot #%d: ", i_slot );

        sinfo.num = i_slot;
        if ( ioctl( p_sys->i_ca_handle, CA_GET_SLOT_INFO, &sinfo ) != 0 )
        {
            char buf[1000];
            strerror_r( errno, buf, sizeof( buf ) );
            p += sprintf( p, "ioctl CA_GET_SLOT_INFO failed (%s)<br>\n", buf );
            continue;
        }

#define CHECK_TYPE( x, s )                                                  \
        if ( sinfo.type & (CA_##x) )                                        \
            p += sprintf( p, "%s", s );

        CHECK_TYPE( CI, "high level, " );
        CHECK_TYPE( CI_LINK, "link layer level, " );
        CHECK_TYPE( CI_PHYS, "physical layer level, " );
#undef CHECK_TYPE

        if ( sinfo.flags & CA_CI_MODULE_READY )
        {
            en50221_mmi_object_t *p_object = en50221_GetMMIObject( p_access,
                                                                       i_slot );

            p += sprintf( p, "module present and ready<p>\n" );
            p += sprintf( p, "<form action=index.html method=get>\n" );
            p += sprintf( p, "<input type=hidden name=slot value=\"%d\">\n",
                          i_slot );

            if ( p_object == NULL )
            {
                p += sprintf( p, "<input type=submit name=open value=\"Open session\">\n" );
            }
            else
            {
                switch ( p_object->i_object_type )
                {
                case EN50221_MMI_ENQ:
                    p += sprintf( p, "<input type=hidden name=type value=enq>\n" );
                    p += sprintf( p, "<table border=1><tr><th>%s</th></tr>\n",
                                  p_object->u.enq.psz_text );
                    if ( p_object->u.enq.b_blind == false )
                        p += sprintf( p, "<tr><td><input type=text name=answ></td></tr>\n" );
                    else
                        p += sprintf( p, "<tr><td><input type=password name=answ></td></tr>\n" );
                    break;

                case EN50221_MMI_MENU:
                    p += sprintf( p, "<input type=hidden name=type value=menu>\n" );
                    p += sprintf( p, "<table border=1><tr><th>%s</th></tr>\n",
                                  p_object->u.menu.psz_title );
                    p += sprintf( p, "<tr><td>%s</td></tr><tr><td>\n",
                                  p_object->u.menu.psz_subtitle );
                    for ( i = 0; i < p_object->u.menu.i_choices; i++ )
                        p += sprintf( p, "<input type=radio name=choice value=\"%d\">%s<br>\n", i + 1, p_object->u.menu.ppsz_choices[i] );
                    p += sprintf( p, "</td></tr><tr><td>%s</td></tr>\n",
                                  p_object->u.menu.psz_bottom );
                    break;

                case EN50221_MMI_LIST:
                    p += sprintf( p, "<input type=hidden name=type value=menu>\n" );
                    p += sprintf( p, "<input type=hidden name=choice value=0>\n" );
                    p += sprintf( p, "<table border=1><tr><th>%s</th></tr>\n",
                                  p_object->u.menu.psz_title );
                    p += sprintf( p, "<tr><td>%s</td></tr><tr><td>\n",
                                  p_object->u.menu.psz_subtitle );
                    for ( i = 0; i < p_object->u.menu.i_choices; i++ )
                        p += sprintf( p, "%s<br>\n",
                                      p_object->u.menu.ppsz_choices[i] );
                    p += sprintf( p, "</td></tr><tr><td>%s</td></tr>\n",
                                  p_object->u.menu.psz_bottom );
                    break;

                default:
                    p += sprintf( p, "<table><tr><th>Unknown MMI object type</th></tr>\n" );
                }

                p += sprintf( p, "</table><p><input type=submit name=ok value=\"OK\">\n" );
                p += sprintf( p, "<input type=submit name=cancel value=\"Cancel\">\n" );
                p += sprintf( p, "<input type=submit name=close value=\"Close Session\">\n" );
            }
            p += sprintf( p, "</form>\n" );
        }
        else if ( sinfo.flags & CA_CI_MODULE_PRESENT )
            p += sprintf( p, "module present, not ready<br>\n" );
        else
            p += sprintf( p, "module not present<br>\n" );
    }

out:
    vlc_mutex_lock( &p_sys->httpd_mutex );
    p_sys->b_request_mmi_info = false;
    vlc_cond_signal( &p_sys->httpd_cond );
    vlc_mutex_unlock( &p_sys->httpd_mutex );
}
#endif

/*****************************************************************************
 * CAMSet :
 *****************************************************************************/
int CAMSet( access_t * p_access, dvbpsi_pmt_t *p_pmt )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->i_ca_handle == 0 )
    {
        dvbpsi_DeletePMT( p_pmt );
        return VLC_EGENERIC;
    }

    en50221_SetCAPMT( p_access, p_pmt );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CAMClose :
 *****************************************************************************/
void CAMClose( access_t * p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    en50221_End( p_access );

    if ( p_sys->i_ca_handle )
    {
        close( p_sys->i_ca_handle );
    }
}

