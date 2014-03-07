/*****************************************************************************
 * sdi.c: SDI helpers
 *****************************************************************************
 * Copyright (C) 2014 Rafaël Carré
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include <vlc_common.h>
#include <vlc_block.h>

#include <inttypes.h>

void v210_convert(uint16_t *dst, const uint32_t *bytes, const int width, const int height);

block_t *vanc_to_cc(vlc_object_t *, uint16_t *, size_t);
#define vanc_to_cc(obj, buf, words) vanc_to_cc(VLC_OBJECT(obj), buf, words)

#ifdef __cplusplus
}
#endif
