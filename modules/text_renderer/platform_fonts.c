/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2015 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>                                      /* filter_sys_t */
#include <vlc_text_style.h>                                   /* text_style_t*/
#include <ctype.h>

/* apple stuff */
#ifdef __APPLE__
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#include <Carbon/Carbon.h>
#endif
#include <sys/param.h>                         /* for MAXPATHLEN */
#undef HAVE_FONTCONFIG
#endif

/* Win32 GDI */
#ifdef _WIN32
# include <windows.h>
# include <shlobj.h>
# include <usp10.h>
# include <vlc_charset.h>                                     /* FromT */
#endif

/* FontConfig */
#ifdef HAVE_FONTCONFIG
# include <fontconfig/fontconfig.h>
#endif

#ifdef __ANDROID__
#include <vlc_xml.h>
#include <vlc_stream.h>
#endif

#include "platform_fonts.h"
#include "freetype.h"

static FT_Face GetFace( filter_t *p_filter, vlc_font_t *p_font )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_font->p_face )
        return p_font->p_face;

    p_font->p_face = LoadFace( p_filter, p_font->psz_fontfile, p_font->i_index,
                               p_sys->p_default_style );

    return p_font->p_face;
}

static vlc_font_t *GetBestFont( filter_t *p_filter, const vlc_family_t *p_family,
                                bool b_bold, bool b_italic, uni_char_t codepoint )
{
    int i_best_score = 0;
    vlc_font_t *p_best_font = p_family->p_fonts;

    for( vlc_font_t *p_font = p_family->p_fonts; p_font; p_font = p_font->p_next )
    {
        int i_score = 0;

        if( codepoint )
        {
            FT_Face p_face = GetFace( p_filter, p_font );
            if( p_face && FT_Get_Char_Index( p_face, codepoint ) )
                i_score += 1000;
        }

        if( !!p_font->b_bold == !!b_bold )
            i_score += 100;
        if( !!p_font->b_italic == !!b_italic )
            i_score += 10;

        if( i_score > i_best_score )
        {
            p_best_font = p_font;
            i_best_score = i_score;
        }
    }

    return p_best_font;
}

static vlc_family_t *SearchFallbacks( filter_t *p_filter, vlc_family_t *p_fallbacks,
                                      uni_char_t codepoint )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_family_t *p_family = NULL;

    for( vlc_family_t *p_fallback = p_fallbacks; p_fallback;
         p_fallback = p_fallback->p_next )
    {
        if( !p_fallback->p_fonts )
        {
            const vlc_family_t *p_temp =
                    p_sys->pf_get_family( p_filter, p_fallback->psz_name );
            if( !p_temp || !p_temp->p_fonts )
                continue;
            p_fallback->p_fonts = p_temp->p_fonts;
        }

        FT_Face p_face = GetFace( p_filter, p_fallback->p_fonts );
        if( !p_face || !FT_Get_Char_Index( p_face, codepoint ) )
            continue;
        p_family = p_fallback;
        break;
    }

    return p_family;
}

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
                         const char *psz_key )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_family_t *p_family = NULL;

    p_family = calloc( 1, sizeof( *p_family ) );

    char *psz_name;
    if( psz_family && *psz_family )
        psz_name = ToLower( psz_family );
    else
        if( asprintf( &psz_name, FB_NAME"-%02d",
                      p_sys->i_fallback_counter++ ) < 0 )
            psz_name = NULL;

    char *psz_lc = NULL;
    if( likely( psz_name ) )
    {
        if( !psz_key )
            psz_lc = strdup( psz_name );
        else
            psz_lc = ToLower( psz_key );
    }

    if( unlikely( !p_family || !psz_name || !psz_lc ) )
    {
        free( p_family );
        free( psz_name );
        free( psz_lc );
        return NULL;
    }

    p_family->psz_name = psz_name;

    if( pp_list )
        AppendFamily( pp_list, p_family );

    if( p_dict )
    {
        vlc_family_t *p_root = vlc_dictionary_value_for_key( p_dict, psz_lc );
        if( p_root )
            AppendFamily( &p_root, p_family );
        else
            vlc_dictionary_insert( p_dict, psz_lc, p_family );
    }

    free( psz_lc );
    return p_family;
}

vlc_font_t *NewFont( char *psz_fontfile, int i_index,
                     bool b_bold, bool b_italic,
                     vlc_family_t *p_parent )
{
    vlc_font_t *p_font = calloc( 1, sizeof( *p_font ) );

    if( unlikely( !p_font ) )
    {
        free( psz_fontfile );
        return NULL;
    }

    p_font->psz_fontfile = psz_fontfile;
    p_font->i_index = i_index;
    p_font->b_bold = b_bold;
    p_font->b_italic = b_italic;

    if( p_parent )
    {
        /* Keep regular faces first */
        if( p_parent->p_fonts
         && ( p_parent->p_fonts->b_bold || p_parent->p_fonts->b_italic )
         && !b_bold && !b_italic )
        {
            p_font->p_next = p_parent->p_fonts;
            p_parent->p_fonts = p_font;
        }
        else
            AppendFont( &p_parent->p_fonts, p_font );
    }

    return p_font;
}

