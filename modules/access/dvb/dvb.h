/*****************************************************************************
 * dvb.h : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2005 VLC authors and VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *          Christopher Ross <chris@tebibyte.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Local structures
 *****************************************************************************/
typedef struct demux_handle_t
{
    int i_pid;
    int i_handle;
    int i_type;
} demux_handle_t;

typedef struct frontend_t frontend_t;
typedef struct
{
    int i_snr;              /**< Signal Noise ratio */
    int i_ber;              /**< Bitrate error ratio */
    int i_signal_strenth;   /**< Signal strength */
} frontend_statistic_t;

typedef struct
{
    bool b_has_signal;
    bool b_has_carrier;
    bool b_has_lock;
} frontend_status_t;

#define MAX_DEMUX 256

struct scan_t;
struct scan_parameter_t;

struct access_sys_t
{
    int i_handle, i_frontend_handle;
    demux_handle_t p_demux_handles[MAX_DEMUX];
    frontend_t *p_frontend;
    mtime_t i_frontend_timeout;
    bool b_budget_mode;

    struct cam *p_cam;

    /* */
    int i_read_once;

    int i_stat_counter;

    /* Scan */
    struct scan_t *scan;
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

int  FrontendOpen( access_t * );
void FrontendPoll( access_t *p_access );
int  FrontendSet( access_t * );
void FrontendClose( access_t * );

int  FrontendGetStatistic( access_t *, frontend_statistic_t * );
void FrontendGetStatus( access_t *, frontend_status_t * );
int  FrontendGetScanParameter( access_t *, struct scan_parameter_t * );

int DMXSetFilter( access_t *, int i_pid, int * pi_fd, int i_type );
int DMXUnsetFilter( access_t *, int i_fd );

int  DVROpen( access_t * );
void DVRClose( access_t * );
