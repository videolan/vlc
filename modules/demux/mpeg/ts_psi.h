/*****************************************************************************
 * ts_psi.h: Transport Stream input module for VLC.
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
#ifndef VLC_TS_PSI_H
#define VLC_TS_PSI_H

#include "ts_pid_fwd.h"

typedef enum
{
    TS_PMT_REGISTRATION_NONE = 0,
    TS_PMT_REGISTRATION_BLURAY,
    TS_PMT_REGISTRATION_ATSC,
    TS_PMT_REGISTRATION_ARIB,
} ts_pmt_registration_type_t;

bool ts_psi_PAT_Attach( ts_pid_t *, void * );
void ts_psi_Packet_Push( ts_pid_t *, const uint8_t * );

int UserPmt( demux_t *p_demux, const char * );

#endif
