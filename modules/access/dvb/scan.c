/*****************************************************************************
 * scan.c: DVB scanner helpers
 *****************************************************************************
 * Copyright (C) 2008,2010 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          David Kaplan <david@2of1.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
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
#include <vlc_block.h>
#include <vlc_dialog.h>
#include <vlc_charset.h>

#include <sys/types.h>

/* Include dvbpsi headers */
#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/descriptor.h>
#include <dvbpsi/pat.h>
#include <dvbpsi/dr.h>
#include <dvbpsi/demux.h>
#include <dvbpsi/sdt.h>
#include <dvbpsi/nit.h>

#include "dvb.h"
#include "scan.h"
#include "scan_list.h"
#include "../../demux/dvb-text.h"
#include "../../mux/mpeg/dvbpsi_compat.h"

#define PSI_PAT_PID 0x00
#define SI_NIT_PID  0x10
#define SI_SDT_PID  0x11

#define NIT_CURRENT_NETWORK_TABLE_ID    0x40
#define NIT_OTHER_NETWORK_TABLE_ID      0x41
#define SDT_CURRENT_TS_TABLE_ID         0x42
#define SDT_OTHER_TS_TABLE_ID           0x46

#define NETWORK_ID_RESERVED             0x0000

typedef enum
{
    SERVICE_TYPE_RESERVED                        = 0x00,
    SERVICE_TYPE_DIGITAL_TELEVISION              = 0x01,
    SERVICE_TYPE_DIGITAL_RADIO                   = 0x02,
    SERVICE_TYPE_DIGITAL_MPEG2_HD                = 0x11,
    SERVICE_TYPE_DIGITAL_TELEVISION_AC_SD        = 0x16,
    SERVICE_TYPE_DIGITAL_TELEVISION_AC_HD        = 0x19,
    SERVICE_TYPE_DIGITAL_RADIO_AC                = 0x0A,
} scan_service_type_t;

typedef struct scan_multiplex_t scan_multiplex_t;

struct scan_service_t
{
    const scan_multiplex_t *p_mplex; /* multiplex reference */
    const void * stickyref; /* Callee private storage across updates */

    uint16_t i_original_network_id;
    uint16_t i_program;     /* program number (service id) */

    scan_service_type_t type;

    char *psz_name;     /* channel name in utf8 */
    char *psz_provider; /* service provider */
    uint16_t i_channel; /* logical channel number */
    bool b_crypted;     /* True if potentially crypted */

    char *psz_original_network_name;

};

struct scan_multiplex_t
{
    scan_tuner_config_t cfg;
    uint16_t         i_network_id;
    uint16_t         i_ts_id;
    char            *psz_network_name;
    size_t           i_services;
    scan_service_t **pp_services;
    int i_snr;
    bool b_scanned;

    uint8_t i_nit_version;
    uint8_t i_sdt_version;
};

typedef struct
{
    scan_modulation_t modulation;
    unsigned i_symbolrate_index;
    unsigned i_index;
} scan_enumeration_t;

struct scan_t
{
    vlc_object_t *p_obj;
    scan_frontend_tune_cb pf_tune;
    scan_demux_filter_cb pf_filter;
    scan_frontend_stats_cb pf_stats;
    scan_demux_read_cb   pf_read;
    scan_service_notify_cb pf_notify_service;
    void *p_cbdata;

    vlc_dialog_id *p_dialog_id;

    scan_parameter_t parameter;
    int64_t i_time_start;

    size_t i_multiplex_toscan;

    size_t             i_multiplex;
    scan_multiplex_t **pp_multiplex;
    bool               b_multiplexes_from_nit;

    scan_list_entry_t *p_scanlist;
    size_t             i_scanlist;
    const scan_list_entry_t *p_current;

    scan_enumeration_t spectrum;
};

typedef struct
{
    vlc_object_t *p_obj;

    scan_tuner_config_t cfg;
    int i_snr;

    struct
    {
        dvbpsi_pat_t *p_pat;
        dvbpsi_sdt_t *p_sdt;
        dvbpsi_nit_t *p_nit;
    } local;

    struct
    {
        dvbpsi_sdt_t **pp_sdt;
        size_t i_sdt;
        dvbpsi_nit_t **pp_nit;
        size_t i_nit;
    } others;

    scan_type_t type;
    bool b_use_nit;
    uint16_t i_nit_pid;

    dvbpsi_t *p_pathandle;
    dvbpsi_t *p_sdthandle;
    dvbpsi_t *p_nithandle;
} scan_session_t;

static scan_session_t * scan_session_New( scan_t *p_scan, const scan_tuner_config_t *p_cfg );
static void scan_session_Destroy( scan_t *p_scan, scan_session_t *p_session );
static bool scan_session_Push( scan_session_t *p_scan, const uint8_t *p_packet );
static unsigned scan_session_GetTablesTimeout( const scan_session_t *p_session );

/* */
static void scan_tuner_config_Init( scan_tuner_config_t *p_cfg, const scan_parameter_t *p_params )
{
    memset( p_cfg, 0, sizeof(*p_cfg) );
    p_cfg->coderate_lp = SCAN_CODERATE_AUTO;
    p_cfg->coderate_hp = SCAN_CODERATE_AUTO;
    p_cfg->inner_fec = SCAN_CODERATE_AUTO;
    switch(p_params->type)
    {
        case SCAN_DVB_T: p_cfg->delivery = SCAN_DELIVERY_DVB_T; break;
        case SCAN_DVB_S: p_cfg->delivery = SCAN_DELIVERY_DVB_S; break;
        case SCAN_DVB_C: p_cfg->delivery = SCAN_DELIVERY_DVB_C; break;
        default: p_cfg->delivery = SCAN_DELIVERY_UNKNOWN; break;
    }
    p_cfg->type = p_params->type;
}

static bool scan_tuner_config_StandardValidate( const scan_tuner_config_t *p_cfg )
{
    if( p_cfg->i_frequency == 0 ||
        p_cfg->i_frequency == UINT32_MAX / 10 ) /* Invalid / broken transponder info on French TNT */
        return false;

    if( p_cfg->type == SCAN_DVB_T && p_cfg->i_bandwidth == 0 )
        return false;

    return true;
}

static scan_service_t *scan_service_New( uint16_t i_program )
{
    scan_service_t *p_srv = malloc( sizeof(*p_srv) );
    if( !p_srv )
        return NULL;

    p_srv->p_mplex = NULL;
    p_srv->stickyref = NULL;
    p_srv->i_program = i_program;
    p_srv->i_original_network_id = NETWORK_ID_RESERVED;

    p_srv->type = SERVICE_TYPE_RESERVED;
    p_srv->psz_name = NULL;
    p_srv->psz_provider = NULL;
    p_srv->psz_original_network_name = NULL;
    p_srv->i_channel = -1;
    p_srv->b_crypted = false;

    return p_srv;
}

static void scan_service_Delete( scan_service_t *p_srv )
{
    free( p_srv->psz_original_network_name );
    free( p_srv->psz_name );
    free( p_srv->psz_provider );
    free( p_srv );
}

static uint32_t decode_BCD( uint32_t input )
{
    uint32_t output = 0;
    for( short index=28; index >= 0 ; index -= 4 )
    {
        output *= 10;
        output += ((input >> index) & 0x0f);
    };
    return output;
}

static int scan_service_type_Supported( scan_service_type_t service_type )
{
    switch( service_type )
    {
        case SERVICE_TYPE_DIGITAL_TELEVISION:
        case SERVICE_TYPE_DIGITAL_RADIO:
        case SERVICE_TYPE_DIGITAL_MPEG2_HD:
        case SERVICE_TYPE_DIGITAL_TELEVISION_AC_SD:
        case SERVICE_TYPE_DIGITAL_TELEVISION_AC_HD:
        case SERVICE_TYPE_DIGITAL_RADIO_AC:
            return true;
        default:
            break;
    }
    return false;
}

static scan_multiplex_t *scan_multiplex_New( const scan_tuner_config_t *p_cfg, uint16_t i_ts_id )
{
    scan_multiplex_t *p_mplex = malloc( sizeof(*p_mplex) );
    if( likely(p_mplex) )
    {
        p_mplex->cfg = *p_cfg;
        p_mplex->i_ts_id = i_ts_id;
        p_mplex->i_network_id = NETWORK_ID_RESERVED;
        p_mplex->psz_network_name = NULL;
        p_mplex->i_services = 0;
        p_mplex->pp_services = NULL;
        p_mplex->i_nit_version = UINT8_MAX;
        p_mplex->i_sdt_version = UINT8_MAX;
        p_mplex->i_snr = -1;
        p_mplex->b_scanned = false;
    }
    return p_mplex;
}

static void scan_multiplex_Clean( scan_multiplex_t *p_mplex )
{
    for( size_t i=0; i<p_mplex->i_services; i++ )
        scan_service_Delete( p_mplex->pp_services[i] );
    free( p_mplex->pp_services );
    free( p_mplex->psz_network_name );
}

static void scan_multiplex_Delete( scan_multiplex_t *p_mplex )
{
    scan_multiplex_Clean( p_mplex );
    free( p_mplex );
}

