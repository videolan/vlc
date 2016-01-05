/*****************************************************************************
 * hpack.h: HPACK Header Compression for HTTP/2
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * \defgroup hpack HPACK compression
 * HTTP/2 header compression (HPACK)
 * \ingroup h2
 * @{
 */

struct hpack_decoder;

struct hpack_decoder *hpack_decode_init(size_t header_table_size);
void hpack_decode_destroy(struct hpack_decoder *);

int hpack_decode(struct hpack_decoder *dec, const uint8_t *data,
                 size_t length, char *headers[][2], unsigned max);

size_t hpack_encode_hdr_neverindex(uint8_t *restrict buf, size_t size,
                                   const char *name, const char *value);
size_t hpack_encode(uint8_t *restrict buf, size_t size,
                    const char *const headers[][2], unsigned count);

/** @} */
