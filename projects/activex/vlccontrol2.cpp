/*****************************************************************************
 * vlccontrol2.cpp: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
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

#include <stdio.h>
#include <shlwapi.h>
#include <wininet.h>
#include <tchar.h>

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

        *mute = libvlc_audio_get_mute(p_libvlc, &ex) ?
                        VARIANT_TRUE : VARIANT_FALSE;
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCAudio,
                         libvlc_exception_get_message(&ex));
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
            _p_instance->setErrorInfo(IID_IVLCAudio,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCAudio::get_volume(long* volume)
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
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCAudio,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCAudio::put_volume(long volume)
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
            _p_instance->setErrorInfo(IID_IVLCAudio,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCAudio::get_track(long* track)
{
    if( NULL == track )
        return E_POINTER;

    libvlc_media_player_t* p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *track = libvlc_audio_get_track(p_md, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCAudio,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCAudio::put_track(long track)
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_audio_set_track(p_md, track, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCAudio,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCAudio::get_channel(long *channel)
{
    if( NULL == channel )
        return E_POINTER;

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *channel = libvlc_audio_get_channel(p_libvlc, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCAudio,
                        libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCAudio::put_channel(long channel)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_audio_set_channel(p_libvlc, channel, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCAudio,
                         libvlc_exception_get_message(&ex));
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
            _p_instance->setErrorInfo(IID_IVLCAudio,
                         libvlc_exception_get_message(&ex));
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

STDMETHODIMP VLCInput::get_length(double* length)
{
    if( NULL == length )
        return E_POINTER;
    *length = 0;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *length = (double)libvlc_media_player_get_length(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCInput,
                     libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_position(double* position)
{
    if( NULL == position )
        return E_POINTER;

    *position = 0.0f;
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *position = libvlc_media_player_get_position(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCInput,
                     libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::put_position(double position)
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_set_position(p_md, position, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCInput,
                     libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_time(double* time)
{
    if( NULL == time )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *time = (double)libvlc_media_player_get_time(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCInput,
                     libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::put_time(double time)
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_set_time(p_md, (int64_t)time, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCInput,
                     libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_state(long* state)
{
    if( NULL == state )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *state = libvlc_media_player_get_state(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        libvlc_exception_clear(&ex);
        // don't fail, just return the idle state
        *state = 0;
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_rate(double* rate)
{
    if( NULL == rate )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *rate = libvlc_media_player_get_rate(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCInput,
                     libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::put_rate(double rate)
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_set_rate(p_md, rate, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCInput,
                     libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_fps(double* fps)
{
    if( NULL == fps )
        return E_POINTER;

    *fps = 0.0;
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *fps = libvlc_media_player_get_fps(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCInput,
                     libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCInput::get_hasVout(VARIANT_BOOL* hasVout)
{
    if( NULL == hasVout )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *hasVout = libvlc_media_player_has_vout(p_md, &ex) ?
                                VARIANT_TRUE : VARIANT_FALSE;
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCInput,
                     libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

/*******************************************************************************/

