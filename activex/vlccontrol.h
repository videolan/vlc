/*****************************************************************************
 * vlccontrol.h: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _VLCCONTROL_H_
#define _VLCCONTROL_H_

#include <oaidl.h>
#include "axvlc_idl.h"

class VLCControl : public IVLCControl
{
    
public:

    VLCControl(VLCPlugin *p_instance) :  _p_instance(p_instance), _p_typeinfo(NULL) {};
    virtual ~VLCControl();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( (NULL != ppv)
         && (IID_IUnknown == riid)
         && (IID_IDispatch == riid)
         && (IID_IVLCControl == riid) ) {
            AddRef();
            *ppv = dynamic_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->Release(); };

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
    STDMETHODIMP put_Playing(VARIANT_BOOL isPlaying);
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
 
private:

    HRESULT      getTypeInfo();

    VLCPlugin *_p_instance;
    ITypeInfo *_p_typeinfo;

};
 
#endif

