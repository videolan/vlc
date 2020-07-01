/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2015 VLC authors and VideoLAN
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

/** \defgroup freetype_fonts Freetype Fonts management 
 * \ingroup freetype
 * Freetype text rendering cross platform
 * @{
 * \file
 * Freetype module
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Debug Stuff */
//#define DEBUG_PLATFORM_FONTS

#ifdef DEBUG_PLATFORM_FONTS
  #define Debug(...) msg_Dbg(__VA_ARGS__)
#else
  #define Debug(...)
#endif

#include "freetype.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default fonts */
#ifdef __APPLE__
# define SYSTEM_FONT_PATH                   "/System/Library/Fonts"
# define SYSTEM_DEFAULT_FONT_FILE           "HelveticaNeue.dfont"
# define SYSTEM_DEFAULT_FAMILY              "Helvetica Neue"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "Monaco.dfont"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY    "Monaco"
# define HAVE_GET_FONT_BY_FAMILY_NAME
# undef  HAVE_FONTCONFIG
#elif defined( _WIN32 )
# define SYSTEM_FONT_PATH                   "" /* Default path font found at run-time */
# define SYSTEM_DEFAULT_FONT_FILE           "arial.ttf"
# define SYSTEM_DEFAULT_FAMILY              "Arial"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "cour.ttf"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY    "Courier New"
# define HAVE_GET_FONT_BY_FAMILY_NAME
# undef  HAVE_FONTCONFIG
#elif defined( __OS2__ )
# define SYSTEM_FONT_PATH                   "/psfonts"
# define SYSTEM_DEFAULT_FONT_FILE           "tnrwt_k.ttf"
# define SYSTEM_DEFAULT_FAMILY              "Times New Roman WT K"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "mtsansdk.ttf"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY    "Monotype Sans Duospace WT K"
#elif defined( __ANDROID__ )
# define SYSTEM_FONT_PATH                   "/system/fonts"
# define SYSTEM_DEFAULT_FONT_FILE           "Roboto-Regular.ttf"
# define SYSTEM_DEFAULT_FAMILY              "sans-serif"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "DroidSansMono.ttf"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY    "Monospace"
# define HAVE_GET_FONT_BY_FAMILY_NAME
#else
# define SYSTEM_FONT_PATH                   "/usr/share/fonts/truetype/freefont"
# define SYSTEM_DEFAULT_FONT_FILE           "FreeSerifBold.ttf"
# define SYSTEM_DEFAULT_FAMILY              "Serif Bold"
# define SYSTEM_DEFAULT_MONOSPACE_FONT_FILE "FreeMono.ttf"
# define SYSTEM_DEFAULT_MONOSPACE_FAMILY    "Monospace"
#endif

#if defined(HAVE_FONTCONFIG) && !defined(HAVE_GET_FONT_BY_FAMILY_NAME)
# define HAVE_GET_FONT_BY_FAMILY_NAME
#endif

#ifndef DEFAULT_FONT_FILE
# define DEFAULT_FONT_FILE SYSTEM_DEFAULT_FONT_FILE
#endif

#ifndef DEFAULT_FAMILY
# define DEFAULT_FAMILY SYSTEM_DEFAULT_FAMILY
#endif

#ifndef DEFAULT_MONOSPACE_FONT_FILE
# define DEFAULT_MONOSPACE_FONT_FILE SYSTEM_DEFAULT_MONOSPACE_FONT_FILE
#endif

#ifndef DEFAULT_MONOSPACE_FAMILY
# define DEFAULT_MONOSPACE_FAMILY SYSTEM_DEFAULT_MONOSPACE_FAMILY
#endif

/**
 * Representation of the fonts (linked-list)
 */
typedef struct vlc_font_t vlc_font_t;
struct vlc_font_t
{
    vlc_font_t *p_next;   /**< next font in the chain */
    /**
     * path to the font file on disk, or ":/x" for font attachments, where x
     * is the attachment index within \ref filter_sys_t::pp_font_attachments
     */
    char       *psz_fontfile;
    int         i_index;   /**< index of the font in the font file, starts at 0 */
    bool        b_bold;    /**< if the font is a bold version */
    bool        b_italic;  /**< if the font is an italic version */
    FT_Face     p_face;    /**< the freetype structure for the font */
};

/**
 * Representation of font families (linked-list)
 */
struct vlc_family_t
{
    vlc_family_t *p_next; /**< next family in the chain */
    /**
     * Human-readable name, usually requested.
     * Can be fallback-xxxx for font attachments with no family name, and for fallback
     * fonts in Android.
     * This field is used only for loading the family fonts, and for debugging.
     * Apart from that, families are accessed either through the
     * \ref filter_sys_t::family_map dictionary, or as a member of some fallback list
     * in \ref filter_sys_t::fallback_map. And this field plays no role in either of
     * the two cases.
     */
    char         *psz_name;
    vlc_font_t   *p_fonts; /**< fonts matching this family */
};

