/*****************************************************************************
 * tsutil.h
 *****************************************************************************
 * Copyright (C) 2001-2005, 2015 VLC authors and VideoLAN
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
#ifndef VLC_MPEG_TSUTIL_H_
#define VLC_MPEG_TSUTIL_H_

typedef void(*PEStoTSCallback)(void *, block_t *);

void PEStoTS( void *p_opaque, PEStoTSCallback pf_callback, block_t *p_pes,
              uint16_t i_pid, bool *pb_discontinuity, uint8_t *pi_continuity_counter );

#endif