static bool scan_multiplex_AddService( scan_multiplex_t *p_mplex, scan_service_t *p_service )
{
    if( unlikely(p_service->p_mplex) ) /* Already belongs to another multiplex, should never happen */
        return false;

    scan_service_t **pp_realloc = realloc( p_mplex->pp_services,
                                           sizeof(scan_service_t *) * (p_mplex->i_services + 1) );
    if( unlikely(!pp_realloc) )
        return false;
    pp_realloc[p_mplex->i_services] = p_service;
    p_mplex->pp_services = pp_realloc;
    p_mplex->i_services++;
    p_service->p_mplex = p_mplex;
    return true;
}

static scan_service_t * scan_multiplex_FindService( const scan_multiplex_t *p_mplex, uint16_t i_program )
{
    for( size_t i = 0; i < p_mplex->i_services; i++ )
    {
        if( p_mplex->pp_services[i]->i_program == i_program )
            return p_mplex->pp_services[i];
    }
    return NULL;
}

void scan_parameter_Init( scan_parameter_t *p_dst )
{
    memset( p_dst, 0, sizeof(*p_dst) );
}

void scan_parameter_Clean( scan_parameter_t *p_dst )
{
    free( p_dst->psz_scanlist_file );
}

static void scan_parameter_Copy( const scan_parameter_t *p_src, scan_parameter_t *p_dst )
{
    scan_parameter_Clean( p_dst );
    *p_dst = *p_src;
    if( p_src->psz_scanlist_file )
        p_dst->psz_scanlist_file = strdup( p_src->psz_scanlist_file );
}

static void scan_Prepare( vlc_object_t *p_obj, const scan_parameter_t *p_parameter, scan_t *p_scan )
{
    if( p_parameter->type == SCAN_DVB_S &&
        p_parameter->psz_scanlist_file && p_parameter->scanlist_format == FORMAT_DVBv3 )
    {
        p_scan->p_scanlist =
                scan_list_dvbv3_load( p_obj, p_parameter->psz_scanlist_file, &p_scan->i_scanlist );
        if( p_scan->p_scanlist )
            msg_Dbg( p_scan->p_obj, "using satellite config file (%s)", p_parameter->psz_scanlist_file );
    }
    else if( p_parameter->psz_scanlist_file &&
             p_parameter->scanlist_format == FORMAT_DVBv5 )
    {
        if( p_parameter->type == SCAN_DVB_T )
        {
            p_scan->p_scanlist = scan_list_dvbv5_load( p_obj,
                                                       p_parameter->psz_scanlist_file,
                                                       &p_scan->i_scanlist );
        }
    }
}

static void scan_Debug_Parameters( vlc_object_t *p_obj, const scan_parameter_t *p_parameter )
{
    const char rgc_types[3] = {'T', 'S', 'C' };
    if( !p_parameter->type )
        return;

    msg_Dbg( p_obj, "DVB-%c scanning:", rgc_types[ p_parameter->type - 1 ] );

    if( p_parameter->type != SCAN_DVB_S )
    {
        msg_Dbg( p_obj, " - frequency [%d, %d]",
                 p_parameter->frequency.i_min, p_parameter->frequency.i_max );
        msg_Dbg( p_obj, " - bandwidth [%d,%d]",
                 p_parameter->bandwidth.i_min, p_parameter->bandwidth.i_max );
        msg_Dbg( p_obj, " - exhaustive mode %s", p_parameter->b_exhaustive ? "on" : "off" );
    }

    if( p_parameter->type == SCAN_DVB_C )
        msg_Dbg( p_obj, " - scannin modulations %s", p_parameter->b_modulation_set ? "off" : "on" );

    if( p_parameter->type == SCAN_DVB_S && p_parameter->psz_scanlist_file )
        msg_Dbg( p_obj, " - satellite [%s]", p_parameter->psz_scanlist_file );

    msg_Dbg( p_obj, " - use NIT %s", p_parameter->b_use_nit ? "on" : "off" );
    msg_Dbg( p_obj, " - FTA only %s", p_parameter->b_free_only ? "on" : "off" );
}

/* */
scan_t *scan_New( vlc_object_t *p_obj, const scan_parameter_t *p_parameter,
                  scan_frontend_tune_cb pf_frontend,
                  scan_frontend_stats_cb pf_status,
                  scan_demux_filter_cb pf_filter,
                  scan_demux_read_cb pf_read,
                  void *p_cbdata )
{
    if( p_parameter->type == SCAN_NONE )
        return NULL;

    scan_t *p_scan = malloc( sizeof( *p_scan ) );
    if( unlikely(p_scan == NULL) )
        return NULL;

    p_scan->p_obj = VLC_OBJECT(p_obj);
    p_scan->pf_tune = pf_frontend;
    p_scan->pf_stats = pf_status;
    p_scan->pf_read = pf_read;
    p_scan->pf_filter = pf_filter;
    p_scan->pf_notify_service = NULL;
    p_scan->p_cbdata = p_cbdata;
    p_scan->p_dialog_id = NULL;
    p_scan->i_multiplex = 0;
    p_scan->pp_multiplex = NULL;
    p_scan->i_multiplex_toscan = 0;
    p_scan->b_multiplexes_from_nit = false;
    scan_parameter_Init( &p_scan->parameter );
    scan_parameter_Copy( p_parameter, &p_scan->parameter );
    p_scan->i_time_start = mdate();
    p_scan->p_scanlist = NULL;
    p_scan->i_scanlist = 0;

    scan_Prepare( p_obj, p_parameter, p_scan );
    p_scan->p_current = p_scan->p_scanlist;

    p_scan->spectrum.i_index = 0;
    p_scan->spectrum.i_symbolrate_index = 0;
    p_scan->spectrum.modulation = 0;

    scan_Debug_Parameters( p_obj, p_parameter );

    return p_scan;
}

void scan_Destroy( scan_t *p_scan )
{
    if( !p_scan )
        return;
    if( p_scan->p_dialog_id != NULL )
        vlc_dialog_release( p_scan->p_obj, p_scan->p_dialog_id );

    scan_parameter_Clean( &p_scan->parameter );

    for( size_t i = 0; i < p_scan->i_multiplex; i++ )
        scan_multiplex_Delete( p_scan->pp_multiplex[i] );
    free( p_scan->pp_multiplex );

    scan_list_entries_release( p_scan->p_scanlist );

    free( p_scan );
}

static void scan_SetMultiplexScanStatus( scan_t *p_scan, scan_multiplex_t *p_mplex, bool b_scanned )
{
    if( p_mplex->b_scanned != b_scanned )
    {
        p_mplex->b_scanned = b_scanned;
        p_scan->i_multiplex_toscan += ( b_scanned ) ? -1 : 1;
    }
}

static bool scan_AddMultiplex( scan_t *p_scan, scan_multiplex_t *p_mplex )
{
    scan_multiplex_t **pp_realloc = realloc( p_scan->pp_multiplex,
                                             sizeof(scan_multiplex_t *) * (p_scan->i_multiplex + 1) );
    if( unlikely(!pp_realloc) )
        return false;
    pp_realloc[p_scan->i_multiplex] = p_mplex;
    p_scan->pp_multiplex = pp_realloc;
    p_scan->i_multiplex++;
    if( !p_mplex->b_scanned )
        p_scan->i_multiplex_toscan++;
    return true;
}

static scan_multiplex_t * scan_FindMultiplex( const scan_t *p_scan, uint16_t i_ts_id )
{
    for( size_t i = 0; i < p_scan->i_multiplex; i++ )
    {
        if( p_scan->pp_multiplex[i]->i_ts_id == i_ts_id )
            return p_scan->pp_multiplex[i];
    }
    return NULL;
}

static scan_multiplex_t *scan_FindOrCreateMultiplex( scan_t *p_scan, uint16_t i_ts_id,
                                                     const scan_tuner_config_t *p_cfg )
{
    scan_multiplex_t *p_mplex = scan_FindMultiplex( p_scan, i_ts_id );
    if( p_mplex == NULL )
    {
        p_mplex = scan_multiplex_New( p_cfg, i_ts_id );
        if( likely(p_mplex) )
        {
            if ( unlikely(!scan_AddMultiplex( p_scan, p_mplex )) ) /* OOM */
            {
                scan_multiplex_Delete( p_mplex );
                return NULL;
            }
        }
    }
    return p_mplex;
}

static size_t scan_CountServices( const scan_t *p_scan )
{
    size_t i_total_services = 0;
    for( size_t j = 0; j < p_scan->i_multiplex; j++ )
        i_total_services += p_scan->pp_multiplex[j]->i_services;
    return i_total_services;
}

static int Scan_Next_DVB_SpectrumExhaustive( const scan_parameter_t *p_params, scan_enumeration_t *p_spectrum,
                                             scan_tuner_config_t *p_cfg, double *pf_pos )
{
    unsigned i_bandwidth_count = p_params->bandwidth.i_max - p_params->bandwidth.i_min + 1;
    unsigned i_frequency_step = p_params->frequency.i_step ? p_params->frequency.i_step : 166667;
    unsigned i_frequency_count = (p_params->frequency.i_max - p_params->frequency.i_min) / p_params->frequency.i_step;

    if( p_spectrum->i_index > i_frequency_count * i_bandwidth_count )
        return VLC_EGENERIC;

    const int i_bi = p_spectrum->i_index % i_bandwidth_count;
    const int i_fi = p_spectrum->i_index / i_bandwidth_count;

    p_cfg->i_frequency = p_params->frequency.i_min + i_fi * i_frequency_step;
    p_cfg->i_bandwidth = p_params->bandwidth.i_min + i_bi;

    *pf_pos = (double)p_spectrum->i_index / i_frequency_count;

    p_spectrum->i_index++;

    return VLC_SUCCESS;
}

