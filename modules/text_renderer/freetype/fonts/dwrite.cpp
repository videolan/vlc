/*****************************************************************************
 * dwrite.cpp : DirectWrite font functions
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Salah-Eddin Shaban <salah@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_charset.h>

#include <dwrite_2.h>
#include <wrl/client.h>
#include <vector>
#include <stdexcept>
#include <regex>

#include "../platform_fonts.h"
#include "backends.h"

using Microsoft::WRL::ComPtr;
using namespace std;

typedef HRESULT ( WINAPI *DWriteCreateFactoryProc ) (
        _In_ DWRITE_FACTORY_TYPE factory_type,
        _In_ REFIID              iid,
        _Out_ IUnknown         **pp_factory );

struct dw_sys_t
{
    HMODULE                                  p_dw_dll;
    ComPtr< IDWriteFactory2 >                p_dw_factory;
    ComPtr< IDWriteFontCollection >          p_dw_system_fonts;
    ComPtr< IDWriteNumberSubstitution >      p_dw_substitution;
    ComPtr< IDWriteFontFallback >            p_dw_fallbacks;
    vector< FT_Stream >                      streams;

    dw_sys_t( HMODULE p_dw_dll ) : p_dw_dll( p_dw_dll )
    {
        /* This will fail on versions of Windows prior to 8.1 */
#if VLC_WINSTORE_APP
        if( DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof( IDWriteFactory2 ),
                reinterpret_cast<IUnknown **>( p_dw_factory.GetAddressOf() ) ) )
            throw runtime_error( "failed to create DWrite factory" );
#else
        DWriteCreateFactoryProc pf =
                ( DWriteCreateFactoryProc ) GetProcAddress( p_dw_dll, "DWriteCreateFactory" );

        if( pf == NULL )
            throw runtime_error( "GetProcAddress() failed" );

        if( pf( DWRITE_FACTORY_TYPE_SHARED, __uuidof( IDWriteFactory2 ),
                reinterpret_cast<IUnknown **>( p_dw_factory.GetAddressOf() ) ) )
            throw runtime_error( "failed to create DWrite factory" );
#endif

        if( p_dw_factory->GetSystemFontCollection( p_dw_system_fonts.GetAddressOf() ) )
            throw runtime_error( "GetSystemFontCollection() failed" );

        if( p_dw_factory->GetSystemFontFallback( p_dw_fallbacks.GetAddressOf() ) )
            throw runtime_error( "GetSystemFontFallback() failed" );

        if( p_dw_factory->CreateNumberSubstitution( DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL,
                L"en-US", true, p_dw_substitution.GetAddressOf() ) )
            throw runtime_error( "CreateNumberSubstitution() failed" );
    }

    ~dw_sys_t()
    {
        for( unsigned int i = 0; i < streams.size(); ++i )
        {
            FT_Stream p_stream = streams.at( i );
            IDWriteFontFileStream *p_dw_stream = ( IDWriteFontFileStream * ) p_stream->descriptor.pointer;
            p_dw_stream->Release();
            free( p_stream );
        }
    }
};

static inline void AppendFamily( vlc_family_t **pp_list, vlc_family_t *p_family )
{
    while( *pp_list )
        pp_list = &( *pp_list )->p_next;

    *pp_list = p_family;
}

extern "C" int InitDWrite( vlc_font_select_t *fs )
{
    dw_sys_t *p_dw_sys;
    HMODULE p_dw_dll = NULL;

    try
    {
#if VLC_WINSTORE_APP
        p_dw_sys = new dw_sys_t( p_dw_dll );
#else
        p_dw_dll = LoadLibrary( TEXT( "Dwrite.dll" ) );
        if( p_dw_dll == NULL )
            return VLC_EGENERIC;

        p_dw_sys = new dw_sys_t( p_dw_dll );
#endif
    }
    catch( const exception &e )
    {
#if !VLC_WINSTORE_APP
        FreeLibrary( p_dw_dll );
        (void)e;
#else
        msg_Err( fs->p_obj, "InitDWrite(): %s", e.what() );
#endif

        return VLC_EGENERIC;
    }

    fs->p_dw_sys = p_dw_sys;
    msg_Dbg( fs->p_obj, "Using DWrite backend" );
    return VLC_SUCCESS;
}

