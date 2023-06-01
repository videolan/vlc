/*****************************************************************************
 * storage.h
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#ifndef HLS_STORAGE_H
#define HLS_STORAGE_H

/**
 * Handy simple storage abstraction to allow seamless support of in-memory or
 * filesystem HLS segment/manifest storage.
 */

struct hls_storage_config
{
    const char *name;
    const char *mime;
};

typedef struct hls_storage
{
    const char *mime;
    /**
     * Get a copy of the whole storage content.
     *
     * \param[out] dest Pointer on a byte buffer, will be freshly allocated by
     * the function call. \return Byte count of the byte buffer. \retval -1 On
     * allocation error.
     */
    ssize_t (*get_content)(const struct hls_storage *, uint8_t **dest);
} hls_storage_t;

/**
 * Create an HLS opaque storage from a chain of blocks.
 *
 * \note The returned storage must be destroyed with \ref hls_storage_Destroy.
 *
 * \param content The block chain.
 * \param hls_storage_config The storage specific config.
 * \param hls_config The global hls config.
 *
 * \return An opaque pointer on the HLS storage.
 * \retval NULL on allocation error.
 */
hls_storage_t *hls_storage_FromBlocks(block_t *content,
                                      const struct hls_storage_config *,
                                      const struct hls_config *) VLC_USED;

/**
 * Create an HLS opaque storage from a byte buffer.
 *
 * \note The returned storage must be destroyed with \ref hls_storage_Destroy.
 *
 * \param data Pointer on the buffer.
 * \param size Byte size of the buffer.
 * \param hls_storage_config The storage specific config.
 * \param hls_config The global hls config.
 *
 * \return An opaque pointer on the HLS storage.
 * \retval NULL on allocation error.
 */
hls_storage_t *hls_storage_FromBytes(void *data,
                                     size_t size,
                                     const struct hls_storage_config *,
                                     const struct hls_config *) VLC_USED;

size_t hls_storage_GetSize(const hls_storage_t *);

void hls_storage_Destroy(hls_storage_t *);

#endif
