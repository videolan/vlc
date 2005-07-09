/*****************************************************************************
 * dvb.h : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman@saman>
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


/*****************************************************************************
 * Devices location
 *****************************************************************************/
#define DMX      "/dev/dvb/adapter%d/demux%d"
#define FRONTEND "/dev/dvb/adapter%d/frontend%d"
#define DVR      "/dev/dvb/adapter%d/dvr%d"
#define CA       "/dev/dvb/adapter%d/ca%d"

/*****************************************************************************
 * Local structures
 *****************************************************************************/
typedef struct
{
    int i_pid;
    int i_handle;
    int i_type;
} demux_handle_t;

typedef struct frontend_t frontend_t;

typedef struct
{
    int i_slot;
    int i_resource_id;
    void (* pf_handle)( access_t *, int, uint8_t *, int );
    void (* pf_close)( access_t *, int );
    void (* pf_manage)( access_t *, int );
    void *p_sys;
} en50221_session_t;

#define MAX_DEMUX 256
#define MAX_CI_SLOTS 16
#define MAX_SESSIONS 32
#define MAX_PROGRAMS 24

struct access_sys_t
{
    int i_handle, i_frontend_handle;
    demux_handle_t p_demux_handles[MAX_DEMUX];
    frontend_t *p_frontend;
    vlc_bool_t b_budget_mode;

    /* CA management */
    int i_ca_handle;
    int i_nb_slots;
    vlc_bool_t pb_active_slot[MAX_CI_SLOTS];
    vlc_bool_t pb_tc_has_data[MAX_CI_SLOTS];
    en50221_session_t p_sessions[MAX_SESSIONS];
    mtime_t i_ca_timeout, i_ca_next_event, i_frontend_timeout;
    dvbpsi_pmt_t *pp_selected_programs[MAX_PROGRAMS];

    /* */
    int i_read_once;
};

#define VIDEO0_TYPE     1
#define AUDIO0_TYPE     2
#define TELETEXT0_TYPE  3
#define SUBTITLE0_TYPE  4
#define PCR0_TYPE       5
#define TYPE_INTERVAL   5
#define OTHER_TYPE     21

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int  E_(FrontendOpen)( access_t * );
void E_(FrontendPoll)( access_t *p_access );
int  E_(FrontendSet)( access_t * );
void E_(FrontendClose)( access_t * );

int E_(DMXSetFilter)( access_t *, int i_pid, int * pi_fd, int i_type );
int E_(DMXUnsetFilter)( access_t *, int i_fd );

int  E_(DVROpen)( access_t * );
void E_(DVRClose)( access_t * );

int  E_(CAMOpen)( access_t * );
int  E_(CAMPoll)( access_t * );
int  E_(CAMSet)( access_t *, dvbpsi_pmt_t * );
void E_(CAMClose)( access_t * );

int E_(en50221_Poll)( access_t * );
int E_(en50221_SetCAPMT)( access_t *, dvbpsi_pmt_t * );
void E_(en50221_End)( access_t * );