void FreeFamiliesAndFonts( vlc_family_t *p_family )
{
    if( p_family->p_next )
        FreeFamiliesAndFonts( p_family->p_next );

    for( vlc_font_t *p_font = p_family->p_fonts; p_font; )
    {
        vlc_font_t *p_temp = p_font->p_next;
        free( p_font->psz_fontfile );
        free( p_font );
        p_font = p_temp;
    }

    free( p_family->psz_name );
    free( p_family );
}

void FreeFamilies( void *p_families, void *p_obj )
{
    vlc_family_t *p_family = ( vlc_family_t * ) p_families;

    if( p_family->p_next )
        FreeFamilies( p_family->p_next, p_obj );

    free( p_family->psz_name );
    free( p_family );
}

vlc_family_t *InitDefaultList( filter_t *p_filter, const char *const *ppsz_default,
                               int i_size )
{

    vlc_family_t  *p_default  = NULL;
    filter_sys_t  *p_sys = p_filter->p_sys;

    for( int i = 0; i < i_size; ++i )
    {
        const vlc_family_t *p_family =
                p_sys->pf_get_family( p_filter, ppsz_default[ i ] );

        if( p_family )
        {
            vlc_family_t *p_temp =
                NewFamily( p_filter, ppsz_default[ i ], &p_default, NULL, NULL );

            if( unlikely( !p_temp ) )
                goto error;

            p_temp->p_fonts = p_family->p_fonts;
        }
    }

    if( p_default )
        vlc_dictionary_insert( &p_sys->fallback_map, FB_LIST_DEFAULT, p_default );

    return p_default;

error:
    if( p_default ) FreeFamilies( p_default, NULL );
    return NULL;
}

void DumpFamily( filter_t *p_filter, const vlc_family_t *p_family,
                 bool b_dump_fonts, int i_max_families )
{

    if( i_max_families < 0 )
        i_max_families = INT_MAX;

    for( int i = 0; p_family && i < i_max_families ; p_family = p_family->p_next, ++i )
    {
        msg_Dbg( p_filter, "\t[0x%"PRIxPTR"] %s",
                 ( uintptr_t ) p_family, p_family->psz_name );

        if( b_dump_fonts )
        {
            for( vlc_font_t *p_font = p_family->p_fonts; p_font; p_font = p_font->p_next )
            {
                const char *psz_style = NULL;
                if( !p_font->b_bold && !p_font->b_italic )
                    psz_style = "Regular";
                else if( p_font->b_bold && !p_font->b_italic )
                    psz_style = "Bold";
                else if( !p_font->b_bold && p_font->b_italic )
                    psz_style = "Italic";
                else if( p_font->b_bold && p_font->b_italic )
                    psz_style = "Bold Italic";

                msg_Dbg( p_filter, "\t\t[0x%"PRIxPTR"] (%s): %s - %d",
                         ( uintptr_t ) p_font, psz_style,
                         p_font->psz_fontfile, p_font->i_index );

            }

        }
    }
}

void DumpDictionary( filter_t *p_filter, const vlc_dictionary_t *p_dict,
                     bool b_dump_fonts, int i_max_families )
{
    char **ppsz_keys = vlc_dictionary_all_keys( p_dict );
    for( int i = 0; ppsz_keys[ i ]; ++i )
    {
        vlc_family_t *p_family = vlc_dictionary_value_for_key( p_dict, ppsz_keys[ i ] );
        msg_Dbg( p_filter, "Key: %s", ppsz_keys[ i ] );
        if( p_family )
            DumpFamily( p_filter, p_family, b_dump_fonts, i_max_families );
        free( ppsz_keys[ i ] );
    }
    free( ppsz_keys );
}

char* ToLower( const char *psz_src )
{
    int i_size = strlen( psz_src ) + 1;
    char *psz_buffer = malloc( i_size );
    if( unlikely( !psz_buffer ) )
        return NULL;

    for( int i = 0; i < i_size; ++i )
        psz_buffer[ i ] = tolower( psz_src[ i ] );

    return psz_buffer;
}

