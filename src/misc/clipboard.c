/*****************************************************************************
 * clipboard.c: Copy image to system clipboard (dispatcher)
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_clipboard.h>

#if defined(__APPLE__)
int vlc_clipboard_CopyImage_cocoa(vlc_object_t *obj,
                                  const void *p_buf, size_t i_buf,
                                  const char *psz_mime);
#elif defined(_WIN32)
int vlc_clipboard_CopyImage_win32(vlc_object_t *obj,
                                  const void *p_buf, size_t i_buf,
                                  const char *psz_mime);
#endif

int vlc_clipboard_CopyImage(vlc_object_t *obj,
                            const block_t *p_image,
                            const char *psz_mime)
{
    if (obj == NULL || p_image == NULL || psz_mime == NULL)
        return VLC_EGENERIC;
    if (p_image->i_buffer == 0 || p_image->p_buffer == NULL)
        return VLC_EGENERIC;

#if defined(__APPLE__)
    return vlc_clipboard_CopyImage_cocoa(obj,
                                         p_image->p_buffer, p_image->i_buffer,
                                         psz_mime);
#elif defined(_WIN32)
    return vlc_clipboard_CopyImage_win32(obj,
                                         p_image->p_buffer, p_image->i_buffer,
                                         psz_mime);
#else
    msg_Dbg(obj, "Clipboard image copy not supported on this platform");
    return VLC_EGENERIC;
#endif
}
