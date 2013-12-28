/*****************************************************************************
 * access.c: DVB card input v4l2 only
 *****************************************************************************
 * Copyright (C) 1998-2010 VLC authors and VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          David Kaplan <david@2of1.org>
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


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_input.h>

#include <sys/types.h>
#include <poll.h>

#include <errno.h>

#include "dvb.h"
#include "scan.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );

#define PROBE_TEXT N_("Probe DVB card for capabilities")
#define PROBE_LONGTEXT N_("Some DVB cards do not like to be probed for their capabilities, you can disable this feature if you experience some trouble.")

/* Satellite */
#define SATELLITE_TEXT N_("Satellite scanning config")
#define SATELLITE_LONGTEXT N_("filename of config file in share/dvb/dvb-s")

vlc_module_begin ()
    set_shortname( N_("DVB") )
    set_description( N_("DVB input with v4l2 support") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_bool( "dvb-probe", true, PROBE_TEXT, PROBE_LONGTEXT, true )
    /* DVB-S (satellite) */
    add_string( "dvb-satellite", NULL, SATELLITE_TEXT, SATELLITE_LONGTEXT,
                true )

    set_capability( "access", 0 )
    add_shortcut( "dvb",                        /* Generic name */
                  "dvb-s", "qpsk", "satellite", /* Satellite */
                  "dvb-c", "cable",             /* Cable */
                  "dvb-t", "terrestrial" )      /* Terrestrial */

    set_callbacks( Open, Close )

vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control( access_t *, int, va_list );

static block_t *BlockScan( access_t * );

#define DVB_READ_ONCE 20
#define DVB_READ_ONCE_START 2
#define DVB_READ_ONCE_SCAN 1
#define TS_PACKET_SIZE 188

#define DVB_SCAN_MAX_SIGNAL_TIME (1000*1000)
#define DVB_SCAN_MAX_LOCK_TIME (5000*1000)
#define DVB_SCAN_MAX_PROBE_TIME (45000*1000)

static void FilterUnset( access_t *, int i_max );
static void FilterSet( access_t *, int i_pid, int i_type );

static void VarInit( access_t * );
static int  ParseMRL( access_t * );

/*****************************************************************************
 * Open: open the frontend device
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;

    /* Only if selected */
    if( *p_access->psz_access == '\0' )
        return VLC_EGENERIC;

    p_access->p_sys = p_sys = calloc( 1, sizeof( access_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Create all variables */
    VarInit( p_access );

    /* Parse the command line */
    if( ParseMRL( p_access ) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    bool b_scan_mode = var_GetInteger( p_access, "dvb-frequency" ) == 0;
    if( b_scan_mode )
    {
        msg_Dbg( p_access, "DVB scan mode selected" );
        p_access->pf_block = BlockScan;
    }
    else
        return VLC_EGENERIC; /* let the DTV plugin do the work */

    /* Getting frontend info */
    if( FrontendOpen( p_access) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Opening DVR device */
    if( DVROpen( p_access ) < 0 )
    {
        FrontendClose( p_access );
        free( p_sys );
        return VLC_EGENERIC;
    }

    {
        scan_parameter_t parameter;
        scan_t *p_scan;

        msg_Dbg( p_access, "setting filter on PAT/NIT/SDT (DVB only)" );
        FilterSet( p_access, 0x00, OTHER_TYPE );    // PAT
        FilterSet( p_access, 0x10, OTHER_TYPE );    // NIT
        FilterSet( p_access, 0x11, OTHER_TYPE );    // SDT

        if( FrontendGetScanParameter( p_access, &parameter ) ||
            (p_scan = scan_New( VLC_OBJECT(p_access), &parameter )) == NULL )
        {
            Close( VLC_OBJECT(p_access) );
            return VLC_EGENERIC;
        }
        p_sys->scan = p_scan;
        p_sys->i_read_once = DVB_READ_ONCE_SCAN;
    }

    /* Set up access */
    p_access->pf_read = NULL;
    p_access->pf_control = Control;
    p_access->pf_seek = NULL;
    access_InitFields( p_access );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close : Close the device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    FilterUnset( p_access, MAX_DEMUX );

    DVRClose( p_access );
    FrontendClose( p_access );
    scan_Destroy( p_sys->scan );

    free( p_sys );
}

/*****************************************************************************
 * BlockScan:
 *****************************************************************************/
static block_t *BlockScan( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    scan_t *p_scan = p_sys->scan;
    scan_configuration_t cfg;

    /* */
    if( scan_Next( p_scan, &cfg ) )
    {
        const bool b_first_eof = !p_access->info.b_eof;

        if( b_first_eof )
            msg_Warn( p_access, "Scanning finished" );

        /* */
        p_access->info.b_eof = true;
        return b_first_eof ? scan_GetM3U( p_scan ) : NULL;
    }

    /* */
    scan_session_t *session = scan_session_New( VLC_OBJECT(p_access), &cfg );
    if( session == NULL )
        return NULL;

    /* */
    msg_Dbg( p_access, "Scanning frequency %d", cfg.i_frequency );
    var_SetInteger( p_access, "dvb-frequency", cfg.i_frequency );
    msg_Dbg( p_access, " bandwidth %d", cfg.i_bandwidth );
    var_SetInteger( p_access, "dvb-bandwidth", cfg.i_bandwidth );
    if ( cfg.c_polarization )
        var_SetInteger( p_access, "dvb-voltage", cfg.c_polarization == 'H' ? 18 : 13 );

    if ( cfg.i_symbolrate )
        var_SetInteger( p_access, "dvb-srate", cfg.i_symbolrate );

    /* Setting frontend parameters for tuning the hardware */
    if( FrontendSet( p_access ) < 0 )
    {
        msg_Err( p_access, "Failed to tune the frontend" );
        p_access->info.b_eof = true;
        scan_session_Destroy( p_scan, session );
        return NULL;
    }

    /* */
    int64_t i_scan_start = mdate();

    bool b_has_dvb_signal = false;
    bool b_has_lock = false;
    int i_best_snr = -1;

    for ( ; ; )
    {
        struct pollfd ufds[2];
        int i_ret;

        /* Initialize file descriptor sets */
        memset (ufds, 0, sizeof (ufds));
        ufds[0].fd = p_sys->i_handle;
        ufds[0].events = POLLIN;
        ufds[1].fd = p_sys->i_frontend_handle;
        ufds[1].events = POLLPRI;

        /* We'll wait 0.1 second if nothing happens */
        /* Find if some data is available */
        i_ret = poll( ufds, 2, 100 );

        if( !vlc_object_alive (p_access) || scan_IsCancelled( p_scan ) )
            break;

        if( i_ret <= 0 )
        {
            const mtime_t i_scan_time = mdate() - i_scan_start;
            frontend_status_t status;

            FrontendGetStatus( p_access, &status );

            b_has_dvb_signal |= status.b_has_carrier;
            b_has_lock |= status.b_has_lock;

            if( ( !b_has_dvb_signal && i_scan_time > DVB_SCAN_MAX_SIGNAL_TIME ) ||
                ( !b_has_lock && i_scan_time > DVB_SCAN_MAX_LOCK_TIME ) ||
                ( i_scan_time > DVB_SCAN_MAX_PROBE_TIME ) )
            {
                msg_Dbg( p_access, "timed out scanning current frequency (s=%d l=%d)", b_has_dvb_signal, b_has_lock );
                break;
            }
        }

        if( i_ret < 0 )
        {
            if( errno == EINTR )
                continue;

            msg_Err( p_access, "poll error: %s", vlc_strerror_c(errno) );
            scan_session_Destroy( p_scan, session );

            p_access->info.b_eof = true;
            return NULL;
        }

        if( ufds[1].revents )
        {
            frontend_statistic_t stat;

            FrontendPoll( p_access );

            if( !FrontendGetStatistic( p_access, &stat ) )
            {
                if( stat.i_snr > i_best_snr )
                    i_best_snr = stat.i_snr;
            }
        }

        if ( p_sys->i_frontend_timeout && mdate() > p_sys->i_frontend_timeout )
        {
            msg_Warn( p_access, "no lock, tuning again" );
            FrontendSet( p_access );
        }

        if ( ufds[0].revents )
        {
            const int i_read_once = 1;
            block_t *p_block = block_Alloc( i_read_once * TS_PACKET_SIZE );

            if( ( i_ret = read( p_sys->i_handle, p_block->p_buffer,
                                i_read_once * TS_PACKET_SIZE ) ) <= 0 )
            {
                msg_Warn( p_access, "read failed: %s", vlc_strerror_c(errno) );
                block_Release( p_block );
                continue;
            }
            p_block->i_buffer = i_ret;

            /* */
            if( scan_session_Push( session, p_block ) )
            {
                msg_Dbg( p_access, "finished scanning current frequency" );
                break;
            }
        }
    }

    /* */
    if( i_best_snr > 0 )
        scan_service_SetSNR( session, i_best_snr );

    scan_session_Destroy( p_scan, session );
    return NULL;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    bool         *pb_bool;
    int64_t      *pi_64;
    double       *pf1, *pf2;
    frontend_statistic_t stat;

    switch( i_query )
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;

        case ACCESS_GET_CONTENT_TYPE:
            *va_arg( args, char** ) = strdup("application/vnd.apple.mpegurl"); // m3u8
            return VLC_SUCCESS;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = DEFAULT_PTS_DELAY;
            break;

        case ACCESS_GET_SIGNAL:
            pf1 = (double*)va_arg( args, double * );
            pf2 = (double*)va_arg( args, double * );

            *pf1 = *pf2 = 0;
            if( !FrontendGetStatistic( p_access, &stat ) )
            {
                *pf1 = (double)stat.i_snr / 65535.0;
                *pf2 = (double)stat.i_signal_strenth / 65535.0;
            }
            break;

        default:
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * FilterSet/FilterUnset:
 *****************************************************************************/
static void FilterSet( access_t *p_access, int i_pid, int i_type )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i;

    /* Find first free slot */
    for( i = 0; i < MAX_DEMUX; i++ )
    {
        if( !p_sys->p_demux_handles[i].i_type )
            break;

        if( p_sys->p_demux_handles[i].i_pid == i_pid )
            return; /* Already set */
    }

    if( i >= MAX_DEMUX )
    {
        msg_Err( p_access, "no free p_demux_handles !" );
        return;
    }

    if( DMXSetFilter( p_access, i_pid,
                           &p_sys->p_demux_handles[i].i_handle, i_type ) )
    {
        msg_Err( p_access, "DMXSetFilter failed" );
        return;
    }
    p_sys->p_demux_handles[i].i_type = i_type;
    p_sys->p_demux_handles[i].i_pid = i_pid;

    if( p_sys->i_read_once < DVB_READ_ONCE )
        p_sys->i_read_once++;
}

static void FilterUnset( access_t *p_access, int i_max )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i;

    for( i = 0; i < i_max; i++ )
    {
        if( p_sys->p_demux_handles[i].i_type )
        {
            DMXUnsetFilter( p_access, p_sys->p_demux_handles[i].i_handle );
            p_sys->p_demux_handles[i].i_type = 0;
        }
    }
}

/*****************************************************************************
 * VarInit/ParseMRL:
 *****************************************************************************/
static void VarInit( access_t *p_access )
{
    var_Create( p_access, "dvb-adapter", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-device", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-frequency", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-inversion", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-probe", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* */
    var_Create( p_access, "dvb-satellite", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-satno", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-voltage", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-high-voltage", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-tone", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-srate", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-lnb-lof1", VLC_VAR_INTEGER );
    var_Create( p_access, "dvb-lnb-lof2", VLC_VAR_INTEGER );
    var_Create( p_access, "dvb-lnb-slof", VLC_VAR_INTEGER );

    /* */
    var_Create( p_access, "dvb-bandwidth", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-transmission", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_access, "dvb-hierarchy", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
}

/* */
static int ParseMRL( access_t *p_access )
{
    char *psz_dup = strdup( p_access->psz_location );
    char *psz_parser = psz_dup;
    vlc_value_t         val;

#define GET_OPTION_INT( option )                                            \
    if ( !strncmp( psz_parser, option "=", strlen(option "=") ) )           \
    {                                                                       \
        val.i_int = strtol( psz_parser + strlen(option "="), &psz_parser,   \
                            0 );                                            \
        var_Set( p_access, "dvb-" option, val );                            \
    }

#define GET_OPTION_BOOL( option )                                           \
    if ( !strncmp( psz_parser, option "=", strlen(option "=") ) )           \
    {                                                                       \
        val.b_bool = strtol( psz_parser + strlen(option "="), &psz_parser,  \
                             0 );                                           \
        var_Set( p_access, "dvb-" option, val );                            \
    }

#define GET_OPTION_STRING( option )                                         \
    if ( !strncmp( psz_parser, option "=", strlen( option "=" ) ) )         \
    {                                                                       \
        psz_parser += strlen( option "=" );                                 \
        val.psz_string = psz_parser;                                        \
        char *p_save;                                                       \
        char *tok = strtok_r(val.psz_string, ":", &p_save);                 \
        val.psz_string[tok - val.psz_string - 1] = 0;                       \
        var_Set( p_access, "dvb-" option, val );                            \
        psz_parser += strlen( val.psz_string );                             \
    }

    while( *psz_parser )
    {
        GET_OPTION_INT("adapter")
        else GET_OPTION_INT("device")
        else GET_OPTION_INT("frequency")
        else GET_OPTION_INT("inversion")
        else GET_OPTION_BOOL("probe")
        else GET_OPTION_BOOL("budget-mode")

        else GET_OPTION_STRING("satellite")
        else GET_OPTION_INT("voltage")
        else GET_OPTION_BOOL("high-voltage")
        else GET_OPTION_INT("tone")
        else GET_OPTION_INT("satno")
        else GET_OPTION_INT("srate")
        else GET_OPTION_INT("lnb-lof1")
        else GET_OPTION_INT("lnb-lof2")
        else GET_OPTION_INT("lnb-slof")

        else GET_OPTION_INT("bandwidth")
        else GET_OPTION_INT("transmission")
        else GET_OPTION_INT("hierarchy")

        /* Redundant with voltage but much easier to use */
        else if( !strncmp( psz_parser, "polarization=",
                           strlen( "polarization=" ) ) )
        {
            psz_parser += strlen( "polarization=" );
            if ( *psz_parser == 'V' || *psz_parser == 'v' )
                val.i_int = 13;
            else if ( *psz_parser == 'H' || *psz_parser == 'h' )
                val.i_int = 18;
            else
            {
                msg_Err( p_access, "illegal polarization %c", *psz_parser );
                free( psz_dup );
                return VLC_EGENERIC;
            }
            var_Set( p_access, "dvb-voltage", val );
        }
        else
        {
            msg_Err( p_access, "unknown option (%s)", psz_parser );
            free( psz_dup );
            return VLC_EGENERIC;
        }

        if ( *psz_parser )
            psz_parser++;
    }
#undef GET_OPTION_INT
#undef GET_OPTION_BOOL
#undef GET_OPTION_STRING

    free( psz_dup );
    return VLC_SUCCESS;
}

