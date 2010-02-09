/*****************************************************************************
 * vlccontrol.cpp: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005-2010 the VideoLAN team
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "plugin.h"
#include "vlccontrol.h"

#include "utils.h"

using namespace std;

VLCControl::~VLCControl()
{
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT VLCControl::getTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IVLCControl, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP VLCControl::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(getTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP VLCControl::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
{
    if( NULL == ppTInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(getTypeInfo()) )
    {
        _p_typeinfo->AddRef();
        *ppTInfo = _p_typeinfo;
        return NOERROR;
    }
    *ppTInfo = NULL;
    return E_NOTIMPL;
};

STDMETHODIMP VLCControl::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames,
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(getTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCControl::Invoke(DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS* pDispParams,
        VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
    if( SUCCEEDED(getTypeInfo()) )
    {
        return DispInvoke(this, _p_typeinfo, dispIdMember, wFlags, pDispParams,
                pVarResult, pExcepInfo, puArgErr);
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCControl::get_Visible(VARIANT_BOOL *isVisible)
{
    if( NULL == isVisible )
        return E_POINTER;

    *isVisible = _p_instance->getVisible() ? VARIANT_TRUE : VARIANT_FALSE;

    return S_OK;
};

STDMETHODIMP VLCControl::put_Visible(VARIANT_BOOL isVisible)
{
    _p_instance->setVisible(isVisible != VARIANT_FALSE);

    return S_OK;
};

STDMETHODIMP VLCControl::play(void)
{
    _p_instance->playlist_play();
    _p_instance->fireOnPlayEvent();
    return S_OK;
};

STDMETHODIMP VLCControl::pause(void)
{
    libvlc_media_player_t* p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        libvlc_media_player_pause(p_md);
        _p_instance->fireOnPauseEvent();
    }
    return result;
};

STDMETHODIMP VLCControl::stop(void)
{
    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        libvlc_media_player_stop(p_md);
        _p_instance->fireOnStopEvent();
    }
    return result;
};

STDMETHODIMP VLCControl::get_Playing(VARIANT_BOOL *isPlaying)
{
    if( NULL == isPlaying )
        return E_POINTER;

    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        *isPlaying = libvlc_media_player_is_playing(p_md) ?
                     VARIANT_TRUE : VARIANT_FALSE;
    } else *isPlaying = VARIANT_FALSE;
    return result;
};

STDMETHODIMP VLCControl::get_Position(float *position)
{
    if( NULL == position )
        return E_POINTER;
    *position = 0.0f;

    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        *position = libvlc_media_player_get_position(p_md);
    }
    return result;
};

STDMETHODIMP VLCControl::put_Position(float position)
{
    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        libvlc_media_player_set_position(p_md, position);
    }
    return result;
};

STDMETHODIMP VLCControl::get_Time(int *seconds)
{
    if( NULL == seconds )
        return E_POINTER;

    *seconds = 0;
    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        *seconds = libvlc_media_player_get_time(p_md);
    }
    return result;
};

STDMETHODIMP VLCControl::put_Time(int seconds)
{
    /* setTime function of the plugin sets the time. */
    _p_instance->setTime(seconds);
    return S_OK;
};

STDMETHODIMP VLCControl::shuttle(int seconds)
{
    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        if( seconds < 0 ) seconds = 0;
        libvlc_media_player_set_time(p_md, (int64_t)seconds);
    }
    return result;

};

STDMETHODIMP VLCControl::fullscreen(void)
{
    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        if( libvlc_media_player_is_playing(p_md) )
        {
            libvlc_toggle_fullscreen(p_md);
        }
    }
    return result;
};

STDMETHODIMP VLCControl::get_Length(int *seconds)
{
    if( NULL == seconds )
        return E_POINTER;
    *seconds = 0;

    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        *seconds = (double)libvlc_media_player_get_length(p_md);
    }
    return result;

};

