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

struct vlc_font_select_t
{
    vlc_object_t *p_obj;
    filter_t *p_filter;

    const vlc_family_t * (*pf_get_family) ( vlc_font_select_t *, const char *psz_family );

    vlc_family_t * (*pf_get_fallbacks) ( vlc_font_select_t *, const char *psz_family,
                     uni_char_t codepoint );

    /**
     * This is the master family list. It owns the lists of vlc_font_t's
     * and should be freed using FreeFamiliesAndFonts()
     */
    vlc_family_t      *p_families;

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
vlc_family_t *FontConfig_GetFallbacks( vlc_font_select_t *, const char *psz_family,
                                       uni_char_t codepoint );
const vlc_family_t *FontConfig_GetFamily( vlc_font_select_t *, const char *psz_family );
int FontConfig_Prepare( vlc_font_select_t * );
void FontConfig_Unprepare( vlc_font_select_t * );
#endif /* FONTCONFIG */

#if defined( _WIN32 )
const vlc_family_t *DWrite_GetFamily( vlc_font_select_t *, const char *psz_family );
vlc_family_t *DWrite_GetFallbacks( vlc_font_select_t *, const char *psz_family,
                                  uni_char_t codepoint );
int InitDWrite( vlc_font_select_t * );
int ReleaseDWrite( vlc_font_select_t * );
int DWrite_GetFontStream( vlc_font_select_t *, int i_index, FT_Stream *pp_stream );
#if !VLC_WINSTORE_APP
vlc_family_t *Win32_GetFallbacks( vlc_font_select_t *, const char *psz_family,
                                  uni_char_t codepoint );

const vlc_family_t *Win32_GetFamily( vlc_font_select_t *, const char *psz_family );
#endif /* !VLC_WINSTORE_APP */
#endif /* _WIN32 */

#ifdef __APPLE__
vlc_family_t *CoreText_GetFallbacks(vlc_font_select_t *, const char *psz_family, uni_char_t codepoint);
const vlc_family_t *CoreText_GetFamily(vlc_font_select_t *, const char *psz_family);
#endif /* __APPLE__ */

#ifdef __ANDROID__
const vlc_family_t *Android_GetFamily( vlc_font_select_t *, const char *psz_family );
vlc_family_t *Android_GetFallbacks( vlc_font_select_t *, const char *psz_family,
                                    uni_char_t codepoint );
int Android_Prepare( vlc_font_select_t * );
#endif /* __ANDROID__ */

#ifdef __cplusplus
}
#endif
