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

typedef enum
{
    TS_PMT_REGISTRATION_NONE = 0,
    TS_PMT_REGISTRATION_BLURAY,
    TS_PMT_REGISTRATION_ATSC,
    TS_PMT_REGISTRATION_ARIB,
} ts_pmt_registration_type_t;

void PATCallBack( void *, dvbpsi_pat_t * );
void PMTCallBack( void *, dvbpsi_pmt_t * );

int UserPmt( demux_t *p_demux, const char * );

#endif