static int Scan_Next_DVBC( const scan_parameter_t *p_params, scan_enumeration_t *p_spectrum,
                           scan_tuner_config_t *p_cfg, double *pf_pos )
{
    bool b_rotate=true;
    if( !p_params->b_modulation_set )
    {
        p_spectrum->modulation = (p_spectrum->modulation >> 1 );
        /* if we iterated all modulations, move on */
        /* dvb utils dvb-c channels files seems to have only
               QAM64...QAM256, so lets just iterate over those */
        if( p_spectrum->modulation < SCAN_MODULATION_QAM_64)
        {
            p_spectrum->modulation = SCAN_MODULATION_QAM_256;
        } else {
            b_rotate=false;
        }
    }
    p_cfg->modulation = p_spectrum->modulation;

    if( p_params->i_symbolrate == 0 )
    {
        /* symbol rates from dvb-tools dvb-c files */
        static const unsigned short symbolrates[] = {
            6900, 6875, 6950
            /* With DR_44 we can cover other symbolrates from NIT-info
                    as all channel-seed files have atleast one channel that
                    has one of these symbolrate
                  */
        };

        enum { num_symbols = (sizeof(symbolrates)/sizeof(*symbolrates)) };

        /* if we rotated modulations, rotate symbolrate */
        if( b_rotate )
        {
            p_spectrum->i_symbolrate_index++;
            p_spectrum->i_symbolrate_index %= num_symbols;
        }
        p_cfg->i_symbolrate = 1000 * (symbolrates[ p_spectrum->i_symbolrate_index ] );

        if( p_spectrum->i_symbolrate_index )
            b_rotate=false;
    }
    else
    {
        p_cfg->i_symbolrate = p_params->i_symbolrate;
    }

    if( p_params->b_exhaustive )
        return Scan_Next_DVB_SpectrumExhaustive( p_params, p_spectrum, p_cfg, pf_pos );

    /* Values taken from dvb-scan utils frequency-files, sorted by how
     * often they appear. This hopefully speeds up finding services. */
    static const unsigned int frequencies[] = { 41000, 39400, 40200,
    38600, 41800, 36200, 44200, 43400, 37000, 35400, 42600, 37800,
    34600, 45800, 45000, 46600, 32200, 51400, 49000, 33800, 31400,
    30600, 47400, 71400, 69000, 68200, 58600, 56200, 54600, 49800,
    48200, 33000, 79400, 72200, 69800, 67400, 66600, 65000, 64200,
    61000, 55400, 53000, 52200, 50600, 29800, 16200, 15400, 11300,
    78600, 77000, 76200, 75400, 74600, 73800, 73000, 70600, 57800,
    57000, 53800, 12100, 81000, 77800, 65800, 63400, 61800, 29000,
    17000, 85000, 84200, 83400, 81800, 80200, 59400, 36900, 28300,
    26600, 25800, 25000, 24200, 23400, 85800, 74800, 73200, 72800,
    72400, 72000, 66000, 65600, 60200, 42500, 41700, 40900, 40100,
    39300, 38500, 37775, 37700, 37200, 36100, 35600, 35300, 34700,
    34500, 33900, 33700, 32900, 32300, 32100, 31500, 31300, 30500,
    29900, 29700, 29100, 28950, 28200, 28000, 27500, 27400, 27200,
    26700, 25900, 25500, 25100, 24300, 24100, 23500, 23200, 22700,
    22600, 21900, 21800, 21100, 20300, 19500, 18700, 17900, 17100,
    16300, 15500, 14700, 14600, 14500, 14300, 13900, 13700, 13100,
    12900, 12500, 12300
    };
    enum { num_frequencies = (sizeof(frequencies)/sizeof(*frequencies)) };

    if( p_spectrum->i_index >= num_frequencies )
        return VLC_EGENERIC; /* End */

    p_cfg->i_frequency = 10000 * ( frequencies[ p_spectrum->i_index ] );
    *pf_pos = (double)(p_spectrum->i_index * 1000 +
                       p_spectrum->i_symbolrate_index * 100 +
                       (256 - (p_spectrum->modulation >> 4)) )
            / (num_frequencies * 1000 + 900 + 16);

    if( b_rotate )
        p_spectrum->i_index++;

    return VLC_SUCCESS;
}

static int Scan_Next_DVBT( const scan_parameter_t *p_params, scan_enumeration_t *p_spectrum,
                           scan_tuner_config_t *p_cfg, double *pf_pos )
{
    if( p_params->b_exhaustive )
        return Scan_Next_DVB_SpectrumExhaustive( p_params, p_spectrum, p_cfg, pf_pos );

    unsigned i_frequency_step = p_params->frequency.i_step ? p_params->frequency.i_step : 166667;

    unsigned i_bandwidth_min = p_params->bandwidth.i_min ? p_params->bandwidth.i_min : 6;
    unsigned i_bandwidth_max = p_params->bandwidth.i_max ? p_params->bandwidth.i_max : 8;
    unsigned i_bandwidth_count = i_bandwidth_max - i_bandwidth_min + 1;

    static const int i_band_count = 2;
    static const struct
    {
        const char *psz_name;
        int i_min;
        int i_max;
    }
    band[2] =
    {
        { "VHF", 174, 230 },
        { "UHF", 470, 862 },
    };
    const int i_offset_count = 5;
    const int i_mhz = 1000000;

    /* We will probe the whole band divided in all bandwidth possibility trying 
     * i_offset_count offset around the position
     */
    for( ;; p_spectrum->i_index++ )
    {

        const int i_bi = p_spectrum->i_index % i_bandwidth_count;
        const int i_oi = (p_spectrum->i_index / i_bandwidth_count) % i_offset_count;
        const int i_fi = (p_spectrum->i_index / i_bandwidth_count) / i_offset_count;

        const int i_bandwidth = i_bandwidth_min + i_bi;
        int i;

        for( i = 0; i < i_band_count; i++ )
        {
            if( i_fi >= band[i].i_min && i_fi <= band[i].i_max )
                break;
        }
        if( i >=i_band_count )
        {
            if( i_fi > band[i_band_count-1].i_max )
            {
                p_spectrum->i_index++;
                return VLC_EGENERIC;
            }
            continue;
        }

        const unsigned i_frequency_min = band[i].i_min*i_mhz + i_bandwidth*i_mhz/2;
        const unsigned i_frequency_base = i_fi*i_mhz;

        if( i_frequency_base >= i_frequency_min && ( i_frequency_base - i_frequency_min ) % ( i_bandwidth*i_mhz ) == 0 )
        {
            const unsigned i_frequency = i_frequency_base + ( i_oi - i_offset_count/2 ) * i_frequency_step;

            p_cfg->i_frequency = i_frequency;
            p_cfg->i_bandwidth = i_bandwidth;

            int i_current = 0, i_total = 0;
            for( i = 0; i < i_band_count; i++ )
            {
                const int i_frag = band[i].i_max-band[i].i_min;

                if( i_fi >= band[i].i_min )
                    i_current += __MIN( i_fi - band[i].i_min, i_frag );
                i_total += i_frag;
            }

            *pf_pos = (double)( i_current + (double)i_oi / i_offset_count ) / i_total;
            p_spectrum->i_index++;
            return VLC_SUCCESS;
        }
    }
}

static int Scan_GetNextSpectrumTunerConfig( scan_t *p_scan, scan_tuner_config_t *p_cfg, double *pf_pos )
{
    int i_ret = VLC_EGENERIC;
    switch( p_scan->parameter.type )
    {
        case SCAN_DVB_T:
            i_ret = Scan_Next_DVBT( &p_scan->parameter, &p_scan->spectrum, p_cfg, pf_pos );
            break;
        case SCAN_DVB_C:
            i_ret = Scan_Next_DVBC( &p_scan->parameter, &p_scan->spectrum, p_cfg, pf_pos );
            break;
        default:
            break;
    }
    return i_ret;
}

