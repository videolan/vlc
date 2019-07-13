/*****************************************************************************
 * ts_streams_private.h: Transport Stream input module for VLC.
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifndef VLC_TS_STREAMS_PRIVATE_H
#define VLC_TS_STREAMS_PRIVATE_H

typedef struct dvbpsi_s dvbpsi_t;
typedef struct ts_sections_processor_t ts_sections_processor_t;

#include "mpeg4_iod.h"
#include "timestamps.h"

#include <vlc_common.h>
#include <vlc_es.h>

struct ts_pat_t
{
    int             i_version;
    int             i_ts_id;
    bool            b_generated;
    dvbpsi_t       *handle;
    DECL_ARRAY(ts_pid_t *) programs;

};

struct ts_pmt_t
{
    dvbpsi_t       *handle;
    int             i_version;
    int             i_number;
    int             i_pid_pcr;
    bool            b_selected;
    /* IOD stuff (mpeg4) */
    od_descriptor_t *iod;
    od_descriptors_t od;

    DECL_ARRAY(ts_pid_t *) e_streams;

    /* Used for ref tracking PSIP pid chain */
    ts_pid_t        *p_atsc_si_basepid;
    /* Used for ref tracking SI pid chain, starting with SDT */
    ts_pid_t        *p_si_sdt_pid;

    struct
    {
        stime_t i_current;
        stime_t i_first; // seen <> != TS_TICK_UNKNOWN
        /* broken PCR handling */
        stime_t i_first_dts;
        stime_t i_pcroffset;
        bool    b_disable; /* ignore PCR field, use dts */
        bool    b_fix_done;
    } pcr;

    struct
    {
        time_t i_event_start;
        time_t i_event_length;
    } eit;

    stime_t i_last_dts;
    uint64_t i_last_dts_byte;

    /* ARIB specific */
    struct
    {
        int i_logo_id;
        int i_download_id;
    } arib;
};

struct ts_es_t
{
    ts_pmt_t *p_program;
    es_format_t  fmt;
    es_out_id_t *id;
    uint16_t i_sl_es_id;
    int         i_next_block_flags;
    ts_es_t *p_extraes; /* Some private streams encapsulate several ES (eg. DVB subtitles) */
    ts_es_t *p_next; /* Next es on same pid from different pmt (shared pid) */
    /* J2K stuff */
    uint8_t  b_interlaced;
    /* Metadata */
    struct
    {
        uint8_t i_service_id;
        uint32_t i_format;
    } metadata;
};

typedef enum
{
    TS_TRANSPORT_PES,
    TS_TRANSPORT_SECTIONS,
    TS_TRANSPORT_IGNORE
} ts_transport_type_t;

struct ts_stream_t
{
    ts_es_t *p_es;

    uint8_t i_stream_type;

    ts_transport_type_t transport;

    struct
    {
        size_t      i_data_size;
        size_t      i_gathered;
        block_t     *p_data;
        block_t     **pp_last;
        uint8_t     saved[5];
        size_t      i_saved;
    } gather;

    bool        b_always_receive;
    bool        b_broken_PUSI_conformance;
    ts_sections_processor_t *p_sections_proc;
    ts_stream_processor_t   *p_proc;

    struct
    {
        block_t *p_head;
        block_t **pp_last;
    } prepcr;

    stime_t i_last_dts;
};

typedef struct ts_si_context_t ts_si_context_t;

struct ts_si_t
{
    dvbpsi_t *handle;
    int       i_version;
    /* Track successfully set pid */
    ts_pid_t *eitpid;
    ts_pid_t *tdtpid;
    ts_pid_t *cdtpid;
};

typedef struct ts_psip_context_t ts_psip_context_t;

struct ts_psip_t
{
    dvbpsi_t       *handle;
    int             i_version;
    ts_es_t    *p_eas_es;
    ts_psip_context_t *p_ctx;
    /* Used to track list of active pid for eit/ett, to call PIDRelease on them.
       VCT table could have been used, but PIDSetup can fail, and we can't alter
       the VCT table accordingly without going ahead of more troubles */
    DECL_ARRAY(ts_pid_t *) eit;

};

#endif
