/*****************************************************************************
 * vlc_decklink.h: Decklink Common includes
 *****************************************************************************
 * Copyright (C) 2018 LTN Global Communications
 *
 * Authors: Devin Heitmueller <dheitmueller@ltnglobal.com>
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

#ifndef VLC_DECKLINK_H
#define VLC_DECKLINK_H 1

/**
 * \file
 * This file defines Decklink portability macros and other functions
 */

#ifdef _WIN32
#include <initguid.h>
#endif
#include <DeckLinkAPI.h>

/* Portability code to deal with differences how the Blackmagic SDK
   handles strings on various platforms */
#if defined(__APPLE__)
#include <vlc_common.h>
#include <vlc_charset.h>
typedef CFStringRef decklink_str_t;
#define DECKLINK_STRDUP(s) FromCFString(s, kCFStringEncodingUTF8)
#define DECKLINK_FREE(s) CFRelease(s)
#define PRIHR  "X"
#elif defined(_WIN32)
#include <vlc_charset.h> // FromWide
typedef BSTR decklink_str_t;
#define DECKLINK_STRDUP(s) FromWide(s)
#define DECKLINK_FREE(s) SysFreeString(s)
#define PRIHR  "lX"

static inline IDeckLinkIterator *CreateDeckLinkIteratorInstance(void)
{
    IDeckLinkIterator *decklink_iterator;
    if (SUCCEEDED(CoCreateInstance(CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS (&decklink_iterator))))
        return decklink_iterator;
    return nullptr;
}

#else
typedef const char* decklink_str_t;
#define DECKLINK_STRDUP(s) strdup(s)
#define DECKLINK_FREE(s) free((void *) s)
#define PRIHR  "X"
#endif

#endif /* VLC_DECKLINK_H */

