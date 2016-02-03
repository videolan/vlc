/*****************************************************************************
 * sections.h: Transport Stream sections assembler
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef TS_SECTIONS_H
#define TS_SECTIONS_H

typedef void(* ts_section_callback_t)( demux_t *, ts_pid_t *, block_t * );
typedef struct ts_sections_processor_t ts_sections_processor_t;

void ts_sections_processor_Add( ts_sections_processor_t **pp_chain,
                                uint8_t i_table_id, uint8_t i_stream_type, bool,
                                ts_section_callback_t pf_callback );

void ts_sections_processor_ChainDelete( ts_sections_processor_t *p_chain );

void ts_sections_processor_Reset( ts_sections_processor_t *p_chain );

void ts_sections_processor_Push( ts_sections_processor_t *p_chain,
                                 uint8_t i_table_id, uint8_t i_stream_type,
                                 demux_t *, ts_pid_t *,
                                 block_t *p_blockchain );

#endif
