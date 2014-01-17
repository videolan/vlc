/*****************************************************************************
 * android_opaque.h: shared structures between MediaCodec decoder
 * and MediaCodec video output
 *****************************************************************************
 * Copyright (C) 2013 Felix Abecassis
 *
 * Authors: Felix Abecassis <felix.abecassis@gmail.com>
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

#ifndef ANDROID_OPAQUE_H_
#define ANDROID_OPAQUE_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

struct picture_sys_t
{
    void (*pf_display_callback)(picture_sys_t*);
    void (*pf_unlock_callback)(picture_sys_t*);
    decoder_t *p_dec;
    uint32_t i_index;
    int b_valid;
};

vlc_mutex_t* get_android_opaque_mutex(void);

#endif
