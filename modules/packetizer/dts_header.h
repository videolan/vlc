/*****************************************************************************
 * dts_header.c: parse DTS audio headers info
 *****************************************************************************
 * Copyright (C) 2004-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar
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

#define DTS_HEADER_SIZE 14

int GetSyncInfo( const uint8_t *p_buf,
                        bool *pb_dts_hd,
                        unsigned int *pi_sample_rate,
                        unsigned int *pi_bit_rate,
                        unsigned int *pi_frame_length,
                        unsigned int *pi_audio_mode );

int SyncCode( const uint8_t * );

