/*****************************************************************************
 * scan.h : functions to ease DVB scanning
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/

#ifdef HAVE_DVBPSI_DR_H
#ifdef _DVBPSI_DR_43_H_
#   define DVBPSI_USE_NIT 1
#   include <dvbpsi/nit.h>
#endif
#else
#ifdef _DVBPSI_DR_43_H_
#   define DVBPSI_USE_NIT 1
#   include "nit.h"
#endif
#endif

#ifndef DVBPSI_USE_NIT
#   warning NIT is not supported by your libdvbpsi version
#endif

typedef enum
{
    SCAN_NONE,
    SCAN_DVB_T,
    SCAN_DVB_S,
    SCAN_DVB_C,
} scan_type_t;

typedef struct
{
    scan_type_t type;
    bool b_exhaustive;
    struct
    {
        int i_min;
        int i_max;
        int i_step;

        int i_count;    /* Number of frequency test to do */
    } frequency;

    struct
    {
        /* Bandwidth should be 6, 7 or 8 */
        int i_min;
        int i_max;
        int i_step;

        int i_count;
    } bandwidth;

} scan_parameter_t;

typedef struct
{
    int i_frequency;
    int i_bandwidth;
} scan_configuration_t;

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

typedef struct
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
#endif

} scan_session_t;

typedef struct
{
    vlc_object_t *p_obj;
    struct dialog_progress_bar_t *p_dialog;
    int64_t i_index;
    scan_parameter_t parameter;
    int64_t i_time_start;

    int            i_service;
    scan_service_t **pp_service;
} scan_t;


scan_service_t *scan_service_New( int i_program, const scan_configuration_t *p_cfg  );
void scan_service_Delete( scan_service_t *p_srv );

int  scan_Init( vlc_object_t *p_obj, scan_t *p_scan, const scan_parameter_t *p_parameter );
void scan_Clean( scan_t *p_scan );

int scan_Next( scan_t *p_scan, scan_configuration_t *p_cfg );

block_t *scan_GetM3U( scan_t *p_scan );
bool scan_IsCancelled( scan_t *p_scan );

int  scan_session_Init( vlc_object_t *p_obj, scan_session_t *p_session, const scan_configuration_t *p_cfg );
void scan_session_Clean( scan_t *p_scan, scan_session_t *p_session );
bool scan_session_Push( scan_session_t *p_scan, block_t *p_block );
void scan_service_SetSNR( scan_session_t *p_scan, int i_snr );

