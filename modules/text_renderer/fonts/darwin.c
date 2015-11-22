/*****************************************************************************
 * darwin.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Salah-Eddin Shaban <salshaaban@gmail.com>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>                                      /* filter_sys_t */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#include "../platform_fonts.h"

char* getPathForFontDescription(CTFontDescriptorRef fontDescriptor);

char* getPathForFontDescription(CTFontDescriptorRef fontDescriptor)
{
    CFURLRef url = CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontURLAttribute);
    CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    char *cpath = (char *)CFStringGetCStringPtr(path, kCFStringEncodingUTF8);
    char *retPath = NULL;
    if (cpath) {
        retPath = strdup(cpath);
    }
    CFRelease(path);
    CFRelease(url);
    return retPath;
}

char* CoreText_Select( filter_t *p_filter, const char* psz_fontname,
                       bool b_bold, bool b_italic,
                       int *i_idx, uni_char_t codepoint )
{
    VLC_UNUSED( i_idx );
    VLC_UNUSED( b_bold );
    VLC_UNUSED( b_italic );
    VLC_UNUSED( codepoint );

    if( psz_fontname == NULL )
        return NULL;

    const size_t numberOfAttributes = 3;
    CTFontDescriptorRef coreTextFontDescriptors[numberOfAttributes];
    CFMutableDictionaryRef coreTextAttributes[numberOfAttributes];
    CFStringRef attributeNames[numberOfAttributes] = {
        kCTFontFamilyNameAttribute,
        kCTFontDisplayNameAttribute,
        kCTFontNameAttribute,
    };

    CFStringRef fontName = CFStringCreateWithCString(kCFAllocatorDefault,
                                                     psz_fontname,
                                                     kCFStringEncodingUTF8);
    for (size_t x = 0; x < numberOfAttributes; x++) {
        coreTextAttributes[x] = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
        CFDictionaryAddValue(coreTextAttributes[x], attributeNames[x], fontName);
        coreTextFontDescriptors[x] = CTFontDescriptorCreateWithAttributes(coreTextAttributes[x]);
    }

    CFArrayRef coreTextFontDescriptorsArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&coreTextFontDescriptors, numberOfAttributes, NULL);

    CTFontCollectionRef coreTextFontCollection = CTFontCollectionCreateWithFontDescriptors(coreTextFontDescriptorsArray, 0);

    CFArrayRef matchedFontDescriptions = CTFontCollectionCreateMatchingFontDescriptors(coreTextFontCollection);

    CFIndex numberOfFoundFontDescriptions = CFArrayGetCount(matchedFontDescriptions);

    char *path = NULL;

    for (CFIndex i = 0; i < numberOfFoundFontDescriptions; i++) {
        CTFontDescriptorRef iter = CFArrayGetValueAtIndex(matchedFontDescriptions, i);
        path = getPathForFontDescription(iter);

        /* check if the path is empty, which can happen in rare circumstances */
        if (path != NULL) {
            if (strcmp("", path) == 0) {
                FREENULL(path);
                continue;
            }
        }

        break;
    }

    msg_Dbg( p_filter, "found '%s'", path );

    CFRelease(matchedFontDescriptions);
    CFRelease(coreTextFontCollection);

    for (size_t x = 0; x < numberOfAttributes; x++) {
        CFRelease(coreTextAttributes[x]);
        CFRelease(coreTextFontDescriptors[x]);
    }

    CFRelease(coreTextFontDescriptorsArray);
    CFRelease(fontName);

    return path;
}
