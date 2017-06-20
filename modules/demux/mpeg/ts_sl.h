/*****************************************************************************
 * ts_sl.h : MPEG SL/FMC handling for TS demuxer
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
#ifndef VLC_TS_SL_H
#define VLC_TS_SL_H

typedef struct es_mpeg4_descriptor_t es_mpeg4_descriptor_t;
typedef struct decoder_config_descriptor_t decoder_config_descriptor_t;

const es_mpeg4_descriptor_t * GetMPEG4DescByEsId( const ts_pmt_t *pmt, uint16_t i_es_id );
void SLPackets_Section_Handler( demux_t *p_demux,
                                const uint8_t *, size_t,
                                const uint8_t *, size_t,
                                void * );
bool SetupISO14496LogicalStream( demux_t *, const decoder_config_descriptor_t *,
                                  es_format_t * );

ts_stream_processor_t *SL_stream_processor_New( ts_stream_t * );

#endif
