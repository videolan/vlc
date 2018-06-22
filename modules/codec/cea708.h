/*****************************************************************************
 * cea708.h : CEA708 subtitles decoder
 *****************************************************************************
 * Copyright © 2017 Videolabs, VideoLAN and VLC authors
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
#ifndef VLC_CEA708_H_
#define VLC_CEA708_H_

typedef void(*service_data_hdlr_t)(void *, uint8_t i_sid, vlc_tick_t,
                                   const uint8_t *p_data, size_t i_data);

/* DVTCC Services demuxing */
#define CEA708_DTVCC_MAX_PKT_SIZE 128
typedef struct cea708_demux_t cea708_demux_t;

cea708_demux_t * CEA708_DTVCC_Demuxer_New( void *, service_data_hdlr_t );
void CEA708_DTVCC_Demuxer_Release( cea708_demux_t * );
void CEA708_DTVCC_Demuxer_Push( cea708_demux_t *h, vlc_tick_t, const uint8_t data[3] );
void CEA708_DTVCC_Demuxer_Flush( cea708_demux_t *h );

/* DVTCC Services decoding */
typedef struct cea708_t cea708_t;

cea708_t *CEA708_Decoder_New( decoder_t * );
void CEA708_Decoder_Release( cea708_t *p_cea708 );
void CEA708_Decoder_Push( cea708_t *p_cea708, vlc_tick_t,
                          const uint8_t *p_data, size_t i_data );
void CEA708_Decoder_Flush( cea708_t *p_cea708 );

#endif
