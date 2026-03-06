/*****************************************************************************
 * vlc_clipboard.h: Clipboard (copy image to system clipboard)
 *****************************************************************************
 * Copyright (C) 2026 the VideoLAN team
 *
 * Authors: Sergey Degtyar <sergeydegtyar@internet.ru>
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

#ifndef VLC_CLIPBOARD_H
#define VLC_CLIPBOARD_H 1

#include <vlc_common.h>
#include <vlc_block.h>

/**
 * \defgroup clipboard Clipboard
 * \ingroup output
 *
 * Copy image data to the system clipboard for pasting into other applications.
 *
 * @{
 */

/**
 * Copy image data to the system clipboard.
 *
 * \param obj VLC object for logging
 * \param p_image Block containing encoded image data (e.g. PNG, JPEG)
 * \param psz_mime MIME type of the image (e.g. "image/png", "image/jpeg")
 * \return VLC_SUCCESS on success, VLC_EGENERIC on failure or unsupported platform
 */
VLC_API int vlc_clipboard_CopyImage(vlc_object_t *obj,
                                    const block_t *p_image,
                                    const char *psz_mime);

/** @} */

#endif /* VLC_CLIPBOARD_H */
