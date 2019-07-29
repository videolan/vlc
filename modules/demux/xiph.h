/*****************************************************************************
 * xiph.h: Xiph helpers
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <assert.h>
#include <limits.h>
#define XIPH_MAX_HEADER_COUNT (256)

/* Temp ffmpeg vorbis format */
static inline bool xiph_IsLavcFormat(const void *extra, unsigned i_extra,
                                     vlc_fourcc_t i_codec)
{
    switch(i_codec)
    {
        case VLC_CODEC_VORBIS:
            return i_extra >= 6 && GetWBE(extra) == 30;
        case VLC_CODEC_THEORA:
            return i_extra >= 6 && GetWBE(extra) == 42;
        default:
            return false;
    }
}

static inline unsigned xiph_CountLavcHeaders(const void *p_extra, unsigned i_extra)
{
    const uint8_t *p = (const uint8_t*) p_extra;
    const uint8_t *p_end = &p[i_extra];
    /* Check headers count */
    for (int i=0; i<3; i++)
    {
        if(p_end - p < 2)
            return 0;
        uint16_t i_size = GetWBE(p);
        if(&p[2U + i_size] > p_end)
            return 0;
        p += 2 + i_size;
    }
    return 3;
}

static inline unsigned xiph_CountHeaders(const void *p_extra, unsigned i_extra)
{
    const uint8_t *p = (const uint8_t*) p_extra;
    if (!i_extra)
        return 0;
    /* First byte is headers count */
    if(1U + *p > i_extra)
        return 0;
    return *p + 1;
}

static inline unsigned xiph_CountUnknownHeaders(const void *p_extra, unsigned i_extra,
                                                vlc_fourcc_t i_codec)
{
    if (xiph_IsLavcFormat(p_extra, i_extra, i_codec))
        return xiph_CountLavcHeaders(p_extra, i_extra);
    else
        return xiph_CountHeaders(p_extra, i_extra);
}

static inline int xiph_SplitLavcHeaders(unsigned packet_size[],
                                        const void *packet[], unsigned *packet_count,
                                        unsigned i_extra, const void *p_extra)
{
    const uint8_t *current = (const uint8_t *)p_extra;
    const uint8_t *end = &current[i_extra];
    if (i_extra < 2)
        return VLC_EGENERIC;
    /* Parse the packet count and their sizes */
    const unsigned count = xiph_CountLavcHeaders(current, i_extra);
    if(count == 0)
        return VLC_EGENERIC;
    if (packet_count)
        *packet_count = count;
    /* count is trusted here (xiph_CountHeaders) */
    for (unsigned i=0; i < count; i++)
    {
        /* each payload is prefixed by word size */
        packet_size[i] = GetWBE(current);
        if(&current[2U + packet_size[i]] > end)
            return VLC_EGENERIC;
        packet[i] = current + 2;
        current += packet_size[i] + 2;
    }
    return VLC_SUCCESS;
}

static inline int xiph_SplitHeaders(unsigned packet_size[],
                                    const void *packet[], unsigned *packet_count,
                                    unsigned i_extra, const void *p_extra)
{
    const uint8_t *current = (const uint8_t *)p_extra;
    const uint8_t *end = &current[i_extra];
    if (i_extra < 1)
        return VLC_EGENERIC;

    /* Parse the packet count and their sizes */
    const unsigned count = xiph_CountHeaders(current, i_extra);
    if(count == 0)
        return VLC_EGENERIC;

    if (packet_count)
        *packet_count = count;

    /* - 1 byte (N-1) packets
     * - N-1 variable length payload sizes
     * - N-1 payloads
     * - Nth packet (remaining)  */

    /* skip count byte header */
    ++current;
    /* read sizes for N-1 packets */
    unsigned total_payload_minus_last = 0;
    for (unsigned i = 0; i < count - 1; i++)
    {
        packet_size[i] = 0;
        for (;;) {
            if (current >= end)
                return VLC_EGENERIC;
            packet_size[i] += *current;
            if (*current++ != 255)
                break;
        }
        if(UINT_MAX - total_payload_minus_last < packet_size[i])
            return VLC_EGENERIC;
        total_payload_minus_last += packet_size[i];
    }
    if(current + total_payload_minus_last > end)
        return VLC_EGENERIC;
    /* set pointers for N-1 packets */
    for (unsigned i = 0; i < count - 1; i++)
    {
        packet[i] = current;
        current += packet_size[i];
    }
    /* Last packet is remaining size */
    packet_size[count - 1] = end - current;
    packet[count - 1] = current;

    return VLC_SUCCESS;
}

static inline int xiph_PackHeaders(int *extra_size, void **extra,
                                   unsigned packet_size[],
                                   const void *const packet[],
                                   unsigned packet_count)
{
    if (packet_count <= 0 || packet_count > XIPH_MAX_HEADER_COUNT)
        return VLC_EGENERIC;

    /* Compute the size needed for the whole extra data */
    unsigned payload_size = 0;
    unsigned header_size = 1;
    for (unsigned i = 0; i < packet_count; i++) {
        payload_size += packet_size[i];
        if (i < packet_count - 1)
            header_size += 1 + packet_size[i] / 255;
    }

    /* */
    *extra_size = header_size + payload_size;
    *extra = malloc(*extra_size);
    if (*extra == NULL)
        return VLC_ENOMEM;

    /* Write the header */
    uint8_t *current = (uint8_t*)*extra;
    *current++ = packet_count - 1;
    for (unsigned i = 0; i < packet_count - 1; i++) {
        unsigned t = packet_size[i];
        for (;;) {
            if (t >= 255) {
                *current++ = 255;
                t -= 255;
            } else {
                *current++ = t;
                break;
            }
        }
    }

    /* Copy the payloads */
    for (unsigned i = 0; i < packet_count; i++) {
        if (packet_size[i] > 0) {
            memcpy(current, packet[i], packet_size[i]);
            current += packet_size[i];
        }
    }
    assert(current == (uint8_t*)*extra + *extra_size);
    return VLC_SUCCESS;
}

static inline int xiph_AppendHeaders(int *extra_size, void **extra,
                                     unsigned size, const void *data)
{
    unsigned packet_size[XIPH_MAX_HEADER_COUNT];
    const void *packet[XIPH_MAX_HEADER_COUNT];
    unsigned count;

    if (*extra_size > 0 && *extra) {
        if (xiph_SplitHeaders(packet_size, packet, &count, *extra_size, *extra))
            return VLC_EGENERIC;
    } else {
        count = 0;
    }
    if (count >= XIPH_MAX_HEADER_COUNT)
        return VLC_EGENERIC;

    void *old = *extra;

    packet_size[count] = size;
    packet[count]      = (void*)data;
    if (xiph_PackHeaders(extra_size, extra, packet_size,
                         packet, count + 1)) {
        *extra_size = 0;
        *extra      = NULL;
    }

    free(old);

    if (*extra_size <= 0)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}
