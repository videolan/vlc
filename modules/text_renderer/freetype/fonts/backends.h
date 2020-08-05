/*****************************************************************************
 * backends.h : freetype font configuration backends declarations
 *****************************************************************************
 * Copyright (C) 2002 - 2015 VLC authors and VideoLAN
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
#ifdef __cplusplus
extern "C" {
#endif

#include "../lru.h"

struct vlc_font_select_t
{
    vlc_object_t *p_obj;
    filter_t *p_filter;

    /* If the callbacks return VLC_SUCCESS, this means the result is valid
       (can still be no match/NULL), and if not, this is a temp error. */
    int (*pf_select_family) ( vlc_font_select_t *, const char *psz_family,
                              const vlc_family_t ** );
    int (*pf_select_among_families)( vlc_font_select_t *, const fontfamilies_t *,
                                     const vlc_family_t ** );

    int (*pf_get_fallbacks) ( vlc_font_select_t *, const char *psz_family,
                              uni_char_t codepoint, vlc_family_t ** );
    int (*pf_get_fallbacks_among_families) ( vlc_font_select_t *, const fontfamilies_t *,
                                             uni_char_t codepoint, vlc_family_t ** );

    /**
     * This is the master family list. It owns the lists of vlc_font_t's
     * and should be freed using FreeFamiliesAndFonts()
     */
    vlc_family_t      *p_families;
    /* We need to limit caching of lookups as we do not control the names */
    vlc_lru           *families_lookup_lru;

    /**
     * This maps a family name to a vlc_family_t within the master list
     */
    vlc_dictionary_t  family_map;

    /**
     * This maps a family name to a fallback list of vlc_family_t's.
     * Fallback lists only reference the lists of vlc_font_t's within the
     * master list, so they should be freed using FreeFamilies()
     */
    vlc_dictionary_t  fallback_map;

    int               i_fallback_counter;

#if defined( _WIN32 )
    void *p_dw_sys;
#endif
};

/***
 * PLATFORM SPECIFIC SELECTORS
 **/
#ifdef HAVE_FONTCONFIG
int FontConfig_GetFallbacksAmongFamilies( vlc_font_select_t *, const fontfamilies_t *,
                                          uni_char_t codepoint, vlc_family_t **pp_result );
int FontConfig_GetFamily( vlc_font_select_t *, const char *psz_family, const vlc_family_t ** );
int FontConfig_SelectAmongFamilies( vlc_font_select_t *fs, const fontfamilies_t *families,
                                    const vlc_family_t **pp_result );
int FontConfig_Prepare( vlc_font_select_t * );
void FontConfig_Unprepare( vlc_font_select_t * );
#endif /* FONTCONFIG */

#if defined( _WIN32 )
int DWrite_GetFamily( vlc_font_select_t *, const char *psz_family, const vlc_family_t ** );
int DWrite_GetFallbacks( vlc_font_select_t *, const char *psz_family,
                         uni_char_t codepoint, vlc_family_t ** );
int InitDWrite( vlc_font_select_t * );
int ReleaseDWrite( vlc_font_select_t * );
int DWrite_GetFontStream( vlc_font_select_t *, int i_index, FT_Stream *pp_stream );
#if !VLC_WINSTORE_APP
int Win32_GetFallbacks( vlc_font_select_t *, const char *psz_family,
                        uni_char_t codepoint, vlc_family_t ** );

int Win32_GetFamily( vlc_font_select_t *, const char *psz_family, const vlc_family_t ** );
#endif /* !VLC_WINSTORE_APP */
#endif /* _WIN32 */

#ifdef __APPLE__
int CoreText_GetFallbacks(vlc_font_select_t *, const char *psz_family,
                          uni_char_t codepoint, vlc_family_t **);
int CoreText_GetFamily(vlc_font_select_t *, const char *psz_family, const vlc_family_t **);
#endif /* __APPLE__ */

#ifdef __ANDROID__
int Android_GetFamily( vlc_font_select_t *, const char *psz_family, const vlc_family_t ** );
int Android_GetFallbacks( vlc_font_select_t *, const char *psz_family,
                          uni_char_t codepoint, vlc_family_t ** );
int Android_Prepare( vlc_font_select_t * );
#endif /* __ANDROID__ */

#ifdef __cplusplus
}
#endif