STDMETHODIMP VLCControl::playFaster(void)
{
    int32_t rate = 2;

    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);

    if( SUCCEEDED(result) )
    {
        libvlc_media_player_set_rate(p_md, rate);
    }
    return result;
};

STDMETHODIMP VLCControl::playSlower(void)
{
    float rate = 0.5;

    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
    {
        libvlc_media_player_set_rate(p_md, rate);
    }
    return result;
};

STDMETHODIMP VLCControl::get_Volume(int *volume)
{
    if( NULL == volume )
        return E_POINTER;

    *volume  = _p_instance->getVolume();
    return S_OK;
};

STDMETHODIMP VLCControl::put_Volume(int volume)
{
    _p_instance->setVolume(volume);
    return S_OK;
};

STDMETHODIMP VLCControl::toggleMute(void)
{
    libvlc_media_player_t *p_md;
    HRESULT result = _p_instance->getMD(&p_md);
    if( SUCCEEDED(result) )
        libvlc_audio_toggle_mute(p_md);
    return result;
};

STDMETHODIMP VLCControl::setVariable(BSTR name, VARIANT value)
{
    libvlc_instance_t* p_libvlc;
    HRESULT result = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(result) )
    {
        _p_instance->setErrorInfo(IID_IVLCControl,
            "setVariable() is an unsafe interface to use. "
            "It has been removed because of security implications." );
    }
    return E_FAIL;
};

STDMETHODIMP VLCControl::getVariable(BSTR name, VARIANT *value)
{
    libvlc_instance_t* p_libvlc;
    HRESULT result = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(result) )
    {
        _p_instance->setErrorInfo(IID_IVLCControl,
            "getVariable() is an unsafe interface to use. "
            "It has been removed because of security implications." );
    }
    return E_FAIL;
};

void VLCControl::FreeTargetOptions(char **cOptions, int cOptionCount)
{
    // clean up
    if( NULL != cOptions )
    {
        for( int pos=0; pos<cOptionCount; ++pos )
        {
            char *cOption = cOptions[pos];
            if( NULL != cOption )
                CoTaskMemFree(cOption);
            else
                break;
        }
        CoTaskMemFree(cOptions);
    }
};

static HRESULT parseStringOptions(int codePage, BSTR bstr, char*** cOptions, int *cOptionCount)
{
    HRESULT hr = E_INVALIDARG;
    if( SysStringLen(bstr) > 0 )
    {
        hr = E_OUTOFMEMORY;
        char *s = CStrFromBSTR(codePage, bstr);
        char *val = s;
        if( val )
        {
            long capacity = 16;
            char **options = (char **)CoTaskMemAlloc(capacity*sizeof(char *));
            if( options )
            {
                int nOptions = 0;

                char *end = val + strlen(val);
                while( val < end )
                {
                    // skip leading blanks
                    while( (val < end)
                        && ((*val == ' ' ) || (*val == '\t')) )
                        ++val;

                    char *start = val;
                    // skip till we get a blank character
                    while( (val < end)
                        && (*val != ' ' )
                        && (*val != '\t') )
                    {
                        char c = *(val++);
                        if( ('\'' == c) || ('"' == c) )
                        {
                            // skip till end of string
                            while( (val < end) && (*(val++) != c ) );
                        }
                    }

                    if( val > start )
                    {
                        if( nOptions == capacity )
                        {
                            capacity += 16;
                            char **moreOptions = (char **)CoTaskMemRealloc(options, capacity*sizeof(char*));
                            if( ! moreOptions )
                            {
                                /* failed to allocate more memory */
                                CoTaskMemFree(s);
                                /* return what we got so far */
                                *cOptionCount = nOptions;
                                *cOptions = options;
                                return NOERROR;
                            }
                            options = moreOptions;
                        }
                        *(val++) = '\0';
                        options[nOptions] = (char *)CoTaskMemAlloc(val-start);
                        if( options[nOptions] )
                        {
                            memcpy(options[nOptions], start, val-start);
                            ++nOptions;
                        }
                        else
                        {
                            /* failed to allocate memory */
                            CoTaskMemFree(s);
                            /* return what we got so far */
                            *cOptionCount = nOptions;
                            *cOptions = options;
                            return NOERROR;
                        }
                    }
                    else
                        // must be end of string
                        break;
                }
                *cOptionCount = nOptions;
                *cOptions = options;
                hr = NOERROR;
            }
            CoTaskMemFree(s);
        }
    }
    return hr;
}

