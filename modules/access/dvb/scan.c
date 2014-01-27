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
#include <vlc_fs.h>
#include <vlc_charset.h>
#include <vlc_access.h>

#include <sys/types.h>

/* Include dvbpsi headers */
#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/descriptor.h>
#include <dvbpsi/pat.h>
#include <dvbpsi/pmt.h>
#include <dvbpsi/dr.h>
#include <dvbpsi/psi.h>
#include <dvbpsi/demux.h>
#include <dvbpsi/sdt.h>
#ifdef _DVBPSI_DR_43_H_
#   define DVBPSI_USE_NIT 1
#   include <dvbpsi/nit.h>
#endif

#include "dvb.h"
#include "scan.h"
#include "../../demux/dvb-text.h"
#include "../../mux/mpeg/dvbpsi_compat.h"

typedef enum
{
    SERVICE_UNKNOWN = 0,
    SERVICE_DIGITAL_RADIO,
    SERVICE_DIGITAL_TELEVISION,
    SERVICE_DIGITAL_TELEVISION_AC_SD,
    SERVICE_DIGITAL_TELEVISION_AC_HD,
} scan_service_type_t;

typedef struct
{
    int  i_program;     /* program number (service id) */
    scan_configuration_t cfg;
    int i_snr;

    scan_service_type_t type;
    char *psz_name;     /* channel name in utf8 or NULL */
    int  i_channel;     /* -1 if unknown */
    bool b_crypted;     /* True if potentially crypted */

    int i_network_id;

    int i_nit_version;
    int i_sdt_version;

} scan_service_t;

struct scan_t
{
    vlc_object_t *p_obj;
    struct dialog_progress_bar_t *p_dialog;
    int64_t i_index;
    scan_parameter_t parameter;
    int64_t i_time_start;

    int            i_service;
    scan_service_t **pp_service;
};

struct scan_session_t
{
    vlc_object_t *p_obj;

    scan_configuration_t cfg;
    int i_snr;

    dvbpsi_handle pat;
    dvbpsi_pat_t *p_pat;
    int i_nit_pid;

    dvbpsi_handle sdt;
    dvbpsi_sdt_t *p_sdt;

#ifdef DVBPSI_USE_NIT
    dvbpsi_handle nit;
    dvbpsi_nit_t *p_nit;
#else
#   warning NIT is not supported by your libdvbpsi version
#endif

};

/* */
static scan_service_t *scan_service_New( int i_program,
                                         const scan_configuration_t *p_cfg )
{
    scan_service_t *p_srv = malloc( sizeof(*p_srv) );
    if( !p_srv )
        return NULL;

    p_srv->i_program = i_program;
    p_srv->cfg = *p_cfg;
    p_srv->i_snr = -1;

    p_srv->type = SERVICE_UNKNOWN;
    p_srv->psz_name = NULL;
    p_srv->i_channel = -1;
    p_srv->b_crypted = false;

    p_srv->i_network_id = -1;
    p_srv->i_nit_version = -1;
    p_srv->i_sdt_version = -1;

    return p_srv;
}

