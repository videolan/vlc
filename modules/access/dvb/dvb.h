/*****************************************************************************
 * dvb.h : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2004 VideoLAN
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

/*****************************************************************************
 * Local structures
 *****************************************************************************/
typedef struct demux_handle_t
{
    int i_pid;
    int i_handle;
    int i_type;
} demux_handle_t;

#define MAX_DEMUX 24

typedef struct thread_dvb_data_t
{
    int i_handle;
    demux_handle_t p_demux_handles[MAX_DEMUX];
    void * p_frontend;
    vlc_bool_t b_budget_mode;
} thread_dvb_data_t;

#define VIDEO0_TYPE 1
#define AUDIO0_TYPE 2
#define TELETEXT0_TYPE 3
#define SUBTITLE0_TYPE 4
#define PCR0_TYPE 5
#define TYPE_INTERVAL 5
#define OTHER_TYPE 21

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int E_(FrontendOpen)( input_thread_t * p_input );
void E_(FrontendClose)( input_thread_t * p_input );
int E_(FrontendSet)( input_thread_t * p_input );
int E_(DMXSetFilter)( input_thread_t * p_input, int i_pid, int * pi_fd,
		      int i_type );
int E_(DMXUnsetFilter)( input_thread_t * p_input, int i_fd );
int E_(DVROpen)( input_thread_t * p_input );
void E_(DVRClose)( input_thread_t * p_input );