char* Generic_Select( filter_t *p_filter, const char* psz_family,
                      bool b_bold, bool b_italic,
                      int *i_idx, uni_char_t codepoint )
{

    filter_sys_t *p_sys = p_filter->p_sys;
    const vlc_family_t *p_family = NULL;
    vlc_family_t *p_fallbacks = NULL;

    if( codepoint )
    {
        /*
         * Try regular face of the same family first.
         * It usually has the best coverage.
         */
        const vlc_family_t *p_temp = p_sys->pf_get_family( p_filter, psz_family );
        if( p_temp && p_temp->p_fonts )
        {
            FT_Face p_face = GetFace( p_filter, p_temp->p_fonts );
            if( p_face && FT_Get_Char_Index( p_face, codepoint ) )
                p_family = p_temp;
        }

        /* Try font attachments */
        if( !p_family )
        {
            p_fallbacks = vlc_dictionary_value_for_key( &p_sys->fallback_map,
                                                        FB_LIST_ATTACHMENTS );
            if( p_fallbacks )
                p_family = SearchFallbacks( p_filter, p_fallbacks, codepoint );
        }

        /* Try system fallbacks */
        if( !p_family )
        {
            p_fallbacks = p_sys->pf_get_fallbacks( p_filter, psz_family, codepoint );
            if( p_fallbacks )
                p_family = SearchFallbacks( p_filter, p_fallbacks, codepoint );
        }

        /* Try the default fallback list, if any */
        if( !p_family )
        {
            p_fallbacks = vlc_dictionary_value_for_key( &p_sys->fallback_map,
                                                        FB_LIST_DEFAULT );
            if( p_fallbacks )
                p_family = SearchFallbacks( p_filter, p_fallbacks, codepoint );
        }

        if( !p_family )
            return NULL;
    }

    if( !p_family )
        p_family = p_sys->pf_get_family( p_filter, psz_family );

    vlc_font_t *p_font;
    if( p_family && ( p_font = GetBestFont( p_filter, p_family, b_bold,
                                            b_italic, codepoint ) ) )
    {
        *i_idx = p_font->i_index;
        return strdup( p_font->psz_fontfile );
    }

    return File_Select( SYSTEM_DEFAULT_FONT_FILE );
}

#ifdef HAVE_FONTCONFIG
void FontConfig_BuildCache( filter_t *p_filter )
{
    /* */
    msg_Dbg( p_filter, "Building font databases.");
    mtime_t t1, t2;
    t1 = mdate();

#ifdef __OS2__
    FcInit();
#endif

#if defined( _WIN32 ) || defined( __APPLE__ )
    dialog_progress_bar_t *p_dialog = NULL;
    FcConfig *fcConfig = FcInitLoadConfig();

    p_dialog = dialog_ProgressCreate( p_filter,
            _("Building font cache"),
            _("Please wait while your font cache is rebuilt.\n"
                "This should take less than a few minutes."), NULL );

/*    if( p_dialog )
        dialog_ProgressSet( p_dialog, NULL, 0.5 ); */

    FcConfigBuildFonts( fcConfig );
#if defined( __APPLE__ )
    // By default, scan only the directory /System/Library/Fonts.
    // So build the set of available fonts under another directories,
    // and add the set to the current configuration.
    FcConfigAppFontAddDir( NULL, "~/Library/Fonts" );
    FcConfigAppFontAddDir( NULL, "/Library/Fonts" );
    FcConfigAppFontAddDir( NULL, "/Network/Library/Fonts" );
    //FcConfigAppFontAddDir( NULL, "/System/Library/Fonts" );
#endif
    if( p_dialog )
    {
//        dialog_ProgressSet( p_dialog, NULL, 1.0 );
        dialog_ProgressDestroy( p_dialog );
        p_dialog = NULL;
    }
#endif
    t2 = mdate();
    msg_Dbg( p_filter, "Took %ld microseconds", (long)((t2 - t1)) );
}

const vlc_family_t *FontConfig_GetFamily( filter_t *p_filter, const char *psz_family )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    char *psz_lc = ToLower( psz_family );

    if( unlikely( !psz_lc ) )
        return NULL;

    vlc_family_t *p_family =
            vlc_dictionary_value_for_key( &p_sys->family_map, psz_lc );

    if( p_family != kVLCDictionaryNotFound )
    {
        free( psz_lc );
        return p_family;
    }

    p_family = NewFamily( p_filter, psz_lc, &p_sys->p_families,
                          &p_sys->family_map, psz_lc );

    free( psz_lc );
    if( !p_family )
        return NULL;

    bool b_bold, b_italic;

    for( int i = 0; i < 4; ++i )
    {
        switch( i )
        {
        case 0:
            b_bold = false;
            b_italic = false;
            break;
        case 1:
            b_bold = true;
            b_italic = false;
            break;
        case 2:
            b_bold = false;
            b_italic = true;
            break;
        case 3:
            b_bold = true;
            b_italic = true;
            break;
        }

        int i_index = 0;
        FcResult result = FcResultMatch;
        FcPattern *pat, *p_pat;
        FcChar8* val_s;
        FcBool val_b;
        char *psz_fontfile = NULL;
        FcConfig* config = NULL;

        /* Create a pattern and fill it */
        pat = FcPatternCreate();
        if (!pat) continue;

        /* */
        FcPatternAddString( pat, FC_FAMILY, (const FcChar8*) psz_family );
        FcPatternAddBool( pat, FC_OUTLINE, FcTrue );
        FcPatternAddInteger( pat, FC_SLANT, b_italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN );
        FcPatternAddInteger( pat, FC_WEIGHT, b_bold ? FC_WEIGHT_EXTRABOLD : FC_WEIGHT_NORMAL );

        /* */
        FcDefaultSubstitute( pat );
        if( !FcConfigSubstitute( config, pat, FcMatchPattern ) )
        {
            FcPatternDestroy( pat );
            continue;
        }

        /* Find the best font for the pattern, destroy the pattern */
        p_pat = FcFontMatch( config, pat, &result );
        FcPatternDestroy( pat );
        if( !p_pat || result == FcResultNoMatch ) continue;

        /* Check the new pattern */
        if( ( FcResultMatch != FcPatternGetBool( p_pat, FC_OUTLINE, 0, &val_b ) )
            || ( val_b != FcTrue ) )
        {
            FcPatternDestroy( p_pat );
            continue;
        }
        if( FcResultMatch != FcPatternGetInteger( p_pat, FC_INDEX, 0, &i_index ) )
        {
            i_index = 0;
        }

        if( FcResultMatch != FcPatternGetString( p_pat, FC_FAMILY, 0, &val_s ) )
        {
            FcPatternDestroy( p_pat );
            continue;
        }

        if( FcResultMatch == FcPatternGetString( p_pat, FC_FILE, 0, &val_s ) )
            psz_fontfile = strdup( (const char*)val_s );

        FcPatternDestroy( p_pat );

        if( !psz_fontfile )
            continue;

        NewFont( psz_fontfile, i_index, b_bold, b_italic, p_family );
    }

    return p_family;
}

