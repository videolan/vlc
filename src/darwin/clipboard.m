/*****************************************************************************
 * clipboard.m: macOS clipboard (NSPasteboard) - copy image
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

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

int vlc_clipboard_CopyImage_cocoa(vlc_object_t *obj,
                                  const void *p_buf, size_t i_buf,
                                  const char *psz_mime)
{
    if (p_buf == NULL || i_buf == 0)
        return VLC_EGENERIC;

    NSData *data = [NSData dataWithBytes:p_buf length:i_buf];
    if (data == nil)
        return VLC_EGENERIC;

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];

    /* Use the MIME type to choose pasteboard type. PNG is widely supported. */
    NSString *type = NSPasteboardTypePNG;
    if (psz_mime != NULL) {
        if (strcasecmp(psz_mime, "image/png") == 0)
            type = NSPasteboardTypePNG;
        else if (strcasecmp(psz_mime, "image/tiff") == 0)
            type = NSPasteboardTypeTIFF;
        else if (strcasecmp(psz_mime, "image/jpeg") == 0)
            type = @"public.jpeg";
    }

    BOOL ok = [pasteboard setData:data forType:type];
    if (!ok && (psz_mime == NULL || strcasecmp(psz_mime, "image/png") != 0))
        ok = [pasteboard setData:data forType:NSPasteboardTypePNG];

    if (!ok) {
        msg_Err(obj, "Failed to write image to pasteboard");
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
