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

#include <assert.h>
#include <vlc_common.h>
#include <vlc_vout_display.h>
#include <android/native_window.h>

struct picture_sys_t
{
    vout_display_sys_t *p_vd_sys;

    union {
        struct {
            vlc_mutex_t lock;
            decoder_t *p_dec;
            int i_index;
            void (*pf_release)(decoder_t *p_dec, unsigned int i_index,
                               bool b_render);
        } hw;
        struct {
            void *p_handle;
            ANativeWindow_Buffer buf;
        } sw;
    } priv;
    bool b_locked;
};

static inline void
AndroidOpaquePicture_DetachDecoder(picture_sys_t *p_picsys)
{
    vlc_mutex_lock(&p_picsys->priv.hw.lock);
    if (p_picsys->priv.hw.i_index >= 0)
    {
        assert(p_picsys->priv.hw.pf_release && p_picsys->priv.hw.p_dec);
        p_picsys->priv.hw.pf_release(p_picsys->priv.hw.p_dec,
                                     (unsigned int) p_picsys->priv.hw.i_index,
                                     false);
        p_picsys->priv.hw.i_index = -1;
    }
    p_picsys->priv.hw.pf_release = NULL;
    p_picsys->priv.hw.p_dec = NULL;
    /* Release p_picsys if references from VOUT and from decoder are NULL */
    if (!p_picsys->p_vd_sys && !p_picsys->priv.hw.p_dec)
    {
        vlc_mutex_unlock(&p_picsys->priv.hw.lock);
        vlc_mutex_destroy(&p_picsys->priv.hw.lock);
        free(p_picsys);
    }
    else
        vlc_mutex_unlock(&p_picsys->priv.hw.lock);
}

static inline void
AndroidOpaquePicture_Release(picture_sys_t *p_picsys, bool b_render)
{
    vlc_mutex_lock(&p_picsys->priv.hw.lock);
    if (p_picsys->priv.hw.i_index >= 0)
    {
        assert(p_picsys->priv.hw.pf_release && p_picsys->priv.hw.p_dec);
        p_picsys->priv.hw.pf_release(p_picsys->priv.hw.p_dec,
                                     (unsigned int) p_picsys->priv.hw.i_index,
                                     b_render);
        p_picsys->priv.hw.i_index = -1;
    }
    vlc_mutex_unlock(&p_picsys->priv.hw.lock);
}

#endif