vlc_family_t *FontConfig_GetFallbacks( filter_t *p_filter, const char *psz_family,
                                       uni_char_t codepoint )
{

    VLC_UNUSED( codepoint );

    vlc_family_t *p_family = NULL;
    filter_sys_t *p_sys    = p_filter->p_sys;

    char *psz_lc = ToLower( psz_family );

    if( unlikely( !psz_lc ) )
        return NULL;

    p_family = vlc_dictionary_value_for_key( &p_sys->fallback_map, psz_lc );

    if( p_family != kVLCDictionaryNotFound )
    {
        free( psz_lc );
        return p_family;
    }
    else
        p_family = NULL;

    const char *psz_last_name = "";
    FcPattern  *p_pattern = FcPatternCreate();
    FcValue     family;
    family.type = FcTypeString;
    family.u.s = ( const FcChar8* ) psz_family;
    FcPatternAdd( p_pattern, FC_FAMILY, family, FcFalse );
    if( FcConfigSubstitute( NULL, p_pattern, FcMatchPattern ) == FcTrue )
    {
        FcDefaultSubstitute( p_pattern );
        FcResult result;
        FcFontSet* p_font_set = FcFontSort( NULL, p_pattern, FcTrue, NULL, &result );
        if( p_font_set )
        {
            for( int i = 0; i < p_font_set->nfont; ++i )
            {
                char* psz_name = NULL;
                FcPatternGetString( p_font_set->fonts[i],
                                    FC_FAMILY, 0, ( FcChar8** )( &psz_name ) );

                /* Avoid duplicate family names */
                if( strcasecmp( psz_last_name, psz_name ) )
                {
                    vlc_family_t *p_temp = NewFamily( p_filter, psz_name,
                                                      &p_family, NULL, NULL );

                    if( unlikely( !p_temp ) )
                    {
                        FcFontSetDestroy( p_font_set );
                        FcPatternDestroy( p_pattern );
                        if( p_family )
                            FreeFamilies( p_family, NULL );
                        free( psz_lc );
                        return NULL;
                    }

                    psz_last_name = p_temp->psz_name;
                }
            }
            FcFontSetDestroy( p_font_set );
        }
    }
    FcPatternDestroy( p_pattern );

    if( p_family )
        vlc_dictionary_insert( &p_sys->fallback_map, psz_lc, p_family );

    free( psz_lc );
    return p_family;
}
#endif

#if defined( _WIN32 ) && !VLC_WINSTORE_APP
#define FONT_DIR_NT _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts")

static char *Trim( char *psz_text )
{
    int i_first_char = -1;
    int i_last_char = -1;
    int i_len = strlen( psz_text );

    for( int i = 0; i < i_len; ++i )
    {
        if( psz_text[i] != ' ')
        {
            if( i_first_char == -1 )
                i_first_char = i;
            i_last_char = i;
        }
    }

    psz_text[ i_last_char + 1 ] = 0;
    if( i_first_char != -1 ) psz_text = psz_text + i_first_char;

    return psz_text;
}

static int ConcatenatedIndex( char *psz_haystack, const char *psz_needle )
{
    char *psz_family = psz_haystack;
    char *psz_terminator = psz_haystack + strlen( psz_haystack );
    int i_index = 0;

    while( psz_family < psz_terminator )
    {
        char *psz_amp = strchr( psz_family, '&' );

        if( !psz_amp ) psz_amp = psz_terminator;

        *psz_amp = 0;

        psz_family = Trim( psz_family );
        if( !strcasecmp( psz_family, psz_needle ) )
            return i_index;

        psz_family = psz_amp + 1;
        ++i_index;
    }

    return -1;
}

