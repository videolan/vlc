/*****************************************************************************
 * ts.h: Transport Stream input module for VLC.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef VLC_TS_H
#define VLC_TS_H

#ifdef HAVE_ARIBB24
    typedef struct arib_instance_t arib_instance_t;
#endif
typedef struct csa_t csa_t;

#define TS_USER_PMT_NUMBER (0)

#define TS_PSI_PAT_PID 0x00

_Static_assert (VLC_TICK_INVALID + 1 == VLC_TICK_0,
                "can't define TS_UNKNOWN reference");
#define TS_TICK_UNKNOWN (VLC_TICK_INVALID - 1)

#define SETANDVALID(a) (a != TS_TICK_UNKNOWN && a != VLC_TICK_INVALID)

typedef enum ts_standards_e
{
    TS_STANDARD_AUTO = 0,
    TS_STANDARD_MPEG,
    TS_STANDARD_DVB,
    TS_STANDARD_ARIB,
    TS_STANDARD_ATSC,
    TS_STANDARD_TDMB,
} ts_standards_e;

typedef struct
{
    int i_service;
} vdr_info_t;

struct demux_sys_t
{
    stream_t   *stream;
    bool        b_canseek;
    bool        b_canfastseek;
    bool        b_lowdelay;
    int         current_title;
    int         current_seekpoint;
    unsigned    updates;
    vlc_mutex_t     csa_lock;

    /* TS packet size (188, 192, 204) */
    unsigned    i_packet_size;

    /* Additional TS packet header size (BluRay TS packets have 4-byte header before sync byte) */
    unsigned    i_packet_header_size;

    /* how many TS packet we read at once */
    unsigned    i_ts_read;

    bool        b_cc_check;
    bool        b_ignore_time_for_positions;

    ts_standards_e standard;

#ifdef HAVE_ARIBB24
    struct
    {
        arib_instance_t *p_instance;
    } arib;
#endif

    /* All pid */
    ts_pid_list_t pids;

    enum
    {
        STREAM_PATPMT,
        USER_PMT,
        USER_TEMPPMT,
    } program_source;
    int         i_pmt_es;

    enum
    {
        DELAY_ES,
        CREATE_ES
    } es_creation;

    /* */
    bool        b_es_id_pid;
    uint16_t    i_next_extraid;

    csa_t       *csa;
    int         i_csa_pkt_size;
    bool        b_split_es;
    bool        b_valid_scrambling;

    bool        b_trust_pcr;
    bool        b_check_pcr_offset;
    unsigned    i_generated_pcr_dpb_offset;

    /* */
    bool        b_access_control;
    bool        b_end_preparse;

    /* */
    time_t      i_network_time;
    time_t      i_network_time_update; /* for network time interpolation */
    bool        b_broken_charset; /* True if broken encoding is used in EPG/SDT */

    /* Selected programs */
    enum
    {
        PROGRAM_AUTO_DEFAULT, /* Always select first program sending data */
        PROGRAM_LIST, /* explicit list of programs, see list below */
        PROGRAM_ALL, /* everything */
    } seltype; /* reflects the DEMUX_SET_GROUP */
    DECL_ARRAY( int ) programs; /* List of selected/access-filtered programs */
    bool        b_default_selection; /* True if set by default to first pmt seen (to get data from filtered access) */

    struct
    {
        stime_t i_first_dts;     /* first dts encountered for the stream */
        int     i_timesourcepid; /* which pid we saved the dts from */
        bool    b_pcrhasnopcrfield;
        enum { PAT_WAITING = 0, PAT_MISSING, PAT_FIXTRIED } status; /* set if we haven't seen PAT within MIN_PAT_INTERVAL */
    } patfix;

    vdr_info_t  vdr;

    /* downloadable content */
    vlc_dictionary_t attachments;

    /* */
    bool        b_start_record;
    char        *record_dir_path;
};

void TsChangeStandard( demux_sys_t *, ts_standards_e );

bool ProgramIsSelected( demux_sys_t *, uint16_t i_pgrm );

void UpdatePESFilters( demux_t *p_demux, bool b_all );

int ProbeStart( demux_t *p_demux, int i_program );
int ProbeEnd( demux_t *p_demux, int i_program );

void AddAndCreateES( demux_t *p_demux, ts_pid_t *pid, bool b_create_delayed );
int FindPCRCandidate( ts_pmt_t *p_pmt );

#endif
