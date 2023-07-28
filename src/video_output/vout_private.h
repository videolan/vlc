/*****************************************************************************
 * vout_private.h : Internal vout definitions
 *****************************************************************************
 * Copyright (C) 2008-2018 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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

#ifndef LIBVLC_VOUT_PRIVATE_H
#define LIBVLC_VOUT_PRIVATE_H 1

#include <vlc_picture_fifo.h>
#include <vlc_picture_pool.h>
#include <vlc_vout_display.h>

typedef struct vout_interlacing_state_t vout_interlacing_state_t;

/* */
struct vout_interlacing_state_t
{
    bool        is_interlaced;
    bool        has_deint;
    vlc_tick_t  date;
};

/* */
vout_display_t *vout_OpenWrapper(vout_thread_t *, const char *,
                     const vout_display_cfg_t *, const video_format_t *, vlc_video_context *);
void vout_CloseWrapper(vout_thread_t *, vout_display_t *vd);

void vout_InitInterlacingSupport(vout_thread_t *, vout_interlacing_state_t *);
void vout_ReinitInterlacingSupport(vout_thread_t *, vout_interlacing_state_t *);
void vout_SetInterlacingState(vout_thread_t *, vout_interlacing_state_t *, bool is_interlaced);

#endif // LIBVLC_VOUT_PRIVATE_H
