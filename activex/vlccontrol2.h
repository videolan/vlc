/*****************************************************************************
 * vlccontrol.h: ActiveX control for VLC
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

#ifndef _VLCCONTROL2_H_
#define _VLCCONTROL2_H_

#include "axvlc_idl.h"

class VLCAudio : public IVLCAudio
{
public:
    VLCAudio(VLCPlugin *p_instance) :
        _p_instance(p_instance), _p_typeinfo(NULL) {};
    virtual ~VLCAudio();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCAudio == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        // behaves as a standalone object
        return E_NOINTERFACE;
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->pUnkOuter->Release(); };

    // IDispatch methods
    STDMETHODIMP GetTypeInfoCount(UINT*);
    STDMETHODIMP GetTypeInfo(UINT, LCID, LPTYPEINFO*);
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*);
    STDMETHODIMP Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);

    // IVLCAudio methods
    STDMETHODIMP get_mute(VARIANT_BOOL*);
    STDMETHODIMP put_mute(VARIANT_BOOL);
    STDMETHODIMP get_volume(int*);
    STDMETHODIMP put_volume(int);
    STDMETHODIMP toggleMute();
 
protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*      _p_instance;
    ITypeInfo*      _p_typeinfo;

};
 
class VLCInput : public IVLCInput
{
public:

    VLCInput(VLCPlugin *p_instance) :
        _p_instance(p_instance), _p_typeinfo(NULL) {};
    virtual ~VLCInput();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCInput == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        // behaves as a standalone object
        return E_NOINTERFACE;
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->pUnkOuter->Release(); };

    // IDispatch methods
    STDMETHODIMP GetTypeInfoCount(UINT*);
    STDMETHODIMP GetTypeInfo(UINT, LCID, LPTYPEINFO*);
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*);
    STDMETHODIMP Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);

    // IVLCInput methods
    STDMETHODIMP get_length(double*);
    STDMETHODIMP get_position(float*);
    STDMETHODIMP put_position(float);
    STDMETHODIMP get_time(double*);
    STDMETHODIMP put_time(double);
    STDMETHODIMP get_state(int*);
    STDMETHODIMP get_rate(float*);
    STDMETHODIMP put_rate(float);
    STDMETHODIMP get_fps(float*);
    STDMETHODIMP get_hasVout(VARIANT_BOOL*);
    
protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*      _p_instance;
    ITypeInfo*      _p_typeinfo;

};
 
class VLCPlaylist : public IVLCPlaylist
{
public:
    VLCPlaylist(VLCPlugin *p_instance) :
        _p_instance(p_instance), _p_typeinfo(NULL) {};
    virtual ~VLCPlaylist();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCPlaylist == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        // behaves as a standalone object
        return E_NOINTERFACE;
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->pUnkOuter->Release(); };

    // IDispatch methods
    STDMETHODIMP GetTypeInfoCount(UINT*);
    STDMETHODIMP GetTypeInfo(UINT, LCID, LPTYPEINFO*);
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*);
    STDMETHODIMP Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);

    // IVLCPlaylist methods
    STDMETHODIMP get_itemCount(int*);
    STDMETHODIMP get_isPlaying(VARIANT_BOOL*);
    STDMETHODIMP add(BSTR, VARIANT, VARIANT, int*);
    STDMETHODIMP play();
    STDMETHODIMP playItem(int);
    STDMETHODIMP togglePause();
    STDMETHODIMP stop();
    STDMETHODIMP next();
    STDMETHODIMP prev();
    STDMETHODIMP clear();
    STDMETHODIMP removeItem(int);
 
protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*  _p_instance;
    ITypeInfo*  _p_typeinfo;

};
 
class VLCVideo : public IVLCVideo
{
public:
    VLCVideo(VLCPlugin *p_instance) :
        _p_instance(p_instance), _p_typeinfo(NULL) {};
    virtual ~VLCVideo();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCVideo == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        // behaves as a standalone object
        return E_NOINTERFACE;
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->pUnkOuter->Release(); };

    // IDispatch methods
    STDMETHODIMP GetTypeInfoCount(UINT*);
    STDMETHODIMP GetTypeInfo(UINT, LCID, LPTYPEINFO*);
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*);
    STDMETHODIMP Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);

    // IVLCVideo methods
    STDMETHODIMP get_fullscreen(VARIANT_BOOL*);
    STDMETHODIMP put_fullscreen(VARIANT_BOOL);
    STDMETHODIMP get_width(int*);
    STDMETHODIMP get_height(int*);
    STDMETHODIMP toggleFullscreen();
 
protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*      _p_instance;
    ITypeInfo*      _p_typeinfo;

};
 
class VLCControl2 : public IVLCControl2
{
    
public:

    VLCControl2(VLCPlugin *p_instance);
    virtual ~VLCControl2();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCControl2 == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->pUnkOuter->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->pUnkOuter->Release(); };

    // IDispatch methods
    STDMETHODIMP GetTypeInfoCount(UINT*);
    STDMETHODIMP GetTypeInfo(UINT, LCID, LPTYPEINFO*);
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*);
    STDMETHODIMP Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);

    // IVLCControl2 methods
    STDMETHODIMP get_AutoLoop(VARIANT_BOOL *autoloop);
    STDMETHODIMP put_AutoLoop(VARIANT_BOOL autoloop);
    STDMETHODIMP get_AutoPlay(VARIANT_BOOL *autoplay);
    STDMETHODIMP put_AutoPlay(VARIANT_BOOL autoplay);
    STDMETHODIMP get_BaseURL(BSTR *url);
    STDMETHODIMP put_BaseURL(BSTR url);
    STDMETHODIMP get_MRL(BSTR *mrl);
    STDMETHODIMP put_MRL(BSTR mrl);
    STDMETHODIMP get_StartTime(int *seconds);
    STDMETHODIMP put_StartTime(int seconds);
    STDMETHODIMP get_VersionInfo(BSTR *version);
    STDMETHODIMP get_Visible(VARIANT_BOOL *visible);
    STDMETHODIMP put_Visible(VARIANT_BOOL visible);
    STDMETHODIMP get_Volume(int *volume);
    STDMETHODIMP put_Volume(int volume);

    STDMETHODIMP get_audio(IVLCAudio**);
    STDMETHODIMP get_input(IVLCInput**);
    STDMETHODIMP get_playlist(IVLCPlaylist**);
    STDMETHODIMP get_video(IVLCVideo**);

protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*      _p_instance;
    ITypeInfo*      _p_typeinfo;

    VLCAudio*       _p_vlcaudio;
    VLCInput*       _p_vlcinput;
    VLCPlaylist*    _p_vlcplaylist;
    VLCVideo*       _p_vlcvideo;
};
 
#endif