extern "C" int ReleaseDWrite( vlc_font_select_t *fs )
{
    dw_sys_t *p_dw_sys = ( dw_sys_t * ) fs->p_dw_sys;

#if VLC_WINSTORE_APP
    delete p_dw_sys;
#else
    HMODULE p_dw_dll = NULL;
    if( p_dw_sys && p_dw_sys->p_dw_dll )
        p_dw_dll = p_dw_sys->p_dw_dll;

    delete p_dw_sys;
    if( p_dw_dll ) FreeLibrary( p_dw_dll );
#endif

    return VLC_SUCCESS;
}

extern "C" int DWrite_GetFontStream( vlc_font_select_t *fs, int i_index, FT_Stream *pp_stream )
{
    dw_sys_t *p_dw_sys = ( dw_sys_t * ) fs->p_dw_sys;

    if( i_index < 0 || i_index >= ( int ) p_dw_sys->streams.size() )
        return VLC_ENOITEM;

    *pp_stream = p_dw_sys->streams.at( i_index );

    return VLC_SUCCESS;
}

static inline void ToUTF16( uint32_t i_codepoint, wchar_t *p_out, uint32_t *i_length )
{
    if( i_codepoint < 0x10000 )
    {
        p_out[ 0 ] = i_codepoint;
        *i_length = 1;
    }
    else
    {
        i_codepoint -= 0x10000;
        p_out[ 0 ] = ( i_codepoint >> 10 ) + 0xd800;
        p_out[ 1 ] = ( i_codepoint & 0x3ff ) + 0xdc00;
        *i_length = 2;
    }
}

extern "C" unsigned long DWrite_Read( FT_Stream p_stream, unsigned long l_offset,
                                      unsigned char *p_buffer, unsigned long l_count )
{
    const void  *p_dw_fragment         = NULL;
    void        *p_dw_fragment_context = NULL;

    if( l_count == 0 ) return 0;

    IDWriteFontFileStream *p_dw_stream = ( IDWriteFontFileStream * ) p_stream->descriptor.pointer;

    if( p_dw_stream->ReadFileFragment( &p_dw_fragment, l_offset, l_count, &p_dw_fragment_context ) )
        return 0;

    memcpy( p_buffer, p_dw_fragment, l_count );

    p_dw_stream->ReleaseFileFragment( p_dw_fragment_context );

    return l_count;
}

extern "C" void DWrite_Close( FT_Stream )
{
}

class TextSource : public IDWriteTextAnalysisSource
{
    IDWriteNumberSubstitution   *mp_substitution;
    wchar_t                      mpwsz_text[ 3 ];
    uint32_t                     mi_text_length;
    ULONG                        ml_ref_count;

public:
    TextSource( IDWriteNumberSubstitution *p_substitution,
                const wchar_t *pwsz_text, uint32_t i_text_length )
              : mp_substitution( p_substitution ), ml_ref_count( 0 )
    {
        memset( mpwsz_text, 0, sizeof( mpwsz_text ) );
        mi_text_length = i_text_length < 2 ? i_text_length : 2;
        memcpy( mpwsz_text, pwsz_text, mi_text_length * sizeof( *mpwsz_text ) );
    }
    virtual ~TextSource() {}

