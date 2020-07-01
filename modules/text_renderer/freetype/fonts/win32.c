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

/** \ingroup freetype_fonts
 * @{
 * \file
 * Win32 font management
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>

#include <ft2build.h>
#include FT_SFNT_NAMES_H

/* Win32 GDI */
#ifdef _WIN32
# include <windows.h>
# include <shlobj.h>
# include <usp10.h>
# include <vlc_charset.h>
# undef HAVE_FONTCONFIG
#endif

#include "../platform_fonts.h"
#include "backends.h"

#if !VLC_WINSTORE_APP
#define FONT_DIR_NT  TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts")

static inline void AppendFamily( vlc_family_t **pp_list, vlc_family_t *p_family )
{
    while( *pp_list )
        pp_list = &( *pp_list )->p_next;

    *pp_list = p_family;
}


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

static int GetFileFontByName( const WCHAR * font_name, char **psz_filename, int *i_index )
{
    HKEY hKey;
    WCHAR vbuffer[MAX_PATH];
    WCHAR dbuffer[256];

    if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, FONT_DIR_NT, 0, KEY_READ, &hKey)
            != ERROR_SUCCESS )
        return 1;

    char *font_name_temp = FromWide( font_name );

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

        char *psz_value = FromWide( vbuffer );

        char *s = strchr( psz_value,'(' );
        if( s != NULL && s != psz_value ) s[-1] = '\0';

        int i_concat_idx = 0;
        if( ( i_concat_idx = ConcatenatedIndex( psz_value, font_name_temp ) ) != -1 )
        {
            *i_index = i_concat_idx;
            *psz_filename = FromWide( dbuffer );
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
        wcscat( wdir, TEXT("\\fonts") );
    }
    return FromWide( wdir );
}

/**
 * Get an SFNT name entry from an SFNT name table.
 *
 * \param p_name_data the start position of the name entry within the table [IN]
 * \param p_storage_start the start position of the string storage area in the table [IN]
 * \param p_table_end the position after the last byte of the table [IN]
 * \param p_sfnt_name pointer to a \ref FT_SfntName to fill [OUT]
 *
 * \return \ref VLC_SUCCESS or \ref VLC_EGENERIC
 */
static int GetSfntNameEntry( FT_Byte *p_name_data, FT_Byte *p_storage_start,
                             FT_Byte *p_table_end, FT_SfntName *p_sfnt_name )
{
    uint16_t i_string_len      = U16_AT( p_name_data + 8 );
    uint16_t i_string_offset   = U16_AT( p_name_data + 10 );
    if( i_string_len == 0
     || p_storage_start + i_string_offset + i_string_len > p_table_end )
        return VLC_EGENERIC;

    p_sfnt_name->platform_id = U16_AT( p_name_data + 0 );
    p_sfnt_name->encoding_id = U16_AT( p_name_data + 2 );
    p_sfnt_name->language_id = U16_AT( p_name_data + 4 );
    p_sfnt_name->name_id     = U16_AT( p_name_data + 6 );
    p_sfnt_name->string_len  = i_string_len;
    p_sfnt_name->string      = p_storage_start + i_string_offset;

    return VLC_SUCCESS;
}

/**
 * Get the SFNT name string matching the specified platform, encoding, name, and language IDs.
 *
 *\param p_table the SFNT table [IN]
 *\param i_size table size in bytes [IN]
 *\param i_platform_id platform ID as specified in the TrueType spec [IN]
 *\param i_encoding_id encoding ID as specified in the TrueType spec [IN]
 *\param i_name_id name ID as specified in the TrueType spec [IN]
 *\param i_language_id language ID as specified in the TrueType spec [IN]
 *\param pp_name the requested name.
 *       This is not null terminated. And can have different encodings
 *       based on the specified platform/encoding IDs [OUT]
 *\param i_name_length the length in bytes of the returned name [OUT]
 *
 *\return \ref VLC_SUCCESS or \ref VLC_EGENERIC
 */