static int GetFileFontByName( LPCTSTR font_name, char **psz_filename, int *i_index )
{
    HKEY hKey;
    TCHAR vbuffer[MAX_PATH];
    TCHAR dbuffer[256];

    if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, FONT_DIR_NT, 0, KEY_READ, &hKey)
            != ERROR_SUCCESS )
        return 1;

    char *font_name_temp = FromT( font_name );

    for( int index = 0;; index++ )
    {
        DWORD vbuflen = MAX_PATH - 1;
        DWORD dbuflen = 255;

        LONG i_result = RegEnumValue( hKey, index, vbuffer, &vbuflen,
                                      NULL, NULL, (LPBYTE)dbuffer, &dbuflen);
        if( i_result != ERROR_SUCCESS )
        {
            free( font_name_temp );
            RegCloseKey( hKey );
            return i_result;
        }

        char *psz_value = FromT( vbuffer );

        char *s = strchr( psz_value,'(' );
        if( s != NULL && s != psz_value ) s[-1] = '\0';

        int i_concat_idx = 0;
        if( ( i_concat_idx = ConcatenatedIndex( psz_value, font_name_temp ) ) != -1 )
        {
            *i_index = i_concat_idx;
            *psz_filename = FromT( dbuffer );
            free( psz_value );
            break;
        }

        free( psz_value );
    }

    free( font_name_temp );
    RegCloseKey( hKey );
    return 0;
}

static char* GetWindowsFontPath()
{
    wchar_t wdir[MAX_PATH];
    if( S_OK != SHGetFolderPathW( NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, wdir ) )
    {
        GetWindowsDirectoryW( wdir, MAX_PATH );
        wcscat( wdir, L"\\fonts" );
    }
    return FromWide( wdir );
}

static int CALLBACK EnumFontCallback(const ENUMLOGFONTEX *lpelfe, const NEWTEXTMETRICEX *metric,
                                     DWORD type, LPARAM lParam)
{
    VLC_UNUSED( metric );
    if( (type & RASTER_FONTTYPE) ) return 1;

    vlc_family_t *p_family = ( vlc_family_t * ) lParam;

    bool b_bold = ( lpelfe->elfLogFont.lfWeight == FW_BOLD );
    bool b_italic = ( lpelfe->elfLogFont.lfItalic != 0 );

    for( vlc_font_t *p_font = p_family->p_fonts; p_font; p_font = p_font->p_next )
        if( !!p_font->b_bold == !!b_bold && !!p_font->b_italic == !!b_italic )
            return 1;

    char *psz_filename = NULL;
    char *psz_fontfile = NULL;
    int   i_index      = 0;

    if( GetFileFontByName( (LPCTSTR)lpelfe->elfFullName, &psz_filename, &i_index ) )
        return 1;

    if( strchr( psz_filename, DIR_SEP_CHAR ) )
        psz_fontfile = psz_filename;
    else
    {
        /* Get Windows Font folder */
        char *psz_win_fonts_path = GetWindowsFontPath();
        if( asprintf( &psz_fontfile, "%s\\%s", psz_win_fonts_path, psz_filename ) == -1 )
        {
            free( psz_filename );
            free( psz_win_fonts_path );
            return 1;
        }
        free( psz_filename );
        free( psz_win_fonts_path );
    }

    NewFont( psz_fontfile, i_index, b_bold, b_italic, p_family );

    return 1;
}

const vlc_family_t *Win32_GetFamily( filter_t *p_filter, const char *psz_family )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    char *psz_lc = ToLower( psz_family );

    if( unlikely( !psz_lc ) )
        return NULL;

    vlc_family_t *p_family =
        vlc_dictionary_value_for_key( &p_sys->family_map, psz_lc );

    free( psz_lc );

    if( p_family )
        return p_family;

    p_family = NewFamily( p_filter, psz_family, &p_sys->p_families,
                          &p_sys->family_map, psz_family );

    if( unlikely( !p_family ) )
        return NULL;

    LOGFONT lf;
    lf.lfCharSet = DEFAULT_CHARSET;

    LPTSTR psz_fbuffer = ToT( psz_family );
    _tcsncpy( (LPTSTR)&lf.lfFaceName, psz_fbuffer, LF_FACESIZE );
    free( psz_fbuffer );

    /* */
    HDC hDC = GetDC( NULL );
    EnumFontFamiliesEx(hDC, &lf, (FONTENUMPROC)&EnumFontCallback, (LPARAM)p_family, 0);
    ReleaseDC(NULL, hDC);

    return p_family;
}

static int CALLBACK MetaFileEnumProc( HDC hdc, HANDLETABLE* table,
                                      CONST ENHMETARECORD* record,
                                      int table_entries, LPARAM log_font )
{
    VLC_UNUSED( hdc );
    VLC_UNUSED( table );
    VLC_UNUSED( table_entries );

    if( record->iType == EMR_EXTCREATEFONTINDIRECTW )
    {
        const EMREXTCREATEFONTINDIRECTW* create_font_record =
                ( const EMREXTCREATEFONTINDIRECTW * ) record;

        *( ( LOGFONT * ) log_font ) = create_font_record->elfw.elfLogFont;
    }
    return 1;
}

