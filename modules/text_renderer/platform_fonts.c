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
# include <vlc_charset.h>                                     /* FromT */
#endif

/* FontConfig */
#ifdef HAVE_FONTCONFIG
# include <fontconfig/fontconfig.h>
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

/***
 * \brief Selects a font matching family, bold, italic provided
 ***/
char* FontConfig_Select( filter_t *p_filter, const char* family,
                         bool b_bold, bool b_italic,
                         int *i_idx, uni_char_t codepoint )
{
    FcResult result = FcResultMatch;
    FcPattern *pat, *p_pat;
    FcChar8* val_s;
    FcBool val_b;
    char *ret = NULL;
    FcConfig* config = NULL;
    VLC_UNUSED(p_filter);
    VLC_UNUSED(codepoint);

    /* Create a pattern and fills it */
    pat = FcPatternCreate();
    if (!pat) return NULL;

    /* */
    FcPatternAddString( pat, FC_FAMILY, (const FcChar8*)family );
    FcPatternAddBool( pat, FC_OUTLINE, FcTrue );
    FcPatternAddInteger( pat, FC_SLANT, b_italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN );
    FcPatternAddInteger( pat, FC_WEIGHT, b_bold ? FC_WEIGHT_EXTRABOLD : FC_WEIGHT_NORMAL );

    /* */
    FcDefaultSubstitute( pat );
    if( !FcConfigSubstitute( config, pat, FcMatchPattern ) )
    {
        FcPatternDestroy( pat );
        return NULL;
    }

    /* Find the best font for the pattern, destroy the pattern */
    p_pat = FcFontMatch( config, pat, &result );
    FcPatternDestroy( pat );
    if( !p_pat || result == FcResultNoMatch ) return NULL;

    /* Check the new pattern */
    if( ( FcResultMatch != FcPatternGetBool( p_pat, FC_OUTLINE, 0, &val_b ) )
        || ( val_b != FcTrue ) )
    {
        FcPatternDestroy( p_pat );
        return NULL;
    }
    if( FcResultMatch != FcPatternGetInteger( p_pat, FC_INDEX, 0, i_idx ) )
    {
        *i_idx = 0;
    }

    if( FcResultMatch != FcPatternGetString( p_pat, FC_FAMILY, 0, &val_s ) )
    {
        FcPatternDestroy( p_pat );
        return NULL;
    }

    /* if( strcasecmp((const char*)val_s, family ) != 0 )
        msg_Warn( p_filter, "fontconfig: selected font family is not"
                            "the requested one: '%s' != '%s'\n",
                            (const char*)val_s, family );   */

    if( FcResultMatch == FcPatternGetString( p_pat, FC_FILE, 0, &val_s ) )
        ret = strdup( (const char*)val_s );

    FcPatternDestroy( p_pat );
    return ret;
}
#endif

#if defined( _WIN32 ) && !VLC_WINSTORE_APP
#define FONT_DIR_NT _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts")

static int GetFileFontByName( LPCTSTR font_name, char **psz_filename )
{
    HKEY hKey;
    TCHAR vbuffer[MAX_PATH];
    TCHAR dbuffer[256];

    if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, FONT_DIR_NT, 0, KEY_READ, &hKey)
            != ERROR_SUCCESS )
        return 1;

    char *font_name_temp = FromT( font_name );
    size_t fontname_len = strlen( font_name_temp );

    for( int index = 0;; index++ )
    {
        DWORD vbuflen = MAX_PATH - 1;
        DWORD dbuflen = 255;

        LONG i_result = RegEnumValue( hKey, index, vbuffer, &vbuflen,
                                      NULL, NULL, (LPBYTE)dbuffer, &dbuflen);
        if( i_result != ERROR_SUCCESS )
        {
            RegCloseKey( hKey );
            return i_result;
        }

        char *psz_value = FromT( vbuffer );

        char *s = strchr( psz_value,'(' );
        if( s != NULL && s != psz_value ) s[-1] = '\0';

        /* Manage concatenated font names */
        if( strchr( psz_value, '&') ) {
            if( strcasestr( psz_value, font_name_temp ) != NULL )
            {
                free( psz_value );
                break;
            }
        }
        else {
            if( strncasecmp( psz_value, font_name_temp, fontname_len ) == 0 )
            {
                free( psz_value );
                break;
            }
        }

        free( psz_value );
    }

    *psz_filename = FromT( dbuffer );
    free( font_name_temp );
    RegCloseKey( hKey );
    return 0;
}

static int CALLBACK EnumFontCallback(const ENUMLOGFONTEX *lpelfe, const NEWTEXTMETRICEX *metric,
                                     DWORD type, LPARAM lParam)
{
    VLC_UNUSED( metric );
    if( (type & RASTER_FONTTYPE) ) return 1;
    // if( lpelfe->elfScript ) FIXME

    return GetFileFontByName( (LPCTSTR)lpelfe->elfFullName, (char **)lParam );
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

char* Win32_Select( filter_t *p_filter, const char* family,
                    bool b_bold, bool b_italic,
                    int *i_idx, uni_char_t codepoint )
{
    VLC_UNUSED( codepoint );
    VLC_UNUSED( i_idx );
    VLC_UNUSED( p_filter );

    if( !family || strlen( family ) < 1 )
        goto fail;

    /* */
    LOGFONT lf;
    lf.lfCharSet = DEFAULT_CHARSET;
    if( b_italic )
        lf.lfItalic = true;
    if( b_bold )
        lf.lfWeight = FW_BOLD;

    LPTSTR psz_fbuffer = ToT( family );
    _tcsncpy( (LPTSTR)&lf.lfFaceName, psz_fbuffer, LF_FACESIZE );
    free( psz_fbuffer );

    /* */
    char *psz_filename = NULL;
    HDC hDC = GetDC( NULL );
    EnumFontFamiliesEx(hDC, &lf, (FONTENUMPROC)&EnumFontCallback, (LPARAM)&psz_filename, 0);
    ReleaseDC(NULL, hDC);

    /* */
    if( psz_filename != NULL )
    {
        /* FIXME: increase i_idx, when concatenated strings  */
        i_idx = 0;

        /* Prepend the Windows Font path, when only a filename was provided */
        if( strchr( psz_filename, DIR_SEP_CHAR ) )
            return psz_filename;
        else
        {
            /* Get Windows Font folder */
            char *psz_win_fonts_path = GetWindowsFontPath();
            char *psz_tmp;
            if( asprintf( &psz_tmp, "%s\\%s", psz_win_fonts_path, psz_filename ) == -1 )
            {
                free( psz_filename );
                free( psz_win_fonts_path );
                return NULL;
            }
            free( psz_filename );
                free( psz_win_fonts_path );

            return psz_tmp;
        }
    }
    else /* Let's take any font we can */
fail:
    {
        char *psz_win_fonts_path = GetWindowsFontPath();
        char *psz_tmp;
        if( asprintf( &psz_tmp, "%s\\%s", psz_win_fonts_path, SYSTEM_DEFAULT_FONT_FILE ) == -1 )
            return NULL;
        else
            return psz_tmp;
    }
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