static int Scan_GetNextTunerConfig( scan_t *p_scan, scan_tuner_config_t *p_cfg, double *pf_pos )
{
    /* Note: Do not forget to advance current scan (avoid frontend tuning errors loops ) */
    if( p_scan->p_scanlist && p_scan->p_current )
    {
        const scan_list_entry_t *p_entry = p_scan->p_current;
        p_cfg->i_frequency = p_entry->i_freq;
        p_cfg->i_bandwidth = p_entry->i_bw / 1000000;
        p_cfg->modulation = p_entry->modulation;

        switch( p_entry->delivery )
        {
            case SCAN_DELIVERY_UNKNOWN:
                break;
            case SCAN_DELIVERY_DVB_T:
                p_cfg->coderate_hp = p_entry->coderate_hp;
                p_cfg->coderate_lp = p_entry->coderate_lp;
                p_cfg->type = SCAN_DVB_T;
                break;
            case SCAN_DELIVERY_DVB_S:
                p_cfg->type = SCAN_DVB_S;
                p_cfg->polarization = p_entry->polarization;
                p_cfg->i_symbolrate = p_entry->i_rate / 1000;
                p_cfg->inner_fec = p_entry->inner_fec;
                break;
            case SCAN_DELIVERY_DVB_C:
                p_cfg->type = SCAN_DVB_C;
                p_cfg->i_symbolrate = p_entry->i_rate / 1000;
                p_cfg->inner_fec = p_entry->inner_fec;
                break;
            default:
                p_cfg->type = SCAN_NONE;
                break;
        }

        p_scan->p_current = p_scan->p_current->p_next;
        *pf_pos = (double) p_scan->spectrum.i_index++ / p_scan->i_scanlist;

        return VLC_SUCCESS;
    }

    if( p_scan->p_scanlist == NULL &&
        ( p_scan->i_multiplex == 0 || /* Stop frequency scanning if we've found a valid NIT */
         (p_scan->parameter.b_use_nit && !p_scan->b_multiplexes_from_nit) ) )
    {
        int i_ret = Scan_GetNextSpectrumTunerConfig( p_scan, p_cfg, pf_pos );
        if( i_ret == VLC_SUCCESS )
            return i_ret;
    }

    if( p_scan->i_multiplex_toscan )
    {
        for( size_t i=0; i<p_scan->i_multiplex_toscan; i++ )
        {
            if( !p_scan->pp_multiplex[i]->b_scanned )
            {
                scan_SetMultiplexScanStatus( p_scan, p_scan->pp_multiplex[i], true );
                *p_cfg = p_scan->pp_multiplex[i]->cfg;
                *pf_pos = (double) 1.0 - (p_scan->i_multiplex / p_scan->i_multiplex_toscan);
                return VLC_SUCCESS;
            }
        }
    }

    return VLC_ENOITEM;
}

static int scan_Next( scan_t *p_scan, scan_tuner_config_t *p_cfg )
{
    double f_position;
    int i_ret;

    if( scan_IsCancelled( p_scan ) )
        return VLC_EGENERIC;

    //do
    {
        scan_tuner_config_Init( p_cfg, &p_scan->parameter );

        i_ret = Scan_GetNextTunerConfig( p_scan, p_cfg, &f_position );
        if( i_ret )
            return i_ret;
    }
    //while( !scan_tuner_config_ParametersValidate( &p_scan->parameter, p_cfg ) );

    const size_t i_total_services = scan_CountServices( p_scan );
    const mtime_t i_eta = f_position > 0.005 ? (mdate() - p_scan->i_time_start) * ( 1.0 / f_position - 1.0 ) : -1;
    char psz_eta[MSTRTIME_MAX_SIZE];
    const char *psz_fmt = _("%.1f MHz (%d services)\n~%s remaining");

    if( i_eta >= 0 )
        msg_Info( p_scan->p_obj, "Scan ETA %s | %f", secstotimestr( psz_eta, i_eta/1000000 ), f_position * 100 );

    if( p_scan->p_dialog_id == NULL )
    {
        p_scan->p_dialog_id =
            vlc_dialog_display_progress( p_scan->p_obj, false,
                                         f_position, _("Cancel"),
                                         _("Scanning DVB"), psz_fmt,
                                         (double)p_cfg->i_frequency / 1000000,
                                         i_total_services,
                                         secstotimestr( psz_eta, i_eta/1000000 ) );
    }
    else
    {
        vlc_dialog_update_progress_text( p_scan->p_obj, p_scan->p_dialog_id,
                                         f_position, psz_fmt,
                                         (double)p_cfg->i_frequency / 1000000,
                                         i_total_services,
                                         secstotimestr( psz_eta, i_eta/1000000 ) );
    }

    return VLC_SUCCESS;
}

bool scan_IsCancelled( scan_t *p_scan )
{
    if( p_scan->p_dialog_id == NULL )
        return false;
    return vlc_dialog_is_cancelled( p_scan->p_obj, p_scan->p_dialog_id );
}

int scan_Run( scan_t *p_scan )
{
    scan_tuner_config_t cfg;
    if( scan_Next( p_scan, &cfg ) )
        return VLC_ENOITEM;

    scan_session_t *session = scan_session_New( p_scan, &cfg );
    if( unlikely(session == NULL) )
        return VLC_EGENERIC;

    if( p_scan->pf_tune( p_scan, p_scan->p_cbdata, &cfg ) != VLC_SUCCESS )
    {
        scan_session_Destroy( p_scan, session );
        return VLC_EGENERIC;
    }

    p_scan->pf_filter( p_scan, p_scan->p_cbdata, PSI_PAT_PID, true );
    p_scan->pf_filter( p_scan, p_scan->p_cbdata, SI_SDT_PID, true );
    if( p_scan->parameter.b_use_nit )
        p_scan->pf_filter( p_scan, p_scan->p_cbdata, SI_NIT_PID, true );

    /* */
    uint8_t packet[TS_PACKET_SIZE * SCAN_READ_BUFFER_COUNT];
    int64_t i_scan_start = mdate();

    for( ;; )
    {
        unsigned i_timeout = scan_session_GetTablesTimeout( session );
        mtime_t i_remaining = mdate() - i_scan_start;
        if( i_remaining > i_timeout )
            break;

        size_t i_packet_count = 0;
        int i_ret = p_scan->pf_read( p_scan, p_scan->p_cbdata,
                                     i_timeout - i_remaining,
                                     SCAN_READ_BUFFER_COUNT,
                                     (uint8_t *) &packet, &i_packet_count );

        if( p_scan->pf_stats )
            p_scan->pf_stats( p_scan, p_scan->p_cbdata, &session->i_snr );

        if ( i_ret != VLC_SUCCESS )
            break;

        for( size_t i=0; i< i_packet_count; i++ )
        {
            if( scan_session_Push( session,
                                   &packet[i * TS_PACKET_SIZE] ) )
                break;
        }
    }

    scan_session_Destroy( p_scan, session );

    return VLC_SUCCESS;
}

static void scan_NotifyService( scan_t *p_scan, scan_service_t *p_service, bool b_updated )
{
    if( !p_scan->pf_notify_service || !scan_service_type_Supported( p_service->type ) )
        return;
    p_service->stickyref = p_scan->pf_notify_service( p_scan, p_scan->p_cbdata,
                                                      p_service, p_service->stickyref,
                                                      b_updated );
}

#define scan_NotifyNewService( a, b ) scan_NotifyService( a, b, false )
#define scan_NotifyUpdatedService( a, b ) scan_NotifyService( a, b, true )

static bool GetOtherNetworkNIT( scan_session_t *p_session, uint16_t i_network_id,
                                dvbpsi_nit_t ***ppp_nit )
{
    for( size_t i=0; i<p_session->others.i_nit; i++ )
    {
        if( p_session->others.pp_nit[i]->i_network_id == i_network_id )
        {
            *ppp_nit = &p_session->others.pp_nit[i];
            return true;
        }
    }

    return false;
}

static bool GetOtherTsSDT( scan_session_t *p_session, uint16_t i_ts_id,
                           dvbpsi_sdt_t ***ppp_sdt )
{
    for( size_t i=0; i<p_session->others.i_sdt; i++ )
    {
        if( p_session->others.pp_sdt[i]->i_extension == i_ts_id )
        {
            *ppp_sdt = &p_session->others.pp_sdt[i];
            return true;
        }
    }

    return false;
}

static void ParsePAT( vlc_object_t *p_obj, scan_t *p_scan,
                      const dvbpsi_pat_t *p_pat, const scan_tuner_config_t *p_cfg,
                      int i_snr )
{
    /* PAT must not create new service without proper config ( local ) */
    if( !p_cfg )
        return;

    scan_multiplex_t *p_mplex = scan_FindOrCreateMultiplex( p_scan, p_pat->i_ts_id, p_cfg );
    if( unlikely(p_mplex == NULL) )
        return;

    if( p_mplex->i_snr > 0 && i_snr > p_mplex->i_snr )
    {
        msg_Info( p_obj, "multiplex ts_id %" PRIu16 " freq %u snr %d replaced by freq %u snr %d",
                  p_mplex->i_ts_id, p_mplex->cfg.i_frequency, p_mplex->i_snr,
                  p_cfg->i_frequency, i_snr );
        p_mplex->cfg = *p_cfg;
    }
    p_mplex->i_snr = i_snr;

    const dvbpsi_pat_program_t *p_program;
    for( p_program = p_pat->p_first_program; p_program != NULL; p_program = p_program->p_next )
    {
        if( p_program->i_number == 0 )  /* NIT */
            continue;

        scan_service_t *s = scan_multiplex_FindService( p_mplex, p_program->i_number );
        if( s == NULL )
        {
            s = scan_service_New( p_program->i_number );
            if( likely(s) )
            {
                if( !scan_multiplex_AddService( p_mplex, s ) ) /* OOM */
                    scan_service_Delete( s );
                else
                    scan_NotifyNewService( p_scan, s );
            }
        }
    }
}

/* FIXME handle properly string (convert to utf8) */
static void PATCallBack( scan_session_t *p_session, dvbpsi_pat_t *p_pat )
{
    vlc_object_t *p_obj = p_session->p_obj;

    /* */
    if( p_session->local.p_pat && p_session->local.p_pat->b_current_next )
    {
        dvbpsi_pat_delete( p_session->local.p_pat );
        p_session->local.p_pat = NULL;
    }
    if( p_session->local.p_pat )
    {
        dvbpsi_pat_delete( p_pat );
        return;
    }

    dvbpsi_pat_program_t *p_program;

    /* */
    p_session->local.p_pat = p_pat;

    /* */
    msg_Dbg( p_obj, "new PAT ts_id=%d version=%d current_next=%d",
             p_pat->i_ts_id, p_pat->i_version, p_pat->b_current_next );
    for( p_program = p_pat->p_first_program; p_program != NULL; p_program = p_program->p_next )
    {
        msg_Dbg( p_obj, "  * number=%d pid=%d", p_program->i_number, p_program->i_pid );
        if( p_program->i_number == 0 )
            p_session->i_nit_pid = p_program->i_pid;
    }
}

