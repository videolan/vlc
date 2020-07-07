/*****************************************************************************
 * filter_priv.h
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 * Copyright (C) 2020 Videolabs
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

#ifndef VLC_GL_FILTER_PRIV_H
#define VLC_GL_FILTER_PRIV_H

#include <vlc_common.h>

#include "filter.h"

struct vlc_gl_filter_priv {
    struct vlc_gl_filter filter;
};

#define vlc_gl_filter_PRIV(filter) \
    container_of(filter, struct vlc_gl_filter_priv, filter)

struct vlc_gl_filter *
vlc_gl_filter_New(void);

void
vlc_gl_filter_Delete(struct vlc_gl_filter *filter);

#endif