HRESULT VLCControl::CreateTargetOptions(int codePage, VARIANT *options, char ***cOptions, int *cOptionCount)
{
    HRESULT hr = E_INVALIDARG;
    if( VT_ERROR == V_VT(options) )
    {
        if( DISP_E_PARAMNOTFOUND == V_ERROR(options) )
        {
            // optional parameter not set
            *cOptions = NULL;
            *cOptionCount = 0;
            return NOERROR;
        }
    }
    else if( (VT_EMPTY == V_VT(options)) || (VT_NULL == V_VT(options)) )
    {
        // null parameter
        *cOptions = NULL;
        *cOptionCount = 0;
        return NOERROR;
    }
    else if( VT_DISPATCH == V_VT(options) )
    {
        // if object is a collection, retrieve enumerator
        VARIANT colEnum;
        V_VT(&colEnum) = VT_UNKNOWN;
        hr = GetObjectProperty(V_DISPATCH(options), DISPID_NEWENUM, colEnum);
        if( SUCCEEDED(hr) )
        {
            IEnumVARIANT *enumVar;
            hr = V_UNKNOWN(&colEnum)->QueryInterface(IID_IEnumVARIANT, (LPVOID *)&enumVar);
            if( SUCCEEDED(hr) )
            {
                long pos = 0;
                long capacity = 16;
                VARIANT option;

                *cOptions = (char **)CoTaskMemAlloc(capacity*sizeof(char *));
                if( NULL != *cOptions )
                {
                    ZeroMemory(*cOptions, sizeof(char *)*capacity);
                    while( SUCCEEDED(hr) && (S_OK == enumVar->Next(1, &option, NULL)) )
                    {
                        if( VT_BSTR == V_VT(&option) )
                        {
                            char *cOption = CStrFromBSTR(codePage, V_BSTR(&option));
                            (*cOptions)[pos] = cOption;
                            if( NULL != cOption )
                            {
                                ++pos;
                                if( pos == capacity )
                                {
                                    char **moreOptions = (char **)CoTaskMemRealloc(*cOptions, (capacity+16)*sizeof(char *));
                                    if( NULL != moreOptions )
                                    {
                                        ZeroMemory(moreOptions+capacity, sizeof(char *)*16);
                                        capacity += 16;
                                        *cOptions = moreOptions;
                                    }
                                    else
                                        hr = E_OUTOFMEMORY;
                                }
                            }
                            else
                                hr = ( SysStringLen(V_BSTR(&option)) > 0 ) ?
                                    E_OUTOFMEMORY : E_INVALIDARG;
                        }
                        else
                            hr = E_INVALIDARG;

                        VariantClear(&option);
                    }
                    *cOptionCount = pos;
                    if( FAILED(hr) )
                    {
                        // free already processed elements
                        FreeTargetOptions(*cOptions, *cOptionCount);
                    }
                }
                else
                    hr = E_OUTOFMEMORY;

                enumVar->Release();
            }
        }
        else
        {
            // coerce object into a string and parse it
            VARIANT v_name;
            VariantInit(&v_name);
            hr = VariantChangeType(&v_name, options, 0, VT_BSTR);
            if( SUCCEEDED(hr) )
            {
                hr = parseStringOptions(codePage, V_BSTR(&v_name), cOptions, cOptionCount);
                VariantClear(&v_name);
            }
        }
    }
    else if( V_ISARRAY(options) )
    {
        // array parameter
        SAFEARRAY *array = V_ISBYREF(options) ? *V_ARRAYREF(options) : V_ARRAY(options);

        if( SafeArrayGetDim(array) != 1 )
            return E_INVALIDARG;

        long lBound = 0;
        long uBound = 0;
        SafeArrayGetLBound(array, 1, &lBound);
        SafeArrayGetUBound(array, 1, &uBound);

        // have we got any options
        if( uBound >= lBound )
        {
            VARTYPE vType;
            hr = SafeArrayGetVartype(array, &vType);
            if( FAILED(hr) )
                return hr;

            long pos;

            // marshall options into an array of C strings
            if( VT_VARIANT == vType )
            {
                *cOptions = (char **)CoTaskMemAlloc(sizeof(char *)*(uBound-lBound+1));
                if( NULL == *cOptions )
                    return E_OUTOFMEMORY;

                ZeroMemory(*cOptions, sizeof(char *)*(uBound-lBound+1));
                for(pos=lBound; (pos<=uBound) && SUCCEEDED(hr); ++pos )
                {
                    VARIANT option;
                    hr = SafeArrayGetElement(array, &pos, &option);
                    if( SUCCEEDED(hr) )
                    {
                        if( VT_BSTR == V_VT(&option) )
                        {
                            char *cOption = CStrFromBSTR(codePage, V_BSTR(&option));
                            (*cOptions)[pos-lBound] = cOption;
                            if( NULL == cOption )
                                hr = ( SysStringLen(V_BSTR(&option)) > 0 ) ?
                                    E_OUTOFMEMORY : E_INVALIDARG;
                        }
                        else
                            hr = E_INVALIDARG;
                        VariantClear(&option);
                    }
                }
            }
            else if( VT_BSTR == vType )
            {
                *cOptions = (char **)CoTaskMemAlloc(sizeof(char *)*(uBound-lBound+1));
                if( NULL == *cOptions )
                    return E_OUTOFMEMORY;

                ZeroMemory(*cOptions, sizeof(char *)*(uBound-lBound+1));
                for(pos=lBound; (pos<=uBound) && SUCCEEDED(hr); ++pos )
                {
                    BSTR option;
                    hr = SafeArrayGetElement(array, &pos, &option);
                    if( SUCCEEDED(hr) )
                    {
                        char *cOption = CStrFromBSTR(codePage, option);

                        (*cOptions)[pos-lBound] = cOption;
                        if( NULL == cOption )
                            hr = ( SysStringLen(option) > 0 ) ?
                                E_OUTOFMEMORY : E_INVALIDARG;
                        SysFreeString(option);
                    }
                }
            }
            else
            {
                // unsupported type
                return E_INVALIDARG;
            }

            *cOptionCount = pos-lBound;
            if( FAILED(hr) )
            {
                // free already processed elements
                FreeTargetOptions(*cOptions, *cOptionCount);
            }
        }
        else
        {
            // empty array
            *cOptions = NULL;
            *cOptionCount = 0;
            return NOERROR;
        }
    }
    else if( VT_UNKNOWN == V_VT(options) )
    {
        // coerce object into a string and parse it
        VARIANT v_name;
        VariantInit(&v_name);
        hr = VariantChangeType(&v_name, options, 0, VT_BSTR);
        if( SUCCEEDED(hr) )
        {
            hr = parseStringOptions(codePage, V_BSTR(&v_name), cOptions, cOptionCount);
            VariantClear(&v_name);
        }
    }
    else if( VT_BSTR == V_VT(options) )
    {
        hr = parseStringOptions(codePage, V_BSTR(options), cOptions, cOptionCount);
    }
    return hr;
};