static void ParseSDT( vlc_object_t *p_obj, scan_t *p_scan, const dvbpsi_sdt_t *p_sdt )
{
    VLC_UNUSED(p_obj);
    /* SDT must not create new service without proper config ( local )
       or it must has been created by another network NIT (providing freq).
       Guaranteed by parsing order( PAT, current ts SDT ) or ( NIT, SDT ) */
    scan_multiplex_t *p_mplex = scan_FindMultiplex( p_scan, p_sdt->i_extension );
    if( unlikely(p_mplex == NULL) )
        return ;

    scan_SetMultiplexScanStatus( p_scan, p_mplex, true );

    if( p_mplex->i_sdt_version == UINT8_MAX )
        p_mplex->i_sdt_version = p_sdt->i_version;

    for( const dvbpsi_sdt_service_t *p_srv = p_sdt->p_first_service;
                                     p_srv; p_srv = p_srv->p_next )
    {
        bool b_newservice = false;
        scan_service_t *s = scan_multiplex_FindService( p_mplex, p_srv->i_service_id );
        if( s == NULL )
        {
            b_newservice = true;
            s = scan_service_New( p_srv->i_service_id );
            if( unlikely(s == NULL) )
                continue;
            if( !scan_multiplex_AddService( p_mplex, s ) )
            {
                scan_service_Delete( s );
                continue;
            }
        }

        s->b_crypted = p_srv->b_free_ca;

        for( dvbpsi_descriptor_t *p_dr = p_srv->p_first_descriptor;
                                  p_dr; p_dr = p_dr->p_next )
        {
            if( p_dr->i_tag != 0x48 )
                continue;

            dvbpsi_service_dr_t *pD = dvbpsi_DecodeServiceDr( p_dr );
            if( pD )
            {
                if( !s->psz_name )
                    s->psz_name = vlc_from_EIT( pD->i_service_name,
                                                pD->i_service_name_length );
                free( s->psz_provider );
                s->psz_provider = vlc_from_EIT( pD->i_service_provider_name,
                                                pD->i_service_provider_name_length );

                s->type = pD->i_service_type;
            }
        }

        scan_NotifyService( p_scan, s, !b_newservice );
    }
}

static void SDTCallBack( scan_session_t *p_session, dvbpsi_sdt_t *p_sdt )
{
    vlc_object_t *p_obj = p_session->p_obj;
    dvbpsi_sdt_t **pp_stored_sdt = NULL;
    if( p_sdt->i_table_id == SDT_OTHER_TS_TABLE_ID )
    {
        if( !GetOtherTsSDT( p_session, p_sdt->i_extension, &pp_stored_sdt ) )
        {
            dvbpsi_sdt_t **pp_realloc = realloc( p_session->others.pp_sdt,
                                                (p_session->others.i_sdt + 1) * sizeof( *pp_realloc ) );
            if( !pp_realloc ) /* oom */
            {
                dvbpsi_sdt_delete( p_sdt );
                return;
            }
            pp_stored_sdt = &pp_realloc[p_session->others.i_sdt];
            p_session->others.pp_sdt = pp_realloc;
            p_session->others.i_sdt++;
        }
    }
    else /* SDT_CURRENT_TS_TABLE_ID */
    {
        pp_stored_sdt = &p_session->local.p_sdt;
    }

    /* Store, replace, or discard */
    if( *pp_stored_sdt )
    {
        if( (*pp_stored_sdt)->i_version == p_sdt->i_version ||
            (*pp_stored_sdt)->b_current_next > p_sdt->b_current_next )
        {
            /* Duplicate or stored one isn't current */
            dvbpsi_sdt_delete( p_sdt );
            return;
        }
        dvbpsi_sdt_delete( *pp_stored_sdt );
    }
    *pp_stored_sdt = p_sdt;

    /* */
    msg_Dbg( p_obj, "new SDT %s ts_id=%d version=%d current_next=%d network_id=%d",
             ( p_sdt->i_table_id == SDT_CURRENT_TS_TABLE_ID ) ? "local" : "other",
             p_sdt->i_extension, p_sdt->i_version, p_sdt->b_current_next,
             p_sdt->i_network_id );

    dvbpsi_sdt_service_t *p_srv;
    for( p_srv = p_sdt->p_first_service; p_srv; p_srv = p_srv->p_next )
    {
        dvbpsi_descriptor_t *p_dr;

        msg_Dbg( p_obj, "  * service id=%d eit schedule=%d present=%d running=%d free_ca=%d",
                 p_srv->i_service_id, p_srv->b_eit_schedule,
                 p_srv->b_eit_present, p_srv->i_running_status,
                 p_srv->b_free_ca );
        for( p_dr = p_srv->p_first_descriptor; p_dr; p_dr = p_dr->p_next )
        {
            if( p_dr->i_tag == 0x48 )
            {
                dvbpsi_service_dr_t *pD = dvbpsi_DecodeServiceDr( p_dr );
                if( pD )
                {
                    char str2[257];

                    memcpy( str2, pD->i_service_name, pD->i_service_name_length );
                    str2[pD->i_service_name_length] = '\0';

                    msg_Dbg( p_obj, "    - type=%d name=%s",
                             pD->i_service_type, str2 );
                }
            }
            else
            {
                msg_Dbg( p_obj, "    * dsc 0x%x", p_dr->i_tag );
            }
        }
    }
}

static scan_coderate_t ConvertDelDrInnerFec( uint8_t v )
{
    switch(v)
    {
        default:
        case 0x0: return SCAN_CODERATE_AUTO;
        case 0x1: return SCAN_CODERATE_1_2;
        case 0x2: return SCAN_CODERATE_2_3;
        case 0x3: return SCAN_CODERATE_3_4;
        case 0x4: return SCAN_CODERATE_5_6;
        case 0x5: return SCAN_CODERATE_7_8;
        case 0x6: return SCAN_CODERATE_8_9;
        case 0x7: return SCAN_CODERATE_3_5;
        case 0x8: return SCAN_CODERATE_4_5;
        case 0x9: return SCAN_CODERATE_9_10;
        case 0xF: return SCAN_CODERATE_NONE;
    }
}

static scan_coderate_t ConvertDelDrCodeRate( uint8_t v )
{
    if( v > 0x04 )
        return SCAN_CODERATE_AUTO;
    else
        return ConvertDelDrInnerFec( v + 1 );
}