/*
 * This is a hack used by Chrome and WebKit to expose the fallback font used
 * by Uniscribe for some given text for use with custom shapers / font engines.
 */
static char *UniscribeFallback( const char *psz_family, uni_char_t codepoint )
{
    HDC          hdc          = NULL;
    HDC          meta_file_dc = NULL;
    HENHMETAFILE meta_file    = NULL;
    LPTSTR       psz_fbuffer  = NULL;
    char        *psz_result   = NULL;

    hdc = CreateCompatibleDC( NULL );
    if( !hdc )
        return NULL;

    meta_file_dc = CreateEnhMetaFile( hdc, NULL, NULL, NULL );
    if( !meta_file_dc )
        goto error;

    LOGFONT lf;
    memset( &lf, 0, sizeof( lf ) );

    psz_fbuffer = ToT( psz_family );
    if( !psz_fbuffer )
        goto error;
    _tcsncpy( ( LPTSTR ) &lf.lfFaceName, psz_fbuffer, LF_FACESIZE );
    free( psz_fbuffer );

    lf.lfCharSet = DEFAULT_CHARSET;
    HFONT hFont = CreateFontIndirect( &lf );
    if( !hFont )
        goto error;

    HFONT hOriginalFont = SelectObject( meta_file_dc, hFont );

    TCHAR text = codepoint;

    SCRIPT_STRING_ANALYSIS script_analysis;
    HRESULT hresult = ScriptStringAnalyse( meta_file_dc, &text, 1, 0, -1,
                            SSA_METAFILE | SSA_FALLBACK | SSA_GLYPHS | SSA_LINK,
                            0, NULL, NULL, NULL, NULL, NULL, &script_analysis );

    if( SUCCEEDED( hresult ) )
    {
        hresult = ScriptStringOut( script_analysis, 0, 0, 0, NULL, 0, 0, FALSE );
        ScriptStringFree( &script_analysis );
    }

    SelectObject( meta_file_dc, hOriginalFont );
    DeleteObject( hFont );
    meta_file = CloseEnhMetaFile( meta_file_dc );

    if( SUCCEEDED( hresult ) )
    {
        LOGFONT log_font;
        log_font.lfFaceName[ 0 ] = 0;
        EnumEnhMetaFile( 0, meta_file, MetaFileEnumProc, &log_font, NULL );
        if( log_font.lfFaceName[ 0 ] )
            psz_result = FromT( log_font.lfFaceName );
    }

    DeleteEnhMetaFile(meta_file);
    DeleteDC( hdc );
    return psz_result;

error:
    if( meta_file_dc ) DeleteEnhMetaFile( CloseEnhMetaFile( meta_file_dc ) );
    if( hdc ) DeleteDC( hdc );
    return NULL;
}

vlc_family_t *Win32_GetFallbacks( filter_t *p_filter, const char *psz_family,
                                  uni_char_t codepoint )
{
    vlc_family_t  *p_family      = NULL;
    vlc_family_t  *p_fallbacks   = NULL;
    filter_sys_t  *p_sys         = p_filter->p_sys;
    char          *psz_uniscribe = NULL;


    char *psz_lc = ToLower( psz_family );

    if( unlikely( !psz_lc ) )
        return NULL;

    p_fallbacks = vlc_dictionary_value_for_key( &p_sys->fallback_map, psz_lc );

    if( p_fallbacks )
        p_family = SearchFallbacks( p_filter, p_fallbacks, codepoint );

    if( !p_family )
    {
        psz_uniscribe = UniscribeFallback( psz_lc, codepoint );

        if( !psz_uniscribe )
            goto done;

        const vlc_family_t *p_uniscribe = Win32_GetFamily( p_filter, psz_uniscribe );
        if( !p_uniscribe || !p_uniscribe->p_fonts )
            goto done;

        FT_Face p_face = GetFace( p_filter, p_uniscribe->p_fonts );

        if( !p_face || !FT_Get_Char_Index( p_face, codepoint ) )
            goto done;

        p_family = NewFamily( p_filter, psz_uniscribe, NULL, NULL, NULL );

        if( unlikely( !p_family ) )
            goto done;

        p_family->p_fonts = p_uniscribe->p_fonts;

        if( p_fallbacks )
            AppendFamily( &p_fallbacks, p_family );
        else
            vlc_dictionary_insert( &p_sys->fallback_map,
                                   psz_lc, p_family );
    }

done:
    free( psz_lc );
    free( psz_uniscribe );
    return p_family;
}
#endif /* _WIN32 */

