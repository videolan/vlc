/*****************************************************************************
 * clipboard.c: Windows clipboard (Win32 API) - copy image
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

#include <windows.h>

/* Copy image data to clipboard. On Windows, CF_DIB expects a DIB (device-
 * independent bitmap); raw PNG/JPEG is not a standard format. So we register
 * a private format for PNG data so that some apps can paste, or we convert
 * to DIB. For now we try to set PNG data as a custom format for compatibility
 * with apps that support "PNG" clipboard format. */
int vlc_clipboard_CopyImage_win32(vlc_object_t *obj,
                                  const void *p_buf, size_t i_buf,
                                  const char *psz_mime)
{
    VLC_UNUSED(psz_mime);

    if (!OpenClipboard(NULL)) {
        msg_Err(obj, "OpenClipboard failed");
        return VLC_EGENERIC;
    }

    EmptyClipboard();

    /* Register and set PNG data so that apps supporting image/png can paste */
    UINT cf = RegisterClipboardFormatW(L"PNG");
    if (cf == 0) {
        CloseClipboard();
        msg_Err(obj, "RegisterClipboardFormat(PNG) failed");
        return VLC_EGENERIC;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, i_buf);
    if (hMem == NULL) {
        CloseClipboard();
        return VLC_EGENERIC;
    }
    void *pMem = GlobalLock(hMem);
    if (pMem == NULL) {
        GlobalFree(hMem);
        CloseClipboard();
        return VLC_EGENERIC;
    }
    memcpy(pMem, p_buf, i_buf);
    GlobalUnlock(hMem);

    if (SetClipboardData(cf, hMem) == NULL) {
        GlobalFree(hMem);
        CloseClipboard();
        msg_Err(obj, "SetClipboardData failed");
        return VLC_EGENERIC;
    }
    CloseClipboard();
    return VLC_SUCCESS;
}
