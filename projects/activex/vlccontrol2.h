/*****************************************************************************
 * vlccontrol.h: ActiveX control for VLC
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

#ifndef _VLCCONTROL2_H_
#define _VLCCONTROL2_H_

#include "axvlc_idl.h"

#include <vlc/libvlc.h>

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
    STDMETHODIMP get_volume(long*);
    STDMETHODIMP put_volume(long);
    STDMETHODIMP get_track(long*);
    STDMETHODIMP put_track(long);
    STDMETHODIMP get_channel(long*);
    STDMETHODIMP put_channel(long);
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
    STDMETHODIMP get_position(double*);
    STDMETHODIMP put_position(double);
    STDMETHODIMP get_time(double*);
    STDMETHODIMP put_time(double);
    STDMETHODIMP get_state(long*);
    STDMETHODIMP get_rate(double*);
    STDMETHODIMP put_rate(double);
    STDMETHODIMP get_fps(double*);
    STDMETHODIMP get_hasVout(VARIANT_BOOL*);

protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*      _p_instance;
    ITypeInfo*      _p_typeinfo;

};

class VLCMessage: public IVLCMessage
{
public:

    VLCMessage(VLCPlugin *p_instance, struct libvlc_log_message_t &msg) :
        _p_instance(p_instance),
        _p_typeinfo(NULL),
        _refcount(1),
        _msg(msg) {};
    virtual ~VLCMessage();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCMessage == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        // behaves as a standalone object
        return E_NOINTERFACE;
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return InterlockedIncrement(&_refcount); };
    STDMETHODIMP_(ULONG) Release(void)
    {
        ULONG refcount = InterlockedDecrement(&_refcount);
        if( 0 == refcount )
        {
            delete this;
            return 0;
        }
        return refcount;
    };

    // IDispatch methods
    STDMETHODIMP GetTypeInfoCount(UINT*);
    STDMETHODIMP GetTypeInfo(UINT, LCID, LPTYPEINFO*);
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*);
    STDMETHODIMP Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);

    // IVLCMessage methods
    STDMETHODIMP get__Value(VARIANT *);
    STDMETHODIMP get_severity(long *);
    STDMETHODIMP get_type(BSTR *);
    STDMETHODIMP get_name(BSTR *);
    STDMETHODIMP get_header(BSTR *);
    STDMETHODIMP get_message(BSTR *);

protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*      _p_instance;
    ITypeInfo*      _p_typeinfo;
    LONG            _refcount;

    struct libvlc_log_message_t _msg;
};

class VLCLog;

class VLCMessageIterator : public IVLCMessageIterator
{
public:

    VLCMessageIterator(VLCPlugin *p_instance, VLCLog* p_vlclog);
    virtual ~VLCMessageIterator();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCMessageIterator == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        // behaves as a standalone object
        return E_NOINTERFACE;
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return InterlockedIncrement(&_refcount); };
    STDMETHODIMP_(ULONG) Release(void)
    {
        ULONG refcount = InterlockedDecrement(&_refcount);
        if( 0 == refcount )
        {
            delete this;
            return 0;
        }
        return refcount;
    };

    // IDispatch methods
    STDMETHODIMP GetTypeInfoCount(UINT*);
    STDMETHODIMP GetTypeInfo(UINT, LCID, LPTYPEINFO*);
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*);
    STDMETHODIMP Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);

    // IVLCMessageIterator methods
    STDMETHODIMP get_hasNext(VARIANT_BOOL*);
    STDMETHODIMP next(IVLCMessage**);
 
protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*      _p_instance;
    ITypeInfo*      _p_typeinfo;
    LONG            _refcount;

    VLCLog*                 _p_vlclog;
    libvlc_log_iterator_t*  _p_iter;
};

class VLCMessages : public IVLCMessages
{
public:

    VLCMessages(VLCPlugin *p_instance, VLCLog *p_vlclog) :
        _p_vlclog(p_vlclog),
        _p_instance(p_instance),
        _p_typeinfo(NULL) {}
    virtual ~VLCMessages();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCMessages == riid) )
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

    // IVLCMessages methods
    STDMETHODIMP get__NewEnum(LPUNKNOWN*);
    STDMETHODIMP clear();
    STDMETHODIMP get_count(long*);
    STDMETHODIMP iterator(IVLCMessageIterator**);

protected:
    HRESULT loadTypeInfo();

    VLCLog*     _p_vlclog;

private:
    VLCPlugin*  _p_instance;
    ITypeInfo*  _p_typeinfo;
};
 
class VLCLog : public IVLCLog
{
public:

    friend class VLCMessages;
    friend class VLCMessageIterator;

    VLCLog(VLCPlugin *p_instance) :
        _p_log(NULL),
        _p_instance(p_instance),
        _p_typeinfo(NULL),
        _p_vlcmessages(NULL)
    {
        _p_vlcmessages = new VLCMessages(p_instance, this);
    };
    virtual ~VLCLog();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCLog == riid) )
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

    // IVLCLog methods
    STDMETHODIMP get_messages(IVLCMessages**);
    STDMETHODIMP get_verbosity(long *);
    STDMETHODIMP put_verbosity(long);

protected:
    HRESULT loadTypeInfo();

    libvlc_log_t    *_p_log;

private:
    VLCPlugin*      _p_instance;
    ITypeInfo*      _p_typeinfo;

    VLCMessages*    _p_vlcmessages;
};

class VLCPlaylistItems : public IVLCPlaylistItems
{
public:
    VLCPlaylistItems(VLCPlugin *p_instance) :
        _p_instance(p_instance), _p_typeinfo(NULL) {};
    virtual ~VLCPlaylistItems();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCPlaylistItems == riid) )
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

    // IVLCPlaylistItems methods
    STDMETHODIMP get_count(long*);
    STDMETHODIMP clear();
    STDMETHODIMP remove(long);

protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*  _p_instance;
    ITypeInfo*  _p_typeinfo;

};

class VLCPlaylist : public IVLCPlaylist
{
public:
    VLCPlaylist(VLCPlugin *p_instance) :
        _p_instance(p_instance),
        _p_typeinfo(NULL),
        _p_vlcplaylistitems(NULL)
    {
        _p_vlcplaylistitems = new VLCPlaylistItems(p_instance);
    };
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
    STDMETHODIMP get_itemCount(long*);
    STDMETHODIMP get_isPlaying(VARIANT_BOOL*);
    STDMETHODIMP add(BSTR, VARIANT, VARIANT, long*);
    STDMETHODIMP play();
    STDMETHODIMP playItem(long);
    STDMETHODIMP togglePause();
    STDMETHODIMP stop();
    STDMETHODIMP next();
    STDMETHODIMP prev();
    STDMETHODIMP clear();
    STDMETHODIMP removeItem(long);
    STDMETHODIMP get_items(IVLCPlaylistItems**);

protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*  _p_instance;
    ITypeInfo*  _p_typeinfo;

    VLCPlaylistItems*    _p_vlcplaylistitems;
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
    STDMETHODIMP get_width(long*);
    STDMETHODIMP get_height(long*);
    STDMETHODIMP get_aspectRatio(BSTR*);
    STDMETHODIMP put_aspectRatio(BSTR);
    STDMETHODIMP get_subtitle(long*);
    STDMETHODIMP put_subtitle(long);
    STDMETHODIMP get_crop(BSTR*);
    STDMETHODIMP put_crop(BSTR);
    STDMETHODIMP get_teletext(long*);
    STDMETHODIMP put_teletext(long);
    STDMETHODIMP takeSnapshot(LPPICTUREDISP*);
    STDMETHODIMP toggleFullscreen();
    STDMETHODIMP toggleTeletext();

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
    STDMETHODIMP get_Toolbar(VARIANT_BOOL *visible);
    STDMETHODIMP put_Toolbar(VARIANT_BOOL visible);
    STDMETHODIMP get_StartTime(long *seconds);
    STDMETHODIMP put_StartTime(long seconds);
    STDMETHODIMP get_VersionInfo(BSTR *version);
    STDMETHODIMP get_Visible(VARIANT_BOOL *visible);
    STDMETHODIMP put_Visible(VARIANT_BOOL visible);
    STDMETHODIMP get_Volume(long *volume);
    STDMETHODIMP put_Volume(long volume);
    STDMETHODIMP get_BackColor(OLE_COLOR *backcolor);
    STDMETHODIMP put_BackColor(OLE_COLOR backcolor);

    STDMETHODIMP get_audio(IVLCAudio**);
    STDMETHODIMP get_input(IVLCInput**);
    STDMETHODIMP get_log(IVLCLog**);
    STDMETHODIMP get_playlist(IVLCPlaylist**);
    STDMETHODIMP get_video(IVLCVideo**);

protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin*      _p_instance;
    ITypeInfo*      _p_typeinfo;

    VLCAudio*       _p_vlcaudio;
    VLCInput*       _p_vlcinput;
    VLCLog  *       _p_vlclog;
    VLCPlaylist*    _p_vlcplaylist;
    VLCVideo*       _p_vlcvideo;
};

#endif
