/*****************************************************************************
 * ts_psip_dvbpsi_fixes.h : TS demux Broken/missing dvbpsi PSIP handling
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN Authors
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
#ifndef VLC_TS_PSIP_DVBPSI_FIXES_H
#define VLC_TS_PSIP_DVBPSI_FIXES_H

dvbpsi_atsc_stt_t * DVBPlague_STT_Decode( const dvbpsi_psi_section_t* p_section );
dvbpsi_atsc_ett_t * DVBPlague_ETT_Decode( const dvbpsi_psi_section_t* p_section );

#endif
