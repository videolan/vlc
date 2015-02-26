/*****************************************************************************
 * tables.h
 *****************************************************************************
 * Copyright (C) 2001-2005, 2015 VLC authors and VideoLAN
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
#ifndef _TABLES_H
#define _TABLES_H 1

#define MAX_SDT_DESC 64

typedef struct
{
    ts_stream_t ts;
    int i_netid;
    struct
    {
        char *psz_provider;
        char *psz_service_name;  /* name of program */
    } desc[MAX_SDT_DESC];
} sdt_psi_t;

block_t * WritePSISection( dvbpsi_psi_section_t* p_section );

void BuildPAT( dvbpsi_t *p_dvbpsi,
               void *p_opaque, PEStoTSCallback pf_callback,
               int i_tsid, int i_pat_version_number,
               ts_stream_t *p_pat,
               unsigned i_programs, ts_stream_t *p_pmt, const int *pi_programs_number );

typedef struct
{
    const pes_stream_t *pes;
    const ts_stream_t  *ts;
    const es_format_t  *fmt;
    int i_mapped_prog;
} pes_mapped_stream_t;

void BuildPMT( dvbpsi_t *p_dvbpsi, vlc_object_t *p_object,
               void *p_opaque, PEStoTSCallback pf_callback,
               int i_tsid, int i_pmt_version_number,
               int i_pcr_pid,
               sdt_psi_t *p_sdt,
               unsigned i_programs, ts_stream_t *p_pmt, const int *pi_programs_number,
               unsigned i_mapped_streams, const pes_mapped_stream_t *p_mapped_streams );

#endif
