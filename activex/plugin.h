/*****************************************************************************
 * plugin.h: ActiveX control for VLC
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
    STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
    STDMETHODIMP LockServer(BOOL fLock);

    LPCSTR getInPlaceWndClassName(void) const { return TEXT("VLC Plugin In-Place"); };
    LPCSTR getVideoWndClassName(void) const { return TEXT("VLC Plugin Video"); };
    HINSTANCE getHInstance(void) const { return _hinstance; };
    HBITMAP getInPlacePict(void) const { return _inplace_hbitmap; };

protected:

    virtual ~VLCPluginClass();

private:

    LPLONG      _p_class_ref;
    HINSTANCE   _hinstance;
    ATOM        _inplace_wndclass_atom;
    ATOM        _video_wndclass_atom;
    HBITMAP     _inplace_hbitmap;
};

class VLCPlugin : public IUnknown
{

public:

    VLCPlugin(VLCPluginClass *p_class);

    /* IUnknown methods */
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    /* custom methods */
    HRESULT getTypeLib(LCID lcid, ITypeLib **pTL)
        { return LoadRegTypeLib(LIBID_AXVLC, 1, 0, lcid, pTL); };
    REFCLSID getClassID(void) { return (REFCLSID)CLSID_VLCPlugin; };
    REFIID getDispEventID(void) { return (REFIID)DIID_DVLCEvents; };

    HRESULT onInit(BOOL isNew);
    HRESULT onLoad(void);
    HRESULT onClientSiteChanged(LPOLECLIENTSITE pActiveSite);
    HRESULT onClose(DWORD dwSaveOption);

    BOOL isInPlaceActive(void);
    HRESULT onActivateInPlace(LPMSG lpMesg, HWND hwndParent, LPCRECT lprcPosRect, LPCRECT lprcClipRect);
    HRESULT onInPlaceDeactivate(void);
    HWND getInPlaceWindow(void) const { return _inplacewnd; };

    BOOL hasFocus(void);
    void setFocus(BOOL fFocus);

    UINT getCodePage(void) { return _codepage; };
    void setCodePage(UINT cp) { _codepage = cp; };

    int  getVLCObject(void) { return _i_vlc; };

    // control properties
    void setSourceURL(const char *url) { _psz_src = strdup(url); };
    void setAutoStart(BOOL autostart) { _b_autostart = autostart; };
    void setLoopMode(BOOL loopmode) { _b_loopmode = loopmode; };
    void setMute(BOOL mute) {
        if( mute && _i_vlc )
        {
            VLC_VolumeMute(_i_vlc);
        }
    };
    void setSendEvents(BOOL sendevents) { _b_sendevents = sendevents; };
    void setVisible(BOOL fVisible);
    BOOL getVisible(void) { return _b_visible; };

    // container events
    void onPositionChange(LPCRECT lprcPosRect, LPCRECT lprcClipRect);
    void onPaint(HDC hdc, const RECT &bounds, const RECT &pr);

    // control events
    void firePropChangedEvent(DISPID dispid);
    void fireOnPlayEvent(void);
    void fireOnPauseEvent(void);
    void fireOnStopEvent(void);

protected:

    virtual ~VLCPlugin();

private:

    void calcPositionChange(LPRECT lprPosRect, LPCRECT lprcClipRect);

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

    // in place activated window (Clipping window)
    HWND _inplacewnd;
    // video window (Drawing window)
    HWND _videownd;
    RECT _bounds;

    VLCPluginClass *_p_class;
    ULONG _i_ref;

    UINT _codepage;
    char *_psz_src;
    BOOL _b_autostart;
    BOOL _b_loopmode;
    BOOL _b_visible;
    BOOL _b_sendevents;
    int _i_vlc;
};

#endif

