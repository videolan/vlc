/*****************************************************************************
 * copy.h: Fast YV12/NV12 copy
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir_AT_ videolan _DOT_ org>
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

#ifndef _VLC_AVCODEC_COPY_H
#define _VLC_AVCODEC_COPY_H 1

typedef struct {
# ifdef CAN_COMPILE_SSE2
    uint8_t *buffer;
    size_t  size;
# endif
} copy_cache_t;

int  CopyInitCache(copy_cache_t *cache, unsigned width);
void CopyCleanCache(copy_cache_t *cache);

void CopyFromNv12(picture_t *dst, uint8_t *src[2], size_t src_pitch[2],
                  unsigned width, unsigned height,
                  copy_cache_t *cache);
void CopyFromYv12(picture_t *dst, uint8_t *src[3], size_t src_pitch[3],
                  unsigned width, unsigned height,
                  copy_cache_t *cache);

#endif

