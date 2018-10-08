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
#include <vlc_charset.h>                                     /* FromCFString */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#include "../platform_fonts.h"

char* getPathForFontDescription(CTFontDescriptorRef fontDescriptor);
void addNewFontToFamily(filter_t *p_filter, CTFontDescriptorRef iter, char *path, vlc_family_t *family);


char* getPathForFontDescription(CTFontDescriptorRef fontDescriptor)
{
    CFURLRef url = CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontURLAttribute);
    if (url == NULL)
        return NULL;
    CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (path == NULL) {
        CFRelease(url);
        return NULL;
    }
    char *retPath = FromCFString(path, kCFStringEncodingUTF8);
    CFRelease(path);
    CFRelease(url);
    return retPath;
}

void addNewFontToFamily(filter_t *p_filter, CTFontDescriptorRef iter, char *path, vlc_family_t *p_family)
{
    bool b_bold = false;
    bool b_italic = false;
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
    msg_Dbg(p_filter, "New font: bold %i italic %i path '%s'", b_bold, b_italic, path);
#else
    VLC_UNUSED(p_filter);
#endif
    NewFont(path, 0, b_bold, b_italic, p_family);

    CFRelease(fontTraits);
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

    CTFontCollectionRef coreTextFontCollection = NULL;
    CFArrayRef matchedFontDescriptions = NULL;

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

    CFArrayRef coreTextFontDescriptorsArray = CFArrayCreate(kCFAllocatorDefault,
                                                            (const void **)&coreTextFontDescriptors,
                                                            numberOfAttributes, NULL);

    coreTextFontCollection = CTFontCollectionCreateWithFontDescriptors(coreTextFontDescriptorsArray, 0);
    if (coreTextFontCollection == NULL) {
        msg_Warn(p_filter,"CTFontCollectionCreateWithFontDescriptors (1) failed!");
        goto end;
    }

    matchedFontDescriptions = CTFontCollectionCreateMatchingFontDescriptors(coreTextFontCollection);
    if (matchedFontDescriptions == NULL) {
        msg_Warn(p_filter, "CTFontCollectionCreateMatchingFontDescriptors (2) failed!");
        goto end;
    }

    CFIndex numberOfFoundFontDescriptions = CFArrayGetCount(matchedFontDescriptions);

    char *path = NULL;

    /* create a new family object */
    p_family = NewFamily(p_filter, psz_lc, &p_sys->p_families, &p_sys->family_map, psz_lc);
    if (unlikely(!p_family)) {
        goto end;
    }

    for (CFIndex i = 0; i < numberOfFoundFontDescriptions; i++) {
        CTFontDescriptorRef iter = CFArrayGetValueAtIndex(matchedFontDescriptions, i);
        path = getPathForFontDescription(iter);

        /* check if the path is empty, which can happen in rare circumstances */
        if (path == NULL || *path == '\0') {
            FREENULL(path);
            continue;
        }

        addNewFontToFamily(p_filter, iter, path, p_family);
    }

end:
    if (matchedFontDescriptions != NULL) {
        CFRelease(matchedFontDescriptions);
    }
    if (coreTextFontCollection != NULL) {
        CFRelease(coreTextFontCollection);
    }

    for (size_t x = 0; x < numberOfAttributes; x++) {
        CFRelease(coreTextAttributes[x]);
        CFRelease(coreTextFontDescriptors[x]);
    }

    CFRelease(coreTextFontDescriptorsArray);
    CFRelease(familyName);
    free(psz_lc);

    return p_family;
}

vlc_family_t *CoreText_GetFallbacks(filter_t *p_filter, const char *psz_family, uni_char_t codepoint)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if (unlikely(psz_family == NULL)) {
        return NULL;
    }

    vlc_family_t *p_family = NULL;
    CFStringRef postScriptFallbackFontname = NULL;
    CTFontDescriptorRef fallbackFontDescriptor = NULL;
    char *psz_lc_fallback = NULL;
    char *psz_fontPath = NULL;

    CFStringRef familyName = CFStringCreateWithCString(kCFAllocatorDefault,
                                                       psz_family,
                                                       kCFStringEncodingUTF8);
    CTFontRef font = CTFontCreateWithName(familyName, 0, NULL);
    uint32_t littleEndianCodePoint = OSSwapHostToLittleInt32(codepoint);
    CFStringRef codepointString = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                          (const UInt8 *)&littleEndianCodePoint,
                                                          sizeof(littleEndianCodePoint),
                                                          kCFStringEncodingUTF32LE,
                                                          false);
    CTFontRef fallbackFont = CTFontCreateForString(font, codepointString, CFRangeMake(0,1));
    CFStringRef fallbackFontFamilyName = CTFontCopyFamilyName(fallbackFont);

    /* create a new family object */
    char *psz_fallbackFamilyName = FromCFString(fallbackFontFamilyName, kCFStringEncodingUTF8);
    if (psz_fallbackFamilyName == NULL) {
        msg_Warn(p_filter, "Failed to convert font family name CFString to C string");
        goto done;
    }
#ifndef NDEBUG
    msg_Dbg(p_filter, "Will deploy fallback font '%s'", psz_fallbackFamilyName);
#endif

    psz_lc_fallback = ToLower(psz_fallbackFamilyName);

    p_family = vlc_dictionary_value_for_key(&p_sys->family_map, psz_lc_fallback);
    if (p_family) {
        goto done;
    }

    p_family = NewFamily(p_filter, psz_lc_fallback, &p_sys->p_families, &p_sys->family_map, psz_lc_fallback);
    if (unlikely(!p_family)) {
        goto done;
    }

    postScriptFallbackFontname = CTFontCopyPostScriptName(fallbackFont);
    fallbackFontDescriptor = CTFontDescriptorCreateWithNameAndSize(postScriptFallbackFontname, 0.);
    psz_fontPath = getPathForFontDescription(fallbackFontDescriptor);

    /* check if the path is empty, which can happen in rare circumstances */
    if (psz_fontPath == NULL || *psz_fontPath == '\0') {
        goto done;
    }

    addNewFontToFamily(p_filter, fallbackFontDescriptor, strdup(psz_fontPath), p_family);

done:
    CFRelease(familyName);
    CFRelease(font);
    CFRelease(codepointString);
    CFRelease(fallbackFont);
    CFRelease(fallbackFontFamilyName);
    free(psz_fallbackFamilyName);
    free(psz_lc_fallback);
    free(psz_fontPath);
    if (postScriptFallbackFontname != NULL)
        CFRelease(postScriptFallbackFontname);
    if (fallbackFontDescriptor != NULL)
        CFRelease(fallbackFontDescriptor);
    return p_family;
}
