/*****************************************************************************
 * ts_scte.h: TS Demux SCTE section decoders/handlers
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
#ifndef VLC_TS_SCTE_H
#define VLC_TS_SCTE_H

#include "../../codec/scte18.h"

void SCTE18_Section_Callback( dvbpsi_t *p_handle,
                              const dvbpsi_psi_section_t* p_section,
                              void *p_cb_data );
void SCTE27_Section_Callback( demux_t *p_demux,
                              const uint8_t *, size_t,
                              const uint8_t *, size_t,
                              void * );

#endif
