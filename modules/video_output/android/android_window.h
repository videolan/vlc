/*****************************************************************************
 * android_window.c: Android video output module
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
 *          Felix Abecassis <felix.abecassis@gmail.com>
 *          Ming Hu <tewilove@gmail.com>
 *          Ludovic Fauvet <etix@l0cal.com>
 *          Sébastien Toque <xilasz@gmail.com>
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

#ifndef ANDROID_WINDOW_H_
#define ANDROID_WINDOW_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout_display.h>
#include <android/native_window.h>

struct picture_sys_t
{
    vout_display_sys_t *p_vd_sys;

    int (*pf_lock_pic)(picture_t *);
    void (*pf_unlock_pic)(picture_t *, bool b_render);

    union {
        struct {
            decoder_t *p_dec;
            uint32_t i_index;
            bool b_valid;
        } hw;
        struct {
            void *p_handle;
            ANativeWindow_Buffer buf;
        } sw;
    } priv;
    bool b_locked;
};

#endif