/*
** use VARIANT rather than a SAFEARRAY as argument type
** for compatibility with some scripting language (JScript)
*/

STDMETHODIMP VLCControl::addTarget(BSTR uri, VARIANT options, enum VLCPlaylistMode mode, int position)
{
    if( 0 == SysStringLen(uri) )
        return E_INVALIDARG;

    libvlc_instance_t *p_libvlc;
    HRESULT hr = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(hr) )
    {
        char *cUri = CStrFromBSTR(CP_UTF8, uri);
        if( NULL == cUri )
            return E_OUTOFMEMORY;

        int cOptionsCount;
        char **cOptions;

        if( FAILED(CreateTargetOptions(CP_UTF8, &options, &cOptions, &cOptionsCount)) )
            return E_INVALIDARG;

        position = _p_instance->playlist_add_extended_untrusted(cUri,
                       cOptionsCount, const_cast<const char**>(cOptions));

        FreeTargetOptions(cOptions, cOptionsCount);
        CoTaskMemFree(cUri);

        if( position >= 0 )
        {
            if( mode & VLCPlayListAppendAndGo )
                _p_instance->fireOnPlayEvent();
        }
        else
        {
            if( mode & VLCPlayListAppendAndGo )
                _p_instance->fireOnStopEvent();
        }
    }
    return hr;
};

