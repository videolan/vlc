/*****************************************************************************
 * file.h: HTTP read-only file
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

#include <stdint.h>

/**
 * \defgroup http_file Files
 * HTTP read-only files
 * \ingroup http_res
 * @{
 */

struct vlc_http_mgr;
struct vlc_http_resource;
struct block_t;

/**
 * Creates an HTTP file.
 *
 * Allocates a structure for a remote HTTP-served read-only file.
 *
 * @param url URL of the file to read
 * @param ua user agent string (or NULL to ignore)
 * @param ref referral URL (or NULL to ignore)
 *
 * @return an HTTP resource object pointer, or NULL on error
 */
struct vlc_http_resource *vlc_http_file_create(struct vlc_http_mgr *mgr,
                                               const char *url, const char *ua,
                                               const char *ref);

/**
 * Gets file size.
 *
 * Determines the file size in bytes.
 *
 * @return Bytes count or (uintmax_t)-1 if unknown.
 */
uintmax_t vlc_http_file_get_size(struct vlc_http_resource *);

/**
 * Checks seeking support.
 *
 * @retval true if file supports seeking
 * @retval false if file does not support seeking
 */
bool vlc_http_file_can_seek(struct vlc_http_resource *);

/**
 * Sets the read offset.
 *
 * @param offset byte offset of next read
 * @retval 0 if seek succeeded
 * @retval -1 if seek failed
 */
int vlc_http_file_seek(struct vlc_http_resource *, uintmax_t offset);

/**
 * Reads data.
 *
 * Reads data from a file and update the file offset.
 */
struct block_t *vlc_http_file_read(struct vlc_http_resource *);

#define vlc_http_file_get_status vlc_http_res_get_status
#define vlc_http_file_get_redirect vlc_http_res_get_redirect
#define vlc_http_file_get_type vlc_http_res_get_type
#define vlc_http_file_destroy vlc_http_res_destroy

/** @} */
