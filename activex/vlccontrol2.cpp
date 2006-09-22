/*****************************************************************************
 * vlccontrol2.cpp: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "plugin.h"
#include "vlccontrol2.h"
#include "vlccontrol.h"

#include "utils.h"

using namespace std;

VLCAudio::~VLCAudio()
{
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCAudio::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCAudio, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCAudio::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCAudio::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
{
    if( NULL == ppTInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
    {
        _p_typeinfo->AddRef();
        *ppTInfo = _p_typeinfo;
        return NOERROR;
    }
    *ppTInfo = NULL;
    return E_NOTIMPL;
};

STDMETHODIMP VLCAudio::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, 
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCAudio::Invoke(DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS* pDispParams,
        VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispInvoke(this, _p_typeinfo, dispIdMember, wFlags, pDispParams,
                pVarResult, pExcepInfo, puArgErr);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCAudio::get_mute(VARIANT_BOOL* mute)
{
    if( NULL == mute )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *mute = libvlc_audio_get_mute(p_libvlc, &ex) ? VARIANT_TRUE : VARIANT_FALSE;
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCAudio::put_mute(VARIANT_BOOL mute)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_audio_set_mute(p_libvlc, VARIANT_FALSE != mute, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

#include <iostream>

STDMETHODIMP VLCAudio::get_volume(int* volume)
{
    if( NULL == volume )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *volume = libvlc_audio_get_volume(p_libvlc, &ex);
        cerr << "volume is " << *volume;
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCAudio::put_volume(int volume)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_audio_set_volume(p_libvlc, volume, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCAudio::toggleMute()
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_audio_toggle_mute(p_libvlc, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

/*******************************************************************************/

