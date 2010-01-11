/*****************************************************************************
 * scan.c: DVB scanner helpers
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#include <vlc_dialog.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <sys/types.h>
#include <poll.h>

/* Include dvbpsi headers */
#ifdef HAVE_DVBPSI_DR_H
#   include <dvbpsi/dvbpsi.h>
#   include <dvbpsi/descriptor.h>
#   include <dvbpsi/pat.h>
#   include <dvbpsi/pmt.h>
#   include <dvbpsi/dr.h>
#   include <dvbpsi/psi.h>
#   include <dvbpsi/demux.h>
#   include <dvbpsi/sdt.h>
#else
#   include "dvbpsi.h"
#   include "descriptor.h"
#   include "tables/pat.h"
#   include "tables/pmt.h"
#   include "descriptors/dr.h"
#   include "psi.h"
#   include "demux.h"
#   include "tables/sdt.h"
#endif

#ifdef ENABLE_HTTPD
#   include <vlc_httpd.h>
#endif

#include "dvb.h"

/* */
scan_service_t *scan_service_New( int i_program, const scan_configuration_t *p_cfg  )
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

void scan_service_Delete( scan_service_t *p_srv )
{
    free( p_srv->psz_name );
    free( p_srv );
}

/* */
int scan_Init( vlc_object_t *p_obj, scan_t *p_scan, const scan_parameter_t *p_parameter )
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
    }
    else
    {
        return VLC_EGENERIC;
    }

    p_scan->p_obj = VLC_OBJECT(p_obj);
    p_scan->i_index = 0;
    p_scan->p_dialog = NULL;
    TAB_INIT( p_scan->i_service, p_scan->pp_service );
    p_scan->parameter = *p_parameter;
    p_scan->i_time_start = mdate();

    return VLC_SUCCESS;
}
void scan_Clean( scan_t *p_scan )
{
    if( p_scan->p_dialog != NULL )
        dialog_ProgressDestroy( p_scan->p_dialog );

    for( int i = 0; i < p_scan->i_service; i++ )
        scan_service_Delete( p_scan->pp_service[i] );
    TAB_CLEAN( p_scan->i_service, p_scan->pp_service );
}

