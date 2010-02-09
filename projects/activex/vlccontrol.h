/*****************************************************************************
 * vlccontrol.h: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005-2010 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _VLCCONTROL_H_
#define _VLCCONTROL_H_

#include "axvlc_idl.h"

class VLCControl : public IVLCControl
{
public:

    VLCControl(VLCPlugin *p_instance):
        _p_instance(p_instance), _p_typeinfo(NULL) { }
    virtual ~VLCControl();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCControl == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->pUnkOuter->QueryInterface(riid, ppv);
    }

    STDMETHODIMP_(ULONG) AddRef(void)
        { return _p_instance->pUnkOuter->AddRef(); }
    STDMETHODIMP_(ULONG) Release(void)
        { return _p_instance->pUnkOuter->Release(); }

    // IDispatch methods
    STDMETHODIMP GetTypeInfoCount(UINT*);
    STDMETHODIMP GetTypeInfo(UINT, LCID, LPTYPEINFO*);
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*);
    STDMETHODIMP Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);

    // IVLCControl methods
    STDMETHODIMP play(void);
    STDMETHODIMP get_Visible(VARIANT_BOOL *visible);
    STDMETHODIMP put_Visible(VARIANT_BOOL visible);
    STDMETHODIMP pause(void);
    STDMETHODIMP stop(void);
    STDMETHODIMP get_Playing(VARIANT_BOOL *isPlaying);
    STDMETHODIMP get_Position(float *position);
    STDMETHODIMP put_Position(float position);
    STDMETHODIMP get_Time(int *seconds);
    STDMETHODIMP put_Time(int seconds);
    STDMETHODIMP shuttle(int seconds);
    STDMETHODIMP fullscreen();
    STDMETHODIMP get_Length(int *seconds);
    STDMETHODIMP playFaster(void);
    STDMETHODIMP playSlower(void);
    STDMETHODIMP get_Volume(int *volume);
    STDMETHODIMP put_Volume(int volume);
    STDMETHODIMP toggleMute(void);
    STDMETHODIMP setVariable( BSTR name, VARIANT value);
    STDMETHODIMP getVariable( BSTR name, VARIANT *value);
    STDMETHODIMP addTarget( BSTR uri, VARIANT options, enum VLCPlaylistMode mode, int position);
    STDMETHODIMP get_PlaylistIndex(int *index);
    STDMETHODIMP get_PlaylistCount(int *count);
    STDMETHODIMP playlistNext(void);
    STDMETHODIMP playlistPrev(void);
    STDMETHODIMP playlistClear(void);
    STDMETHODIMP get_VersionInfo(BSTR *version);
    STDMETHODIMP get_MRL(BSTR *mrl);
    STDMETHODIMP put_MRL(BSTR mrl);
    STDMETHODIMP get_AutoLoop(VARIANT_BOOL *autoloop);
    STDMETHODIMP put_AutoLoop(VARIANT_BOOL autoloop);
    STDMETHODIMP get_AutoPlay(VARIANT_BOOL *autoplay);
    STDMETHODIMP put_AutoPlay(VARIANT_BOOL autoplay);

    static HRESULT CreateTargetOptions(int codePage, VARIANT *options, char ***cOptions, int *cOptionCount);
    static void FreeTargetOptions(char **cOptions, int cOptionCount);

private:

    HRESULT      getTypeInfo();

    VLCPlugin *_p_instance;
    ITypeInfo *_p_typeinfo;
};

#endif
