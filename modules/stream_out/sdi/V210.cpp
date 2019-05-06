/*****************************************************************************
 * V210.cpp: V210 picture conversion
 *****************************************************************************
 * Copyright © 2014-2016 VideoLAN and VideoLAN Authors
 *                  2018 VideoLabs
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "V210.hpp"

#include <vlc_picture.h>

using namespace sdi;

static inline unsigned clip(unsigned a)
{
    if      (a < 4) return 4;
    else if (a > 1019) return 1019;
    else               return a;
}

static inline void put_le32(uint8_t **p, uint32_t d)
{
    SetDWLE(*p, d);
    (*p) += 4;
}

void V210::Convert(const picture_t *pic, unsigned dst_stride, void *frame_bytes)
{
    unsigned width = pic->format.i_width;
    unsigned height = pic->format.i_height;
    unsigned payload_size = ((width * 8 + 11) / 12) * 4;
    unsigned line_padding = (payload_size < dst_stride) ? dst_stride - payload_size : 0;
    unsigned h, w;
    uint8_t *dst = (uint8_t*)frame_bytes;

    const uint16_t *y = (const uint16_t*)pic->p[0].p_pixels;
    const uint16_t *u = (const uint16_t*)pic->p[1].p_pixels;
    const uint16_t *v = (const uint16_t*)pic->p[2].p_pixels;

#define WRITE_PIXELS(a, b, c)           \
    do {                                \
        val =   clip(*a++);             \
        val |= (clip(*b++) << 10) |     \
               (clip(*c++) << 20);      \
        put_le32(&dst, val);           \
    } while (0)

    for (h = 0; h < height; h++) {
        uint32_t val = 0;
        for (w = 0; w + 5 < width; w += 6) {
            WRITE_PIXELS(u, y, v);
            WRITE_PIXELS(y, u, y);
            WRITE_PIXELS(v, y, u);
            WRITE_PIXELS(y, v, y);
        }
        if (w + 1 < width) {
            WRITE_PIXELS(u, y, v);

            val = clip(*y++);
            if (w + 2 == width)
                put_le32(&dst, val);
#undef WRITE_PIXELS
        }
        if (w + 3 < width) {
            val |= (clip(*u++) << 10) | (clip(*y++) << 20);
            put_le32(&dst, val);

            val = clip(*v++) | (clip(*y++) << 10);
            put_le32(&dst, val);
        }

        memset(dst, 0, line_padding);
        dst += line_padding;

        y += pic->p[0].i_pitch / 2 - width;
        u += pic->p[1].i_pitch / 2 - width / 2;
        v += pic->p[2].i_pitch / 2 - width / 2;
    }
}

void V210::Convert(const uint16_t *src, size_t srccount, void *out)
{
    uint8_t *dst = reinterpret_cast<uint8_t *>(out);
    for (size_t i = 0; i < srccount / 6; i++)
    {
        put_le32(&dst, src[i*6+0] << 10);
        put_le32(&dst, src[i*6+1] | (src[i*6+2] << 20));
        put_le32(&dst, src[i*6+3] << 10);
        put_le32(&dst, src[i*6+4] | (src[i*6+5] << 20));
    }
}