    virtual HRESULT STDMETHODCALLTYPE GetLocaleName( UINT32, UINT32 *,
                                             const WCHAR **ppwsz_locale_name )
    {
        *ppwsz_locale_name = L"en-US";
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE GetNumberSubstitution( UINT32, UINT32 *,
                                                     IDWriteNumberSubstitution **pp_substitution )
    {
        mp_substitution->AddRef();
        *pp_substitution = mp_substitution;
        return S_OK;
    }

    virtual DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection()
    {
        return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    }

    virtual HRESULT STDMETHODCALLTYPE GetTextAtPosition( UINT32 i_text_position, const WCHAR **ppwsz_text,
                                                 UINT32 *pi_text_length )
    {
        if( i_text_position > mi_text_length )
            return E_INVALIDARG;

        *ppwsz_text = mpwsz_text + i_text_position;
        *pi_text_length = mi_text_length - i_text_position;

        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE GetTextBeforePosition( UINT32 i_text_position, const WCHAR **ppwsz_text,
                                                     UINT32 *pi_text_length )
    {
        if( i_text_position > mi_text_length )
            return E_INVALIDARG;

        if( i_text_position == 0 )
        {
            *ppwsz_text = NULL;
            *pi_text_length = 0;
            return S_OK;
        }

        *ppwsz_text = mpwsz_text;
        *pi_text_length = i_text_position;
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, LPVOID *pp_obj )
    {
        if( !pp_obj )
            return E_INVALIDARG;

        *pp_obj = NULL;

        if( riid == IID_IUnknown || riid == __uuidof( IDWriteTextAnalysisSource ) )
        {
            *pp_obj = ( LPVOID ) this;
            AddRef();
            return NOERROR;
        }
        return E_NOINTERFACE;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement( &ml_ref_count );
    }

    virtual ULONG STDMETHODCALLTYPE Release()
    {
        ULONG l_ret = InterlockedDecrement( &ml_ref_count );
        if( l_ret == 0 )
        {
            delete this;
        }

        return l_ret;
    }
};

/*
 * Remove any extra null characters and escape any regex metacharacters
 */
static wstring SanitizeName( const wstring &name )
{
    const auto pattern = wregex{ L"[.^$|()\\[\\]{}*+?\\\\]" };
    const auto replace = wstring{ L"\\\\&" };
    auto result = regex_replace( wstring{ name.c_str() }, pattern, replace,
                                 regex_constants::match_default | regex_constants::format_sed );
    return result;
}

/*
 * Check for a partial match e.g. between Roboto and Roboto Thin.
 * In this case p_unmatched will be set to Thin.
 * Also used for face names, in which case Thin will match Thin,
 * Thin Italic, Thin Oblique...
 */
static bool DWrite_PartialMatch( vlc_font_select_t *fs, const wstring &full_name,
                                 const wstring &partial_name, wstring *p_unmatched = nullptr )
{
    auto pattern = wstring{ L"^" } + SanitizeName( partial_name ) + wstring{ L"\\s*(.*)$" };
    auto rx = wregex{ pattern, wregex::icase };

    auto match = wsmatch{};

    if( !regex_match( full_name, match, rx ) )
        return false;

    msg_Dbg( fs->p_obj, "DWrite_PartialMatch(): %S matches %S", full_name.c_str(), partial_name.c_str() );

    if( p_unmatched )
        *p_unmatched = match[ 1 ].str();

    return true;
}

/*
 * Check for a partial match between a name and any of 3 localized names of a family or face.
 * The 3 locales tested are en-US and the user and system default locales. b_partial determines
 * which parameter has the partial string, p_names or name.
 */
static bool DWrite_PartialMatch( vlc_font_select_t *fs, ComPtr< IDWriteLocalizedStrings > &p_names,
                                 const wstring &name, bool b_partial, wstring *p_unmatched = nullptr )
{
    wchar_t buff_sys[ LOCALE_NAME_MAX_LENGTH ] = {};
    wchar_t buff_usr[ LOCALE_NAME_MAX_LENGTH ] = {};

    GetSystemDefaultLocaleName( buff_sys, LOCALE_NAME_MAX_LENGTH );
    GetUserDefaultLocaleName( buff_usr, LOCALE_NAME_MAX_LENGTH );

    const wchar_t *pp_locales[] = { L"en-US", buff_sys, buff_usr };

    for( int i = 0; i < 3; ++i )
    {
        HRESULT  hr;
        UINT32   i_index;
        UINT32   i_length;
        BOOL     b_exists;
        wstring  locale_name;

        try
        {
            hr = p_names->FindLocaleName( pp_locales[ i ], &i_index, &b_exists );

            if( SUCCEEDED( hr ) && b_exists )
            {
                hr = p_names->GetStringLength( i_index, &i_length );

                if( SUCCEEDED( hr ) )
                {
                    locale_name.resize( i_length + 1 );
                    hr = p_names->GetString( i_index, &locale_name[ 0 ], ( UINT32 ) locale_name.size() );

                    if( SUCCEEDED( hr ) )
                    {
                        bool b_result;
                        if( b_partial )
                            b_result = DWrite_PartialMatch( fs, locale_name, name, p_unmatched );
                        else
                            b_result = DWrite_PartialMatch( fs, name, locale_name, p_unmatched );

                        if( b_result )
                            return true;
                    }
                }
            }
        }
        catch( ... )
        {
        }
    }

    return false;
}

static vector< ComPtr< IDWriteFont > > DWrite_GetFonts( vlc_font_select_t *fs, IDWriteFontFamily *p_dw_family,
                                                        const wstring &face_name )
{
    vector< ComPtr< IDWriteFont > > result;

    if( !face_name.empty() )
    {
        UINT32 i_count = p_dw_family->GetFontCount();
        for( UINT32 i = 0; i < i_count; ++i )
        {
            ComPtr< IDWriteFont > p_dw_font;
            ComPtr< IDWriteLocalizedStrings > p_dw_names;

            if( FAILED( p_dw_family->GetFont( i, p_dw_font.GetAddressOf() ) ) )
                throw runtime_error( "GetFont() failed" );

            if( FAILED( p_dw_font->GetFaceNames( p_dw_names.GetAddressOf() ) ) )
                throw runtime_error( "GetFaceNames() failed" );

            if( DWrite_PartialMatch( fs, p_dw_names, face_name, true ) )
                result.push_back( p_dw_font );
        }
    }
    else
    {
        for( int i = 0; i < 4; ++i )
        {
            ComPtr< IDWriteFont > p_dw_font;
            DWRITE_FONT_STYLE style;
            DWRITE_FONT_WEIGHT weight;

            switch( i )
            {
            case 0:
                weight = DWRITE_FONT_WEIGHT_NORMAL; style = DWRITE_FONT_STYLE_NORMAL;
                break;
            case 1:
                weight = DWRITE_FONT_WEIGHT_BOLD; style = DWRITE_FONT_STYLE_NORMAL;
                break;
            case 2:
                weight = DWRITE_FONT_WEIGHT_NORMAL; style = DWRITE_FONT_STYLE_ITALIC;
                break;
            default:
                weight = DWRITE_FONT_WEIGHT_BOLD; style = DWRITE_FONT_STYLE_ITALIC;
                break;
            }

            if( FAILED( p_dw_family->GetFirstMatchingFont( weight, DWRITE_FONT_STRETCH_NORMAL, style, p_dw_font.GetAddressOf() ) ) )
                throw runtime_error( "GetFirstMatchingFont() failed" );

            result.push_back( p_dw_font );
        }
    }

    return result;
}

static void DWrite_ParseFamily( vlc_font_select_t *fs, IDWriteFontFamily *p_dw_family, const wstring &face_name,
                                vlc_family_t *p_family, vector< FT_Stream > &streams )
{
    vector< ComPtr< IDWriteFont > > fonts = DWrite_GetFonts( fs, p_dw_family, face_name );

    /*
     * We select at most 4 fonts to add to p_family, one for each style. So in case of
     * e.g multiple fonts with weights >= 700 we select the one closest to 700 as the
     * bold font.
     */
    IDWriteFont *p_filtered[ 4 ] = {};

    for( size_t i = 0; i < fonts.size(); ++i )
    {
        ComPtr< IDWriteFont > p_dw_font = fonts[ i ];

        /* Skip oblique. It's handled by FreeType */
        if( p_dw_font->GetStyle() == DWRITE_FONT_STYLE_OBLIQUE )
            continue;

        bool b_bold = p_dw_font->GetWeight() >= 700;
        bool b_italic = p_dw_font->GetStyle() == DWRITE_FONT_STYLE_ITALIC;

        size_t i_index = 0 | ( b_bold ? 1 : 0 ) | ( b_italic ? 2 : 0 );
        IDWriteFont **pp_font = &p_filtered[ i_index ];

        if( *pp_font )
        {
            int i_weight = b_bold ? 700 : 400;
            if( abs( ( *pp_font )->GetWeight() - i_weight ) > abs( p_dw_font->GetWeight() - i_weight ) )
            {
                msg_Dbg( fs->p_obj, "DWrite_ParseFamily(): using font at index %u with weight %u for bold: %d, italic: %d",
                         ( unsigned ) i, p_dw_font->GetWeight(), b_bold, b_italic );
                *pp_font = p_dw_font.Get();
            }
        }
        else
        {
            msg_Dbg( fs->p_obj, "DWrite_ParseFamily(): using font at index %u with weight %u for bold: %d, italic: %d",
                     ( unsigned ) i, p_dw_font->GetWeight(), b_bold, b_italic );
            *pp_font = p_dw_font.Get();
        }
    }

    for( size_t i = 0; i < 4; ++i )
    {
        IDWriteFont                      *p_dw_font;
        ComPtr< IDWriteFontFace >         p_dw_face;
        ComPtr< IDWriteFontFileLoader >   p_dw_loader;
        ComPtr< IDWriteFontFile >         p_dw_file;

        p_dw_font = p_filtered[ i ];
        if( !p_dw_font )
            continue;

        bool b_bold = i & 1;
        bool b_italic = i & 2;

        if( p_dw_font->CreateFontFace( p_dw_face.GetAddressOf() ) )
            throw runtime_error( "CreateFontFace() failed" );

        UINT32 i_num_files = 0;
        if( p_dw_face->GetFiles( &i_num_files, NULL ) || i_num_files == 0 )
            throw runtime_error( "GetFiles() failed" );

        i_num_files = 1;
        if( p_dw_face->GetFiles( &i_num_files, p_dw_file.GetAddressOf() ) )
            throw runtime_error( "GetFiles() failed" );

        UINT32 i_font_index = p_dw_face->GetIndex();

        if( p_dw_file->GetLoader( p_dw_loader.GetAddressOf() ) )
            throw runtime_error( "GetLoader() failed" );

        const void  *key;
        UINT32       keySize;
        if( p_dw_file->GetReferenceKey( &key, &keySize ) )
            throw runtime_error( "GetReferenceKey() failed" );

        ComPtr< IDWriteFontFileStream > p_dw_stream;
        if( p_dw_loader->CreateStreamFromKey( key, keySize, p_dw_stream.GetAddressOf() ) )
            throw runtime_error( "CreateStreamFromKey() failed" );

        UINT64 i_stream_size;
        if( p_dw_stream->GetFileSize( &i_stream_size ) )
            throw runtime_error( "GetFileSize() failed" );

        FT_Stream p_stream = ( FT_Stream ) calloc( 1, sizeof( *p_stream ) );
        if( p_stream == NULL ) throw bad_alloc();

        p_stream->descriptor.pointer = p_dw_stream.Get();
        p_stream->read = DWrite_Read;
        p_stream->close = DWrite_Close;
        p_stream->size = i_stream_size;

        try { streams.push_back( p_stream ); }
        catch( ... ) { free( p_stream ); throw; }
        p_dw_stream.Detach();

        char *psz_font_path = NULL;
        if( asprintf( &psz_font_path, ":dw/%d", ( int ) streams.size() - 1 ) < 0 )
            throw bad_alloc();

        NewFont( psz_font_path, i_font_index, b_bold, b_italic, p_family );
    }
}

extern "C" const vlc_family_t *DWrite_GetFamily( vlc_font_select_t *fs, const char *psz_family )
{
    dw_sys_t                     *p_dw_sys     = ( dw_sys_t * ) fs->p_dw_sys;
    ComPtr< IDWriteFontFamily >   p_dw_family;

    UINT32 i_index;
    BOOL b_exists = false;

    char *psz_lc = ToLower( psz_family );
    if( unlikely( !psz_lc ) )
        return NULL;

    vlc_family_t *p_family =
        ( vlc_family_t * ) vlc_dictionary_value_for_key( &fs->family_map, psz_lc );

    free( psz_lc );

    if( p_family )
        return p_family;

    p_family = NewFamily( fs, psz_family, &fs->p_families,
                          &fs->family_map, psz_family );

    if( unlikely( !p_family ) )
        return NULL;

    msg_Dbg( fs->p_obj, "DWrite_GetFamily(): family name: %s", psz_family );

    wchar_t *pwsz_family = ToWide( psz_family );
    if( unlikely( !pwsz_family ) )
        goto done;

    /* Try to find an exact match first */
    if( SUCCEEDED( p_dw_sys->p_dw_system_fonts->FindFamilyName( pwsz_family, &i_index, &b_exists ) ) && b_exists )
    {
        if( FAILED( p_dw_sys->p_dw_system_fonts->GetFontFamily( i_index, p_dw_family.GetAddressOf() ) ) )
        {
            msg_Err( fs->p_obj, "DWrite_GetFamily: GetFontFamily() failed" );
            goto done;
        }

        try
        {
            DWrite_ParseFamily( fs, p_dw_family.Get(), wstring{}, p_family, p_dw_sys->streams );
        }
        catch( const exception &e )
        {
            msg_Err( fs->p_obj, "DWrite_GetFamily(): %s", e.what() );
            goto done;
        }
    }

    /*
     * DirectWrite does not recognize Roboto Thin and similar as family names.
     * When enumerating names via DirectWrite we get only Roboto. Thin, Black etc
     * are face names within the Roboto family.
     * So we try partial name matching if an exact match cannot be found. In this
     * case Roboto Thin will match the Roboto family, and the unmatched part of
     * the name (i.e Thin) will be used to find faces within that family (Thin,
     * Thin Italic, Thin Oblique...)
     */
    if( !p_dw_family )
    {
        wstring face_name;
        ComPtr< IDWriteLocalizedStrings > p_names;

        UINT32 i_count = p_dw_sys->p_dw_system_fonts->GetFontFamilyCount();

        for( i_index = 0; i_index < i_count; ++i_index )
        {
            ComPtr< IDWriteFontFamily > p_cur_family;
            if( FAILED( p_dw_sys->p_dw_system_fonts->GetFontFamily( i_index, p_cur_family.GetAddressOf() ) ) )
            {
                msg_Err( fs->p_obj, "DWrite_GetFamily: GetFontFamily() failed" );
                continue;
            }

            if( FAILED( p_cur_family->GetFamilyNames( p_names.GetAddressOf() ) ) )
            {
                msg_Err( fs->p_obj, "DWrite_GetFamily: GetFamilyNames() failed" );
                continue;
            }

            if( !DWrite_PartialMatch( fs, p_names, wstring{ pwsz_family }, false, &face_name ) )
                continue;

            try
            {
                DWrite_ParseFamily( fs, p_cur_family.Get(), face_name, p_family, p_dw_sys->streams );
            }
            catch( const exception &e )
            {
                msg_Err( fs->p_obj, "DWrite_GetFamily(): %s", e.what() );
            }

            /*
             * If the requested family is e.g. Microsoft JhengHei UI Light, DWrite_PartialMatch will return
             * true for both Microsoft JhengHei and Microsoft JhengHei UI. When the former is used with
             * DWrite_ParseFamily no faces will be matched (because face_name will be UI Light) and
             * p_family->p_fonts will be NULL. With the latter face_name will be Light and the correct face
             * will be added to p_family, therefore breaking this loop.
             */
            if( p_family->p_fonts )
                break;
        }
    }

done:
    free( pwsz_family );
    return p_family;
}

static char *DWrite_Fallback( vlc_font_select_t *fs, const char *psz_family,
                              uni_char_t codepoint )
{
    dw_sys_t                         *p_dw_sys          = ( dw_sys_t * ) fs->p_dw_sys;
    wchar_t                          *pwsz_buffer       = NULL;
    char                             *psz_result        = NULL;
    UINT32                            i_string_length   = 0;
    UINT32                            i_mapped_length   = 0;
    float                             f_scale;
    ComPtr< IDWriteFont >             p_dw_font;
    ComPtr< IDWriteFontFamily >       p_dw_family;
    ComPtr< IDWriteLocalizedStrings > p_names;

    msg_Dbg( fs->p_obj, "DWrite_Fallback(): family: %s, codepoint: 0x%x", psz_family, codepoint );

    wchar_t p_text[2];
    UINT32  i_text_length;
    ToUTF16( codepoint, p_text, &i_text_length );

    wchar_t *pwsz_family = ToWide( psz_family );
    if( unlikely( !pwsz_family ) ) return NULL;

    ComPtr< TextSource > p_ts;
    p_ts = new(std::nothrow) TextSource( p_dw_sys->p_dw_substitution.Get(), p_text, i_text_length );
    if( unlikely( p_ts == NULL ) ) { goto done; }

    if( p_dw_sys->p_dw_fallbacks->MapCharacters( p_ts.Get(), 0, i_text_length, p_dw_sys->p_dw_system_fonts.Get(), pwsz_family,
                                  DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                  &i_mapped_length, p_dw_font.GetAddressOf(), &f_scale )
     || !p_dw_font )
    {
        msg_Warn( fs->p_obj, "DWrite_Fallback(): MapCharacters() failed" );
        goto done;
    }

    if( p_dw_font->GetFontFamily( p_dw_family.GetAddressOf() ) )
    {
        msg_Err( fs->p_obj, "DWrite_Fallback(): GetFontFamily() failed" );
        goto done;
    }

    if( p_dw_family->GetFamilyNames( p_names.GetAddressOf() ) )
    {
        msg_Err( fs->p_obj, "DWrite_Fallback(): GetFamilyNames() failed" );
        goto done;
    }

    if( p_names->GetStringLength( 0, &i_string_length ) )
    {
        msg_Err( fs->p_obj, "DWrite_Fallback(): GetStringLength() failed" );
        goto done;
    }

    pwsz_buffer = ( wchar_t * ) vlc_alloc( ( i_string_length + 1 ), sizeof( *pwsz_buffer ) );
    if( unlikely( !pwsz_buffer ) )
        goto done;

    if( p_names->GetString( 0, pwsz_buffer, i_string_length + 1 ) )
    {
        msg_Err( fs->p_obj, "DWrite_Fallback(): GetString() failed" );
        goto done;
    }

    psz_result = FromWide( pwsz_buffer );
    msg_Dbg( fs->p_obj, "DWrite_Fallback(): returning %s", psz_result );

done:
    free( pwsz_buffer );
    free( pwsz_family );
    return psz_result;
}

extern "C" vlc_family_t *DWrite_GetFallbacks( vlc_font_select_t *fs, const char *psz_family,
                                              uni_char_t codepoint )
{
    vlc_family_t  *p_family      = NULL;
    vlc_family_t  *p_fallbacks   = NULL;
    char          *psz_fallback  = NULL;


    char *psz_lc = ToLower( psz_family );

    if( unlikely( !psz_lc ) )
        return NULL;

    p_fallbacks = ( vlc_family_t * ) vlc_dictionary_value_for_key( &fs->fallback_map, psz_lc );

    if( p_fallbacks )
        p_family = SearchFallbacks( fs, p_fallbacks, codepoint );

    /*
     * If the fallback list of psz_family has no family which contains the requested
     * codepoint, try DWrite_Fallback(). If it returns a valid family which does
     * contain that codepoint, add the new family to the fallback list to speed up
     * later searches.
     */
    if( !p_family )
    {
        psz_fallback = DWrite_Fallback( fs, psz_lc, codepoint );

        if( !psz_fallback )
            goto done;

        const vlc_family_t *p_fallback = DWrite_GetFamily( fs, psz_fallback );
        if( !p_fallback || !p_fallback->p_fonts )
            goto done;

        if( !GetFace( fs, p_fallback->p_fonts, codepoint ) )
            goto done;

        p_family = NewFamily( fs, psz_fallback, NULL, NULL, NULL );

        if( unlikely( !p_family ) )
            goto done;

        p_family->p_fonts = p_fallback->p_fonts;

        if( p_fallbacks )
            AppendFamily( &p_fallbacks, p_family );
        else
            vlc_dictionary_insert( &fs->fallback_map,
                                   psz_lc, p_family );
    }

done:
    free( psz_lc );
    free( psz_fallback );
    return p_family;
}