VLCInput::~VLCInput()
{
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCInput::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCInput, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCInput::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCInput::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
{
    if( NULL == ppTInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
    {
        _p_typeinfo->AddRef();
        *ppTInfo = _p_typeinfo;
        return NOERROR;
    }
    *ppTInfo = NULL;
    return E_NOTIMPL;
};

STDMETHODIMP VLCInput::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, 
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCInput::Invoke(DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS* pDispParams,
        VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispInvoke(this, _p_typeinfo, dispIdMember, wFlags, pDispParams,
                pVarResult, pExcepInfo, puArgErr);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCInput::get_length(__int64* length)
{
    if( NULL == length )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *length = (__int64)libvlc_input_get_length(p_input, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_position(float* position)
{
    if( NULL == position )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *position = libvlc_input_get_position(p_input, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::put_position(float position)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            libvlc_input_set_position(p_input, position, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_time(__int64* time)
{
    if( NULL == time )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *time = libvlc_input_get_time(p_input, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::put_time(__int64 time)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            libvlc_input_set_time(p_input, time, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_state(int* state)
{
    if( NULL == state )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *state = libvlc_input_get_state(p_input, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        // don't fail, just return the idle state
        *state = 0;
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_rate(float* rate)
{
    if( NULL == rate )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *rate = libvlc_input_get_rate(p_input, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::put_rate(float rate)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            libvlc_input_set_rate(p_input, rate, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_fps(float* fps)
{
    if( NULL == fps )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *fps = libvlc_input_get_fps(p_input, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_hasVout(VARIANT_BOOL* hasVout)
{
    if( NULL == hasVout )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *hasVout = libvlc_input_has_vout(p_input, &ex) ? VARIANT_TRUE : VARIANT_FALSE;
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

/*******************************************************************************/

VLCPlaylist::~VLCPlaylist()
{
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCPlaylist::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCPlaylist, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCPlaylist::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
{
    if( NULL == ppTInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
    {
        _p_typeinfo->AddRef();
        *ppTInfo = _p_typeinfo;
        return NOERROR;
    }
    *ppTInfo = NULL;
    return E_NOTIMPL;
};

STDMETHODIMP VLCPlaylist::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, 
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCPlaylist::Invoke(DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS* pDispParams,
        VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispInvoke(this, _p_typeinfo, dispIdMember, wFlags, pDispParams,
                pVarResult, pExcepInfo, puArgErr);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCPlaylist::get_itemCount(int* count)
{
    if( NULL == count )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *count = libvlc_playlist_items_count(p_libvlc, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::get_isPlaying(VARIANT_BOOL* isPlaying)
{
    if( NULL == isPlaying )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *isPlaying = libvlc_playlist_isplaying(p_libvlc, &ex) ? VARIANT_TRUE: VARIANT_FALSE;
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::add(BSTR uri, VARIANT name, VARIANT options, int* item)
{
    if( NULL == item )
        return E_POINTER;

    if( 0 == SysStringLen(uri) )
        return E_INVALIDARG;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        int i_options;
        char **ppsz_options;

        hr = VLCControl::CreateTargetOptions(CP_UTF8, &options, &ppsz_options, &i_options);
        if( FAILED(hr) )
            return hr;

        char *psz_uri = CStrFromBSTR(CP_UTF8, uri);
        if( NULL == psz_uri )
        {
            VLCControl::FreeTargetOptions(ppsz_options, i_options);
            return E_OUTOFMEMORY;
        }

        char *psz_name = NULL;
        VARIANT v_name;
        VariantInit(&v_name);
        if( SUCCEEDED(VariantChangeType(&v_name, &name, 0, VT_BSTR)) )
        {
            if( SysStringLen(V_BSTR(&v_name)) > 0 )
                psz_name = CStrFromBSTR(CP_UTF8, V_BSTR(&v_name));

            VariantClear(&v_name);
        }

        *item = libvlc_playlist_add_extended(p_libvlc,
            psz_uri,
            psz_name,
            i_options,
            const_cast<const char **>(ppsz_options),
            &ex);

        VLCControl::FreeTargetOptions(ppsz_options, i_options);
        CoTaskMemFree(psz_uri);
        if( psz_name )
            CoTaskMemFree(psz_name);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::play()
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_playlist_play(p_libvlc, -1, 0, NULL, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::playItem(int item)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_playlist_play(p_libvlc, item, 0, NULL, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::togglePause()
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_playlist_pause(p_libvlc, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::stop()
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_playlist_stop(p_libvlc, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::next()
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_playlist_next(p_libvlc, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::prev()
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_playlist_prev(p_libvlc, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::clear()
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_playlist_clear(p_libvlc, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::removeItem(int item)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_playlist_delete_item(p_libvlc, item, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

/*******************************************************************************/

VLCVideo::~VLCVideo()
{
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCVideo::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCVideo, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCVideo::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCVideo::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
{
    if( NULL == ppTInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
    {
        _p_typeinfo->AddRef();
        *ppTInfo = _p_typeinfo;
        return NOERROR;
    }
    *ppTInfo = NULL;
    return E_NOTIMPL;
};

STDMETHODIMP VLCVideo::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, 
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCVideo::Invoke(DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS* pDispParams,
        VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispInvoke(this, _p_typeinfo, dispIdMember, wFlags, pDispParams,
                pVarResult, pExcepInfo, puArgErr);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCVideo::get_fullscreen(VARIANT_BOOL* fullscreen)
{
    if( NULL == fullscreen )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *fullscreen = libvlc_get_fullscreen(p_input, &ex) ? VARIANT_TRUE : VARIANT_FALSE;
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::put_fullscreen(VARIANT_BOOL fullscreen)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            libvlc_set_fullscreen(p_input, VARIANT_FALSE != fullscreen, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::get_width(int* width)
{
    if( NULL == width )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *width = libvlc_video_get_width(p_input, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::get_height(int* height)
{
    if( NULL == height )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_libvlc, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            *height = libvlc_video_get_height(p_input, &ex);
            libvlc_input_free(p_input);
            if( ! libvlc_exception_raised(&ex) )
            {
                return NOERROR;
            }
        }
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

/*******************************************************************************/

VLCControl2::VLCControl2(VLCPlugin *p_instance) :
    VLCConfiguration(p_instance),
    _p_instance(p_instance),
    _p_typeinfo(NULL),
    _p_vlcaudio(NULL),
    _p_vlcinput(NULL),
    _p_vlcplaylist(NULL),
    _p_vlcvideo(NULL)
{
    _p_vlcaudio     = new VLCAudio(p_instance);
    _p_vlcinput     = new VLCInput(p_instance);
    _p_vlcplaylist  = new VLCPlaylist(p_instance);
    _p_vlcvideo     = new VLCVideo(p_instance);
};

VLCControl2::~VLCControl2()
{
    delete _p_vlcvideo;
    delete _p_vlcplaylist;
    delete _p_vlcinput;
    delete _p_vlcaudio;
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCControl2::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCControl2, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCControl2::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCControl2::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
{
    if( NULL == ppTInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
    {
        _p_typeinfo->AddRef();
        *ppTInfo = _p_typeinfo;
        return NOERROR;
    }
    *ppTInfo = NULL;
    return E_NOTIMPL;
};

STDMETHODIMP VLCControl2::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, 
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCControl2::Invoke(DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS* pDispParams,
        VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispInvoke(this, _p_typeinfo, dispIdMember, wFlags, pDispParams,
                pVarResult, pExcepInfo, puArgErr);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCControl2::get_audio(IVLCAudio** obj)
{
    if( NULL == obj )
        return E_POINTER;

    *obj = _p_vlcaudio;
    if( NULL != _p_vlcaudio )
    {
        _p_vlcaudio->AddRef();
        return NOERROR;
    }
    return E_OUTOFMEMORY;
};

STDMETHODIMP VLCControl2::get_input(IVLCInput** obj)
{
    if( NULL == obj )
        return E_POINTER;

    *obj = _p_vlcinput;
    if( NULL != _p_vlcinput )
    {
        _p_vlcinput->AddRef();
        return NOERROR;
    }
    return E_OUTOFMEMORY;
};

STDMETHODIMP VLCControl2::get_playlist(IVLCPlaylist** obj)
{
    if( NULL == obj )
        return E_POINTER;

    *obj = _p_vlcplaylist;
    if( NULL != _p_vlcplaylist )
    {
        _p_vlcplaylist->AddRef();
        return NOERROR;
    }
    return E_OUTOFMEMORY;
};

STDMETHODIMP VLCControl2::get_video(IVLCVideo** obj)
{
    if( NULL == obj )
        return E_POINTER;

    *obj = _p_vlcvideo;
    if( NULL != _p_vlcvideo )
    {
        _p_vlcvideo->AddRef();
        return NOERROR;
    }
    return E_OUTOFMEMORY;
};

