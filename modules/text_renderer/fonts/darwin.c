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

const vlc_family_t *CoreText_GetFamily(filter_t *p_filter, const char *psz_family)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if (unlikely(psz_family == NULL)) {
        return NULL;
    }

    char *psz_lc = ToLower(psz_family);
    if (unlikely(!psz_lc)) {
        return NULL;
    }

    /* let's double check if we have parsed this family already */
    vlc_family_t *p_family = vlc_dictionary_value_for_key(&p_sys->family_map, psz_lc);
    if (p_family) {
        free(psz_lc);
        return p_family;
    }

    /* create a new family object */
    p_family = NewFamily(p_filter, psz_lc, &p_sys->p_families, &p_sys->family_map, psz_lc);
    if (unlikely(!p_family)) {
        return NULL;
    }

    /* we search for family name, display name and name to find them all */
    const size_t numberOfAttributes = 3;
    CTFontDescriptorRef coreTextFontDescriptors[numberOfAttributes];
    CFMutableDictionaryRef coreTextAttributes[numberOfAttributes];
    CFStringRef attributeNames[numberOfAttributes] = {
        kCTFontFamilyNameAttribute,
        kCTFontDisplayNameAttribute,
        kCTFontNameAttribute,
    };

#ifndef NDEBUG
    msg_Dbg(p_filter, "Creating new family for '%s'", psz_family);
#endif

    CFStringRef familyName = CFStringCreateWithCString(kCFAllocatorDefault,
                                                       psz_family,
                                                       kCFStringEncodingUTF8);
    for (size_t x = 0; x < numberOfAttributes; x++) {
        coreTextAttributes[x] = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
        CFDictionaryAddValue(coreTextAttributes[x], attributeNames[x], familyName);
        coreTextFontDescriptors[x] = CTFontDescriptorCreateWithAttributes(coreTextAttributes[x]);
    }

    CFArrayRef coreTextFontDescriptorsArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&coreTextFontDescriptors, numberOfAttributes, NULL);

    CTFontCollectionRef coreTextFontCollection = CTFontCollectionCreateWithFontDescriptors(coreTextFontDescriptorsArray, 0);

    CFArrayRef matchedFontDescriptions = CTFontCollectionCreateMatchingFontDescriptors(coreTextFontCollection);

    CFIndex numberOfFoundFontDescriptions = CFArrayGetCount(matchedFontDescriptions);

    char *path = NULL;
    bool b_bold = false;
    bool b_italic = false;

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

        CFDictionaryRef fontTraits = CTFontDescriptorCopyAttribute(iter, kCTFontTraitsAttribute);
        CFNumberRef trait = CFDictionaryGetValue(fontTraits, kCTFontWeightTrait);
        float traitValue = 0.;
        CFNumberGetValue(trait, kCFNumberFloatType, &traitValue);
        b_bold = traitValue > 0.23;
        trait = CFDictionaryGetValue(fontTraits, kCTFontSlantTrait);
        traitValue = 0.;
        CFNumberGetValue(trait, kCFNumberFloatType, &traitValue);
        b_italic = traitValue > 0.03;

#ifndef NDEBUG
        msg_Dbg(p_filter, "New font for family '%s' bold %i italic %i path '%s'", psz_family, b_bold, b_italic, path);
#endif
        NewFont(path, 0, b_bold, b_italic, p_family);

        CFRelease(fontTraits);
    }

    CFRelease(matchedFontDescriptions);
    CFRelease(coreTextFontCollection);

    for (size_t x = 0; x < numberOfAttributes; x++) {
        CFRelease(coreTextAttributes[x]);
        CFRelease(coreTextFontDescriptors[x]);
    }

    CFRelease(coreTextFontDescriptorsArray);
    CFRelease(familyName);

    return p_family;
}

vlc_family_t *CoreText_GetFallbacks(filter_t *p_filter, const char *psz_family, uni_char_t codepoint)
{
    VLC_UNUSED(p_filter);
    VLC_UNUSED(psz_family);
    VLC_UNUSED(codepoint);
    return NULL;
}
