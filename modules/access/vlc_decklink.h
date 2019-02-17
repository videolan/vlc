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

#include <DeckLinkAPI.h>

/* Portability code to deal with differences how the Blackmagic SDK
   handles strings on various platforms */
#ifdef _WIN32
#error FIXME: Win32 is known to not work for decklink.
#elif defined(__APPLE__)
#include <vlc_common.h>
#include <vlc_charset.h>
typedef CFStringRef decklink_str_t;
#define DECKLINK_STRDUP(s) FromCFString(s, kCFStringEncodingUTF8)
#define DECKLINK_FREE(s) CFRelease(s)
#else
typedef const char* decklink_str_t;
#define DECKLINK_STRDUP strdup
#define DECKLINK_FREE(s) free((void *) s)
#endif

#endif /* VLC_DECKLINK_H */

