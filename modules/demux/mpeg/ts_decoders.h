/*****************************************************************************
 * ts_decoders.h: TS Demux custom tables decoders
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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
#ifndef VLC_TS_DECODERS_H
#define VLC_TS_DECODERS_H

typedef void (* ts_dvbpsi_rawsections_callback_t)( dvbpsi_t *p_dvbpsi,
                                                   const dvbpsi_psi_section_t* p_section,
                                                   void* p_cb_data );

bool ts_dvbpsi_AttachRawSubDecoder( dvbpsi_t* p_dvbpsi,
                                    uint8_t i_table_id, uint16_t i_extension,
                                    ts_dvbpsi_rawsections_callback_t pf_callback,
                                    void* p_cb_data );

void ts_dvbpsi_DetachRawSubDecoder( dvbpsi_t *p_dvbpsi, uint8_t i_table_id, uint16_t i_extension );

bool ts_dvbpsi_AttachRawDecoder( dvbpsi_t* p_dvbpsi,
                                 ts_dvbpsi_rawsections_callback_t pf_callback,
                                 void* p_cb_data );

void ts_dvbpsi_DetachRawDecoder( dvbpsi_t *p_dvbpsi );

#endif