#ifdef __APPLE__
#if !TARGET_OS_IPHONE
char* MacLegacy_Select( filter_t *p_filter, const char* psz_fontname,
                        bool b_bold, bool b_italic,
                        int *i_idx, uni_char_t codepoint )
{
    VLC_UNUSED( b_bold );
    VLC_UNUSED( b_italic );
    VLC_UNUSED( codepoint );
    FSRef ref;
    unsigned char path[MAXPATHLEN];
    char * psz_path;

    CFStringRef  cf_fontName;
    ATSFontRef   ats_font_id;

    *i_idx = 0;

    if( psz_fontname == NULL )
        return NULL;

    msg_Dbg( p_filter, "looking for %s", psz_fontname );
    cf_fontName = CFStringCreateWithCString( kCFAllocatorDefault, psz_fontname, kCFStringEncodingUTF8 );

    ats_font_id = ATSFontFindFromName( cf_fontName, kATSOptionFlagsIncludeDisabledMask );

    if ( ats_font_id == 0 || ats_font_id == 0xFFFFFFFFUL )
    {
        msg_Dbg( p_filter, "ATS couldn't find %s by name, checking family", psz_fontname );
        ats_font_id = ATSFontFamilyFindFromName( cf_fontName, kATSOptionFlagsDefault );

        if ( ats_font_id == 0 || ats_font_id == 0xFFFFFFFFUL )
        {
            msg_Dbg( p_filter, "ATS couldn't find either %s nor its family, checking PS name", psz_fontname );
            ats_font_id = ATSFontFindFromPostScriptName( cf_fontName, kATSOptionFlagsDefault );

            if ( ats_font_id == 0 || ats_font_id == 0xFFFFFFFFUL )
            {
                msg_Err( p_filter, "ATS couldn't find %s (no font name, family or PS name)", psz_fontname );
                CFRelease( cf_fontName );
                return NULL;
            }
        }
    }
    CFRelease( cf_fontName );

    if ( noErr != ATSFontGetFileReference( ats_font_id, &ref ) )
    {
        msg_Err( p_filter, "ATS couldn't get file ref for %s", psz_fontname );
        return NULL;
    }

    /* i_idx calculation by searching preceding fontIDs */
    /* with same FSRef                                       */
    {
        ATSFontRef  id2 = ats_font_id - 1;
        FSRef       ref2;

        while ( id2 > 0 )
        {
            if ( noErr != ATSFontGetFileReference( id2, &ref2 ) )
                break;
            if ( noErr != FSCompareFSRefs( &ref, &ref2 ) )
                break;

            id2 --;
        }
        *i_idx = ats_font_id - ( id2 + 1 );
    }

    if ( noErr != FSRefMakePath( &ref, path, sizeof(path) ) )
    {
        msg_Err( p_filter, "failure when getting path from FSRef" );
        return NULL;
    }
    msg_Dbg( p_filter, "found %s", path );

    psz_path = strdup( (char *)path );

    return psz_path;
}
#endif
#endif

char* Dummy_Select( filter_t *p_filter, const char* psz_font,
                    bool b_bold, bool b_italic,
                    int *i_idx, uni_char_t codepoint )
{
    VLC_UNUSED(p_filter);
    VLC_UNUSED(b_bold);
    VLC_UNUSED(b_italic);
    VLC_UNUSED(codepoint);
    VLC_UNUSED(i_idx);

    char *psz_fontname;
# if defined( _WIN32 ) && !VLC_WINSTORE_APP
    /* Get Windows Font folder */
    char *psz_win_fonts_path = GetWindowsFontPath();
    if( asprintf( &psz_fontname, "%s\\%s", psz_win_fonts_path, psz_font ) == -1 )
    {
        psz_fontname = NULL;
        return NULL;
    }
    free(psz_win_fonts_path);
# else
    psz_fontname = strdup( psz_font );
# endif

    return psz_fontname;
}

