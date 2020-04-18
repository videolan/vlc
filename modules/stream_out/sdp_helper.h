/*****************************************************************************
 * sdp_helper.h:
 *****************************************************************************
 * Copyright (C) 2002-2008 VLC authors and VideoLAN
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

#ifndef VLC_SOUT_SDP_H
#define VLC_SOUT_SDP_H

#include <stddef.h>

struct sockaddr;
struct vlc_memstream;

int vlc_sdp_Start(struct vlc_memstream *, vlc_object_t *obj,
                  const char *cfgpref,
                  const struct sockaddr *src, size_t slen,
                  const struct sockaddr *addr, size_t alen) VLC_USED;

#endif