static void scan_service_Delete( scan_service_t *p_srv )
{
    free( p_srv->psz_name );
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

static int scan_service_type( int service_type )
{
    switch( service_type )
    {
    case 0x01: return SERVICE_DIGITAL_TELEVISION; break;
    case 0x02: return SERVICE_DIGITAL_RADIO; break;
    case 0x16: return SERVICE_DIGITAL_TELEVISION_AC_SD; break;
    case 0x19: return SERVICE_DIGITAL_TELEVISION_AC_HD; break;
    default:   return SERVICE_UNKNOWN; break;
    }
}

/* */
scan_t *scan_New( vlc_object_t *p_obj, const scan_parameter_t *p_parameter )
{
    if( p_parameter->type == SCAN_DVB_T )
    {
        msg_Dbg( p_obj, "DVB-T scanning:" );
        msg_Dbg( p_obj, " - frequency [%d, %d]",
                 p_parameter->frequency.i_min, p_parameter->frequency.i_max );
        msg_Dbg( p_obj, " - bandwidth [%d,%d]",
                 p_parameter->bandwidth.i_min, p_parameter->bandwidth.i_max );
        msg_Dbg( p_obj, " - exhaustive mode %s", p_parameter->b_exhaustive ? "on" : "off" );
    }
    else if( p_parameter->type == SCAN_DVB_C )
    {
        msg_Dbg( p_obj, "DVB-C scanning:" );
        msg_Dbg( p_obj, " - frequency [%d, %d]",
                 p_parameter->frequency.i_min, p_parameter->frequency.i_max );
        msg_Dbg( p_obj, " - bandwidth [%d,%d]",
                 p_parameter->bandwidth.i_min, p_parameter->bandwidth.i_max );
        msg_Dbg( p_obj, " - exhaustive mode %s", p_parameter->b_exhaustive ? "on" : "off" );
        msg_Dbg( p_obj, " - scannin modulations %s", p_parameter->b_modulation_set ? "off" : "on" );
    }
    else if( p_parameter->type == SCAN_DVB_S )
    {
        msg_Dbg( p_obj, "DVB-S scanning:" );
        msg_Dbg( p_obj, " - satellite [%s]", p_parameter->sat_info.psz_name );
    }
    else
    {
        return NULL;
    }
    msg_Dbg( p_obj, " - use NIT %s", p_parameter->b_use_nit ? "on" : "off" );
    msg_Dbg( p_obj, " - FTA only %s", p_parameter->b_free_only ? "on" : "off" );

    scan_t *p_scan = malloc( sizeof( *p_scan ) );
    if( unlikely(p_scan == NULL) )
        return NULL;

    p_scan->p_obj = VLC_OBJECT(p_obj);
    p_scan->i_index = 0;
    p_scan->p_dialog = NULL;
    TAB_INIT( p_scan->i_service, p_scan->pp_service );
    p_scan->parameter = *p_parameter;
    p_scan->i_time_start = mdate();

    return p_scan;
}

void scan_Destroy( scan_t *p_scan )
{
    if( p_scan->p_dialog != NULL )
        dialog_ProgressDestroy( p_scan->p_dialog );

    for( int i = 0; i < p_scan->i_service; i++ )
        scan_service_Delete( p_scan->pp_service[i] );
    TAB_CLEAN( p_scan->i_service, p_scan->pp_service );
    free( p_scan );
}

static int ScanDvbSNextFast( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
{
    msg_Dbg( p_scan->p_obj, "Scan index %"PRId64, p_scan->i_index );

    int *pi_count = &p_scan->parameter.sat_info.i_count;

    if( !p_scan->parameter.sat_info.psz_name )
    {
        msg_Err( p_scan->p_obj, "no satellite selected" );
        return VLC_EGENERIC;
    }

    /* if there are no transponders in mem, laod from config file */
    if( !*pi_count )
    {
        DIR *p_dir;

        char *psz_dir = NULL;
        char *data_dir = config_GetDataDir();

        if( asprintf( &psz_dir, "%s" DIR_SEP "dvb" DIR_SEP "dvb-s", data_dir ) == -1 )
            psz_dir = NULL;
        free( data_dir );

        if( !psz_dir )
        {
            free( p_scan->parameter.sat_info.psz_name );
            return VLC_EGENERIC;
        }

        /* open config directory */
        if( !( p_dir = vlc_opendir( psz_dir ) ) )
        {
            msg_Err( p_scan->p_obj, "could not open satellite info directory (%s)", psz_dir );
            free( p_scan->parameter.sat_info.psz_name );
            return VLC_EGENERIC;
        }

        /* find the requested file in the directory */
        for( ; ; ) {
            char *psz_filename;

            if( ! (psz_filename = vlc_readdir( p_dir ) ) )
                break;

            if( !strncmp( p_scan->parameter.sat_info.psz_name, psz_filename, 20 ) )
            {
                if( asprintf( &p_scan->parameter.sat_info.psz_path, "%s" DIR_SEP "%s", psz_dir, psz_filename ) == -1 )
                    p_scan->parameter.sat_info.psz_path = NULL;

                free( psz_filename );
                break;
            }
            free( psz_filename );
        }

        closedir( p_dir );

        if( !p_scan->parameter.sat_info.psz_path )
        {
            msg_Err( p_scan->p_obj, "could not find satellite config (%s)", p_scan->parameter.sat_info.psz_name );
            free( p_scan->parameter.sat_info.psz_name );
            return VLC_EGENERIC;
        }

        msg_Dbg( p_scan->p_obj, "using satellite config file (%s)", p_scan->parameter.sat_info.psz_path );

        FILE *f = vlc_fopen( p_scan->parameter.sat_info.psz_path, "r" );

        /* parse file */
        if( f )
        {
            scan_dvbs_transponder_t *p_transponders = malloc( sizeof( scan_dvbs_transponder_t ) );
            char type;
            char psz_fec[3];

            int res;
            do
            {
                if ( ( res = fscanf( f, "%c %d %c %d %2s\n",
                            &type,
                            &p_transponders[*pi_count].i_frequency,
                            &p_transponders[*pi_count].c_polarization,
                            &p_transponders[*pi_count].i_symbol_rate,
                            psz_fec ) ) != 5 )
                {
                    msg_Dbg( p_scan->p_obj, "error parsing transponder from file" );
                    continue;
                }

                /* decode fec */
                char psz_fec_list[] = "1/22/33/44/55/66/77/88/9";
                char *p_fec = strstr( psz_fec_list, psz_fec );
                if ( !p_fec )
                    p_transponders[*pi_count].i_fec = 9;    /* FEC_AUTO */
                else
                    p_transponders[*pi_count].i_fec = 1 + ( ( p_fec-psz_fec_list ) / 3 );

                (*pi_count)++;

                p_transponders = realloc(p_transponders, ( *pi_count + 1 ) * sizeof( scan_dvbs_transponder_t ) );
            } while (res != EOF);

            msg_Dbg( p_scan->p_obj, "parsed %d transponders from config", *pi_count);

            fclose( f );
            p_scan->parameter.sat_info.p_transponders = p_transponders;
        }
        else
        {
            msg_Err( p_scan->p_obj, "failed to open satellite file (%s)", p_scan->parameter.sat_info.psz_path );
            free( p_scan->parameter.sat_info.psz_name );
            free( p_scan->parameter.sat_info.psz_path );
            return VLC_EGENERIC;
        }
        free( p_scan->parameter.sat_info.psz_name );
        free( p_scan->parameter.sat_info.psz_path );
    }

    if( p_scan->i_index < *pi_count )
    {
        /* setup params for scan */
        p_cfg->i_symbol_rate = p_scan->parameter.sat_info.p_transponders[p_scan->i_index].i_symbol_rate / 1000;
        p_cfg->i_frequency = p_scan->parameter.sat_info.p_transponders[p_scan->i_index].i_frequency;
        p_cfg->i_fec = p_scan->parameter.sat_info.p_transponders[p_scan->i_index].i_fec;
        p_cfg->c_polarization = p_scan->parameter.sat_info.p_transponders[p_scan->i_index].c_polarization;

        msg_Dbg( p_scan->p_obj,
                 "transponder [%"PRId64"/%d]: frequency=%d, symbolrate=%d, fec=%d, polarization=%c",
                 p_scan->i_index + 1,
                 *pi_count,
                 p_cfg->i_frequency,
                 p_cfg->i_symbol_rate,
                 p_cfg->i_fec,
                 p_cfg->c_polarization );

        *pf_pos = (double)p_scan->i_index / *pi_count;

        return VLC_SUCCESS;
    }

    if( p_scan->parameter.sat_info.p_transponders )
    {
        free( p_scan->parameter.sat_info.p_transponders );
        p_scan->parameter.sat_info.p_transponders = NULL;
    }

    return VLC_EGENERIC;
}

static int ScanDvbCNextFast( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
{
    msg_Dbg( p_scan->p_obj, "Scan index %"PRId64, p_scan->i_index );
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

    if( p_scan->i_index < num_frequencies )
    {
        p_cfg->i_frequency = 10000 * ( frequencies[ p_scan->i_index ] );
        *pf_pos = (double)(p_scan->i_index * 1000 +
                           p_scan->parameter.i_symbolrate * 100 +
                           (256 - (p_scan->parameter.i_modulation >> 4)) )
                           / (num_frequencies * 1000 + 900 + 16);
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static int ScanDvbNextExhaustive( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
{
    if( p_scan->i_index > p_scan->parameter.frequency.i_count * p_scan->parameter.bandwidth.i_count )
        return VLC_EGENERIC;

    const int i_bi = p_scan->i_index % p_scan->parameter.bandwidth.i_count;
    const int i_fi = p_scan->i_index / p_scan->parameter.bandwidth.i_count;

    p_cfg->i_frequency = p_scan->parameter.frequency.i_min + i_fi * p_scan->parameter.frequency.i_step;
    p_cfg->i_bandwidth = p_scan->parameter.bandwidth.i_min + i_bi * p_scan->parameter.bandwidth.i_step;

    *pf_pos = (double)p_scan->i_index / p_scan->parameter.frequency.i_count;
    return VLC_SUCCESS;
}

static int ScanDvbTNextFast( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
{
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
    for( ;; p_scan->i_index++ )
    {

        const int i_bi = p_scan->i_index % p_scan->parameter.bandwidth.i_count;
        const int i_oi = (p_scan->i_index / p_scan->parameter.bandwidth.i_count) % i_offset_count;
        const int i_fi = (p_scan->i_index / p_scan->parameter.bandwidth.i_count) / i_offset_count;

        const int i_bandwidth = p_scan->parameter.bandwidth.i_min + i_bi * p_scan->parameter.bandwidth.i_step;
        int i;

        for( i = 0; i < i_band_count; i++ )
        {
            if( i_fi >= band[i].i_min && i_fi <= band[i].i_max )
                break;
        }
        if( i >=i_band_count )
        {
            if( i_fi > band[i_band_count-1].i_max )
                return VLC_EGENERIC;
            continue;
        }

        const int i_frequency_min = band[i].i_min*i_mhz + i_bandwidth*i_mhz/2;
        const int i_frequency_base = i_fi*i_mhz;

        if( i_frequency_base >= i_frequency_min && ( i_frequency_base - i_frequency_min ) % ( i_bandwidth*i_mhz ) == 0 )
        {
            const int i_frequency = i_frequency_base + ( i_oi - i_offset_count/2 ) * p_scan->parameter.frequency.i_step;

            if( i_frequency < p_scan->parameter.frequency.i_min ||
                i_frequency > p_scan->parameter.frequency.i_max )
                continue;

            p_cfg->i_frequency = i_frequency;
            p_cfg->i_bandwidth = i_bandwidth;

            int i_current = 0, i_total = 0;
            for( int i = 0; i < i_band_count; i++ )
            {
                const int i_frag = band[i].i_max-band[i].i_min;

                if( i_fi >= band[i].i_min )
                    i_current += __MIN( i_fi - band[i].i_min, i_frag );
                i_total += i_frag;
            }

            *pf_pos = (double)( i_current + (double)i_oi / i_offset_count ) / i_total;
            return VLC_SUCCESS;
        }
    }
}

static int ScanDvbCNext( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
{
    bool b_servicefound = false;
#ifdef _DVBPSI_DR_44_H_
    /* We iterate frequencies/modulations/symbolrates until we get first hit and find NIT,
       from that we fill pp_service with configurations and after that we iterate over
       pp_services for all that doesn't have name yet (tune to that cfg and get SDT and name
       for channel).
     */
    for( int i = 0; i < p_scan->i_service; i++ )
    {
        /* We found radio/tv config that doesn't have a name,
           lets tune to that mux
         */
        if( !p_scan->pp_service[i]->psz_name && ( p_scan->pp_service[i]->type != SERVICE_UNKNOWN ) )
        {
            p_cfg->i_frequency  = p_scan->pp_service[i]->cfg.i_frequency;
            p_cfg->i_symbolrate = p_scan->pp_service[i]->cfg.i_symbolrate;
            p_cfg->i_modulation = p_scan->pp_service[i]->cfg.i_modulation;
            p_scan->i_index = i+1;
            msg_Dbg( p_scan->p_obj, "iterating to freq: %u, symbolrate %u, "
                     "modulation %u index %"PRId64"/%d",
                     p_cfg->i_frequency, p_cfg->i_symbolrate, p_cfg->i_modulation, p_scan->i_index, p_scan->i_service );
            *pf_pos = (double)i/p_scan->i_service;
            return VLC_SUCCESS;
        }
    }
    /* We should have iterated all channels by now */
    if( p_scan->i_service )
        return VLC_EGENERIC;
#else
    /* fallback to old, so when we get one channe, use that
       symbolrate/modulation until bitter end
     */
    for( int i=0; i < p_scan->i_service; i++ )
    {
        b_servicefound = p_scan->pp_service[i]->type != SERVICE_UNKNOWN;
        if( b_servicefound )
            break;
    }
#endif

    if( !b_servicefound )
    {
        bool b_rotate=true;
        if( !p_scan->parameter.b_modulation_set )
        {
            p_scan->parameter.i_modulation = (p_scan->parameter.i_modulation >> 1 );
            /* if we iterated all modulations, move on */
            /* dvb utils dvb-c channels files seems to have only
               QAM64...QAM256, so lets just iterate over those */
            if( p_scan->parameter.i_modulation < 64)
            {
                p_scan->parameter.i_modulation = 256;
            } else {
                b_rotate=false;
            }
            msg_Dbg( p_scan->p_obj, "modulation %d ", p_scan->parameter.i_modulation);
        }
        if( !p_scan->parameter.b_symbolrate_set )
        {
            /* symbol rates from dvb-tools dvb-c files */
            static const unsigned short symbolrates[] = {
             6900, 6875, 6950
             /* With DR_44 we can cover other symbolrates from NIT-info
                as all channel-seed files have atleast one channel that
                has one of these symbolrate
              */
#ifndef _DVBPSI_DR_44_H_
             ,7000, 3450, 6111,
             6428, 6952, 5900, 5000
#endif
             };

            enum { num_symbols = (sizeof(symbolrates)/sizeof(*symbolrates)) };

            /* if we rotated modulations, rotate symbolrate */
            if( b_rotate )
            {
                p_scan->parameter.i_symbolrate++;
                p_scan->parameter.i_symbolrate %= num_symbols;
            }
            p_cfg->i_symbolrate = 1000 * (symbolrates[ p_scan->parameter.i_symbolrate ] );
            msg_Dbg( p_scan->p_obj, "symbolrate %d", p_cfg->i_symbolrate );
            if( p_scan->parameter.i_symbolrate )
                b_rotate=false;
        }
        if( !b_rotate && p_scan->i_index )
            p_scan->i_index--;
    }
    p_cfg->i_modulation = p_scan->parameter.i_modulation;
    if( !p_cfg->i_symbolrate )
        p_cfg->i_symbolrate = var_GetInteger( p_scan->p_obj, "dvb-srate" );

    if( p_scan->parameter.b_exhaustive )
        return ScanDvbNextExhaustive( p_scan, p_cfg, pf_pos );
    else
        return ScanDvbCNextFast( p_scan, p_cfg, pf_pos );
}

static int ScanDvbTNext( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
{
    if( p_scan->parameter.b_exhaustive )
        return ScanDvbNextExhaustive( p_scan, p_cfg, pf_pos );
    else
        return ScanDvbTNextFast( p_scan, p_cfg, pf_pos );
}

static int ScanDvbSNext( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
{
    if( p_scan->parameter.b_exhaustive )
        msg_Dbg( p_scan->p_obj, "no exhaustive svb-d scan mode" );

    return ScanDvbSNextFast( p_scan, p_cfg, pf_pos );
}

int scan_Next( scan_t *p_scan, scan_configuration_t *p_cfg )
{
    double f_position;
    int i_ret;

    if( scan_IsCancelled( p_scan ) )
        return VLC_EGENERIC;

    memset( p_cfg, 0, sizeof(*p_cfg) );
    switch( p_scan->parameter.type )
    {
    case SCAN_DVB_T:
        i_ret = ScanDvbTNext( p_scan, p_cfg, &f_position );
        break;
    case SCAN_DVB_C:
        i_ret = ScanDvbCNext( p_scan, p_cfg, &f_position );
        break;
    case SCAN_DVB_S:
        i_ret = ScanDvbSNext( p_scan, p_cfg, &f_position );
        break;
    default:
        i_ret = VLC_EGENERIC;
        break;
    }

    if( i_ret )
        return i_ret;

    char *psz_text;
    int i_service = 0;

    for( int i = 0; i < p_scan->i_service; i++ )
    {
        if( p_scan->pp_service[i]->psz_name )
            i_service++;
    }

    const mtime_t i_eta = f_position > 0.005 ? (mdate() - p_scan->i_time_start) * ( 1.0 / f_position - 1.0 ) : -1;
    char psz_eta[MSTRTIME_MAX_SIZE];

    if( asprintf( &psz_text, _("%.1f MHz (%d services)\n~%s remaining"),
                  (double)p_cfg->i_frequency / 1000000, i_service, secstotimestr( psz_eta, i_eta/1000000 ) ) >= 0 )
    {
        if( i_eta >= 0 )
            msg_Info( p_scan->p_obj, "Scan ETA %s | %f", secstotimestr( psz_eta, i_eta/1000000 ), f_position * 100 );

        if( p_scan->p_dialog == NULL )
            p_scan->p_dialog = dialog_ProgressCreate( p_scan->p_obj, _("Scanning DVB"), psz_text, _("Cancel") );
        if( p_scan->p_dialog != NULL )
            dialog_ProgressSet( p_scan->p_dialog, psz_text, f_position );
        free( psz_text );
    }

    p_scan->i_index++;
    return VLC_SUCCESS;
}

bool scan_IsCancelled( scan_t *p_scan )
{
    return p_scan->p_dialog && dialog_ProgressCancelled( p_scan->p_dialog );
}

static scan_service_t *ScanFindService( scan_t *p_scan, int i_service_start, int i_program )
{
    for( int i = i_service_start; i < p_scan->i_service; i++ )
    {
        if( p_scan->pp_service[i]->i_program == i_program )
            return p_scan->pp_service[i];
    }
    return NULL;
}

/* FIXME handle properly string (convert to utf8) */
static void PATCallBack( scan_session_t *p_session, dvbpsi_pat_t *p_pat )
{
    vlc_object_t *p_obj = p_session->p_obj;

    msg_Dbg( p_obj, "PATCallBack" );

    /* */
    if( p_session->p_pat && p_session->p_pat->b_current_next )
    {
        dvbpsi_DeletePAT( p_session->p_pat );
        p_session->p_pat = NULL;
    }
    if( p_session->p_pat )
    {
        dvbpsi_DeletePAT( p_pat );
        return;
    }

    dvbpsi_pat_program_t *p_program;

    /* */
    p_session->p_pat = p_pat;

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
static void SDTCallBack( scan_session_t *p_session, dvbpsi_sdt_t *p_sdt )
{
    vlc_object_t *p_obj = p_session->p_obj;

    msg_Dbg( p_obj, "SDTCallBack" );

    if( p_session->p_sdt && p_session->p_sdt->b_current_next )
    {
        dvbpsi_DeleteSDT( p_session->p_sdt );
        p_session->p_sdt = NULL;
    }
    if( p_session->p_sdt )
    {
        dvbpsi_DeleteSDT( p_sdt );
        return;
    }

    /* */
    p_session->p_sdt = p_sdt;

    /* */
    msg_Dbg( p_obj, "new SDT ts_id=%d version=%d current_next=%d network_id=%d",
#if (DVBPSI_VERSION_INT >= DVBPSI_VERSION_WANTED(1,0,0))
             p_sdt->i_extension,
#else
             p_sdt->i_ts_id,
#endif
             p_sdt->i_version, p_sdt->b_current_next,
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
                char str2[257];

                memcpy( str2, pD->i_service_name, pD->i_service_name_length );
                str2[pD->i_service_name_length] = '\0';

                msg_Dbg( p_obj, "    - type=%d name=%s",
                         pD->i_service_type, str2 );
            }
            else
            {
                msg_Dbg( p_obj, "    * dsc 0x%x", p_dr->i_tag );
            }
        }
    }
}

#ifdef DVBPSI_USE_NIT
static void NITCallBack( scan_session_t *p_session, dvbpsi_nit_t *p_nit )
{
    vlc_object_t *p_obj = p_session->p_obj;
    access_t *p_access = (access_t*)p_obj;
    access_sys_t *p_sys = p_access->p_sys;
    scan_t *p_scan = p_sys->scan;

    msg_Dbg( p_obj, "NITCallBack" );
    msg_Dbg( p_obj, "new NIT network_id=%d version=%d current_next=%d",
             p_nit->i_network_id, p_nit->i_version, p_nit->b_current_next );

    /* */
    if( p_session->p_nit && p_session->p_nit->b_current_next )
    {
        dvbpsi_DeleteNIT( p_session->p_nit );
        p_session->p_nit = NULL;
    }
    if( p_session->p_nit )
    {
        dvbpsi_DeleteNIT( p_nit );
        return;
    }

    /* */
    p_session->p_nit = p_nit;

    dvbpsi_descriptor_t *p_dsc;
    for( p_dsc = p_nit->p_first_descriptor; p_dsc != NULL; p_dsc = p_dsc->p_next )
    {
        if( p_dsc->i_tag == 0x40 )
        {
            msg_Dbg( p_obj, "   * network name descriptor" );
            char str1[257];

            memcpy( str1, p_dsc->p_data, p_dsc->i_length );
            str1[p_dsc->i_length] = '\0';
            msg_Dbg( p_obj, "       * name %s", str1 );
        }
        else if( p_dsc->i_tag == 0x4a )
        {
            msg_Dbg( p_obj, "   * linkage descriptor" );
            uint16_t i_ts_id = GetWBE( &p_dsc->p_data[0] );
            uint16_t i_on_id = GetWBE( &p_dsc->p_data[2] );
            uint16_t i_service_id = GetWBE( &p_dsc->p_data[4] );
            int i_linkage_type = p_dsc->p_data[6];

            msg_Dbg( p_obj, "       * ts_id %d", i_ts_id );
            msg_Dbg( p_obj, "       * on_id %d", i_on_id );
            msg_Dbg( p_obj, "       * service_id %d", i_service_id );
            msg_Dbg( p_obj, "       * linkage_type %d", i_linkage_type );
        }
        else 
        {
            msg_Dbg( p_obj, "   * dsc 0x%x", p_dsc->i_tag );
        }
    }

    dvbpsi_nit_ts_t *p_ts;
    for( p_ts = p_nit->p_first_ts; p_ts != NULL; p_ts = p_ts->p_next )
    {
        msg_Dbg( p_obj, "   * ts ts_id=0x%x original_id=0x%x", p_ts->i_ts_id, p_ts->i_orig_network_id );

        uint32_t i_private_data_id = 0;
        dvbpsi_descriptor_t *p_dsc;
        scan_configuration_t *p_cfg = malloc(sizeof(*p_cfg));
        if(!p_cfg) return;
        memset(p_cfg,0,sizeof(*p_cfg));
        for( p_dsc = p_ts->p_first_descriptor; p_dsc != NULL; p_dsc = p_dsc->p_next )
        {
            if( p_dsc->i_tag == 0x41 )
            {
                msg_Dbg( p_obj, "       * service list descriptor" );
                for( int i = 0; i < p_dsc->i_length/3; i++ )
                {
                    uint16_t i_service_id = GetWBE( &p_dsc->p_data[3*i+0] );
                    uint8_t  i_service_type = p_dsc->p_data[3*i+2];
                    msg_Dbg( p_obj, "           * service_id=%d type=%d", i_service_id, i_service_type );
#ifdef _DVBPSI_DR_44_H_
                    if( (ScanFindService( p_scan, 0, i_service_id ) == NULL) &&
                         scan_service_type( i_service_type ) != SERVICE_UNKNOWN )
                    {
                       scan_service_t *s = scan_service_New( i_service_id, p_cfg );
                       s->type          = scan_service_type( i_service_type );
                       s->i_network_id  = p_nit->i_network_id;
                       s->i_nit_version = p_nit->i_version;
                       TAB_APPEND( p_scan->i_service, p_scan->pp_service, s );
                    }
#endif
                }
            }
            else if( p_dsc->i_tag == 0x5a )
            {
                dvbpsi_terr_deliv_sys_dr_t *p_t = dvbpsi_DecodeTerrDelivSysDr( p_dsc );
                msg_Dbg( p_obj, "       * terrestrial delivery system" );
                msg_Dbg( p_obj, "           * centre_frequency 0x%x", p_t->i_centre_frequency  );
                msg_Dbg( p_obj, "           * bandwidth %d", 8 - p_t->i_bandwidth );
                msg_Dbg( p_obj, "           * constellation %d", p_t->i_constellation );
                msg_Dbg( p_obj, "           * hierarchy %d", p_t->i_hierarchy_information );
                msg_Dbg( p_obj, "           * code_rate hp %d lp %d", p_t->i_code_rate_hp_stream, p_t->i_code_rate_lp_stream );
                msg_Dbg( p_obj, "           * guard_interval %d", p_t->i_guard_interval );
                msg_Dbg( p_obj, "           * transmission_mode %d", p_t->i_transmission_mode );
                msg_Dbg( p_obj, "           * other_frequency_flag %d", p_t->i_other_frequency_flag );
            }
#ifdef _DVBPSI_DR_44_H_
            else if( p_dsc->i_tag == 0x44 )
            {
                dvbpsi_cable_deliv_sys_dr_t *p_t = dvbpsi_DecodeCableDelivSysDr( p_dsc );
                msg_Dbg( p_obj, "       * Cable delivery system");

                p_cfg->i_frequency =  decode_BCD( p_t->i_frequency ) * 100;
                msg_Dbg( p_obj, "           * frequency %d", p_cfg->i_frequency );
                p_cfg->i_symbolrate =  decode_BCD( p_t->i_symbol_rate ) * 100;
                msg_Dbg( p_obj, "           * symbolrate %u", p_cfg->i_symbolrate );
                p_cfg->i_modulation = (8 << p_t->i_modulation);
                msg_Dbg( p_obj, "           * modulation %u", p_cfg->i_modulation );
            }
#endif
            else if( p_dsc->i_tag == 0x5f )
            {
                msg_Dbg( p_obj, "       * private data specifier descriptor" );
                i_private_data_id = GetDWBE( &p_dsc->p_data[0] );
                msg_Dbg( p_obj, "           * value 0x%8.8x", i_private_data_id );
            }
            else if( i_private_data_id == 0x28 && p_dsc->i_tag == 0x83 )
            {
                msg_Dbg( p_obj, "       * logical channel descriptor (EICTA)" );
                for( int i = 0; i < p_dsc->i_length/4; i++ )
                {
                    uint16_t i_service_id = GetWBE( &p_dsc->p_data[4*i+0] );
                    int i_channel_number = GetWBE( &p_dsc->p_data[4*i+2] ) & 0x3ff;
                    msg_Dbg( p_obj, "           * service_id=%d channel_number=%d", i_service_id, i_channel_number );
                    scan_service_t *s = ScanFindService( p_scan, 0, i_service_id );
                    if( s && s->i_channel < 0 ) s->i_channel = i_channel_number;
                }

            }
            else
            {
                msg_Warn( p_obj, "       * dsc 0x%x", p_dsc->i_tag );
            }
        }
    }
}
#endif

#if (DVBPSI_VERSION_INT >= DVBPSI_VERSION_WANTED(1,0,0))
static void PSINewTableCallBack( dvbpsi_t *h, uint8_t i_table_id, uint16_t i_extension, void *p_data )
{
    scan_session_t *p_session = (scan_session_t *)p_data;

    if( i_table_id == 0x42 )
    {
        if( !dvbpsi_sdt_attach( h, i_table_id, i_extension, (dvbpsi_sdt_callback)SDTCallBack, p_session ) )
            msg_Err( p_session->p_obj, "PSINewTableCallback: failed attaching SDTCallback" );
    }
    else if( i_table_id == 0x40 || i_table_id == 0x41 )
    {
        if( !dvbpsi_nit_attach( h, i_table_id, i_extension, (dvbpsi_nit_callback)NITCallBack, p_session ) )
            msg_Err( p_session->p_obj, "PSINewTableCallback: failed attaching NITCallback" );
    }
}
#else
static void PSINewTableCallBack( scan_session_t *p_session, dvbpsi_handle h, uint8_t  i_table_id, uint16_t i_extension )
{
    if( i_table_id == 0x42 )
        dvbpsi_AttachSDT( h, i_table_id, i_extension, (dvbpsi_sdt_callback)SDTCallBack, p_session );
# ifdef DVBPSI_USE_NIT
    else if( i_table_id == 0x40 || i_table_id == 0x41 )
        dvbpsi_AttachNIT( h, i_table_id, i_extension, (dvbpsi_nit_callback)NITCallBack, p_session );
# endif
}
#endif

scan_session_t *scan_session_New( vlc_object_t *p_obj,
                                  const scan_configuration_t *p_cfg )
{
    scan_session_t *p_session = malloc( sizeof( *p_session ) );
    if( unlikely(p_session == NULL) )
        return NULL;
    p_session->p_obj = p_obj;
    p_session->cfg = *p_cfg;
    p_session->i_snr = -1;
    p_session->pat = NULL;
    p_session->p_pat = NULL;
    p_session->i_nit_pid = -1;
    p_session->sdt = NULL;
    p_session->p_sdt = NULL;
#ifdef DVBPSI_USE_NIT
    p_session->nit = NULL;
    p_session->p_nit = NULL;
#endif
    return p_session;;
}

void scan_session_Destroy( scan_t *p_scan, scan_session_t *p_session )
{
    const int i_service_start = p_scan->i_service;

    dvbpsi_pat_t *p_pat = p_session->p_pat;
    dvbpsi_sdt_t *p_sdt = p_session->p_sdt;

#ifdef DVBPSI_USE_NIT
    dvbpsi_nit_t *p_nit = p_session->p_nit;
#endif

    if( p_pat )
    {
        /* Parse PAT */
        dvbpsi_pat_program_t *p_program;
        for( p_program = p_pat->p_first_program; p_program != NULL; p_program = p_program->p_next )
        {
            if( p_program->i_number == 0 )  /* NIT */
                continue;

            scan_service_t *s = ScanFindService( p_scan, 0, p_program->i_number );
            if( s == NULL )
            {
                s = scan_service_New( p_program->i_number, &p_session->cfg );
                TAB_APPEND( p_scan->i_service, p_scan->pp_service, s );
            }
        }
    }
    /* Parse SDT */
    if( p_pat && p_sdt )
    {
        dvbpsi_sdt_service_t *p_srv;
        for( p_srv = p_sdt->p_first_service; p_srv; p_srv = p_srv->p_next )
        {
            scan_service_t *s = ScanFindService( p_scan, 0, p_srv->i_service_id );
            dvbpsi_descriptor_t *p_dr;

            if( s )
                s->b_crypted = p_srv->b_free_ca;

            for( p_dr = p_srv->p_first_descriptor; p_dr; p_dr = p_dr->p_next )
            {
                if( p_dr->i_tag == 0x48 )
                {
                    dvbpsi_service_dr_t *pD = dvbpsi_DecodeServiceDr( p_dr );

                    if( s )
                    {
                        if( !s->psz_name )
                            s->psz_name = vlc_from_EIT( pD->i_service_name,
                                                   pD->i_service_name_length );

                        if( s->type == SERVICE_UNKNOWN )
                            s->type = scan_service_type( pD->i_service_type );
                    }
                }
            }
        }
    }

#ifdef DVBPSI_USE_NIT
    /* Parse NIT */
    if( p_pat && p_nit )
    {
        dvbpsi_nit_ts_t *p_ts;
        for( p_ts = p_nit->p_first_ts; p_ts != NULL; p_ts = p_ts->p_next )
        {
            uint32_t i_private_data_id = 0;
            dvbpsi_descriptor_t *p_dsc;

            if( p_ts->i_orig_network_id != p_nit->i_network_id || p_ts->i_ts_id != p_pat->i_ts_id )
                continue;

            for( p_dsc = p_ts->p_first_descriptor; p_dsc != NULL; p_dsc = p_dsc->p_next )
            {
                if( p_dsc->i_tag == 0x5f )
                {
                    i_private_data_id = GetDWBE( &p_dsc->p_data[0] );
                }
                else if( i_private_data_id == 0x28 && p_dsc->i_tag == 0x83 )
                {
                    for( int i = 0; i < p_dsc->i_length/4; i++ )
                    {
                        uint16_t i_service_id = GetWBE( &p_dsc->p_data[4*i+0] );
                        int i_channel_number = GetWBE( &p_dsc->p_data[4*i+2] ) & 0x3ff;

                        scan_service_t *s = ScanFindService( p_scan, i_service_start, i_service_id );
                        if( s && s->i_channel < 0 )
                            s->i_channel = i_channel_number;
                    }
                }
            }
        }
    }
#endif

    /* */
    for( int i = i_service_start; i < p_scan->i_service; i++ )
    {
        scan_service_t *p_srv = p_scan->pp_service[i];

        p_srv->i_snr = p_session->i_snr;
        if( p_sdt )
            p_srv->i_sdt_version = p_sdt->i_version;
#ifdef DVBPSI_USE_NIT
        if( p_nit )
        {
            p_srv->i_network_id = p_nit->i_network_id;
            p_srv->i_nit_version = p_nit->i_version;
        }
#endif
    }

    /* */
    if( p_session->pat )
        dvbpsi_DetachPAT( p_session->pat );
    if( p_session->p_pat )
        dvbpsi_DeletePAT( p_session->p_pat );

    if( p_session->sdt )
        dvbpsi_DetachDemux( p_session->sdt );
    if( p_session->p_sdt )
        dvbpsi_DeleteSDT( p_session->p_sdt );

#ifdef DVBPSI_USE_NIT
    if( p_session->nit )
        dvbpsi_DetachDemux( p_session->nit );
    if( p_session->p_nit )
        dvbpsi_DeleteNIT( p_session->p_nit );
#endif
    free( p_session );
}

static int ScanServiceCmp( const void *a, const void *b )
{
    scan_service_t *sa = *(scan_service_t**)a;
    scan_service_t *sb = *(scan_service_t**)b;

    if( sa->i_channel == sb->i_channel )
    {
        if( sa->psz_name && sb->psz_name )
            return strcmp( sa->psz_name, sb->psz_name );
        return 0;
    }
    if( sa->i_channel == -1 )
        return 1;
    else if( sb->i_channel == -1 )
        return -1;

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

block_t *scan_GetM3U( scan_t *p_scan )
{
    vlc_object_t *p_obj = p_scan->p_obj;
    block_t *p_playlist = NULL;

    if( p_scan->i_service <= 0 )
        return NULL;

    /* */
    qsort( p_scan->pp_service, p_scan->i_service, sizeof(scan_service_t*), ScanServiceCmp );

    /* */
    p_playlist = BlockString( "#EXTM3U\n\n" );/* */

    for( int i = 0; i < p_scan->i_service; i++ )
    {
        scan_service_t *s = p_scan->pp_service[i];

        if( s->type == SERVICE_UNKNOWN )
        {
            /* We should only select service that have been described by SDT */
            msg_Dbg( p_obj, "scan_GetM3U: ignoring service number %d", s->i_program );
            continue;
        }

        const char *psz_type;
        switch( s->type )
        {
        case SERVICE_DIGITAL_TELEVISION:       psz_type = "Digital television"; break;
        case SERVICE_DIGITAL_TELEVISION_AC_SD: psz_type = "Digital television advanced codec SD"; break;
        case SERVICE_DIGITAL_TELEVISION_AC_HD: psz_type = "Digital television advanced codec HD"; break;
        case SERVICE_DIGITAL_RADIO:            psz_type = "Digital radio"; break;
        default:
            psz_type = "Unknown";
            break;
        }
        msg_Warn( p_obj, "scan_GetM3U: service number %d type '%s' name '%s' channel %d cypted=%d| network_id %d (nit:%d sdt:%d)| f=%d bw=%d snr=%d modulation=%d",
                  s->i_program, psz_type, s->psz_name, s->i_channel, s->b_crypted,
                  s->i_network_id, s->i_nit_version, s->i_sdt_version,
                  s->cfg.i_frequency, s->cfg.i_bandwidth, s->i_snr, s->cfg.i_modulation );

        if( !s->cfg.i_fec )
            s->cfg.i_fec = 9;   /* FEC_AUTO */

        char *psz;
        if( asprintf( &psz, "#EXTINF:,,%s\n"
                        "#EXTVLCOPT:program=%d\n"
                        "dvb://frequency=%d:bandwidth=%d:voltage=%d:fec=%d:modulation=%d:srate=%d\n"
                        "\n",
                      s->psz_name && * s->psz_name ? s->psz_name : "Unknown",
                      s->i_program,
                      s->cfg.i_frequency,
                      s->cfg.i_bandwidth,
                      s->cfg.c_polarization == 'H' ? 18 : 13,
                      s->cfg.i_fec,
                      s->cfg.i_modulation,
                      s->cfg.i_symbolrate ) < 0 )
            psz = NULL;
        if( psz )
        {
            block_t *p_block = BlockString( psz );
            if( p_block )
                block_ChainAppend( &p_playlist, p_block );
        }
    }

    return p_playlist ? block_ChainGather( p_playlist ) : NULL;
}

bool scan_session_Push( scan_session_t *p_scan, block_t *p_block )
{
    if( p_block->i_buffer < 188 || p_block->p_buffer[0] != 0x47 )
    {
        block_Release( p_block );
        return false;
    }

    /* */
    const int i_pid = ( (p_block->p_buffer[1]&0x1f)<<8) | p_block->p_buffer[2];
    if( i_pid == 0x00 )
    {
        if( !p_scan->pat )
#if (DVBPSI_VERSION_INT >= DVBPSI_VERSION_WANTED(1,0,0))
        {
            p_scan->pat = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
            if( !p_scan->pat )
            {
                block_Release( p_block );
                return false;
            }
            p_scan->pat->p_sys = (void *) VLC_OBJECT(p_scan->p_obj);
            if( !dvbpsi_pat_attach( p_scan->pat, (dvbpsi_pat_callback)PATCallBack, p_scan ) )
            {
                dvbpsi_delete( p_scan->pat );
                p_scan->pat = NULL;
                block_Release( p_block );
                return false;
            }
        }
#else
            p_scan->pat = dvbpsi_AttachPAT( (dvbpsi_pat_callback)PATCallBack, p_scan );
#endif
        if( p_scan->pat )
            dvbpsi_PushPacket( p_scan->pat, p_block->p_buffer );
    }
    else if( i_pid == 0x11 )
    {
        if( !p_scan->sdt )
#if (DVBPSI_VERSION_INT >= DVBPSI_VERSION_WANTED(1,0,0))
        {
            p_scan->sdt = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
            if( !p_scan->sdt )
            {
                block_Release( p_block );
                return false;
            }
            p_scan->sdt->p_sys = (void *) VLC_OBJECT(p_scan->p_obj);
            if( !dvbpsi_AttachDemux( p_scan->sdt, (dvbpsi_demux_new_cb_t)PSINewTableCallBack, p_scan ) )
            {
                dvbpsi_delete( p_scan->sdt );
                p_scan->sdt = NULL;
                block_Release( p_block );
                return false;
            }
        }
#else
            p_scan->sdt = dvbpsi_AttachDemux( (dvbpsi_demux_new_cb_t)PSINewTableCallBack, p_scan );
#endif

        if( p_scan->sdt )
            dvbpsi_PushPacket( p_scan->sdt, p_block->p_buffer );
    }
    else /*if( i_pid == p_scan->i_nit_pid )*/
    {
#ifdef DVBPSI_USE_NIT
        if( !p_scan->nit )
# if (DVBPSI_VERSION_INT >= DVBPSI_VERSION_WANTED(1,0,0))
        {
            p_scan->nit = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
            if( !p_scan->nit )
            {
                block_Release( p_block );
                return false;
            }
            p_scan->nit->p_sys = (void *) VLC_OBJECT(p_scan->p_obj);
            if( !dvbpsi_AttachDemux( p_scan->nit, (dvbpsi_demux_new_cb_t)PSINewTableCallBack, p_scan ) )
            {
                dvbpsi_delete( p_scan->nit );
                p_scan->nit = NULL;
                block_Release( p_block );
                return false;
            }
        }
# else
            p_scan->nit = dvbpsi_AttachDemux( (dvbpsi_demux_new_cb_t)PSINewTableCallBack, p_scan );
# endif
        if( p_scan->nit )
            dvbpsi_PushPacket( p_scan->nit, p_block->p_buffer );
#endif
    }

    block_Release( p_block );

    return p_scan->p_pat && p_scan->p_sdt && 
#ifdef DVBPSI_USE_NIT
        p_scan->p_nit;
#else
        true;
#endif
}

void scan_service_SetSNR( scan_session_t *p_session, int i_snr )
{
    p_session->i_snr = i_snr;
}