static void ParseNIT( vlc_object_t *p_obj, scan_t *p_scan,
                      const dvbpsi_nit_t *p_nit, const scan_tuner_config_t *p_cfg )
{
    for( const dvbpsi_nit_ts_t *p_ts = p_nit->p_first_ts;
                                p_ts != NULL; p_ts = p_ts->p_next )
    {
        msg_Dbg( p_obj, "   * ts ts_id=0x%x original_id=0x%x", p_ts->i_ts_id, p_ts->i_orig_network_id );

        uint32_t i_private_data_id = 0;
        dvbpsi_descriptor_t *p_dsc;
        scan_tuner_config_t tscfg;
        scan_tuner_config_Init( &tscfg, &p_scan->parameter );
        if( p_cfg != NULL ) // p_nit->i_table_id != NIT_CURRENT_NETWORK_TABLE_ID
            tscfg = *p_cfg;

        dvbpsi_service_list_dr_t *p_sl = NULL;
        dvbpsi_lcn_dr_t *p_lc = NULL;
        dvbpsi_descriptor_t *p_nn = NULL;

        for( p_dsc = p_ts->p_first_descriptor; p_dsc != NULL; p_dsc = p_dsc->p_next )
        {
            if( p_dsc->i_tag == 0x41 )
            {
                /* Store it and process it after signal config
                 * (required for NIT describing other networks) */
                p_sl = dvbpsi_DecodeServiceListDr( p_dsc );
            }
            else if( p_dsc->i_tag == 0x5a )
            {
                dvbpsi_terr_deliv_sys_dr_t *p_t = dvbpsi_DecodeTerrDelivSysDr( p_dsc );
                if( p_t )
                {
                    tscfg.i_frequency =  p_t->i_centre_frequency / 10;
                    tscfg.i_bandwidth =  8 - p_t->i_bandwidth;
                    switch(p_t->i_constellation)
                    {
                        case 0x00:
                            tscfg.modulation = SCAN_MODULATION_QPSK;
                            break;
                        case 0x01:
                            tscfg.modulation = SCAN_MODULATION_QAM_16;
                            break;
                        case 0x02:
                            tscfg.modulation = SCAN_MODULATION_QAM_64;
                            break;
                        default:
                            tscfg.modulation = SCAN_MODULATION_AUTO;
                            break;
                    }

                    tscfg.coderate_hp = ConvertDelDrCodeRate( p_t->i_code_rate_hp_stream );
                    if( p_t->i_hierarchy_information == 0x0 || p_t->i_hierarchy_information == 0x4 )
                        tscfg.coderate_lp = SCAN_CODERATE_NONE;
                    else
                        tscfg.coderate_lp = ConvertDelDrCodeRate( p_t->i_code_rate_lp_stream );

                    msg_Dbg( p_obj, "       * terrestrial delivery system" );
                    msg_Dbg( p_obj, "           * centre_frequency %u", tscfg.i_frequency );
                    msg_Dbg( p_obj, "           * bandwidth %u", tscfg.i_bandwidth );
                    msg_Dbg( p_obj, "           * modulation %s", scan_value_modulation( tscfg.modulation ) );
                    msg_Dbg( p_obj, "           * hierarchy %d", p_t->i_hierarchy_information );
                    msg_Dbg( p_obj, "           * code_rate hp %s lp %s", scan_value_coderate( tscfg.coderate_hp ),
                                                                          scan_value_coderate( tscfg.coderate_hp ) );
                    msg_Dbg( p_obj, "           * guard_interval %d", p_t->i_guard_interval );
                    msg_Dbg( p_obj, "           * transmission_mode %d", p_t->i_transmission_mode );
                    msg_Dbg( p_obj, "           * other_frequency_flag %d", p_t->i_other_frequency_flag );
                }
            }
            else if( p_dsc->i_tag == 0x43 )
            {
                dvbpsi_sat_deliv_sys_dr_t *p_s = dvbpsi_DecodeSatDelivSysDr( p_dsc );
                if( p_s )
                {
                    tscfg.i_frequency =  decode_BCD( p_s->i_frequency ) * 1000;
                    tscfg.i_symbolrate =  decode_BCD( p_s->i_symbol_rate ) * 100;
                    if( unlikely(p_s->i_polarization > 0x03) )
                        p_s->i_polarization = 0;

                    const scan_polarization_t polarizations[] = {
                                                  SCAN_POLARIZATION_HORIZONTAL,
                                                  SCAN_POLARIZATION_VERTICAL,
                                                  SCAN_POLARIZATION_CIRC_LEFT,
                                                  SCAN_POLARIZATION_CIRC_RIGHT };
                    tscfg.polarization = polarizations[p_s->i_polarization];

                    switch(p_s->i_modulation_type)
                    {
                        default:
                        case 0x00:
                            tscfg.modulation = SCAN_MODULATION_AUTO;
                            break;
                        case 0x01:
                            tscfg.modulation = SCAN_MODULATION_QPSK;
                            break;
                        case 0x02:
                            tscfg.modulation = SCAN_MODULATION_PSK_8;
                            break;
                        case 0x03:
                            tscfg.modulation = SCAN_MODULATION_QAM_16;
                            break;
                    }

                    tscfg.delivery = (p_s->i_modulation_system == 0x01) ? SCAN_DELIVERY_DVB_S2
                                                                        : SCAN_DELIVERY_DVB_S;
                    tscfg.inner_fec = ConvertDelDrInnerFec( p_s->i_fec_inner );

                    msg_Dbg( p_obj, "       * satellite delivery system" );
                    msg_Dbg( p_obj, "           * frequency %u", tscfg.i_frequency );
                    msg_Dbg( p_obj, "           * symbolrate %u", tscfg.i_symbolrate );
                    msg_Dbg( p_obj, "           * polarization %c", (char) tscfg.polarization );
                    msg_Dbg( p_obj, "           * modulation %s", scan_value_modulation( tscfg.modulation ) );
                    msg_Dbg( p_obj, "           * fec inner %s", scan_value_coderate( tscfg.inner_fec ) );
                    if( tscfg.delivery == SCAN_DELIVERY_DVB_S2 )
                    {
                        msg_Dbg( p_obj, "           * system DVB-S2" );
                    }
                }
            }
            else if( p_dsc->i_tag == 0x44 )
            {
                dvbpsi_cable_deliv_sys_dr_t *p_t = dvbpsi_DecodeCableDelivSysDr( p_dsc );
                if( p_t )
                {
                    tscfg.i_frequency =  decode_BCD( p_t->i_frequency ) * 100;
                    tscfg.i_symbolrate =  decode_BCD( p_t->i_symbol_rate ) * 100;
                    if( p_t->i_modulation <= 0x05 )
                        tscfg.modulation = p_t->i_modulation;
                    else
                        tscfg.modulation = SCAN_MODULATION_AUTO;
                    tscfg.inner_fec = ConvertDelDrInnerFec( p_t->i_fec_inner );

                    msg_Dbg( p_obj, "       * Cable delivery system");
                    msg_Dbg( p_obj, "           * frequency %d", tscfg.i_frequency );
                    msg_Dbg( p_obj, "           * symbolrate %u", tscfg.i_symbolrate );
                    msg_Dbg( p_obj, "           * modulation %s", scan_value_modulation( tscfg.modulation ) );
                    msg_Dbg( p_obj, "           * fec inner %s", scan_value_coderate( tscfg.inner_fec ) );
                }
            }
            else if( p_dsc->i_tag == 0x5f && p_dsc->i_length > 3 )
            {
                msg_Dbg( p_obj, "       * private data specifier descriptor" );
                i_private_data_id = GetDWBE( &p_dsc->p_data[0] );
                msg_Dbg( p_obj, "           * value 0x%8.8x", i_private_data_id );
            }
            else if( i_private_data_id == 0x28 && p_dsc->i_tag == 0x83 )
            {
                msg_Dbg( p_obj, "       * logical channel descriptor (EICTA)" );
                p_lc = dvbpsi_DecodeLCNDr( p_dsc );
            }
            else if( p_dsc->i_tag == 0x40 && p_dsc->i_length > 0 ) /* Network Name */
            {
                p_nn = p_dsc;
            }
            else
            {
                msg_Warn( p_obj, "       * dsc 0x%x", p_dsc->i_tag );
            }
        }

        bool b_valid = scan_tuner_config_StandardValidate( &tscfg );
        if( b_valid && p_nit->i_table_id == NIT_CURRENT_NETWORK_TABLE_ID )
        {
            p_scan->b_multiplexes_from_nit |= b_valid;
        }

        scan_multiplex_t *p_mplex = scan_FindMultiplex( p_scan, p_ts->i_ts_id );
        if( p_mplex == NULL && b_valid )
        {
            p_mplex = scan_multiplex_New( &tscfg, p_ts->i_ts_id );
            if( likely(p_mplex) )
            {
                if ( unlikely(!scan_AddMultiplex( p_scan, p_mplex )) )
                {
                    scan_multiplex_Delete( p_mplex );
                    p_mplex = NULL;
                }
            }
        }

        if( unlikely(!p_mplex) )
            continue;

        if( p_mplex->i_network_id == NETWORK_ID_RESERVED )
        {
            p_mplex->i_network_id = p_nit->i_network_id;
            p_mplex->i_nit_version = p_nit->i_version;
        }

        /* Now process service list, and create them if tuner config is known */
        if( p_sl )
        {
            msg_Dbg( p_obj, "       * service list descriptor" );
            for( uint8_t i = 0; p_sl && i < p_sl->i_service_count; i++ )
            {
                const uint16_t i_service_id = p_sl->i_service[i].i_service_id;
                const uint8_t i_service_type = p_sl->i_service[i].i_service_type;
                msg_Dbg( p_obj, "           * service_id=%" PRIu16 " type=%" PRIu8,
                                i_service_id, i_service_type );

                if( !p_cfg || p_cfg->i_frequency == 0 )
                {
                    msg_Warn( p_obj, "cannot create service_id=%" PRIu16 " ts_id=%" PRIu16 " (no config)",
                                     i_service_id, p_ts->i_ts_id );
                    continue;
                }

                bool b_newservice = false;
                scan_service_t *s = scan_multiplex_FindService( p_mplex, i_service_id );
                if( s == NULL )
                {
                    s = scan_service_New( i_service_id );
                    if( unlikely(s == NULL) )
                        continue;

                    b_newservice = true;
                    s->type = i_service_type;
                    s->i_original_network_id = p_ts->i_orig_network_id;
                    if( !scan_multiplex_AddService( p_mplex, s ) )
                    {
                        scan_service_Delete( s );
                        s = NULL;
                    }
                }

                if ( s->psz_original_network_name == NULL && p_nn )
                    s->psz_original_network_name = strndup( (const char*) p_nn->p_data, p_nn->i_length );

                scan_NotifyService( p_scan, s, !b_newservice );
            }
        }

        /* Set virtual channel numbers */
        if( p_lc )
        {
            for( int i = 0; i < p_lc->i_number_of_entries; i++ )
            {
                const uint16_t i_service_id = p_lc->p_entries[i].i_service_id;
                const uint16_t i_channel_number = p_lc->p_entries[i].i_logical_channel_number;
                msg_Dbg( p_obj, "           * service_id=%" PRIu16 " channel_number=%" PRIu16,
                                i_service_id, i_channel_number );
                scan_service_t *s = scan_multiplex_FindService( p_mplex, i_service_id );
                if( s )
                    s->i_channel = i_channel_number;
            }
        }
    }
}