static int ScanDvbCNextFast( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
{
    msg_Dbg( p_scan->p_obj, "Scan index %"PRId64, p_scan->i_index );
    /* Values taken from dvb-scan utils frequency-files, sorted by how
     * often they appear. This hopefully speeds up finding services. */
    static const unsigned short frequencies[] = {
     410, 426, 418, 394, 402, 362,
     370, 354, 346, 442, 434, 386,
     378, 450, 306, 162, 154, 474,
     466, 458, 338, 754, 714, 586,
     562, 546, 514, 490, 314, 170,
     113, 770, 762, 746, 738, 730,
     722, 706, 690, 682, 674, 666,
     650, 642, 634, 554, 538, 530,
     506, 498, 330, 322, 283, 850,
     842, 834, 818, 810, 802, 794,
     786, 778, 748, 732, 728, 724,
     720, 698, 660, 658, 656, 610,
     594, 578, 570, 522, 482, 377,
     372, 347, 339, 323, 315, 299,
     298, 291, 275, 267, 259, 255,
     251, 243, 235, 232, 227, 219,
     211, 203, 195, 187, 179, 171,
     163, 155, 147, 146, 143, 139,
     131, 123, 121
    };
    enum { num_frequencies = (sizeof(frequencies)/sizeof(*frequencies)) };

    if( p_scan->i_index < num_frequencies )
    {
        p_cfg->i_frequency = 1000000 * ( frequencies[ p_scan->i_index ] );
        *pf_pos = (double)p_scan->i_index / num_frequencies;
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static int ScanDvbTNextExhaustive( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
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
    if( p_scan->parameter.b_exhaustive )
        return ScanDvbTNextExhaustive( p_scan, p_cfg, pf_pos );
    else
        return ScanDvbCNextFast( p_scan, p_cfg, pf_pos );
}

static int ScanDvbTNext( scan_t *p_scan, scan_configuration_t *p_cfg, double *pf_pos )
{
    if( p_scan->parameter.b_exhaustive )
        return ScanDvbTNextExhaustive( p_scan, p_cfg, pf_pos );
    else
        return ScanDvbTNextFast( p_scan, p_cfg, pf_pos );
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
        if( p_scan->pp_service[i]->type != SERVICE_UNKNOWN )
            i_service++;
    }

    if( asprintf( &psz_text, _("%.1f MHz (%d services)"), 
                  (double)p_cfg->i_frequency / 1000000, i_service ) >= 0 )
    {
        const mtime_t i_eta = f_position > 0.005 ? (mdate() - p_scan->i_time_start) * ( 1.0 / f_position - 1.0 ) : -1;
        char psz_eta[MSTRTIME_MAX_SIZE];

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
             p_sdt->i_ts_id, p_sdt->i_version, p_sdt->b_current_next,
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

static void PSINewTableCallBack( scan_session_t *p_session, dvbpsi_handle h, uint8_t  i_table_id, uint16_t i_extension )
{
    if( i_table_id == 0x42 )
        dvbpsi_AttachSDT( h, i_table_id, i_extension, (dvbpsi_sdt_callback)SDTCallBack, p_session );
#ifdef DVBPSI_USE_NIT
    else if( i_table_id == 0x40 )
        dvbpsi_AttachNIT( h, i_table_id, i_extension, (dvbpsi_nit_callback)NITCallBack, p_session );
#endif
}


int scan_session_Init( vlc_object_t *p_obj, scan_session_t *p_session, const scan_configuration_t *p_cfg )
{
    /* */
    memset( p_session, 0, sizeof(*p_session) );
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
    return VLC_SUCCESS;
}

void scan_session_Clean( scan_t *p_scan, scan_session_t *p_session )
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

            scan_service_t *s = scan_service_New( p_program->i_number, &p_session->cfg );
            TAB_APPEND( p_scan->i_service, p_scan->pp_service, s );
        }
    }
    /* Parse SDT */
    if( p_pat && p_sdt )
    {
        dvbpsi_sdt_service_t *p_srv;
        for( p_srv = p_sdt->p_first_service; p_srv; p_srv = p_srv->p_next )
        {
            scan_service_t *s = ScanFindService( p_scan, i_service_start, p_srv->i_service_id );
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
                            s->psz_name = dvbsi_to_utf8( pD->i_service_name, pD->i_service_name_length );

                        if( s->type == SERVICE_UNKNOWN )
                        {
                            switch( pD->i_service_type )
                            {
                            case 0x01: s->type = SERVICE_DIGITAL_TELEVISION; break;
                            case 0x02: s->type = SERVICE_DIGITAL_RADIO; break;
                            case 0x16: s->type = SERVICE_DIGITAL_TELEVISION_AC_SD; break;
                            case 0x19: s->type = SERVICE_DIGITAL_TELEVISION_AC_HD; break;
                            }
                        }
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
        msg_Warn( p_obj, "scan_GetM3U: service number %d type '%s' name '%s' channel %d cypted=%d| network_id %d (nit:%d sdt:%d)| f=%d bw=%d snr=%d",
                  s->i_program, psz_type, s->psz_name, s->i_channel, s->b_crypted,
                  s->i_network_id, s->i_nit_version, s->i_sdt_version,
                  s->cfg.i_frequency, s->cfg.i_bandwidth, s->i_snr );

        char *psz;
        if( asprintf( &psz, "#EXTINF:,,%s\n"
                        "#EXTVLCOPT:program=%d\n"
                        "dvb://frequency=%d:bandwidth=%d\n"
                        "\n",
                      s->psz_name && * s->psz_name ? s->psz_name : "Unknown",
                      s->i_program,
                      s->cfg.i_frequency, s->cfg.i_bandwidth ) < 0 )
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
            p_scan->pat = dvbpsi_AttachPAT( (dvbpsi_pat_callback)PATCallBack, p_scan );

        if( p_scan->pat )
            dvbpsi_PushPacket( p_scan->pat, p_block->p_buffer );
    }
    else if( i_pid == 0x11 )
    {
        if( !p_scan->sdt )
            p_scan->sdt = dvbpsi_AttachDemux( (dvbpsi_demux_new_cb_t)PSINewTableCallBack, p_scan );

        if( p_scan->sdt )
            dvbpsi_PushPacket( p_scan->sdt, p_block->p_buffer );
    }
    else if( i_pid == p_scan->i_nit_pid )
    {
#ifdef DVBPSI_USE_NIT
        if( !p_scan->nit )
            p_scan->nit = dvbpsi_AttachDemux( (dvbpsi_demux_new_cb_t)PSINewTableCallBack, p_scan );

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

