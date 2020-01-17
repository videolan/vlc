/*****************************************************************************
 * subpic.h:
 *****************************************************************************
 * Copyright © 2018-2020 John Cox
 *
 * Authors: John Cox <jc@kynesim.co.uk>
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

#ifndef VLC_HW_MMAL_SUBPIC_H_
#define VLC_HW_MMAL_SUBPIC_H_

typedef struct subpic_reg_stash_s
{
    MMAL_PORT_T * port;
    MMAL_POOL_T * pool;
    int display_id;  // -1 => do not set
    unsigned int layer;
    // Shadow  vars so we can tell if stuff has changed
    MMAL_RECT_T dest_rect;
    unsigned int alpha;
    unsigned int seq;
} subpic_reg_stash_t;

int hw_mmal_subpic_update(vlc_object_t * const p_filter,
                          MMAL_BUFFER_HEADER_T * const sub_buf,
                          subpic_reg_stash_t * const spe,
                          const video_format_t * const fmt,
                          const MMAL_RECT_T * const scale_out,
                          const uint64_t pts);

void hw_mmal_subpic_flush(subpic_reg_stash_t * const spe);

void hw_mmal_subpic_close(subpic_reg_stash_t * const spe);

// If display id is -1 it will be unset
MMAL_STATUS_T hw_mmal_subpic_open(vlc_object_t * const, subpic_reg_stash_t * const spe, MMAL_PORT_T * const port,
                                  const int display_id, const unsigned int layer);

#endif