static void NITCallBack( scan_session_t *p_session, dvbpsi_nit_t *p_nit )
{
    vlc_object_t *p_obj = p_session->p_obj;
    dvbpsi_nit_t **pp_stored_nit = NULL;

    if( p_nit->i_table_id == NIT_OTHER_NETWORK_TABLE_ID )
    {
        if( !GetOtherNetworkNIT( p_session, p_nit->i_network_id, &pp_stored_nit ) )
        {
            dvbpsi_nit_t **pp_realloc = realloc( p_session->others.pp_nit,
                                                (p_session->others.i_nit + 1) * sizeof( *pp_realloc ) );
            if( !pp_realloc ) /* oom */
            {
                dvbpsi_nit_delete( p_nit );
                return;
            }
            pp_stored_nit = &pp_realloc[p_session->others.i_nit];
            p_session->others.pp_nit = pp_realloc;
            p_session->others.i_nit++;
        }
    }
    else /* NIT_CURRENT_NETWORK_TABLE_ID */
    {
        pp_stored_nit = &p_session->local.p_nit;
    }

    /* Store, replace, or discard */
    if( *pp_stored_nit )
    {
        if( (*pp_stored_nit)->i_version == p_nit->i_version ||
            (*pp_stored_nit)->b_current_next > p_nit->b_current_next )
        {
            /* Duplicate or stored one isn't current */
            dvbpsi_nit_delete( p_nit );
            return;
        }
        dvbpsi_nit_delete( *pp_stored_nit );
    }
    *pp_stored_nit = p_nit;

    msg_Dbg( p_obj, "new NIT %s network_id=%d version=%d current_next=%d",
             ( p_nit->i_table_id == NIT_CURRENT_NETWORK_TABLE_ID ) ? "local" : "other",
             p_nit->i_network_id, p_nit->i_version, p_nit->b_current_next );

    /* */
    dvbpsi_descriptor_t *p_dsc;
    for( p_dsc = p_nit->p_first_descriptor; p_dsc != NULL; p_dsc = p_dsc->p_next )
    {
        if( p_dsc->i_tag == 0x40 && p_dsc->i_length > 0 )
        {
            msg_Dbg( p_obj, "   * network name descriptor" );
            char str1[257];

            memcpy( str1, p_dsc->p_data, p_dsc->i_length );
            str1[p_dsc->i_length] = '\0';
            msg_Dbg( p_obj, "       * name %s", str1 );
        }
#if 0
        else if( p_dsc->i_tag == 0x4a )
        {
            dvbpsi_linkage_dr_t *p_l = dvbpsi_DecodeLinkageDr( p_dsc );
            if( p_l )
            {
                msg_Dbg( p_obj, "   * linkage descriptor" );
                msg_Dbg( p_obj, "       * ts_id %" PRIu16, p_l->i_transport_stream_id );
                msg_Dbg( p_obj, "       * on_id %" PRIu16, p_l->i_original_network_id );
                msg_Dbg( p_obj, "       * service_id %" PRIu16, p_l->i_service_id );
                msg_Dbg( p_obj, "       * linkage_type %" PRIu8, p_l->i_linkage_type );
            }
        }
#endif
        else
        {
            msg_Dbg( p_obj, "   * dsc 0x%x", p_dsc->i_tag );
        }
    }

}

static void PSINewTableCallBack( dvbpsi_t *h, uint8_t i_table_id, uint16_t i_extension, void *p_data )
{
    scan_session_t *p_session = (scan_session_t *)p_data;

    if( i_table_id == SDT_CURRENT_TS_TABLE_ID || i_table_id == SDT_OTHER_TS_TABLE_ID )
    {
        if( !dvbpsi_sdt_attach( h, i_table_id, i_extension, (dvbpsi_sdt_callback)SDTCallBack, p_session ) )
            msg_Err( p_session->p_obj, "PSINewTableCallback: failed attaching SDTCallback" );
    }
    else if( i_table_id == NIT_CURRENT_NETWORK_TABLE_ID || i_table_id == NIT_OTHER_NETWORK_TABLE_ID )
    {
        if( !dvbpsi_nit_attach( h, i_table_id, i_extension, (dvbpsi_nit_callback)NITCallBack, p_session ) )
            msg_Err( p_session->p_obj, "PSINewTableCallback: failed attaching NITCallback" );
    }
}

static scan_session_t *scan_session_New( scan_t *p_scan, const scan_tuner_config_t *p_cfg )
{
    scan_session_t *p_session = malloc( sizeof( *p_session ) );
    if( unlikely(p_session == NULL) )
        return NULL;
    p_session->p_obj = p_scan->p_obj;
    p_session->cfg = *p_cfg;
    p_session->i_snr = -1;
    p_session->local.p_pat = NULL;
    p_session->local.p_sdt = NULL;
    p_session->local.p_nit = NULL;
    p_session->i_nit_pid = -1;
    p_session->b_use_nit = p_scan->parameter.b_use_nit;
    p_session->type = p_scan->parameter.type;
    p_session->others.i_nit = 0;
    p_session->others.i_sdt = 0;
    p_session->others.pp_nit = NULL;
    p_session->others.pp_sdt = NULL;
    p_session->p_pathandle = NULL;
    p_session->p_sdthandle = NULL;
    p_session->p_nithandle = NULL;
    return p_session;;
}

static void scan_session_Delete( scan_session_t *p_session )
{
    for( size_t i=0; i< p_session->others.i_sdt; i++ )
        dvbpsi_sdt_delete( p_session->others.pp_sdt[i] );
    free( p_session->others.pp_sdt );

    for( size_t i=0; i< p_session->others.i_nit; i++ )
        dvbpsi_nit_delete( p_session->others.pp_nit[i] );
    free( p_session->others.pp_nit );

    if( p_session->p_pathandle )
    {
        dvbpsi_pat_detach( p_session->p_pathandle );
        if( p_session->local.p_pat )
            dvbpsi_pat_delete( p_session->local.p_pat );
    }

    if( p_session->p_sdthandle )
    {
        dvbpsi_DetachDemux( p_session->p_sdthandle );
        if( p_session->local.p_sdt )
            dvbpsi_sdt_delete( p_session->local.p_sdt );
    }

    if( p_session->p_nithandle )
    {
        dvbpsi_DetachDemux( p_session->p_nithandle );
        if( p_session->local.p_nit )
            dvbpsi_nit_delete( p_session->local.p_nit );
    }

    free( p_session );
}

static void scan_session_Destroy( scan_t *p_scan, scan_session_t *p_session )
{
    dvbpsi_pat_t *p_pat = p_session->local.p_pat;
    dvbpsi_sdt_t *p_sdt = p_session->local.p_sdt;
    dvbpsi_nit_t *p_nit = p_session->local.p_nit;

    /* Parse PAT (Declares only local services/programs) */
    if( p_pat )
        ParsePAT( p_scan->p_obj, p_scan, p_pat, &p_session->cfg, p_session->i_snr );

    /* Parse NIT (Declares local services/programs) */
    if( p_nit )
        ParseNIT( p_scan->p_obj, p_scan, p_nit, &p_session->cfg );

    /* Parse SDT (Maps names to programs) */
    if( p_sdt )
        ParseSDT( p_scan->p_obj, p_scan, p_sdt );

    /* Do the same for all other networks */
    for( size_t i=0; i<p_session->others.i_nit; i++ )
        ParseNIT( p_scan->p_obj, p_scan, p_session->others.pp_nit[i], NULL );

    /* Map service name for all other ts/networks */
    for( size_t i=0; i<p_session->others.i_sdt; i++ )
        ParseSDT( p_scan->p_obj, p_scan, p_session->others.pp_sdt[i] );

    /* */
    scan_session_Delete( p_session );
}

static int ScanServiceCmp( const void *a, const void *b )
{
    const scan_service_t *sa = *((const scan_service_t**)a);
    const scan_service_t *sb = *((const scan_service_t**)b);

    if( sa->i_channel == sb->i_channel )
    {
        if( sa->psz_name && sb->psz_name )
            return strcmp( sa->psz_name, sb->psz_name );
        return 0;
    }

    if( sa->i_channel < sb->i_channel )
        return -1;
    else if( sa->i_channel > sb->i_channel )
        return 1;
    return 0;
}

static block_t *BlockString( const char *psz )
{
    block_t *p = block_Alloc( strlen(psz) );
    if( p )
        memcpy( p->p_buffer, psz, p->i_buffer );
    return p;
}

void scan_set_NotifyCB( scan_t *p_scan, scan_service_notify_cb pf )
{
    p_scan->pf_notify_service = pf;
}

const char * scan_service_GetName( const scan_service_t *s )
{
    return s->psz_name;
}

const char * scan_service_GetProvider( const scan_service_t *s )
{
    return s->psz_provider;
}

uint16_t scan_service_GetProgram( const scan_service_t *s )
{
    return s->i_program;
}

const char * scan_service_GetNetworkName( const scan_service_t *s )
{
    if( s->p_mplex )
        return s->p_mplex->psz_network_name;
    else
        return NULL;
}