STDMETHODIMP VLCControl::get_PlaylistIndex(int *index)
{
    if( NULL == index )
        return E_POINTER;

    *index = 0;
    libvlc_instance_t *p_libvlc;
    HRESULT result = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(result) )
    {
        *index = _p_instance->playlist_get_current_index();
    }
    return result;
};

STDMETHODIMP VLCControl::get_PlaylistCount(int *count)
{
    if( NULL == count )
        return E_POINTER;

    *count = _p_instance->playlist_count();
    return S_OK;
};

STDMETHODIMP VLCControl::playlistNext(void)
{
    libvlc_instance_t* p_libvlc;
    HRESULT result = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(result) )
    {
        _p_instance->playlist_next();
    }
    return result;
};

STDMETHODIMP VLCControl::playlistPrev(void)
{
    libvlc_instance_t* p_libvlc;
    HRESULT result = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(result) )
    {
        _p_instance->playlist_prev();
    }
    return result;
};

STDMETHODIMP VLCControl::playlistClear(void)
{
    libvlc_instance_t* p_libvlc;
    HRESULT result = _p_instance->getVLC(&p_libvlc);
    if( SUCCEEDED(result) )
    {
        _p_instance->playlist_clear();
    }
    return result;
};

STDMETHODIMP VLCControl::get_VersionInfo(BSTR *version)
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

STDMETHODIMP VLCControl::get_MRL(BSTR *mrl)
{
    if( NULL == mrl )
        return E_POINTER;

    *mrl = SysAllocStringLen(_p_instance->getMRL(),
                SysStringLen(_p_instance->getMRL()));
    return S_OK;
};

STDMETHODIMP VLCControl::put_MRL(BSTR mrl)
{
    _p_instance->setMRL(mrl);

    return S_OK;
};

STDMETHODIMP VLCControl::get_AutoPlay(VARIANT_BOOL *autoplay)
{
    if( NULL == autoplay )
        return E_POINTER;

    *autoplay = _p_instance->getAutoPlay() ? VARIANT_TRUE: VARIANT_FALSE;
    return S_OK;
};

STDMETHODIMP VLCControl::put_AutoPlay(VARIANT_BOOL autoplay)
{
    _p_instance->setAutoPlay((VARIANT_FALSE != autoplay) ? TRUE: FALSE);
    return S_OK;
};

STDMETHODIMP VLCControl::get_AutoLoop(VARIANT_BOOL *autoloop)
{
    if( NULL == autoloop )
        return E_POINTER;

    *autoloop = _p_instance->getAutoLoop() ? VARIANT_TRUE: VARIANT_FALSE;
    return S_OK;
};

STDMETHODIMP VLCControl::put_AutoLoop(VARIANT_BOOL autoloop)
{
    _p_instance->setAutoLoop((VARIANT_FALSE != autoloop) ? TRUE: FALSE);
    return S_OK;
};
