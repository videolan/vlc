/*****************************************************************************
 * vlc_hmd_controller.h: API for HMD controller interfaces
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

#ifndef VLC_HMD_CONTROLLER_H
#define VLC_HMD_CONTROLLER_H

#include <vlc_common.h>
#include <vlc_picture.h>


typedef struct hmd_controller_pos_t
{
    // Positiosn in the virtual world.
    float f_depth;
    float f_left;
    float f_right;
    float f_top;
    float f_bottom;
} hmd_controller_pos_t;


struct vlc_hmd_controller_t{
    bool b_visible;
    picture_t *p_pic;

    hmd_controller_pos_t pos;
};


static inline vlc_hmd_controller_t *vlc_hmd_controller_New()
{
    return (vlc_hmd_controller_t *)calloc(1, sizeof(vlc_hmd_controller_t));
}

static inline void vlc_hmd_controller_Release(vlc_hmd_controller_t *p_ctl)
{
    if (p_ctl->p_pic != NULL)
        picture_Release(p_ctl->p_pic);
    free(p_ctl);
}

#endif // VLC_HMD_CONTROLLER_H