char * scan_service_GetUri( const scan_service_t *s )
{
    char *psz_mrl = NULL;
    int i_ret = -1;
    switch( s->p_mplex->cfg.type )
    {
        case SCAN_DVB_T:
            i_ret = asprintf( &psz_mrl, "dvb://frequency=%d:bandwidth=%d:modulation=%s",
                              s->p_mplex->cfg.i_frequency,
                              s->p_mplex->cfg.i_bandwidth,
                              scan_value_modulation( s->p_mplex->cfg.modulation ) );
            break;
        case SCAN_DVB_S:
            i_ret = asprintf( &psz_mrl, "dvb://frequency=%d:srate=%d:polarization=%c:fec=%s",
                              s->p_mplex->cfg.i_frequency,
                              s->p_mplex->cfg.i_symbolrate,
                              (char) s->p_mplex->cfg.polarization,
                              scan_value_coderate( s->p_mplex->cfg.inner_fec ) );
            break;
        case SCAN_DVB_C:
            i_ret = asprintf( &psz_mrl, "dvb://frequency=%d:srate=%d:modulation=%s:fec=%s",
                              s->p_mplex->cfg.i_frequency,
                              s->p_mplex->cfg.i_symbolrate,
                              scan_value_modulation( s->p_mplex->cfg.modulation ),
                              scan_value_coderate( s->p_mplex->cfg.inner_fec ) );
        default:
            break;
    }
    return (i_ret >=0) ? psz_mrl : NULL;
}

block_t *scan_GetM3U( scan_t *p_scan )
{
    vlc_object_t *p_obj = p_scan->p_obj;
    block_t *p_playlist = BlockString( "#EXTM3U\n\n" );
    if( !p_playlist )
        return NULL;

    const size_t i_total_services = scan_CountServices( p_scan );
    size_t i_filtered_count = 0;
    const scan_service_t **pp_filtered_list = vlc_alloc( i_total_services, sizeof(scan_service_t *) );
    if( !pp_filtered_list )
    {
        block_Release( p_playlist );
        return NULL;
    }

    for( size_t j = 0; j < p_scan->i_multiplex; j++ )
    {
        const scan_multiplex_t *p_mplex = p_scan->pp_multiplex[j];
        for( size_t i = 0; i < p_mplex->i_services; i++ )
        {
            const scan_service_t *s = p_mplex->pp_services[i];
            if( !scan_service_type_Supported( s->type ) )
            {
                /* We should only select service that have been described by SDT */
                msg_Dbg( p_obj, "scan_GetM3U: ignoring service number %d", s->i_program );
                continue;
            }
            pp_filtered_list[i_filtered_count++] = s;
        }
    }

    /* */
    qsort( pp_filtered_list, i_filtered_count, sizeof(scan_service_t *), ScanServiceCmp );

    for( size_t i = 0; i < i_filtered_count; i++ )
    {
        const scan_service_t *s = pp_filtered_list[i];

        const char *psz_type;
        switch( s->type )
        {
            case SERVICE_TYPE_DIGITAL_TELEVISION:       psz_type = "Digital television"; break;
            case SERVICE_TYPE_DIGITAL_TELEVISION_AC_SD: psz_type = "Digital television advanced codec SD"; break;
            case SERVICE_TYPE_DIGITAL_TELEVISION_AC_HD: psz_type = "Digital television advanced codec HD"; break;
            case SERVICE_TYPE_DIGITAL_RADIO:            psz_type = "Digital radio"; break;
            default:
                psz_type = "Unknown";
                break;
        }
        msg_Warn( p_obj, "scan_GetM3U: service number %d type '%s' name '%s' channel %d cypted=%d|"
                         "network_id %d (nit:%d sdt:%d)| f=%d bw=%d snr=%d modulation=%s",
                  s->i_program, psz_type, s->psz_name, s->i_channel, s->b_crypted,
                  s->p_mplex->i_network_id, s->p_mplex->i_nit_version, s->p_mplex->i_sdt_version,
                  s->p_mplex->cfg.i_frequency, s->p_mplex->cfg.i_bandwidth, s->p_mplex->i_snr,
                  scan_value_modulation( s->p_mplex->cfg.modulation ) );

        char *psz_mrl = scan_service_GetUri( s );
        if( psz_mrl == NULL )
            continue;

        char *psz;
        int i_ret = asprintf( &psz, "#EXTINF:,,%s\n"
                                    "#EXTVLCOPT:program=%d\n"
                                    "%s\n\n",
                          s->psz_name && * s->psz_name ? s->psz_name : "Unknown",
                          s->i_program,
                          psz_mrl );
        free( psz_mrl );
        if( i_ret != -1 )
        {
            block_t *p_block = BlockString( psz );
            if( p_block )
                block_ChainAppend( &p_playlist, p_block );
            free( psz );
        }
    }

    free( pp_filtered_list );

    return p_playlist ? block_ChainGather( p_playlist ) : NULL;
}

#define dvbpsi_packet_push(a,b) dvbpsi_packet_push(a, (uint8_t *)b)

static bool scan_session_Push( scan_session_t *p_scan, const uint8_t *p_packet )
{
    if( p_packet[0] != 0x47 )
        return false;

    /* */
    const int i_pid = ( (p_packet[1]&0x1f)<<8) | p_packet[2];
    if( i_pid == PSI_PAT_PID )
    {
        if( !p_scan->p_pathandle )
        {
            p_scan->p_pathandle = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
            if( !p_scan->p_pathandle )
                return false;

            p_scan->p_pathandle->p_sys = (void *) p_scan->p_obj;
            if( !dvbpsi_pat_attach( p_scan->p_pathandle, (dvbpsi_pat_callback)PATCallBack, p_scan ) )
            {
                dvbpsi_delete( p_scan->p_pathandle );
                p_scan->p_pathandle = NULL;
                return false;
            }
        }
        if( p_scan->p_pathandle )
            dvbpsi_packet_push( p_scan->p_pathandle, p_packet );
    }
    else if( i_pid == SI_SDT_PID )
    {
        if( !p_scan->p_sdthandle )
        {
            p_scan->p_sdthandle = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
            if( !p_scan->p_sdthandle )
                return false;

            p_scan->p_sdthandle->p_sys = (void *) p_scan->p_obj;
            if( !dvbpsi_AttachDemux( p_scan->p_sdthandle, (dvbpsi_demux_new_cb_t)PSINewTableCallBack, p_scan ) )
            {
                dvbpsi_delete( p_scan->p_sdthandle );
                p_scan->p_sdthandle = NULL;
                return false;
            }
        }

        if( p_scan->p_sdthandle )
            dvbpsi_packet_push( p_scan->p_sdthandle, p_packet );
    }
    else if( p_scan->b_use_nit && i_pid == SI_NIT_PID ) /*if( i_pid == p_scan->i_nit_pid )*/
    {
        if( !p_scan->p_nithandle )
        {
            p_scan->p_nithandle = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
            if( !p_scan->p_nithandle )
                return false;

            p_scan->p_nithandle->p_sys = (void *) p_scan->p_obj;
            if( !dvbpsi_AttachDemux( p_scan->p_nithandle, (dvbpsi_demux_new_cb_t)PSINewTableCallBack, p_scan ) )
            {
                dvbpsi_delete( p_scan->p_nithandle );
                p_scan->p_nithandle = NULL;
                return false;
            }
        }
        if( p_scan->p_nithandle )
            dvbpsi_packet_push( p_scan->p_nithandle, p_packet );
    }

    return p_scan->local.p_pat && p_scan->local.p_sdt &&
            (!p_scan->b_use_nit || p_scan->local.p_nit);
}

static unsigned scan_session_GetTablesTimeout( const scan_session_t *p_session )
{
    unsigned i_time = 0;
    if( !p_session->local.p_pat )
    {
        i_time = 500;
    }
    else if( !p_session->local.p_sdt )
    {
        i_time = 2*1000;
    }
    else if( !p_session->local.p_nit && p_session->b_use_nit )
    {
        if( p_session->type == SCAN_DVB_T )
            i_time = 6000;
        else
            i_time = 5000;
    }

    return i_time * 2 * 1000;
}

const char *scan_value_modulation( scan_modulation_t m )
{
    switch(m)
    {
        case SCAN_MODULATION_QAM_16:     return "16QAM";
        case SCAN_MODULATION_QAM_32:     return "32QAM";
        case SCAN_MODULATION_QAM_64:     return "64QAM";
        case SCAN_MODULATION_QAM_128:    return "128QAM";
        case SCAN_MODULATION_QAM_256:    return "256QAM";
        case SCAN_MODULATION_QAM_AUTO:   return "QAM";
        case SCAN_MODULATION_PSK_8:      return "8PSK";
        case SCAN_MODULATION_QPSK:       return "QPSK";
        case SCAN_MODULATION_DQPSK:      return "DQPSK";
        case SCAN_MODULATION_APSK_16:    return "16APSK";
        case SCAN_MODULATION_APSK_32:    return "32APSK";
        case SCAN_MODULATION_VSB_8:      return "8VSB";
        case SCAN_MODULATION_VSB_16:     return "16VSB";
        case SCAN_MODULATION_QAM_4NR:
        case SCAN_MODULATION_AUTO:
        default:                        return "";
    }
}

const char *scan_value_coderate( scan_coderate_t c )
{
    switch( c )
    {
        case SCAN_CODERATE_NONE: return "0";
        case SCAN_CODERATE_1_2:  return "1/2";
        case SCAN_CODERATE_2_3:  return "2/3";
        case SCAN_CODERATE_3_4:  return "3/4";
        case SCAN_CODERATE_3_5:  return "3/5";
        case SCAN_CODERATE_4_5:  return "4/5";
        case SCAN_CODERATE_5_6:  return "5/6";
        case SCAN_CODERATE_7_8:  return "7/8";
        case SCAN_CODERATE_8_9:  return "8/9";
        case SCAN_CODERATE_9_10: return "9/10";
        case SCAN_CODERATE_AUTO:
        default:                 return "";
    }
}
