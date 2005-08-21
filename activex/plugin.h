/*****************************************************************************
 * plugin.h: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
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

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <ole2.h>
#include <olectl.h>

#include <vlc/vlc.h>

extern const GUID CLSID_VLCPlugin; 
extern const GUID LIBID_AXVLC; 
extern const GUID DIID_DVLCEvents; 

class VLCPluginClass : public IClassFactory
{

public:

    VLCPluginClass(LONG *p_class_ref,HINSTANCE hInstance);

    /* IUnknown methods */
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    /* IClassFactory methods */
    STDMETHODIMP CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void **ppv);
    STDMETHODIMP LockServer(BOOL fLock);

    LPCSTR getInPlaceWndClassName(void) const { return TEXT("VLC Plugin In-Place"); };
    LPCSTR getVideoWndClassName(void) const { return TEXT("VLC Plugin Video"); };
    HINSTANCE getHInstance(void) const { return _hinstance; };
    LPPICTURE getInPlacePict(void) const
        { if( NULL != _inplace_picture) _inplace_picture->AddRef(); return _inplace_picture; };

protected:

    virtual ~VLCPluginClass();

private:

    LPLONG      _p_class_ref;
    HINSTANCE   _hinstance;
    ATOM        _inplace_wndclass_atom;
    ATOM        _video_wndclass_atom;
    LPPICTURE   _inplace_picture;
};

class VLCPlugin : public IUnknown
{

public:

    VLCPlugin(VLCPluginClass *p_class, LPUNKNOWN pUnkOuter);

    /* IUnknown methods */
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    /* custom methods */
    HRESULT getTypeLib(LCID lcid, ITypeLib **pTL) { return LoadRegTypeLib(LIBID_AXVLC, 1, 0, lcid, pTL); };
    REFCLSID getClassID(void) { return (REFCLSID)CLSID_VLCPlugin; };
    REFIID getDispEventID(void) { return (REFIID)DIID_DVLCEvents; };

    /*
    ** persistant properties
    */
    void setMRL(BSTR mrl)
    {
        SysFreeString(_bstr_mrl);
        _bstr_mrl = SysAllocString(mrl);
        setDirty(TRUE);
    };
    const BSTR getMRL(void) { return _bstr_mrl; };

    inline void setAutoPlay(BOOL autoplay)
    {
        _b_autoplay = autoplay;
        setDirty(TRUE);
    };
    inline BOOL getAutoPlay(void) { return _b_autoplay; };

    inline void setAutoLoop(BOOL autoloop) 
    {
        _b_autoloop = autoloop;
        setDirty(TRUE);
    };
    inline BOOL getAutoLoop(void) { return _b_autoloop;};

    void setVisible(BOOL fVisible);
    BOOL getVisible(void) { return _b_visible; };

    // control size in HIMETRIC
    inline void setExtent(const SIZEL& extent)
    {
        _extent = extent;
        setDirty(TRUE);
    };
    const SIZEL& getExtent(void) { return _extent; };

    // transient properties 
    inline void setMute(BOOL mute) { _b_mute = mute; };

    inline void setPicture(LPPICTURE pict)
    {
        if( NULL != _p_pict )
            _p_pict->Release();
        if( NULL != pict )
            _p_pict->AddRef();
        _p_pict = pict;
    };

    inline LPPICTURE getPicture(void)
    {
        if( NULL != _p_pict )
            _p_pict->AddRef();
        return _p_pict;
    };
    
    BOOL hasFocus(void);
    void setFocus(BOOL fFocus);

    inline UINT getCodePage(void) { return _i_codepage; };
    inline void setCodePage(UINT cp) { _i_codepage = cp; };

    inline BOOL isUserMode(void) { return _b_usermode; };
    inline void setUserMode(BOOL um) { _b_usermode = um; };

    inline BOOL isDirty(void) { return _b_dirty; };
    inline void setDirty(BOOL dirty) { _b_dirty = dirty; };

    inline BOOL isRunning(void) { return 0 != _i_vlc; };

    // control geometry within container
    RECT getPosRect(void) { return _posRect; }; 
    inline HWND getInPlaceWindow(void) const { return _inplacewnd; };
    BOOL isInPlaceActive(void);

    inline int getVLCObject(void) const { return _i_vlc; };

    /*
    ** container events
    */
    HRESULT onInit(void);
    HRESULT onLoad(void);
    HRESULT onActivateInPlace(LPMSG lpMesg, HWND hwndParent, LPCRECT lprcPosRect, LPCRECT lprcClipRect);
    HRESULT onInPlaceDeactivate(void);
    HRESULT onAmbientChanged(LPUNKNOWN pContainer, DISPID dispID);
    HRESULT onClose(DWORD dwSaveOption);
    void onPositionChange(LPCRECT lprcPosRect, LPCRECT lprcClipRect);
    void onDraw(DVTARGETDEVICE * ptd, HDC hicTargetDev,
            HDC hdcDraw, LPCRECTL lprcBounds, LPCRECTL lprcWBounds);
    void onPaint(HDC hdc, const RECT &bounds, const RECT &pr);

    /*
    ** control events
    */
    void freezeEvents(BOOL freeze);
    void firePropChangedEvent(DISPID dispid);
    void fireOnPlayEvent(void);
    void fireOnPauseEvent(void);
    void fireOnStopEvent(void);

    // controlling IUnknown interface
    LPUNKNOWN pUnkOuter;

protected:

    virtual ~VLCPlugin();

private:

    //implemented interfaces
    class VLCOleObject *vlcOleObject;
    class VLCOleControl *vlcOleControl;
    class VLCOleInPlaceObject *vlcOleInPlaceObject;
    class VLCOleInPlaceActiveObject *vlcOleInPlaceActiveObject;
    class VLCPersistStreamInit *vlcPersistStreamInit;
    class VLCPersistStorage *vlcPersistStorage;
    class VLCPersistPropertyBag *vlcPersistPropertyBag;
    class VLCProvideClassInfo *vlcProvideClassInfo;
    class VLCConnectionPointContainer *vlcConnectionPointContainer;
    class VLCObjectSafety *vlcObjectSafety;
    class VLCControl *vlcControl;
    class VLCViewObject *vlcViewObject;
    class VLCDataObject *vlcDataObject;

    // in place activated window (Clipping window)
    HWND _inplacewnd;
    // video window (Drawing window)
    HWND _videownd;

    VLCPluginClass *_p_class;
    ULONG _i_ref;

    LPPICTURE _p_pict;
    UINT _i_codepage;
    BOOL _b_usermode;
    BSTR _bstr_mrl;
    BOOL _b_autoplay;
    BOOL _b_autoloop;
    BOOL _b_visible;
    BOOL _b_mute;
    BOOL _b_dirty;
    int  _i_vlc;

    SIZEL _extent;
    RECT _posRect;
};

#endif

