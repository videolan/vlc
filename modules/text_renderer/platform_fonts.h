/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Felix Paul Kühne <fkuehne@videolan.org>
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

#ifndef PLATFORM_FONTS_H
#define PLATFORM_FONTS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef __OS2__
typedef uint16_t uni_char_t;
# define FREETYPE_TO_UCS    "UCS-2LE"
#else
typedef uint32_t uni_char_t;
# if defined(WORDS_BIGENDIAN)
#  define FREETYPE_TO_UCS   "UCS-4BE"
# else
#  define FREETYPE_TO_UCS   "UCS-4LE"
# endif
#endif

/* Default fonts */
#ifdef __APPLE__
# define SYSTEM_DEFAULT_FONT_FILE "/Library/Fonts/Arial Unicode.ttf"
# define SYSTEM_DEFAULT_FAMILY "Arial Unicode MS"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "/System/Library/Fonts/Monaco.dfont"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY "Monaco"
#elif defined( _WIN32 )
# define SYSTEM_DEFAULT_FONT_FILE "arial.ttf" /* Default path font found at run-time */
# define SYSTEM_DEFAULT_FAMILY "Arial"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "cour.ttf"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY "Courier New"
#elif defined( __OS2__ )
# define SYSTEM_DEFAULT_FONT_FILE "/psfonts/tnrwt_k.ttf"
# define SYSTEM_DEFAULT_FAMILY "Times New Roman WT K"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "/psfonts/mtsansdk.ttf"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY "Monotype Sans Duospace WT K"
#elif defined( __ANDROID__ )
# define SYSTEM_DEFAULT_FONT_FILE "/system/fonts/DroidSans-Bold.ttf"
# define SYSTEM_DEFAULT_FAMILY "Droid Sans Bold"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "/system/fonts/DroidSansMono.ttf"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY "Droid Sans Mono"
#else
# define SYSTEM_DEFAULT_FONT_FILE "/usr/share/fonts/truetype/freefont/FreeSerifBold.ttf"
# define SYSTEM_DEFAULT_FAMILY "Serif Bold"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "/usr/share/fonts/truetype/freefont/FreeMono.ttf"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY "Monospace"
#endif

#ifndef DEFAULT_FONT_FILE
#define DEFAULT_FONT_FILE SYSTEM_DEFAULT_FONT_FILE
#endif

#ifndef DEFAULT_FAMILY
#define DEFAULT_FAMILY SYSTEM_DEFAULT_FAMILY
#endif

#ifndef DEFAULT_MONOSPACE_FONT_FILE
#define DEFAULT_MONOSPACE_FONT_FILE SYSTEM_DEFAULT_MONOSPACE_FONT_FILE
#endif

#ifndef DEFAULT_MONOSPACE_FAMILY
#define DEFAULT_MONOSPACE_FAMILY SYSTEM_DEFAULT_MONOSPACE_FAMILY
#endif

typedef struct vlc_font_t vlc_font_t;
struct vlc_font_t
{
    vlc_font_t *p_next;
    char       *psz_fontfile;
    int         i_index;
    bool        b_bold;
    bool        b_italic;
    FT_Face     p_face;
};

typedef struct vlc_family_t vlc_family_t;
struct vlc_family_t
{
    vlc_family_t *p_next;
    char         *psz_name;
    vlc_font_t   *p_fonts;
};

#define FB_LIST_ATTACHMENTS "attachments"
#define FB_LIST_DEFAULT     "default"
#define FB_NAME             "fallback"

#ifdef HAVE_FONTCONFIG
const vlc_family_t *FontConfig_GetFamily( filter_t *p_filter, const char *psz_family );
vlc_family_t *FontConfig_GetFallbacks( filter_t *p_filter, const char *psz_family,
                                       uni_char_t codepoint );
void FontConfig_BuildCache( filter_t *p_filter );
#endif

#if defined( _WIN32 ) && !VLC_WINSTORE_APP
char* Win32_Select( filter_t *p_filter, const char* family,
                    bool b_bold, bool b_italic,
                    int *i_idx, uni_char_t codepoint );

#endif /* _WIN32 */

#ifdef __APPLE__
#if !TARGET_OS_IPHONE
char* MacLegacy_Select( filter_t *p_filter, const char* psz_fontname,
                        bool b_bold, bool b_italic,
                        int *i_idx, uni_char_t codepoint );
#endif
#endif

char* Dummy_Select( filter_t *p_filter, const char* family,
                    bool b_bold, bool b_italic,
                    int *i_idx, uni_char_t codepoint );

#define File_Select(a) Dummy_Select(NULL, a, 0, 0, NULL, 0)

char* Generic_Select( filter_t *p_filter, const char* family,
                      bool b_bold, bool b_italic,
                      int *i_idx, uni_char_t codepoint );

static inline void AppendFont( vlc_font_t **pp_list, vlc_font_t *p_font )
{
    while( *pp_list )
        pp_list = &( *pp_list )->p_next;

    *pp_list = p_font;
}

static inline void AppendFamily( vlc_family_t **pp_list, vlc_family_t *p_family )
{
    while( *pp_list )
        pp_list = &( *pp_list )->p_next;

    *pp_list = p_family;
}

vlc_family_t *NewFamily( filter_t *p_filter, const char *psz_family,
                         vlc_family_t **pp_list, vlc_dictionary_t *p_dict,
                         const char *psz_key );

/* This function takes ownership of psz_fontfile */
vlc_font_t *NewFont( char *psz_fontfile, int i_index,
                     bool b_bold, bool b_italic,
                     vlc_family_t *p_parent );

void FreeFamiliesAndFonts( vlc_family_t *p_family );
void FreeFamilies( void *p_families, void *p_obj );


vlc_family_t *InitDefaultList( filter_t *p_filter, const char *const *ppsz_default,
                               int i_size );

void DumpFamily( filter_t *p_filter, const vlc_family_t *p_family,
                 bool b_dump_fonts, int i_max_families );

void DumpDictionary( filter_t *p_filter, const vlc_dictionary_t *p_dict,
                     bool b_dump_fonts, int i_max_families );

char* ToLower( const char *psz_src );

#endif //PLATFORM_FONTS_H
