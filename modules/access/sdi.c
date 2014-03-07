/*****************************************************************************
 * sdi.c: SDI helpers
 *****************************************************************************
 * Copyright (C) 2014 Rafaël Carré
 * Copyright (C) 2009 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2009 Baptiste Coudurier <baptiste dot coudurier at gmail dot com>
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

#include "sdi.h"

static inline uint32_t av_le2ne32(uint32_t val)
{
    union {
        uint32_t v;
        uint8_t b[4];
    } u;
    u.v = val;
    return (u.b[0] << 0) | (u.b[1] << 8) | (u.b[2] << 16) | (u.b[3] << 24);
}

void v210_convert(uint16_t *dst, const uint32_t *bytes, const int width, const int height)
{
    const int stride = ((width + 47) / 48) * 48 * 8 / 3 / 4;
    uint16_t *y = &dst[0];
    uint16_t *u = &dst[width * height * 2 / 2];
    uint16_t *v = &dst[width * height * 3 / 2];

#define READ_PIXELS(a, b, c)         \
    do {                             \
        val  = av_le2ne32(*src++);   \
        *a++ =  val & 0x3FF;         \
        *b++ = (val >> 10) & 0x3FF;  \
        *c++ = (val >> 20) & 0x3FF;  \
    } while (0)

    for (int h = 0; h < height; h++) {
        const uint32_t *src = bytes;
        uint32_t val = 0;
        int w;
        for (w = 0; w < width - 5; w += 6) {
            READ_PIXELS(u, y, v);
            READ_PIXELS(y, u, y);
            READ_PIXELS(v, y, u);
            READ_PIXELS(y, v, y);
        }
        if (w < width - 1) {
            READ_PIXELS(u, y, v);

            val  = av_le2ne32(*src++);
            *y++ =  val & 0x3FF;
        }
        if (w < width - 3) {
            *u++ = (val >> 10) & 0x3FF;
            *y++ = (val >> 20) & 0x3FF;

            val  = av_le2ne32(*src++);
            *v++ =  val & 0x3FF;
            *y++ = (val >> 10) & 0x3FF;
        }

        bytes += stride;
    }
}

#undef vanc_to_cc
block_t *vanc_to_cc(vlc_object_t *obj, uint16_t *buf, size_t words)
{
    if (words < 3) {
        msg_Err(obj, "VANC line too small (%zu words)", words);
        return NULL;
    }

    static const uint8_t vanc_header[6] = { 0x00, 0x00, 0xff, 0x03, 0xff, 0x03 };
    if (memcmp(vanc_header, buf, 3*2)) {
        /* Does not start with the VANC header */
        return NULL;
    }

    size_t len = (buf[5] & 0xff) + 6 + 1;
    if (len > words) {
        msg_Err(obj, "Data Count (%zu) > line length (%zu)", len, words);
        return NULL;
    }

    uint16_t vanc_sum = 0;
    for (size_t i = 3; i < len - 1; i++) {
        uint16_t v = buf[i];
        int np = v >> 8;
        int p = parity(v & 0xff);
        if ((!!p ^ !!(v & 0x100)) || (np != 1 && np != 2)) {
            msg_Err(obj, "Parity incorrect for word %zu", i);
            return NULL;
        }
        vanc_sum += v;
        vanc_sum &= 0x1ff;
        buf[i] &= 0xff;
    }

    vanc_sum |= ((~vanc_sum & 0x100) << 1);
    if (buf[len - 1] != vanc_sum) {
        msg_Err(obj, "VANC checksum incorrect: 0x%.4x != 0x%.4x", vanc_sum, buf[len-1]);
        return NULL;
    }

    if (buf[3] != 0x61 /* DID */ || buf[4] != 0x01 /* SDID = CEA-708 */) {
        //msg_Err(obj, "Not a CEA-708 packet: DID = 0x%.2x SDID = 0x%.2x", buf[3], buf[4]);
        // XXX : what is Not a CEA-708 packet: DID = 0x61 SDID = 0x02 ?
        return NULL;
    }

    /* CDP follows */
    uint16_t *cdp = &buf[6];
    if (cdp[0] != 0x96 || cdp[1] != 0x69) {
        msg_Err(obj, "Invalid CDP header 0x%.2x 0x%.2x", cdp[0], cdp[1]);
        return NULL;
    }

    len -= 7; // remove VANC header and checksum

    if (cdp[2] != len) {
        msg_Err(obj, "CDP len %d != %zu", cdp[2], len);
        return NULL;
    }

    uint8_t cdp_sum = 0;
    for (size_t i = 0; i < len - 1; i++)
        cdp_sum += cdp[i];
    cdp_sum = cdp_sum ? 256 - cdp_sum : 0;
    if (cdp[len - 1] != cdp_sum) {
        msg_Err(obj, "CDP checksum invalid 0x%.4x != 0x%.4x", cdp_sum, cdp[len-1]);
        return NULL;
    }

    uint8_t rate = cdp[3];
    if (!(rate & 0x0f)) {
        msg_Err(obj, "CDP frame rate invalid (0x%.2x)", rate);
        return NULL;
    }
    rate >>= 4;
    if (rate > 8) {
        msg_Err(obj, "CDP frame rate invalid (0x%.2x)", rate);
        return NULL;
    }

    if (!(cdp[4] & 0x43)) /* ccdata_present | caption_service_active | reserved */ {
        msg_Err(obj, "CDP flags invalid (0x%.2x)", cdp[4]);
        return NULL;
    }

    uint16_t hdr = (cdp[5] << 8) | cdp[6];
    if (cdp[7] != 0x72) /* ccdata_id */ {
        msg_Err(obj, "Invalid ccdata_id 0x%.2x", cdp[7]);
        return NULL;
    }

    unsigned cc_count = cdp[8];
    if (!(cc_count & 0xe0)) {
        msg_Err(obj, "Invalid cc_count 0x%.2x", cc_count);
        return NULL;
    }

    cc_count &= 0x1f;
    if ((len - 13) < cc_count * 3) {
        msg_Err(obj, "Invalid cc_count %d (> %zu)", cc_count * 3, len - 13);
        return NULL;
    }

    if (cdp[len - 4] != 0x74) /* footer id */ {
        msg_Err(obj, "Invalid footer id 0x%.2x", cdp[len-4]);
        return NULL;
    }

    uint16_t ftr = (cdp[len - 3] << 8) | cdp[len - 2];
    if (ftr != hdr) {
        msg_Err(obj, "Header 0x%.4x != Footer 0x%.4x", hdr, ftr);
        return NULL;
    }

    block_t *cc = block_Alloc(cc_count * 3);

    for (size_t i = 0; i < cc_count; i++) {
        cc->p_buffer[3*i+0] = cdp[9 + 3*i+0] /* & 3 */;
        cc->p_buffer[3*i+1] = cdp[9 + 3*i+1];
        cc->p_buffer[3*i+2] = cdp[9 + 3*i+2];
    }

    return cc;
}
