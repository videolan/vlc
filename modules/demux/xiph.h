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
#define XIPH_MAX_HEADER_COUNT (256)

/* Temp ffmpeg vorbis format */
static inline bool xiph_IsOldFormat( const void *extra, unsigned int i_extra )
{
    if ( i_extra >= 6 && GetWBE( extra ) == 30 )
        return true;
    else
        return false;
}

static inline unsigned int xiph_CountHeaders( const void *extra, unsigned int i_extra )
{
    const uint8_t *p_extra = (uint8_t*) extra;
    if ( !i_extra ) return 0;
    if ( xiph_IsOldFormat( extra, i_extra ) )
    {
        /* Check headers count */
        unsigned int overall_len = 6;
        for ( int i=0; i<3; i++ )
        {
            uint16_t i_size = GetWBE( extra );
            p_extra += 2 + i_size;
            if ( overall_len > i_extra - i_size )
                return 0;
            overall_len += i_size;
        }
        return 3;
    }
    else
    {
        return *p_extra + 1;
    }
}

static inline int xiph_SplitHeaders(unsigned packet_size[], void * packet[], unsigned *packet_count,
                                    unsigned extra_size, const void *extra)
{
    const uint8_t *current = (const uint8_t *)extra;
    const uint8_t *end = &current[extra_size];
    if (extra_size < 1)
        return VLC_EGENERIC;

    /* Parse the packet count and their sizes */
    const unsigned count = xiph_CountHeaders( current++, extra_size );
    if (packet_count)
        *packet_count = count;

    if ( xiph_IsOldFormat( extra, extra_size ) )
    {
        uint8_t *p_extra = (uint8_t*) extra;
        unsigned int overall_len = count << 1;
        for ( unsigned int i=0; i < count; i++ ) {
            packet_size[i] = GetWBE( p_extra );
            packet[i] = p_extra + 2;
            p_extra += packet_size[i] + 2;
            if (overall_len > extra_size - packet_size[i])
                return VLC_EGENERIC;
            overall_len += packet_size[i];
        }
    }
    else
    {
        int size = 0;
        for (unsigned i = 0; i < count - 1; i++) {
            packet_size[i] = 0;
            for (;;) {
                if (current >= end)
                    return VLC_EGENERIC;
                packet_size[i] += *current;
                if (*current++ != 255)
                    break;
            }
            size += packet_size[i];
        }
        if (end - current < size)
            return VLC_EGENERIC;
        packet_size[count - 1] = end - current - size;

        for (unsigned i = 0; i < count; i++)
            if (packet_size[i] > 0) {
                packet[i] = (void *) current;
                current += packet_size[i];
            }
    }
    return VLC_SUCCESS;
}

static inline int xiph_PackHeaders(int *extra_size, void **extra,
                                   unsigned packet_size[], const void *packet[], unsigned packet_count )
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
    void *packet[XIPH_MAX_HEADER_COUNT];
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
                         (const void **)packet, count + 1)) {
        *extra_size = 0;
        *extra      = NULL;
    }

    free(old);

    if (*extra_size <= 0)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