static int GetSfntNameString( FT_Byte *p_table, FT_UInt i_size, FT_UShort i_platform_id,
                              FT_UShort i_encoding_id, FT_UShort i_name_id, FT_UShort i_language_id,
                              FT_Byte **pp_name, FT_UInt *i_name_length )
{
    uint16_t i_name_count     = U16_AT( p_table + 2 );
    uint16_t i_storage_offset = U16_AT( p_table + 4 );
    FT_Byte *p_storage        = p_table + i_storage_offset;
    FT_Byte *p_names          = p_table + 6;

    const int i_entry_size = 12;

    for(int i = 0; i < i_name_count; ++i)
    {
        FT_SfntName sfnt_name;

        if( GetSfntNameEntry( p_names + i * i_entry_size, p_storage, p_table + i_size, &sfnt_name ) )
            return VLC_EGENERIC;

        if( sfnt_name.platform_id == i_platform_id && sfnt_name.encoding_id == i_encoding_id
         && sfnt_name.name_id == i_name_id && sfnt_name.language_id == i_language_id )
        {
            *i_name_length = sfnt_name.string_len;
            *pp_name = sfnt_name.string;

            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}

/**
 * Get the font's full English name.
 *
 * If the font has a family name or style name that matches the
 * system's locale, EnumFontCallback() will be called with a \ref ENUMLOGFONTEX
 * that has the localized name.
 *
 * We have to get the English name because that's what the Windows registry uses
 * for name to file mapping.
 */
static WCHAR *GetFullEnglishName( const ENUMLOGFONTEX *lpelfe )
{

    HFONT    hFont      = NULL;
    HDC      hDc        = NULL;
    FT_Byte *p_table    = NULL;
    WCHAR   *psz_result = NULL;

    hFont = CreateFontIndirect( &lpelfe->elfLogFont );

    if( !hFont )
        return NULL;

    hDc = CreateCompatibleDC( NULL );

    if( !hDc )
    {
        DeleteObject( hFont );
        return NULL;
    }

    HFONT hOriginalFont = ( HFONT ) SelectObject( hDc, hFont );

    const uint32_t i_name_tag = ntoh32( ( uint32_t ) 'n' << 24
                                      | ( uint32_t ) 'a' << 16
                                      | ( uint32_t ) 'm' << 8
                                      | ( uint32_t ) 'e' << 0 );

    int i_size = GetFontData( hDc, i_name_tag, 0, 0, 0 );

    if( i_size <= 0 )
        goto done;

    p_table = malloc( i_size );

    if( !p_table )
        goto done;

    if( GetFontData( hDc, i_name_tag, 0, p_table, i_size ) <= 0 )
        goto done;

    FT_Byte *p_name = NULL;
    FT_UInt  i_name_length = 0;

    /* FIXME: Try other combinations of platform/encoding/language IDs if necessary */
    if( GetSfntNameString( p_table, i_size, 3, 1, 4, 0x409, &p_name, &i_name_length) )
        goto done;

    int i_length_in_wchars = i_name_length / 2;
    wchar_t *psz_name = vlc_alloc( i_length_in_wchars + 1, sizeof( *psz_name ) );

    if( !psz_name )
        goto done;

    for( int i = 0; i < i_length_in_wchars; ++i )
        psz_name[ i ] = U16_AT( p_name + i * 2 );
    psz_name[ i_length_in_wchars ] = 0;

    psz_result = psz_name;

done:
    free( p_table );
    SelectObject( hDc, hOriginalFont );
    DeleteObject( hFont );
    DeleteDC( hDc );

    return psz_result;
}

struct enumFontCallbackContext
{
    vlc_font_select_t *fs;
    vlc_family_t *p_family;
};

static int CALLBACK EnumFontCallback(const ENUMLOGFONTEX *lpelfe, const NEWTEXTMETRICEX *metric,
                                     DWORD type, LPARAM lParam)
{
    VLC_UNUSED( metric );
    if( (type & RASTER_FONTTYPE) ) return 1;

    struct enumFontCallbackContext *ctx = ( struct enumFontCallbackContext * ) lParam;
    vlc_family_t *p_family = ctx->p_family;

    bool b_bold = ( lpelfe->elfLogFont.lfWeight == FW_BOLD );
    bool b_italic = ( lpelfe->elfLogFont.lfItalic != 0 );

    /*
     * This function will be called by Windows as many times for each font
     * of the family as the number of scripts the font supports.
     * Check to avoid duplicates.
     */
    for( vlc_font_t *p_font = p_family->p_fonts; p_font; p_font = p_font->p_next )
        if( !!p_font->b_bold == !!b_bold && !!p_font->b_italic == !!b_italic )
            return 1;

    char *psz_filename = NULL;
    char *psz_fontfile = NULL;
    int   i_index      = 0;

    if( GetFileFontByName( lpelfe->elfFullName, &psz_filename, &i_index ) )
    {
        WCHAR *psz_english_name = GetFullEnglishName( lpelfe );

        if( !psz_english_name )
            return 1;

        if( GetFileFontByName( psz_english_name, &psz_filename, &i_index ) )
        {
            free( psz_english_name );
            return 1;
        }

        free( psz_english_name );
    }

    if( strchr( psz_filename, DIR_SEP_CHAR ) )
        psz_fontfile = psz_filename;
    else
    {
        psz_fontfile = MakeFilePath( ctx->fs, psz_filename );
        free( psz_filename );
        if( !psz_fontfile )
            return 1;
    }

    NewFont( psz_fontfile, i_index, b_bold, b_italic, p_family );

    return 1;
}

const vlc_family_t *Win32_GetFamily( vlc_font_select_t *fs, const char *psz_family )
{
    char *psz_lc = ToLower( psz_family );

    if( unlikely( !psz_lc ) )
        return NULL;

    vlc_family_t *p_family =
        vlc_dictionary_value_for_key( &fs->family_map, psz_lc );

    free( psz_lc );

    if( p_family )
        return p_family;

    p_family = NewFamily( fs, psz_family, &fs->p_families,
                          &fs->family_map, psz_family );

    if( unlikely( !p_family ) )
        return NULL;

    LOGFONT lf;
    lf.lfCharSet = DEFAULT_CHARSET;

    LPTSTR psz_fbuffer = ToWide( psz_family );
    wcsncpy( (LPTSTR)&lf.lfFaceName, psz_fbuffer, LF_FACESIZE );
    free( psz_fbuffer );

    /* */
    HDC hDC = GetDC( NULL );
    struct enumFontCallbackContext ctx;
    ctx.fs = fs;
    ctx.p_family = NULL;
    EnumFontFamiliesEx(hDC, &lf, (FONTENUMPROC)&EnumFontCallback, (LPARAM)&ctx, 0);
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

/**
 * This is a hack used by Chrome and WebKit to expose the fallback font used
 * by Uniscribe for some given text for use with custom shapers / font engines.
 */
static char *UniscribeFallback( const char *psz_family, uni_char_t codepoint )
{
    HDC          hdc          = NULL;
    HDC          meta_file_dc = NULL;
    HENHMETAFILE meta_file    = NULL;
    char        *psz_result   = NULL;

    hdc = CreateCompatibleDC( NULL );
    if( !hdc )
        return NULL;

    meta_file_dc = CreateEnhMetaFile( hdc, NULL, NULL, NULL );
    if( !meta_file_dc )
        goto error;

    LOGFONT lf;
    memset( &lf, 0, sizeof( lf ) );

    wchar_t *psz_fbuffer = ToWide( psz_family );
    if( !psz_fbuffer )
        goto error;
    wcsncpy( ( LPTSTR ) &lf.lfFaceName, psz_fbuffer, LF_FACESIZE );
    free( psz_fbuffer );

    lf.lfCharSet = DEFAULT_CHARSET;
    HFONT hFont = CreateFontIndirect( &lf );
    if( !hFont )
        goto error;

    HFONT hOriginalFont = SelectObject( meta_file_dc, hFont );

    WCHAR text = codepoint;

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
            psz_result = FromWide( log_font.lfFaceName );
    }

    DeleteEnhMetaFile(meta_file);
    DeleteDC( hdc );
    return psz_result;

error:
    if( meta_file_dc ) DeleteEnhMetaFile( CloseEnhMetaFile( meta_file_dc ) );
    if( hdc ) DeleteDC( hdc );
    return NULL;
}

vlc_family_t *Win32_GetFallbacks( vlc_font_select_t *fs, const char *psz_family,
                                  uni_char_t codepoint )
{
    vlc_family_t  *p_family      = NULL;
    vlc_family_t  *p_fallbacks   = NULL;
    char          *psz_uniscribe = NULL;


    char *psz_lc = ToLower( psz_family );

    if( unlikely( !psz_lc ) )
        return NULL;

    p_fallbacks = vlc_dictionary_value_for_key( &fs->fallback_map, psz_lc );

    if( p_fallbacks )
        p_family = SearchFallbacks( fs, p_fallbacks, codepoint );

    /*
     * If the fallback list of psz_family has no family which contains the requested
     * codepoint, try UniscribeFallback(). If it returns a valid family which does
     * contain that codepoint, add the new family to the fallback list to speed up
     * later searches.
     */
    if( !p_family )
    {
        psz_uniscribe = UniscribeFallback( psz_lc, codepoint );

        if( !psz_uniscribe )
            goto done;

        const vlc_family_t *p_uniscribe = Win32_GetFamily( fs, psz_uniscribe );
        if( !p_uniscribe || !p_uniscribe->p_fonts )
            goto done;

        if( !GetFace( fs, p_uniscribe->p_fonts, codepoint ) )
            goto done;

        p_family = NewFamily( fs, psz_uniscribe, NULL, NULL, NULL );

        if( unlikely( !p_family ) )
            goto done;

        p_family->p_fonts = p_uniscribe->p_fonts;

        if( p_fallbacks )
            AppendFamily( &p_fallbacks, p_family );
        else
            vlc_dictionary_insert( &fs->fallback_map,
                                   psz_lc, p_family );
    }

done:
    free( psz_lc );
    free( psz_uniscribe );
    return p_family;
}

char * MakeFilePath( vlc_font_select_t *fs, const char *psz_filename )
{
    VLC_UNUSED(fs);

    if( !psz_filename )
        return NULL;

    /* FIXME test FQN */

    /* Get Windows Font folder */
    char *psz_fonts_path = GetWindowsFontPath();

    char *psz_filepath;
    if( asprintf( &psz_filepath, "%s" DIR_SEP "%s",
                  psz_fonts_path ? psz_fonts_path : "",
                  psz_filename ) == -1 )
        psz_filepath = NULL;
    free( psz_fonts_path );

    return psz_filepath;
}

#endif