VLCLog::~VLCLog()
{
    delete _p_vlcmessages;
    if( _p_log )
        libvlc_log_close(_p_log, NULL);

    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCLog::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCLog, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCLog::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCLog::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
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

STDMETHODIMP VLCLog::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames,
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCLog::Invoke(DISPID dispIdMember, REFIID riid,
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

STDMETHODIMP VLCLog::get_messages(IVLCMessages** obj)
{
    if( NULL == obj )
        return E_POINTER;

    *obj = _p_vlcmessages;
    if( NULL != _p_vlcmessages )
    {
        _p_vlcmessages->AddRef();
        return NOERROR;
    }
    return E_OUTOFMEMORY;
};

STDMETHODIMP VLCLog::get_verbosity(long* level)
{
    if( NULL == level )
        return E_POINTER;

    if( _p_log )
    {
        libvlc_instance_t* p_libvlc;
        HRESULT hr = _p_instance->getVLC(&p_libvlc);
        if( SUCCEEDED(hr) )
        {
            libvlc_exception_t ex;
            libvlc_exception_init(&ex);

            *level = libvlc_get_log_verbosity(p_libvlc, &ex);
            if( libvlc_exception_raised(&ex) )
            {
                _p_instance->setErrorInfo(IID_IVLCLog,
                             libvlc_exception_get_message(&ex));
                libvlc_exception_clear(&ex);
                return E_FAIL;
            }
        }
        return hr;
    }
    else
    {
        /* log is not enabled, return -1 */
        *level = -1;
        return NOERROR;
    }
};

STDMETHODIMP VLCLog::put_verbosity(long verbosity)
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        if( verbosity >= 0 )
        {
            if( ! _p_log )
            {
                _p_log = libvlc_log_open(p_libvlc, &ex);
                if( libvlc_exception_raised(&ex) )
                {
                    _p_instance->setErrorInfo(IID_IVLCLog,
                                 libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return E_FAIL;
                }
            }
            libvlc_set_log_verbosity(p_libvlc, (unsigned)verbosity, &ex);
            if( libvlc_exception_raised(&ex) )
            {
                _p_instance->setErrorInfo(IID_IVLCLog,
                             libvlc_exception_get_message(&ex));
                libvlc_exception_clear(&ex);
                return E_FAIL;
            }
        }
        else if( _p_log )
        {
            /* close log  when verbosity is set to -1 */
            libvlc_log_close(_p_log, &ex);
            _p_log = NULL;
            if( libvlc_exception_raised(&ex) )
            {
                _p_instance->setErrorInfo(IID_IVLCLog,
                             libvlc_exception_get_message(&ex));
                libvlc_exception_clear(&ex);
                return E_FAIL;
            }
        }
    }
    return hr;
};

/*******************************************************************************/

/* STL forward iterator used by VLCEnumIterator class to implement IEnumVARIANT */

class VLCMessageSTLIterator
{

public:

    VLCMessageSTLIterator(IVLCMessageIterator* iter) : iter(iter), msg(NULL)
    {
        // get first message
        operator++();
    };

    VLCMessageSTLIterator(const VLCMessageSTLIterator& other)
    {
        iter = other.iter;
        if( iter )
            iter->AddRef();
        msg = other.msg;
        if( msg )
            msg->AddRef();
    };

    virtual ~VLCMessageSTLIterator()
    {
        if( msg )
            msg->Release();

        if( iter )
            iter->Release();
    };

    // we only need prefix ++ operator
    VLCMessageSTLIterator& operator++()
    {
        VARIANT_BOOL hasNext = VARIANT_FALSE;
        if( iter )
        {
            iter->get_hasNext(&hasNext);

            if( msg )
            {
                msg->Release();
                msg = NULL;
            }
            if( VARIANT_TRUE == hasNext ) {
                iter->next(&msg);
            }
        }
        return *this;
    };

    VARIANT operator*() const
    {
        VARIANT v;
        VariantInit(&v);
        if( msg )
        {
            if( SUCCEEDED(msg->QueryInterface(IID_IDispatch,
                          (LPVOID*)&V_DISPATCH(&v))) )
            {
                V_VT(&v) = VT_DISPATCH;
            }
        }
        return v;
    };

    bool operator==(const VLCMessageSTLIterator& other) const
    {
        return msg == other.msg;
    };

    bool operator!=(const VLCMessageSTLIterator& other) const
    {
        return msg != other.msg;
    };

private:
    IVLCMessageIterator* iter;
    IVLCMessage*         msg;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////

VLCMessages::~VLCMessages()
{
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCMessages::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCMessages, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCMessages::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCMessages::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
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

STDMETHODIMP VLCMessages::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames,
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCMessages::Invoke(DISPID dispIdMember, REFIID riid,
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

STDMETHODIMP VLCMessages::get__NewEnum(LPUNKNOWN* _NewEnum)
{
    if( NULL == _NewEnum )
        return E_POINTER;

    IVLCMessageIterator* iter = NULL;
    iterator(&iter);

    *_NewEnum= new VLCEnumIterator<IID_IEnumVARIANT,
                       IEnumVARIANT,
                       VARIANT,
                       VLCMessageSTLIterator>
                       (VLCMessageSTLIterator(iter), VLCMessageSTLIterator(NULL));

    return *_NewEnum ? S_OK : E_OUTOFMEMORY;
};

STDMETHODIMP VLCMessages::clear()
{
    libvlc_log_t *p_log = _p_vlclog->_p_log;
    if( p_log )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_log_clear(p_log, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCMessages,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
    }
    return NOERROR;
};

STDMETHODIMP VLCMessages::get_count(long* count)
{
    if( NULL == count )
        return E_POINTER;

    libvlc_log_t *p_log = _p_vlclog->_p_log;
    if( p_log )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *count = libvlc_log_count(p_log, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCMessages,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
    }
    else
        *count = 0;
    return S_OK;
};

STDMETHODIMP VLCMessages::iterator(IVLCMessageIterator** iter)
{
    if( NULL == iter )
        return E_POINTER;

    *iter = new VLCMessageIterator(_p_instance, _p_vlclog);

    return *iter ? S_OK : E_OUTOFMEMORY;
};

/*******************************************************************************/

VLCMessageIterator::VLCMessageIterator(VLCPlugin *p_instance, VLCLog* p_vlclog ) :
    _p_instance(p_instance),
    _p_typeinfo(NULL),
    _refcount(1),
    _p_vlclog(p_vlclog)
{
    if( p_vlclog->_p_log )
    {
        _p_iter = libvlc_log_get_iterator(p_vlclog->_p_log, NULL);
    }
    else
        _p_iter = NULL;
};

VLCMessageIterator::~VLCMessageIterator()
{
    if( _p_iter )
        libvlc_log_iterator_free(_p_iter, NULL);

    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCMessageIterator::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCMessageIterator, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCMessageIterator::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCMessageIterator::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
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

STDMETHODIMP VLCMessageIterator::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames,
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCMessageIterator::Invoke(DISPID dispIdMember, REFIID riid,
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

STDMETHODIMP VLCMessageIterator::get_hasNext(VARIANT_BOOL* hasNext)
{
    if( NULL == hasNext )
        return E_POINTER;

    if( _p_iter &&  _p_vlclog->_p_log )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *hasNext = libvlc_log_iterator_has_next(_p_iter, &ex) ?
                                     VARIANT_TRUE : VARIANT_FALSE;
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCMessageIterator,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
    }
    else
    {
        *hasNext = VARIANT_FALSE;
    }
    return S_OK;
};

STDMETHODIMP VLCMessageIterator::next(IVLCMessage** message)
{
    if( NULL == message )
        return E_POINTER;

    if( _p_iter &&  _p_vlclog->_p_log )
    {
        struct libvlc_log_message_t buffer;

        buffer.sizeof_msg = sizeof(buffer);

        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_log_iterator_next(_p_iter, &buffer, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCMessageIterator,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        *message = new VLCMessage(_p_instance, buffer);
        return *message ? NOERROR : E_OUTOFMEMORY;
    }
    return E_FAIL;
};

/*******************************************************************************/

VLCMessage::~VLCMessage()
{
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCMessage::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCMessage, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCMessage::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCMessage::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
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

STDMETHODIMP VLCMessage::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames,
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCMessage::Invoke(DISPID dispIdMember, REFIID riid,
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

inline const char *msgSeverity(int sev)
{
    switch( sev )
    {
        case 0:
            return "info";
        case 1:
            return "error";
        case 2:
            return "warning";
        default:
            return "debug";
    }
};

STDMETHODIMP VLCMessage::get__Value(VARIANT* _Value)
{
    if( NULL == _Value )
        return E_POINTER;

    char buffer[256];

    snprintf(buffer, sizeof(buffer), "%s %s %s: %s",
        _msg.psz_type, _msg.psz_name, msgSeverity(_msg.i_severity), _msg.psz_message);

    V_VT(_Value) = VT_BSTR;
    V_BSTR(_Value) = BSTRFromCStr(CP_UTF8, buffer);

    return S_OK;
};

STDMETHODIMP VLCMessage::get_severity(long* level)
{
    if( NULL == level )
        return E_POINTER;

    *level = _msg.i_severity;

    return S_OK;
};

STDMETHODIMP VLCMessage::get_type(BSTR* type)
{
    if( NULL == type )
        return E_POINTER;

    *type = BSTRFromCStr(CP_UTF8, _msg.psz_type);

    return NOERROR;
};

STDMETHODIMP VLCMessage::get_name(BSTR* name)
{
    if( NULL == name )
        return E_POINTER;

    *name = BSTRFromCStr(CP_UTF8, _msg.psz_name);

    return NOERROR;
};

STDMETHODIMP VLCMessage::get_header(BSTR* header)
{
    if( NULL == header )
        return E_POINTER;

    *header = BSTRFromCStr(CP_UTF8, _msg.psz_header);

    return NOERROR;
};

STDMETHODIMP VLCMessage::get_message(BSTR* message)
{
    if( NULL == message )
        return E_POINTER;

    *message = BSTRFromCStr(CP_UTF8, _msg.psz_message);

    return NOERROR;
};

/*******************************************************************************/

VLCPlaylistItems::~VLCPlaylistItems()
{
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCPlaylistItems::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCPlaylistItems, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCPlaylistItems::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCPlaylistItems::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
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

STDMETHODIMP VLCPlaylistItems::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames,
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCPlaylistItems::Invoke(DISPID dispIdMember, REFIID riid,
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

STDMETHODIMP VLCPlaylistItems::get_count(long* count)
{
    if( NULL == count )
        return E_POINTER;

    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    *count = _p_instance->playlist_count(&ex);
    if( libvlc_exception_raised(&ex) )
    {
        _p_instance->setErrorInfo(IID_IVLCPlaylistItems,
            libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return NOERROR;
};

STDMETHODIMP VLCPlaylistItems::clear()
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        _p_instance->playlist_clear(&ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCPlaylistItems,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylistItems::remove(long item)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        _p_instance->playlist_delete_item(item, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCPlaylistItems,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

/*******************************************************************************/

VLCPlaylist::~VLCPlaylist()
{
    delete _p_vlcplaylistitems;
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

STDMETHODIMP VLCPlaylist::get_itemCount(long* count)
{
    if( NULL == count )
        return E_POINTER;

    *count = 0;
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    *count = _p_instance->playlist_count(&ex);
    if( libvlc_exception_raised(&ex) )
    {
        _p_instance->setErrorInfo(IID_IVLCPlaylist,
            libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return NOERROR;
};

STDMETHODIMP VLCPlaylist::get_isPlaying(VARIANT_BOOL* isPlaying)
{
    if( NULL == isPlaying )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *isPlaying = libvlc_media_player_is_playing(p_md, &ex) ?
                                    VARIANT_TRUE: VARIANT_FALSE;
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCPlaylist,
                         libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::add(BSTR uri, VARIANT name, VARIANT options, long* item)
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

        char *psz_uri = NULL;
        if( SysStringLen(_p_instance->getBaseURL()) > 0 )
        {
            /*
            ** if the MRL a relative URL, we should end up with an absolute URL
            */
            LPWSTR abs_url = CombineURL(_p_instance->getBaseURL(), uri);
            if( NULL != abs_url )
            {
                psz_uri = CStrFromWSTR(CP_UTF8, abs_url, wcslen(abs_url));
                CoTaskMemFree(abs_url);
            }
            else
            {
                psz_uri = CStrFromBSTR(CP_UTF8, uri);
            }
        }
        else
        {
            /*
            ** baseURL is empty, assume MRL is absolute
            */
            psz_uri = CStrFromBSTR(CP_UTF8, uri);
        }

        if( NULL == psz_uri )
        {
            return E_OUTOFMEMORY;
        }

        int i_options;
        char **ppsz_options;

        hr = VLCControl::CreateTargetOptions(CP_UTF8, &options, &ppsz_options, &i_options);
        if( FAILED(hr) )
        {
            CoTaskMemFree(psz_uri);
            return hr;
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

        *item = _p_instance->playlist_add_extended_untrusted(psz_uri,
                    i_options, const_cast<const char **>(ppsz_options), &ex);

        VLCControl::FreeTargetOptions(ppsz_options, i_options);
        CoTaskMemFree(psz_uri);
        if( psz_name )
            CoTaskMemFree(psz_name);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCPlaylist,
                libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::play()
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    _p_instance->playlist_play(&ex);
    if( libvlc_exception_raised(&ex) )
    {
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return NOERROR;
};

STDMETHODIMP VLCPlaylist::playItem(long item)
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    _p_instance->playlist_play_item(item,&ex);
    if( libvlc_exception_raised(&ex) )
    {
        _p_instance->setErrorInfo(IID_IVLCPlaylist,
            libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return NOERROR;
};

STDMETHODIMP VLCPlaylist::togglePause()
{
    libvlc_media_player_t* p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_pause(p_md, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCPlaylist,
                libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::stop()
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_stop(p_md, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCPlaylist,
                libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::next()
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    _p_instance->playlist_next(&ex);
    if( libvlc_exception_raised(&ex) )
    {
        _p_instance->setErrorInfo(IID_IVLCPlaylist,
            libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return NOERROR;
};

STDMETHODIMP VLCPlaylist::prev()
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    _p_instance->playlist_prev(&ex);
    if( libvlc_exception_raised(&ex) )
    {
        _p_instance->setErrorInfo(IID_IVLCPlaylist,
            libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return NOERROR;
};

STDMETHODIMP VLCPlaylist::clear()
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    _p_instance->playlist_clear(&ex);
    if( libvlc_exception_raised(&ex) )
    {
        _p_instance->setErrorInfo(IID_IVLCPlaylist,
            libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return NOERROR;
};

STDMETHODIMP VLCPlaylist::removeItem(long item)
{
    libvlc_instance_t* p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        _p_instance->playlist_delete_item(item, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCPlaylist,
                libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCPlaylist::get_items(IVLCPlaylistItems** obj)
{
    if( NULL == obj )
        return E_POINTER;

    *obj = _p_vlcplaylistitems;
    if( NULL != _p_vlcplaylistitems )
    {
        _p_vlcplaylistitems->AddRef();
        return NOERROR;
    }
    return E_OUTOFMEMORY;
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

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *fullscreen = libvlc_get_fullscreen(p_md, &ex) ? VARIANT_TRUE : VARIANT_FALSE;
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::put_fullscreen(VARIANT_BOOL fullscreen)
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_set_fullscreen(p_md, VARIANT_FALSE != fullscreen, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::get_width(long* width)
{
    if( NULL == width )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *width = libvlc_video_get_width(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::get_height(long* height)
{
    if( NULL == height )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *height = libvlc_video_get_height(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::get_aspectRatio(BSTR* aspect)
{
    if( NULL == aspect )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        char *psz_aspect = libvlc_video_get_aspect_ratio(p_md, &ex);

        if( ! libvlc_exception_raised(&ex) )
        {
            if( NULL == psz_aspect )
                return E_OUTOFMEMORY;

            *aspect = BSTRFromCStr(CP_UTF8, psz_aspect);
            free( psz_aspect );
            psz_aspect = NULL;
            return (NULL == *aspect) ? E_OUTOFMEMORY : NOERROR;
        }
        free( psz_aspect );
        psz_aspect = NULL;

        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::put_aspectRatio(BSTR aspect)
{
    if( NULL == aspect )
        return E_POINTER;

    if( 0 == SysStringLen(aspect) )
        return E_INVALIDARG;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        char *psz_aspect = CStrFromBSTR(CP_UTF8, aspect);
        if( NULL == psz_aspect )
        {
            return E_OUTOFMEMORY;
        }

        libvlc_video_set_aspect_ratio(p_md, psz_aspect, &ex);

        CoTaskMemFree(psz_aspect);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCVideo,
                libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCVideo::get_subtitle(long* spu)
{
    if( NULL == spu )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *spu = libvlc_video_get_spu(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::put_subtitle(long spu)
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_video_set_spu(p_md, spu, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCVideo::get_crop(BSTR* geometry)
{
    if( NULL == geometry )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        char *psz_geometry = libvlc_video_get_crop_geometry(p_md, &ex);

        if( ! libvlc_exception_raised(&ex) )
        {
            if( NULL == psz_geometry )
                return E_OUTOFMEMORY;

            *geometry = BSTRFromCStr(CP_UTF8, psz_geometry);
            free( psz_geometry );
            psz_geometry = NULL;
            return (NULL == geometry) ? E_OUTOFMEMORY : NOERROR;
        }
        free( psz_geometry );
        psz_geometry = NULL;
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::put_crop(BSTR geometry)
{
    if( NULL == geometry )
        return E_POINTER;

    if( 0 == SysStringLen(geometry) )
        return E_INVALIDARG;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        char *psz_geometry = CStrFromBSTR(CP_UTF8, geometry);
        if( NULL == psz_geometry )
        {
            return E_OUTOFMEMORY;
        }

        libvlc_video_set_crop_geometry(p_md, psz_geometry, &ex);

        CoTaskMemFree(psz_geometry);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCVideo,
                libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCVideo::get_teletext(long* page)
{
    if( NULL == page )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        *page = libvlc_video_get_teletext(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::put_teletext(long page)
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_video_set_teletext(p_md, page, &ex);
        if( libvlc_exception_raised(&ex) )
        {
            _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return E_FAIL;
        }
        return NOERROR;
    }
    return hr;
};

STDMETHODIMP VLCVideo::takeSnapshot(LPPICTUREDISP* picture)
{
    if( NULL == picture )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        static int uniqueId = 0;
        TCHAR path[MAX_PATH+1];

        int pathlen = GetTempPath(MAX_PATH-24, path);
        if( (0 == pathlen) || (pathlen > (MAX_PATH-24)) )
            return E_FAIL;

        /* check temp directory path by openning it */
        {
            HANDLE dirHandle = CreateFile(path, GENERIC_READ,
                       FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            if( INVALID_HANDLE_VALUE == dirHandle )
            {
                _p_instance->setErrorInfo(IID_IVLCVideo,
                        "Invalid temporary directory for snapshot images, check values of TMP, TEMP envars.");
                return E_FAIL;
            }
            else
            {
                BY_HANDLE_FILE_INFORMATION bhfi;
                BOOL res = GetFileInformationByHandle(dirHandle, &bhfi);
                CloseHandle(dirHandle);
                if( !res || !(bhfi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
                {
                    _p_instance->setErrorInfo(IID_IVLCVideo,
                            "Invalid temporary directory for snapshot images, check values of TMP, TEMP envars.");
                    return E_FAIL;
                }
            }
        }

        TCHAR filepath[MAX_PATH+1];

        _stprintf(filepath, TEXT("%sAXVLC%lXS%lX.bmp"),
                 path, GetCurrentProcessId(), ++uniqueId);

#ifdef _UNICODE
        /* reuse path storage for UTF8 string */
        char *psz_filepath = (char *)path;
        WCHAR* wpath = filepath;
#else
        char *psz_filepath = path;
        /* first convert to unicode using current code page */
        WCHAR wpath[MAX_PATH+1];
        if( 0 == MultiByteToWideChar(CP_ACP, 0, filepath, -1, wpath, sizeof(wpath)/sizeof(WCHAR)) )
            return E_FAIL;
#endif
        /* convert to UTF8 */
        pathlen = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, psz_filepath, sizeof(path), NULL, NULL);
        // fail if path is 0 or too short (i.e pathlen is the same as storage size)
        if( (0 == pathlen) || (sizeof(path) == pathlen) )
            return E_FAIL;

        /* take snapshot into file */
        libvlc_video_take_snapshot(p_md, psz_filepath, 0, 0, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            hr = E_FAIL;
            /* open snapshot file */
            HANDLE snapPic = LoadImage(NULL, filepath, IMAGE_BITMAP,0, 0, LR_CREATEDIBSECTION|LR_LOADFROMFILE);
            if( snapPic )
            {
                PICTDESC snapDesc;

                snapDesc.cbSizeofstruct = sizeof(PICTDESC);
                snapDesc.picType        = PICTYPE_BITMAP;
                snapDesc.bmp.hbitmap    = (HBITMAP)snapPic;
                snapDesc.bmp.hpal       = NULL;

                hr = OleCreatePictureIndirect(&snapDesc, IID_IPictureDisp, TRUE, (LPVOID*)picture);
                if( FAILED(hr) )
                {
                    *picture = NULL;
                    DeleteObject(snapPic);
                }
            }
            DeleteFile(filepath);
            return hr;
        }
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::toggleFullscreen()
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_toggle_fullscreen(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

STDMETHODIMP VLCVideo::toggleTeletext()
{
    libvlc_media_player_t *p_md;
    HRESULT hr = _p_instance->getMD(&p_md);
    if( SUCCEEDED(hr) )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_toggle_teletext(p_md, &ex);
        if( ! libvlc_exception_raised(&ex) )
        {
            return NOERROR;
        }
        _p_instance->setErrorInfo(IID_IVLCVideo, libvlc_exception_get_message(&ex));
        libvlc_exception_clear(&ex);
        return E_FAIL;
    }
    return hr;
};

/*******************************************************************************/

VLCControl2::VLCControl2(VLCPlugin *p_instance) :
    _p_instance(p_instance),
    _p_typeinfo(NULL),
    _p_vlcaudio(NULL),
    _p_vlcinput(NULL),
    _p_vlcplaylist(NULL),
    _p_vlcvideo(NULL)
{
    _p_vlcaudio     = new VLCAudio(p_instance);
    _p_vlcinput     = new VLCInput(p_instance);
    _p_vlclog       = new VLCLog(p_instance);
    _p_vlcplaylist  = new VLCPlaylist(p_instance);
    _p_vlcvideo     = new VLCVideo(p_instance);
};

VLCControl2::~VLCControl2()
{
    delete _p_vlcvideo;
    delete _p_vlcplaylist;
    delete _p_vlclog;
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

STDMETHODIMP VLCControl2::get_AutoLoop(VARIANT_BOOL *autoloop)
{
    if( NULL == autoloop )
        return E_POINTER;

    *autoloop = _p_instance->getAutoLoop() ? VARIANT_TRUE: VARIANT_FALSE;
    return S_OK;
};

STDMETHODIMP VLCControl2::put_AutoLoop(VARIANT_BOOL autoloop)
{
    _p_instance->setAutoLoop((VARIANT_FALSE != autoloop) ? TRUE: FALSE);
    return S_OK;
};

STDMETHODIMP VLCControl2::get_AutoPlay(VARIANT_BOOL *autoplay)
{
    if( NULL == autoplay )
        return E_POINTER;

    *autoplay = _p_instance->getAutoPlay() ? VARIANT_TRUE: VARIANT_FALSE;
    return S_OK;
};

STDMETHODIMP VLCControl2::put_AutoPlay(VARIANT_BOOL autoplay)
{
    _p_instance->setAutoPlay((VARIANT_FALSE != autoplay) ? TRUE: FALSE);
    return S_OK;
};

STDMETHODIMP VLCControl2::get_BaseURL(BSTR *url)
{
    if( NULL == url )
        return E_POINTER;

    *url = SysAllocStringLen(_p_instance->getBaseURL(),
                SysStringLen(_p_instance->getBaseURL()));
    return NOERROR;
};

STDMETHODIMP VLCControl2::put_BaseURL(BSTR mrl)
{
    _p_instance->setBaseURL(mrl);

    return S_OK;
};

STDMETHODIMP VLCControl2::get_MRL(BSTR *mrl)
{
    if( NULL == mrl )
        return E_POINTER;

    *mrl = SysAllocStringLen(_p_instance->getMRL(),
                SysStringLen(_p_instance->getMRL()));
    return NOERROR;
};

STDMETHODIMP VLCControl2::put_MRL(BSTR mrl)
{
    _p_instance->setMRL(mrl);

    return S_OK;
};


STDMETHODIMP VLCControl2::get_Toolbar(VARIANT_BOOL *visible)
{
    if( NULL == visible )
        return E_POINTER;

    /*
     * Note to developpers
     *
     * Returning the _b_toolbar is closer to the method specification.
     * But returning True when toolbar is not implemented so not displayed
     * could be bad for ActiveX users which rely on this value to show their
     * own toolbar if not provided by the ActiveX.
     *
     * This is the reason why FALSE is returned, until toolbar get implemented.
     */

    /* DISABLED for now */
    //  *visible = _p_instance->getShowToolbar() ? VARIANT_TRUE: VARIANT_FALSE;

    *visible = VARIANT_FALSE;

    return S_OK;
};

STDMETHODIMP VLCControl2::put_Toolbar(VARIANT_BOOL visible)
{
    _p_instance->setShowToolbar((VARIANT_FALSE != visible) ? TRUE: FALSE);
    return S_OK;
};


STDMETHODIMP VLCControl2::get_StartTime(long *seconds)
{
    if( NULL == seconds )
        return E_POINTER;

    *seconds = _p_instance->getStartTime();

    return S_OK;
};

STDMETHODIMP VLCControl2::put_StartTime(long seconds)
{
    _p_instance->setStartTime(seconds);

    return NOERROR;
};

STDMETHODIMP VLCControl2::get_VersionInfo(BSTR *version)
{
    if( NULL == version )
        return E_POINTER;

    const char *versionStr = libvlc_get_version();
    if( NULL != versionStr )
    {
        *version = BSTRFromCStr(CP_UTF8, versionStr);

        return (NULL == *version) ? E_OUTOFMEMORY : NOERROR;
    }
    *version = NULL;
    return E_FAIL;
};

STDMETHODIMP VLCControl2::get_Visible(VARIANT_BOOL *isVisible)
{
    if( NULL == isVisible )
        return E_POINTER;

    *isVisible = _p_instance->getVisible() ? VARIANT_TRUE : VARIANT_FALSE;

    return NOERROR;
};

STDMETHODIMP VLCControl2::put_Visible(VARIANT_BOOL isVisible)
{
    _p_instance->setVisible(isVisible != VARIANT_FALSE);

    return NOERROR;
};

STDMETHODIMP VLCControl2::get_Volume(long *volume)
{
    if( NULL == volume )
        return E_POINTER;

    *volume  = _p_instance->getVolume();
    return NOERROR;
};

STDMETHODIMP VLCControl2::put_Volume(long volume)
{
    _p_instance->setVolume(volume);
    return NOERROR;
};

STDMETHODIMP VLCControl2::get_BackColor(OLE_COLOR *backcolor)
{
    if( NULL == backcolor )
        return E_POINTER;

    *backcolor  = _p_instance->getBackColor();
    return NOERROR;
};

STDMETHODIMP VLCControl2::put_BackColor(OLE_COLOR backcolor)
{
    _p_instance->setBackColor(backcolor);
    return NOERROR;
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

STDMETHODIMP VLCControl2::get_log(IVLCLog** obj)
{
    if( NULL == obj )
        return E_POINTER;

    *obj = _p_vlclog;
    if( NULL != _p_vlclog )
    {
        _p_vlclog->AddRef();
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
