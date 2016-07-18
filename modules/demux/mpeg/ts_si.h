/*****************************************************************************
 * ts_psi_eit.h : TS demuxer SI handling
 *****************************************************************************
 * Copyright (C) 2014-2016 - VideoLAN Authors
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
#ifndef VLC_TS_PSI_EIT_H
#define VLC_TS_PSI_EIT_H

#include "ts_pid_fwd.h"

//#define SI_DEBUG_EIT

#define TS_SI_SDT_PID 0x11
#define TS_SI_EIT_PID 0x12
#define TS_SI_TDT_PID 0x14

#define TS_SI_TDT_TABLE_ID     0x70
#define TS_SI_TOT_TABLE_ID     0x73

#define TS_SI_RUNSTATUS_UNDEFINED 0x00
#define TS_SI_RUNSTATUS_STOPPED   0x01
#define TS_SI_RUNSTATUS_STARTING  0x02
#define TS_SI_RUNSTATUS_PAUSING   0x03
#define TS_SI_RUNSTATUS_RUNNING   0x04
#define TS_SI_RUNSTATUS_OFF_AIR   0x05

void ts_si_Packet_Push( ts_pid_t *, const uint8_t * );

bool ts_attach_SI_Tables_Decoders( ts_pid_t * );

#endif