#ifdef __ANDROID__
static int Android_ParseFamily( filter_t *p_filter, xml_reader_t *p_xml )
{
    filter_sys_t     *p_sys       = p_filter->p_sys;
    vlc_dictionary_t *p_dict      = &p_sys->family_map;
    vlc_family_t     *p_family    = NULL;
    char             *psz_lc      = NULL;
    int               i_counter   = 0;
    bool              b_bold      = false;
    bool              b_italic    = false;
    const char       *p_node      = NULL;
    int               i_type      = 0;

    while( ( i_type = xml_ReaderNextNode( p_xml, &p_node ) ) > 0 )
    {
        switch( i_type )
        {
        case XML_READER_STARTELEM:
            /*
             * Multiple names can reference the same family in Android. When
             * the first name is encountered we set p_family to the vlc_family_t
             * in the master list matching this name, and if no such family
             * exists we create a new one and add it to the master list.
             * If the master list does contain a family with that name it's one
             * of the font attachments, and the family will end up having embedded
             * fonts and system fonts.
             */
            if( !strcasecmp( "name", p_node ) )
            {
                i_type = xml_ReaderNextNode( p_xml, &p_node );

                if( i_type != XML_READER_TEXT || !p_node || !*p_node )
                {
                    msg_Warn( p_filter, "Android_ParseFamily: empty name" );
                    continue;
                }

                psz_lc = ToLower( p_node );
                if( unlikely( !psz_lc ) )
                    return VLC_ENOMEM;

                if( !p_family )
                {
                    p_family = vlc_dictionary_value_for_key( p_dict, psz_lc );
                    if( p_family == kVLCDictionaryNotFound )
                    {
                        p_family =
                            NewFamily( p_filter, psz_lc, &p_sys->p_families, NULL, NULL );

                        if( unlikely( !p_family ) )
                        {
                            free( psz_lc );
                            return VLC_ENOMEM;
                        }

                    }
                }

                if( vlc_dictionary_value_for_key( p_dict, psz_lc ) == kVLCDictionaryNotFound )
                    vlc_dictionary_insert( p_dict, psz_lc, p_family );
                free( psz_lc );
            }
            /*
             * If p_family has not been set by the time we encounter the first file,
             * it means this family has no name, and should be used only as a fallback.
             * We create a new family for it in the master list and later add it to
             * the "default" fallback list.
             */
            else if( !strcasecmp( "file", p_node ) )
            {
                i_type = xml_ReaderNextNode( p_xml, &p_node );

                if( i_type != XML_READER_TEXT || !p_node || !*p_node )
                {
                    ++i_counter;
                    continue;
                }

                if( !p_family )
                    p_family = NewFamily( p_filter, NULL, &p_sys->p_families,
                                          &p_sys->family_map, NULL );

                if( unlikely( !p_family ) )
                    return VLC_ENOMEM;

                switch( i_counter )
                {
                case 0:
                    b_bold = false;
                    b_italic = false;
                    break;
                case 1:
                    b_bold = true;
                    b_italic = false;
                    break;
                case 2:
                    b_bold = false;
                    b_italic = true;
                    break;
                case 3:
                    b_bold = true;
                    b_italic = true;
                    break;
                default:
                    msg_Warn( p_filter, "Android_ParseFamily: too many files" );
                    return VLC_EGENERIC;
                }

                char *psz_fontfile = NULL;
                if( asprintf( &psz_fontfile, "%s/%s", ANDROID_FONT_PATH, p_node ) < 0
                 || !NewFont( psz_fontfile, 0, b_bold, b_italic, p_family ) )
                    return VLC_ENOMEM;

                ++i_counter;
            }
            break;

        case XML_READER_ENDELEM:
            if( !strcasecmp( "family", p_node ) )
            {
                if( !p_family )
                {
                    msg_Warn( p_filter, "Android_ParseFamily: empty family" );
                    return VLC_EGENERIC;
                }

                if( strcasestr( p_family->psz_name, FB_NAME ) )
                {
                    vlc_family_t *p_fallback =
                        NewFamily( p_filter, p_family->psz_name,
                                   NULL, &p_sys->fallback_map, FB_LIST_DEFAULT );

                    if( unlikely( !p_fallback ) )
                        return VLC_ENOMEM;

                    p_fallback->p_fonts = p_family->p_fonts;
                }

                return VLC_SUCCESS;
            }
            break;
        }
    }

    msg_Warn( p_filter, "Android_ParseFamily: Corrupt font configuration file" );
    return VLC_EGENERIC;
}

int Android_ParseSystemFonts( filter_t *p_filter, const char *psz_path )
{
    int i_ret = VLC_SUCCESS;
    stream_t *p_stream = stream_UrlNew( p_filter, psz_path );

    if( !p_stream )
        return VLC_EGENERIC;

    xml_reader_t *p_xml = xml_ReaderCreate( p_filter, p_stream );

    if( !p_xml )
    {
        stream_Delete( p_stream );
        return VLC_EGENERIC;
    }

    const char *p_node;
    int i_type;
    while( ( i_type = xml_ReaderNextNode( p_xml, &p_node ) ) > 0 )
    {
        if( i_type == XML_READER_STARTELEM && !strcasecmp( "family", p_node ) )
        {
            if( ( i_ret = Android_ParseFamily( p_filter, p_xml ) ) )
                break;
        }
    }

    xml_ReaderDelete( p_xml );
    stream_Delete( p_stream );
    return i_ret;
}

const vlc_family_t *Android_GetFamily( filter_t *p_filter, const char *psz_family )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    char *psz_lc = ToLower( psz_family );
    if( unlikely( !psz_lc ) )
        return NULL;

    vlc_family_t *p_family =
            vlc_dictionary_value_for_key( &p_sys->family_map, psz_lc );

    free( psz_lc );

    if( p_family == kVLCDictionaryNotFound )
        return NULL;

    return p_family;
}

vlc_family_t *Android_GetFallbacks( filter_t *p_filter, const char *psz_family,
                                    uni_char_t codepoint )
{
    VLC_UNUSED( codepoint );

    vlc_family_t *p_fallbacks = NULL;
    filter_sys_t *p_sys = p_filter->p_sys;
    char *psz_lc = ToLower( psz_family );
    if( unlikely( !psz_lc ) )
        return NULL;

    p_fallbacks = vlc_dictionary_value_for_key( &p_sys->fallback_map, psz_lc );

    free( psz_lc );

    if( p_fallbacks == kVLCDictionaryNotFound )
        return NULL;

    return p_fallbacks;
}
#endif
