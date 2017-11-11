/*****************************************************************************
 * dwrite.cpp : DirectWrite font functions
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
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

#include "../platform_fonts.h"

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

extern "C" int InitDWrite( filter_t *p_filter )
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
        msg_Err( p_filter, "InitDWrite(): %s", e.what() );
#endif

        return VLC_EGENERIC;
    }

    p_filter->p_sys->p_dw_sys = p_dw_sys;
    msg_Dbg( p_filter, "Using DWrite backend" );
    return VLC_SUCCESS;
}

extern "C" int ReleaseDWrite( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    dw_sys_t *p_dw_sys = ( dw_sys_t * ) p_sys->p_dw_sys;

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

extern "C" int DWrite_GetFontStream( filter_t *p_filter, int i_index, FT_Stream *pp_stream )
{
    dw_sys_t *p_dw_sys = ( dw_sys_t * ) p_filter->p_sys->p_dw_sys;

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
    IDWriteFactory              *mp_factory;
    IDWriteNumberSubstitution   *mp_substitution;
    wchar_t                      mpwsz_text[ 3 ];
    uint32_t                     mi_text_length;
    ULONG                        ml_ref_count;

public:
    TextSource( IDWriteFactory *p_factory, IDWriteNumberSubstitution *p_substitution,
                const wchar_t *pwsz_text, uint32_t i_text_length )
              : mp_factory( p_factory ), mp_substitution( p_substitution ), ml_ref_count( 0 )
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

static void DWrite_ParseFamily( IDWriteFontFamily *p_dw_family, vlc_family_t *p_family, vector< FT_Stream > &streams )
{
    for( int i = 0; i < 4; ++i )
    {
        ComPtr< IDWriteFont >             p_dw_font;
        ComPtr< IDWriteFontFace >         p_dw_face;
        ComPtr< IDWriteFontFileLoader >   p_dw_loader;
        ComPtr< IDWriteFontFile >         p_dw_file;

        DWRITE_FONT_STYLE style;
        DWRITE_FONT_WEIGHT weight;
        bool b_bold, b_italic;

        switch( i )
        {
        case 0:
            weight = DWRITE_FONT_WEIGHT_NORMAL; style = DWRITE_FONT_STYLE_NORMAL;
            b_bold = false; b_italic = false; break;
        case 1:
            weight = DWRITE_FONT_WEIGHT_BOLD; style = DWRITE_FONT_STYLE_NORMAL;
            b_bold = true; b_italic = false; break;
        case 2:
            weight = DWRITE_FONT_WEIGHT_NORMAL; style = DWRITE_FONT_STYLE_ITALIC;
            b_bold = false; b_italic = true; break;
        case 3:
            weight = DWRITE_FONT_WEIGHT_BOLD; style = DWRITE_FONT_STYLE_ITALIC;
            b_bold = true; b_italic = true; break;
        }

        if( p_dw_family->GetFirstMatchingFont( weight, DWRITE_FONT_STRETCH_NORMAL, style, p_dw_font.GetAddressOf() )
         || !p_dw_font )
            throw runtime_error( "GetFirstMatchingFont() failed" );

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

extern "C" const vlc_family_t *DWrite_GetFamily( filter_t *p_filter, const char *psz_family )
{
    filter_sys_t                 *p_sys        = p_filter->p_sys;
    dw_sys_t                     *p_dw_sys     = ( dw_sys_t * ) p_sys->p_dw_sys;
    ComPtr< IDWriteFontFamily >   p_dw_family;

    char *psz_lc = ToLower( psz_family );
    if( unlikely( !psz_lc ) )
        return NULL;

    vlc_family_t *p_family =
        ( vlc_family_t * ) vlc_dictionary_value_for_key( &p_sys->family_map, psz_lc );

    free( psz_lc );

    if( p_family )
        return p_family;

    p_family = NewFamily( p_filter, psz_family, &p_sys->p_families,
                          &p_sys->family_map, psz_family );

    if( unlikely( !p_family ) )
        return NULL;

    msg_Dbg( p_filter, "DWrite_GetFamily(): family name: %s", psz_family );

    wchar_t *pwsz_family = ToWide( psz_family );
    if( unlikely( !pwsz_family ) )
        goto done;

    UINT32 i_index;
    BOOL b_exists;

    if( p_dw_sys->p_dw_system_fonts->FindFamilyName( pwsz_family, &i_index, &b_exists ) || !b_exists )
    {
        msg_Warn( p_filter, "DWrite_GetFamily: FindFamilyName() failed" );
        goto done;
    }

    if( p_dw_sys->p_dw_system_fonts->GetFontFamily( i_index, p_dw_family.GetAddressOf() ) )
    {
        msg_Err( p_filter, "DWrite_GetFamily: GetFontFamily() failed" );
        goto done;
    }

    try
    {
        DWrite_ParseFamily( p_dw_family.Get(), p_family, p_dw_sys->streams );
    }
    catch( const exception &e )
    {
        msg_Err( p_filter, "DWrite_GetFamily(): %s", e.what() );
    }

done:
    free( pwsz_family );
    return p_family;
}

static char *DWrite_Fallback( filter_t *p_filter, const char *psz_family,
                              uni_char_t codepoint )
{
    filter_sys_t                     *p_sys             = p_filter->p_sys;
    dw_sys_t                         *p_dw_sys          = ( dw_sys_t * ) p_sys->p_dw_sys;
    wchar_t                          *pwsz_buffer       = NULL;
    char                             *psz_result        = NULL;
    UINT32                            i_string_length   = 0;
    UINT32                            i_mapped_length   = 0;
    float                             f_scale;
    ComPtr< IDWriteFont >             p_dw_font;
    ComPtr< IDWriteFontFamily >       p_dw_family;
    ComPtr< IDWriteLocalizedStrings > p_names;

    msg_Dbg( p_filter, "DWrite_Fallback(): family: %s, codepoint: 0x%x", psz_family, codepoint );

    wchar_t p_text[2];
    UINT32  i_text_length;
    ToUTF16( codepoint, p_text, &i_text_length );

    wchar_t *pwsz_family = ToWide( psz_family );
    if( unlikely( !pwsz_family ) ) return NULL;

    ComPtr< TextSource > p_ts;
    p_ts = new(std::nothrow) TextSource( p_dw_sys->p_dw_factory.Get(), p_dw_sys->p_dw_substitution.Get(), p_text, i_text_length );
    if( unlikely( p_ts == NULL ) ) { goto done; }

    if( p_dw_sys->p_dw_fallbacks->MapCharacters( p_ts.Get(), 0, i_text_length, p_dw_sys->p_dw_system_fonts.Get(), pwsz_family,
                                  DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                  &i_mapped_length, p_dw_font.GetAddressOf(), &f_scale )
     || !p_dw_font )
    {
        msg_Warn( p_filter, "DWrite_Fallback(): MapCharacters() failed" );
        goto done;
    }

    if( p_dw_font->GetFontFamily( p_dw_family.GetAddressOf() ) )
    {
        msg_Err( p_filter, "DWrite_Fallback(): GetFontFamily() failed" );
        goto done;
    }

    if( p_dw_family->GetFamilyNames( p_names.GetAddressOf() ) )
    {
        msg_Err( p_filter, "DWrite_Fallback(): GetFamilyNames() failed" );
        goto done;
    }

    if( p_names->GetStringLength( 0, &i_string_length ) )
    {
        msg_Err( p_filter, "DWrite_Fallback(): GetStringLength() failed" );
        goto done;
    }

    pwsz_buffer = ( wchar_t * ) vlc_alloc( ( i_string_length + 1 ), sizeof( *pwsz_buffer ) );
    if( unlikely( !pwsz_buffer ) )
        goto done;

    if( p_names->GetString( 0, pwsz_buffer, i_string_length + 1 ) )
    {
        msg_Err( p_filter, "DWrite_Fallback(): GetString() failed" );
        goto done;
    }

    psz_result = FromWide( pwsz_buffer );
    msg_Dbg( p_filter, "DWrite_Fallback(): returning %s", psz_result );

done:
    free( pwsz_buffer );
    free( pwsz_family );
    return psz_result;
}

extern "C" vlc_family_t *DWrite_GetFallbacks( filter_t *p_filter, const char *psz_family,
                                              uni_char_t codepoint )
{
    filter_sys_t  *p_sys         = p_filter->p_sys;
    vlc_family_t  *p_family      = NULL;
    vlc_family_t  *p_fallbacks   = NULL;
    char          *psz_fallback  = NULL;


    char *psz_lc = ToLower( psz_family );

    if( unlikely( !psz_lc ) )
        return NULL;

    p_fallbacks = ( vlc_family_t * ) vlc_dictionary_value_for_key( &p_sys->fallback_map, psz_lc );

    if( p_fallbacks )
        p_family = SearchFallbacks( p_filter, p_fallbacks, codepoint );

    /*
     * If the fallback list of psz_family has no family which contains the requested
     * codepoint, try DWrite_Fallback(). If it returns a valid family which does
     * contain that codepoint, add the new family to the fallback list to speed up
     * later searches.
     */
    if( !p_family )
    {
        psz_fallback = DWrite_Fallback( p_filter, psz_lc, codepoint );

        if( !psz_fallback )
            goto done;

        const vlc_family_t *p_fallback = DWrite_GetFamily( p_filter, psz_fallback );
        if( !p_fallback || !p_fallback->p_fonts )
            goto done;

        FT_Face p_face = GetFace( p_filter, p_fallback->p_fonts );

        if( !p_face || !FT_Get_Char_Index( p_face, codepoint ) )
            goto done;

        p_family = NewFamily( p_filter, psz_fallback, NULL, NULL, NULL );

        if( unlikely( !p_family ) )
            goto done;

        p_family->p_fonts = p_fallback->p_fonts;

        if( p_fallbacks )
            AppendFamily( &p_fallbacks, p_family );
        else
            vlc_dictionary_insert( &p_sys->fallback_map,
                                   psz_lc, p_family );
    }

done:
    free( psz_lc );
    free( psz_fallback );
    return p_family;
}