#define FB_LIST_ATTACHMENTS "attachments"
#define FB_LIST_DEFAULT     "default"
#define FB_NAME             "fallback"

vlc_font_select_t * FontSelectNew( filter_t * );
void FontSelectDelete( vlc_font_select_t * );

/**
 * Get a pointer to the vlc_family_t in the master list that matches \p psz_family.
 * Add this family to the list if it hasn't been added yet.
 */
const vlc_family_t * FontSelectGetFamily( vlc_font_select_t *, const char *psz_family );

/**
 * Get the fallback list for \p psz_family from the system and cache
 * it in \ref fallback_map.
 * On Windows fallback lists are populated progressively as required
 * using Uniscribe, so we need the codepoint here.
 */
vlc_family_t * FontSelectGetFallbacks( vlc_font_select_t *, const char *psz_family,
                                       uni_char_t codepoint );


/* ******************
 * Family and fonts *
 ********************/

/**
 * Creates a new family.
 *
 * \param psz_family the usual font family name, human-readable;
 *                   if NULL, will use "fallback-xxxx"[IN]
 * \param pp_list the family list where to append the new family;
 *        can be NULL if not in a list, or if the family is to be appended to a fallback list
 *        within \ref filter_sys_t::fallback_map [IN]
 * \param p_dict dictionary where to insert this family; can be NULL.
 *        If the dictionary already has an entry for \p psz_key, which is often the case when adding
 *        a new fallback to a fallback list within \ref filter_sys_t::fallback_map, the new family will be
 *        appended there [IN]
 * \param psz_key specific key for the dictionary.
 *        If NULL will use whatever is used for the family name, whether it is the specified \p psz_family
 *        or "fallback-xxxx" [IN]
 *
 * \return the new family representation
 */
vlc_family_t *NewFamily( vlc_font_select_t *, const char *psz_family,
                         vlc_family_t **pp_list, vlc_dictionary_t *p_dict,
                         const char *psz_key );

vlc_family_t * DeclareNewFamily( vlc_font_select_t *, const char *psz_family );
int DeclareFamilyAsAttachMenFallback( vlc_font_select_t *, vlc_family_t * );

char *CreateUniqueFamilyKey( vlc_font_select_t * );

/**
 * Creates a new font.
 *
 * \param psz_fontfile font file, or ":/x" for font attachments, where x is the attachment index
 *        within \ref filter_sys_t::pp_font_attachments [IN]
 * \param i_index index of the font in the font file [IN]
 * \param b_bold is a bold font or not [IN]
 * \param b_bold is an italic or not [IN]
 * \param p_parent parent family.
 *                 If not NULL, the font will be associated to this family, and
 *                 appended to the font list in that family [IN]
 *
 * \remark This function takes ownership of \p psz_fontfile
 * \return the new font
 */
vlc_font_t *NewFont( char *psz_fontfile, int i_index,
                     bool b_bold, bool b_italic,
                     vlc_family_t *p_parent );

/**
 * Free families and fonts associated.
 *
 * \param p_family the family to free [IN]
 */
void FreeFamiliesAndFonts( vlc_family_t *p_family );

/**
 * Free families, but not the fonts associated.
 *
 * \param p_families the families to free [IN]
 */
void FreeFamilies( void *p_families, void *p_obj );

/**
 * Construct the default fallback list
 *
 * The default fallback list will be searched when all else fails.
 * It should contain at least one font for each Unicode script.
 *
 * No default list is required with FontConfig because FontConfig returns
 * comprehensive fallback lists. On Windows a default list is used because
 * Uniscribe seems less reliable than FontConfig in this regard.
 *
 * On Android, all fallback families reside within the default fallback list,
 * which is populated in Android_ParseFamily(), in the XML_READER_ENDELEM section
 * of that function, where each family is added to the default list if
 * its name has "fallback" in it. So InitDefaultList() is not called on Android.
 *
 * \param fs the vlc_font_select_t handle [IN]
 * \param ppsz_default the table default fonts [IN]
 * \param i_size the size of the supplied table [IN]
 *
 * \return the default fallback list
 */
vlc_family_t *InitDefaultList( vlc_font_select_t *fs, const char *const *ppsz_default,
                               int i_size );

/* Debug Helpers */
#ifdef DEBUG_PLATFORM_FONTS
void DumpFamilies( vlc_font_select_t * );
#endif

/* String helpers */
char* ToLower( const char *psz_src );

/* Size helper, depending on the scaling factor */
int ConvertToLiveSize( filter_t *p_filter, const text_style_t *p_style );


/* Only for fonts implementors */
vlc_family_t *SearchFallbacks( vlc_font_select_t *, vlc_family_t *p_fallbacks,
                                      uni_char_t codepoint );
FT_Face GetFace( vlc_font_select_t *, vlc_font_t *p_font, uni_char_t codepoint );

char * MakeFilePath( vlc_font_select_t *, const char *psz_filename );

#ifdef __cplusplus
}
#endif

#endif //PLATFORM_FONTS_H
