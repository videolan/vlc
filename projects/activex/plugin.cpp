/*****************************************************************************
 * plugin.cpp: ActiveX control for VLC
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

#include "oleobject.h"
#include "olecontrol.h"
#include "oleinplaceobject.h"
#include "oleinplaceactiveobject.h"
#include "persistpropbag.h"
#include "persiststreaminit.h"
#include "persiststorage.h"
#include "provideclassinfo.h"
#include "connectioncontainer.h"
#include "objectsafety.h"
#include "vlccontrol.h"
#include "vlccontrol2.h"
#include "viewobject.h"
#include "dataobject.h"
#include "supporterrorinfo.h"

#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <winreg.h>
#include <winuser.h>
#include <servprov.h>
#include <shlwapi.h>
#include <wininet.h>

using namespace std;

////////////////////////////////////////////////////////////////////////
//class factory

static LRESULT CALLBACK VLCInPlaceClassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VLCPlugin *p_instance = reinterpret_cast<VLCPlugin *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch( uMsg )
    {
        case WM_ERASEBKGND:
            return 1L;

        case WM_PAINT:
            PAINTSTRUCT ps;
            RECT pr;
            if( GetUpdateRect(hWnd, &pr, FALSE) )
            {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                BeginPaint(hWnd, &ps);
                p_instance->onPaint(ps.hdc, bounds, pr);
                EndPaint(hWnd, &ps);
            }
            return 0L;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
};

VLCPluginClass::VLCPluginClass(LONG *p_class_ref, HINSTANCE hInstance, REFCLSID rclsid) :
    _p_class_ref(p_class_ref),
    _hinstance(hInstance),
    _classid(rclsid),
    _inplace_picture(NULL)
{
    WNDCLASS wClass;

    if( ! GetClassInfo(hInstance, getInPlaceWndClassName(), &wClass) )
    {
        wClass.style          = CS_NOCLOSE|CS_DBLCLKS;
        wClass.lpfnWndProc    = VLCInPlaceClassWndProc;
        wClass.cbClsExtra     = 0;
        wClass.cbWndExtra     = 0;
        wClass.hInstance      = hInstance;
        wClass.hIcon          = NULL;
        wClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wClass.hbrBackground  = NULL;
        wClass.lpszMenuName   = NULL;
        wClass.lpszClassName  = getInPlaceWndClassName();

        _inplace_wndclass_atom = RegisterClass(&wClass);
    }
    else
    {
        _inplace_wndclass_atom = 0;
    }

    HBITMAP hbitmap = (HBITMAP)LoadImage(getHInstance(), MAKEINTRESOURCE(2), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
    if( NULL != hbitmap )
    {
        PICTDESC pictDesc;

        pictDesc.cbSizeofstruct = sizeof(PICTDESC);
        pictDesc.picType        = PICTYPE_BITMAP;
        pictDesc.bmp.hbitmap    = hbitmap;
        pictDesc.bmp.hpal       = NULL;

        if( FAILED(OleCreatePictureIndirect(&pictDesc, IID_IPicture, TRUE, reinterpret_cast<LPVOID*>(&_inplace_picture))) )
            _inplace_picture = NULL;
    }
    AddRef();
};

VLCPluginClass::~VLCPluginClass()
{
    if( 0 != _inplace_wndclass_atom )
        UnregisterClass(MAKEINTATOM(_inplace_wndclass_atom), _hinstance);

    if( NULL != _inplace_picture )
        _inplace_picture->Release();
};

STDMETHODIMP VLCPluginClass::QueryInterface(REFIID riid, void **ppv)
{
    if( NULL == ppv )
        return E_INVALIDARG;

    if( (IID_IUnknown == riid)
     || (IID_IClassFactory == riid) )
    {
        AddRef();
        *ppv = reinterpret_cast<LPVOID>(this);

        return NOERROR;
    }

    *ppv = NULL;

    return E_NOINTERFACE;
};

STDMETHODIMP_(ULONG) VLCPluginClass::AddRef(void)
{
    return InterlockedIncrement(_p_class_ref);
};

STDMETHODIMP_(ULONG) VLCPluginClass::Release(void)
{
    ULONG refcount = InterlockedDecrement(_p_class_ref);
    if( 0 == refcount )
    {
        delete this;
        return 0;
    }
    return refcount;
};

STDMETHODIMP VLCPluginClass::CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void **ppv)
{
    if( NULL == ppv )
        return E_POINTER;

    *ppv = NULL;

    if( (NULL != pUnkOuter) && (IID_IUnknown != riid) ) {
        return CLASS_E_NOAGGREGATION;
    }

    VLCPlugin *plugin = new VLCPlugin(this, pUnkOuter);
    if( NULL != plugin )
    {
        HRESULT hr = plugin->QueryInterface(riid, ppv);
        // the following will destroy the object if QueryInterface() failed
        plugin->Release();
        return hr;
    }
    return E_OUTOFMEMORY;
};

STDMETHODIMP VLCPluginClass::LockServer(BOOL fLock)
{
    if( fLock )
        AddRef();
    else
        Release();

    return S_OK;
};

////////////////////////////////////////////////////////////////////////

VLCPlugin::VLCPlugin(VLCPluginClass *p_class, LPUNKNOWN pUnkOuter) :
    _inplacewnd(NULL),
    _p_class(p_class),
    _i_ref(1UL),
    _p_libvlc(NULL),
    _p_mlist(NULL),
    _p_mplayer(NULL),
    _i_midx(-1),
    _i_codepage(CP_ACP),
    _b_usermode(TRUE)
{
    p_class->AddRef();

    vlcOleControl = new VLCOleControl(this);
    vlcOleInPlaceObject = new VLCOleInPlaceObject(this);
    vlcOleInPlaceActiveObject = new VLCOleInPlaceActiveObject(this);
    vlcPersistStorage = new VLCPersistStorage(this);
    vlcPersistStreamInit = new VLCPersistStreamInit(this);
    vlcPersistPropertyBag = new VLCPersistPropertyBag(this);
    vlcProvideClassInfo = new VLCProvideClassInfo(this);
    vlcConnectionPointContainer = new VLCConnectionPointContainer(this);
    vlcObjectSafety = new VLCObjectSafety(this);
    vlcControl = new VLCControl(this);
    vlcControl2 = new VLCControl2(this);
    vlcViewObject = new VLCViewObject(this);
    vlcDataObject = new VLCDataObject(this);
    vlcOleObject = new VLCOleObject(this);
    vlcSupportErrorInfo = new VLCSupportErrorInfo(this);

    // configure controlling IUnknown interface for implemented interfaces
    this->pUnkOuter = (NULL != pUnkOuter) ? pUnkOuter : dynamic_cast<LPUNKNOWN>(this);

    // default picure
    _p_pict = p_class->getInPlacePict();

    // make sure that persistable properties are initialized
    onInit();
};

VLCPlugin::~VLCPlugin()
{
    /*
    ** bump refcount to avoid recursive release from
    ** following interfaces when releasing this interface
    */
    AddRef();

    delete vlcSupportErrorInfo;
    delete vlcOleObject;
    delete vlcDataObject;
    delete vlcViewObject;
    delete vlcControl2;
    delete vlcControl;
    delete vlcConnectionPointContainer;
    delete vlcProvideClassInfo;
    delete vlcPersistPropertyBag;
    delete vlcPersistStreamInit;
    delete vlcPersistStorage;
    delete vlcOleInPlaceActiveObject;
    delete vlcOleInPlaceObject;
    delete vlcObjectSafety;

    delete vlcOleControl;
    if( _p_pict )
        _p_pict->Release();

    SysFreeString(_bstr_mrl);
    SysFreeString(_bstr_baseurl);

    if( _p_mplayer ) { libvlc_media_player_release(_p_mplayer); _p_mplayer=NULL; }
    if( _p_mlist )   { libvlc_media_list_release(_p_mlist); _p_mlist=NULL; }
    if( _p_libvlc )  { libvlc_release(_p_libvlc); _p_libvlc=NULL; }

    _p_class->Release();
};

STDMETHODIMP VLCPlugin::QueryInterface(REFIID riid, void **ppv)
{
    if( NULL == ppv )
        return E_INVALIDARG;

    if( IID_IUnknown == riid )
        *ppv = reinterpret_cast<LPVOID>(this);
    else if( IID_IOleObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleObject);
    else if( IID_IOleControl == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleControl);
    else if( IID_IOleWindow == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleInPlaceObject);
    else if( IID_IOleInPlaceObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleInPlaceObject);
    else if( IID_IOleInPlaceActiveObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleInPlaceActiveObject);
    else if( IID_IPersist == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcPersistStreamInit);
    else if( IID_IPersistStreamInit == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcPersistStreamInit);
    else if( IID_IPersistStorage == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcPersistStorage);
    else if( IID_IPersistPropertyBag == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcPersistPropertyBag);
    else if( IID_IProvideClassInfo == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcProvideClassInfo);
    else if( IID_IProvideClassInfo2 == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcProvideClassInfo);
    else if( IID_IConnectionPointContainer == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcConnectionPointContainer);
    else if( IID_IObjectSafety == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcObjectSafety);
    else if( IID_IDispatch == riid )
        *ppv = (CLSID_VLCPlugin2 == getClassID()) ?
                reinterpret_cast<LPVOID>(vlcControl2) :
                reinterpret_cast<LPVOID>(vlcControl);
    else if( IID_IVLCControl == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcControl);
    else if( IID_IVLCControl2 == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcControl2);
    else if( IID_IViewObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcViewObject);
    else if( IID_IViewObject2 == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcViewObject);
    else if( IID_IDataObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcDataObject);
    else if( IID_ISupportErrorInfo == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcSupportErrorInfo);
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    ((LPUNKNOWN)*ppv)->AddRef();
    return NOERROR;
};

STDMETHODIMP_(ULONG) VLCPlugin::AddRef(void)
{
    return InterlockedIncrement((LONG *)&_i_ref);
};

STDMETHODIMP_(ULONG) VLCPlugin::Release(void)
{
    if( ! InterlockedDecrement((LONG *)&_i_ref) )
    {
        delete this;
        return 0;
    }
    return _i_ref;
};

//////////////////////////////////////

HRESULT VLCPlugin::onInit(void)
{
    if( NULL == _p_libvlc )
    {
        // initialize persistable properties
        _b_autoplay   = TRUE;
        _b_autoloop   = FALSE;
        _b_toolbar    = FALSE;
        _bstr_baseurl = NULL;
        _bstr_mrl     = NULL;
        _b_visible    = TRUE;
        _b_mute       = FALSE;
        _i_volume     = 50;
        _i_time       = 0;
        _i_backcolor  = 0;
        // set default/preferred size (320x240) pixels in HIMETRIC
        HDC hDC = CreateDevDC(NULL);
        _extent.cx = 320;
        _extent.cy = 240;
        HimetricFromDP(hDC, (LPPOINT)&_extent, 1);
        DeleteDC(hDC);

        return S_OK;
    }
    return CO_E_ALREADYINITIALIZED;
};

HRESULT VLCPlugin::onLoad(void)
{
    if( SysStringLen(_bstr_baseurl) == 0 )
    {
        /*
        ** try to retreive the base URL using the client site moniker, which for Internet Explorer
        ** is the URL of the page the plugin is embedded into.
        */
        LPOLECLIENTSITE pClientSite;
        if( SUCCEEDED(vlcOleObject->GetClientSite(&pClientSite)) && (NULL != pClientSite) )
        {
            IBindCtx *pBC = 0;
            if( SUCCEEDED(CreateBindCtx(0, &pBC)) )
            {
                LPMONIKER pContMoniker = NULL;
                if( SUCCEEDED(pClientSite->GetMoniker(OLEGETMONIKER_ONLYIFTHERE,
                                OLEWHICHMK_CONTAINER, &pContMoniker)) )
                {
                    LPOLESTR base_url;
                    if( SUCCEEDED(pContMoniker->GetDisplayName(pBC, NULL, &base_url)) )
                    {
                        /*
                        ** check that the moniker name is a URL
                        */
                        if( UrlIsW(base_url, URLIS_URL) )
                        {
                            /* copy base URL */
                            _bstr_baseurl = SysAllocString(base_url);
                        }
                        CoTaskMemFree(base_url);
                    }
                }
            }
        }
    }
    setDirty(FALSE);
    return S_OK;
};


void VLCPlugin::initVLC()
{
    extern HMODULE DllGetModule();

    /*
    ** default initialization options
    */
    const char *ppsz_argv[32] = { };
    int   ppsz_argc = 0;

    char p_progpath[MAX_PATH];
    {
        TCHAR w_progpath[MAX_PATH];
        DWORD len = GetModuleFileName(DllGetModule(), w_progpath, MAX_PATH);
        if( len > 0 )
        {
            len = WideCharToMultiByte(CP_UTF8, 0, w_progpath, len, p_progpath,
                       sizeof(p_progpath)-1, NULL, NULL);
            if( len > 0 )
            {
                p_progpath[len] = '\0';
                ppsz_argv[0] = p_progpath;
            }
        }
    }

    ppsz_argv[ppsz_argc++] = "-vv";

    HKEY h_key;
    char p_pluginpath[MAX_PATH];
    if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, TEXT("Software\\VideoLAN\\VLC"),
                      0, KEY_READ, &h_key ) == ERROR_SUCCESS )
    {
        DWORD i_type, i_data = MAX_PATH;
        TCHAR w_pluginpath[MAX_PATH];
        if( RegQueryValueEx( h_key, TEXT("InstallDir"), 0, &i_type,
                             (LPBYTE)w_pluginpath, &i_data ) == ERROR_SUCCESS )
        {
            if( i_type == REG_SZ )
            {
                if( WideCharToMultiByte(CP_UTF8, 0, w_pluginpath, -1, p_pluginpath,
                         sizeof(p_pluginpath)-sizeof("\\plugins")+1, NULL, NULL) )
                {
                    strcat( p_pluginpath, "\\plugins" );
                    ppsz_argv[ppsz_argc++] = "--plugin-path";
                    ppsz_argv[ppsz_argc++] = p_pluginpath;
                }
            }
        }
        RegCloseKey( h_key );
    }

    // make sure plugin isn't affected with VLC single instance mode
    ppsz_argv[ppsz_argc++] = "--no-one-instance";

    /* common settings */
    ppsz_argv[ppsz_argc++] = "--no-stats";
    ppsz_argv[ppsz_argc++] = "--no-media-library";
    ppsz_argv[ppsz_argc++] = "--ignore-config";
    ppsz_argv[ppsz_argc++] = "--intf=dummy";

    // loop mode is a configuration option only
    if( _b_autoloop )
        ppsz_argv[ppsz_argc++] = "--loop";

    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    _p_libvlc = libvlc_new(ppsz_argc, ppsz_argv, &ex);
    if( libvlc_exception_raised(&ex) )
        return;

    _p_mlist = libvlc_media_list_new(_p_libvlc, &ex);
    if( libvlc_exception_raised(&ex) )
    {
        libvlc_release(_p_libvlc);
        return;
    }

    // initial volume setting
    libvlc_audio_set_volume(_p_libvlc, _i_volume, NULL);
    if( _b_mute )
    {
        libvlc_audio_set_mute(_p_libvlc, TRUE, NULL);
    }

    // initial playlist item
    if( SysStringLen(_bstr_mrl) > 0 )
    {
        char *psz_mrl = NULL;

        if( SysStringLen(_bstr_baseurl) > 0 )
        {
            /*
            ** if the MRL a relative URL, we should end up with an absolute URL
            */
            LPWSTR abs_url = CombineURL(_bstr_baseurl, _bstr_mrl);
            if( NULL != abs_url )
            {
                psz_mrl = CStrFromWSTR(CP_UTF8, abs_url, wcslen(abs_url));
                CoTaskMemFree(abs_url);
            }
            else
            {
                psz_mrl = CStrFromBSTR(CP_UTF8, _bstr_mrl);
            }
        }
        else
        {
            /*
            ** baseURL is empty, assume MRL is absolute
            */
            psz_mrl = CStrFromBSTR(CP_UTF8, _bstr_mrl);
        }
        if( NULL != psz_mrl )
        {
            const char *options[1];
            int i_options = 0;

            char timeBuffer[32];
            if( _i_time )
            {
                snprintf(timeBuffer, sizeof(timeBuffer), ":start-time=%d", _i_time);
                options[i_options++] = timeBuffer;
            }
            // add default target to playlist
            playlist_add_extended_untrusted(psz_mrl, i_options, options, NULL);
            CoTaskMemFree(psz_mrl);
        }
    }
};

void VLCPlugin::setErrorInfo(REFIID riid, const char *description)
{
    vlcSupportErrorInfo->setErrorInfo( getClassID() == CLSID_VLCPlugin2 ?
        OLESTR("VideoLAN.VLCPlugin.2") : OLESTR("VideoLAN.VLCPlugin.1"),
        riid, description );
};

HRESULT VLCPlugin::onAmbientChanged(LPUNKNOWN pContainer, DISPID dispID)
{
    VARIANT v;
    switch( dispID )
    {
        case DISPID_AMBIENT_BACKCOLOR:
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(GetObjectProperty(pContainer, dispID, v)) )
            {
                setBackColor(V_I4(&v));
            }
            break;
        case DISPID_AMBIENT_DISPLAYNAME:
            break;
        case DISPID_AMBIENT_FONT:
            break;
        case DISPID_AMBIENT_FORECOLOR:
            break;
        case DISPID_AMBIENT_LOCALEID:
            break;
        case DISPID_AMBIENT_MESSAGEREFLECT:
            break;
        case DISPID_AMBIENT_SCALEUNITS:
            break;
        case DISPID_AMBIENT_TEXTALIGN:
            break;
        case DISPID_AMBIENT_USERMODE:
            VariantInit(&v);
            V_VT(&v) = VT_BOOL;
            if( SUCCEEDED(GetObjectProperty(pContainer, dispID, v)) )
            {
                setUserMode(V_BOOL(&v) != VARIANT_FALSE);
            }
            break;
        case DISPID_AMBIENT_UIDEAD:
            break;
        case DISPID_AMBIENT_SHOWGRABHANDLES:
            break;
        case DISPID_AMBIENT_SHOWHATCHING:
            break;
        case DISPID_AMBIENT_DISPLAYASDEFAULT:
            break;
        case DISPID_AMBIENT_SUPPORTSMNEMONICS:
            break;
        case DISPID_AMBIENT_AUTOCLIP:
            break;
        case DISPID_AMBIENT_APPEARANCE:
            break;
        case DISPID_AMBIENT_CODEPAGE:
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(GetObjectProperty(pContainer, dispID, v)) )
            {
                setCodePage(V_I4(&v));
            }
            break;
        case DISPID_AMBIENT_PALETTE:
            break;
        case DISPID_AMBIENT_CHARSET:
            break;
        case DISPID_AMBIENT_RIGHTTOLEFT:
            break;
        case DISPID_AMBIENT_TOPTOBOTTOM:
            break;
        case DISPID_UNKNOWN:
            /*
            ** multiple property change, look up the ones we are interested in
            */
            VariantInit(&v);
            V_VT(&v) = VT_BOOL;
            if( SUCCEEDED(GetObjectProperty(pContainer, DISPID_AMBIENT_USERMODE, v)) )
            {
                setUserMode(V_BOOL(&v) != VARIANT_FALSE);
            }
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(GetObjectProperty(pContainer, DISPID_AMBIENT_CODEPAGE, v)) )
            {
                setCodePage(V_I4(&v));
            }
            break;
    }
    return S_OK;
};

HRESULT VLCPlugin::onClose(DWORD dwSaveOption)
{
    if( isInPlaceActive() )
    {
        onInPlaceDeactivate();
    }
    if( isRunning() )
    {
        libvlc_instance_t* p_libvlc = _p_libvlc;

        IVLCLog *p_log;
        if( SUCCEEDED(vlcControl2->get_log(&p_log)) )
        {
            // make sure the log is disabled
            p_log->put_verbosity(-1);
            p_log->Release();
        }

        _p_libvlc = NULL;
        vlcDataObject->onClose();

        if( p_libvlc )
            libvlc_release(p_libvlc);
    }
    return S_OK;
};

BOOL VLCPlugin::isInPlaceActive(void)
{
    return (NULL != _inplacewnd);
};

HRESULT VLCPlugin::onActivateInPlace(LPMSG lpMesg, HWND hwndParent, LPCRECT lprcPosRect, LPCRECT lprcClipRect)
{
    RECT clipRect = *lprcClipRect;
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    /*
    ** record keeping of control geometry within container
    */
    _posRect = *lprcPosRect;

    /*
    ** Create a window for in place activated control.
    ** the window geometry matches the control viewport
    ** within container so that embedded video is always
    ** properly displayed.
    */
    _inplacewnd = CreateWindow(_p_class->getInPlaceWndClassName(),
            TEXT("VLC Plugin In-Place Window"),
            WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
            lprcPosRect->left,
            lprcPosRect->top,
            lprcPosRect->right-lprcPosRect->left,
            lprcPosRect->bottom-lprcPosRect->top,
            hwndParent,
            0,
            _p_class->getHInstance(),
            NULL
           );

    if( NULL == _inplacewnd )
        return E_FAIL;

    SetWindowLongPtr(_inplacewnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    /* change cliprect coordinates system relative to window bounding rect */
    OffsetRect(&clipRect, -lprcPosRect->left, -lprcPosRect->top);

    HRGN clipRgn = CreateRectRgnIndirect(&clipRect);
    SetWindowRgn(_inplacewnd, clipRgn, TRUE);

    if( _b_usermode )
    {
        /* will run vlc if not done already */
        libvlc_instance_t* p_libvlc;
        HRESULT result = getVLC(&p_libvlc);
        if( FAILED(result) )
            return result;

        /* set internal video width and height */
        libvlc_video_set_size(p_libvlc,
            lprcPosRect->right-lprcPosRect->left,
            lprcPosRect->bottom-lprcPosRect->top,
            NULL );

        /* set internal video parent window */
        libvlc_video_set_parent(p_libvlc,
            reinterpret_cast<libvlc_drawable_t>(_inplacewnd), NULL);

        if( _b_autoplay && playlist_select(0,NULL) )
        {
            libvlc_media_player_play(_p_mplayer,NULL);
            fireOnPlayEvent();
        }
    }

    if( isVisible() )
        ShowWindow(_inplacewnd, SW_SHOW);

    return S_OK;
};

HRESULT VLCPlugin::onInPlaceDeactivate(void)
{
    if( isPlaying(NULL) )
    {
        playlist_stop(NULL);
        fireOnStopEvent();
    }

    DestroyWindow(_inplacewnd);
    _inplacewnd = NULL;

    return S_OK;
};

void VLCPlugin::setVisible(BOOL fVisible)
{
    if( fVisible != _b_visible )
    {
        _b_visible = fVisible;
        if( isInPlaceActive() )
        {
            ShowWindow(_inplacewnd, fVisible ? SW_SHOW : SW_HIDE);
            if( fVisible )
                InvalidateRect(_inplacewnd, NULL, TRUE);
        }
        setDirty(TRUE);
        firePropChangedEvent(DISPID_Visible);
    }
};

void VLCPlugin::setVolume(int volume)
{
    if( volume < 0 )
        volume = 0;
    else if( volume > 200 )
        volume = 200;

    if( volume != _i_volume )
    {
        _i_volume = volume;
        if( isRunning() )
        {
            libvlc_audio_set_volume(_p_libvlc, _i_volume, NULL);
        }
        setDirty(TRUE);
    }
};

void VLCPlugin::setBackColor(OLE_COLOR backcolor)
{
    if( _i_backcolor != backcolor )
    {
        _i_backcolor = backcolor;
        if( isInPlaceActive() )
        {

        }
        setDirty(TRUE);
    }
};

void VLCPlugin::setTime(int seconds)
{
    if( seconds < 0 )
        seconds = 0;

    if( seconds != _i_time )
    {
        setStartTime(_i_time);
        if( NULL != _p_mplayer )
        {
            libvlc_media_player_set_time(_p_mplayer, _i_time, NULL);
        }
    }
};

void VLCPlugin::setFocus(BOOL fFocus)
{
    if( fFocus )
        SetActiveWindow(_inplacewnd);
};

BOOL VLCPlugin::hasFocus(void)
{
    return GetActiveWindow() == _inplacewnd;
};

void VLCPlugin::onDraw(DVTARGETDEVICE * ptd, HDC hicTargetDev,
        HDC hdcDraw, LPCRECTL lprcBounds, LPCRECTL lprcWBounds)
{
    if( isVisible() )
    {
        long width = lprcBounds->right-lprcBounds->left;
        long height = lprcBounds->bottom-lprcBounds->top;

        RECT bounds = { lprcBounds->left, lprcBounds->top, lprcBounds->right, lprcBounds->bottom };

        if( isUserMode() )
        {
            /* VLC is in user mode, just draw background color */
            COLORREF colorref = RGB(0, 0, 0);
            OleTranslateColor(_i_backcolor, (HPALETTE)GetStockObject(DEFAULT_PALETTE), &colorref);
            if( colorref != RGB(0, 0, 0) )
            {
                /* custom background */
                HBRUSH colorbrush = CreateSolidBrush(colorref);
                FillRect(hdcDraw, &bounds, colorbrush);
                DeleteObject((HANDLE)colorbrush);
            }
            else
            {
                /* black background */
                FillRect(hdcDraw, &bounds, (HBRUSH)GetStockObject(BLACK_BRUSH));
            }
        }
        else
        {
            /* VLC is in design mode, draw the VLC logo */
            FillRect(hdcDraw, &bounds, (HBRUSH)GetStockObject(WHITE_BRUSH));

            LPPICTURE pict = getPicture();
            if( NULL != pict )
            {
                OLE_XSIZE_HIMETRIC picWidth;
                OLE_YSIZE_HIMETRIC picHeight;

                pict->get_Width(&picWidth);
                pict->get_Height(&picHeight);

                SIZEL picSize = { picWidth, picHeight };

                if( NULL != hicTargetDev )
                {
                    DPFromHimetric(hicTargetDev, (LPPOINT)&picSize, 1);
                }
                else if( NULL != (hicTargetDev = CreateDevDC(ptd)) )
                {
                    DPFromHimetric(hicTargetDev, (LPPOINT)&picSize, 1);
                    DeleteDC(hicTargetDev);
                }

                if( picSize.cx > width-4 )
                    picSize.cx = width-4;
                if( picSize.cy > height-4 )
                    picSize.cy = height-4;

                LONG dstX = lprcBounds->left+(width-picSize.cx)/2;
                LONG dstY = lprcBounds->top+(height-picSize.cy)/2;

                if( NULL != lprcWBounds )
                {
                    RECT wBounds = { lprcWBounds->left, lprcWBounds->top, lprcWBounds->right, lprcWBounds->bottom };
                    pict->Render(hdcDraw, dstX, dstY, picSize.cx, picSize.cy,
                            0L, picHeight, picWidth, -picHeight, &wBounds);
                }
                else
                    pict->Render(hdcDraw, dstX, dstY, picSize.cx, picSize.cy,
                            0L, picHeight, picWidth, -picHeight, NULL);

                pict->Release();
            }

            SelectObject(hdcDraw, GetStockObject(BLACK_BRUSH));

            MoveToEx(hdcDraw, bounds.left, bounds.top, NULL);
            LineTo(hdcDraw, bounds.left+width-1, bounds.top);
            LineTo(hdcDraw, bounds.left+width-1, bounds.top+height-1);
            LineTo(hdcDraw, bounds.left, bounds.top+height-1);
            LineTo(hdcDraw, bounds.left, bounds.top);
        }
    }
};

void VLCPlugin::onPaint(HDC hdc, const RECT &bounds, const RECT &clipRect)
{
    if( isVisible() )
    {
        /* if VLC is in design mode, draw control logo */
        HDC hdcDraw = CreateCompatibleDC(hdc);
        if( NULL != hdcDraw )
        {
            SIZEL size = getExtent();
            DPFromHimetric(hdc, (LPPOINT)&size, 1);
            RECTL posRect = { 0, 0, size.cx, size.cy };

            int width = bounds.right-bounds.left;
            int height = bounds.bottom-bounds.top;

            HBITMAP hBitmap = CreateCompatibleBitmap(hdc, width, height);
            if( NULL != hBitmap )
            {
                HBITMAP oldBmp = (HBITMAP)SelectObject(hdcDraw, hBitmap);

                if( (size.cx != width) || (size.cy != height) )
                {
                    // needs to scale canvas
                    SetMapMode(hdcDraw, MM_ANISOTROPIC);
                    SetWindowExtEx(hdcDraw, size.cx, size.cy, NULL);
                    SetViewportExtEx(hdcDraw, width, height, NULL);
                }

                onDraw(NULL, hdc, hdcDraw, &posRect, NULL);

                SetMapMode(hdcDraw, MM_TEXT);
                BitBlt(hdc, bounds.left, bounds.top,
                        width, height,
                        hdcDraw, 0, 0,
                        SRCCOPY);

                SelectObject(hdcDraw, oldBmp);
                DeleteObject(hBitmap);
            }
            DeleteDC(hdcDraw);
        }
    }
};

void VLCPlugin::onPositionChange(LPCRECT lprcPosRect, LPCRECT lprcClipRect)
{
    RECT clipRect = *lprcClipRect;

    //RedrawWindow(GetParent(_inplacewnd), &_posRect, NULL, RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);

    /*
    ** record keeping of control geometry within container
    */
    _posRect = *lprcPosRect;

    /*
    ** change in-place window geometry to match clipping region
    */
    SetWindowPos(_inplacewnd, NULL,
            lprcPosRect->left,
            lprcPosRect->top,
            lprcPosRect->right-lprcPosRect->left,
            lprcPosRect->bottom-lprcPosRect->top,
            SWP_NOACTIVATE|
            SWP_NOCOPYBITS|
            SWP_NOZORDER|
            SWP_NOOWNERZORDER );

    /* change cliprect coordinates system relative to window bounding rect */
    OffsetRect(&clipRect, -lprcPosRect->left, -lprcPosRect->top);
    HRGN clipRgn = CreateRectRgnIndirect(&clipRect);
    SetWindowRgn(_inplacewnd, clipRgn, FALSE);

    //RedrawWindow(_videownd, &posRect, NULL, RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    if( isRunning() )
    {
        libvlc_video_set_size(_p_libvlc,
            lprcPosRect->right-lprcPosRect->left,
            lprcPosRect->bottom-lprcPosRect->top,
            NULL );
    }
};

void VLCPlugin::freezeEvents(BOOL freeze)
{
    vlcConnectionPointContainer->freezeEvents(freeze);
};

void VLCPlugin::firePropChangedEvent(DISPID dispid)
{
    vlcConnectionPointContainer->firePropChangedEvent(dispid);
};

void VLCPlugin::fireOnPlayEvent(void)
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_PlayEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnPauseEvent(void)
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_PauseEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnStopEvent(void)
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_StopEvent, &dispparamsNoArgs);
};

bool VLCPlugin::playlist_select( int idx, libvlc_exception_t *ex )
{
    libvlc_media_t *p_m = NULL;

    libvlc_media_list_lock(_p_mlist);

    int count = libvlc_media_list_count(_p_mlist,ex);
    if( libvlc_exception_raised(ex) )
        goto bad_unlock;

    if( idx<0||idx>=count )
        goto bad_unlock;

    _i_midx = idx;

    p_m = libvlc_media_list_item_at_index(_p_mlist,_i_midx,ex);
    libvlc_media_list_unlock(_p_mlist);

    if( libvlc_exception_raised(ex) )
        return false;

    if( _p_mplayer )
    {
        libvlc_media_player_release( _p_mplayer );
        _p_mplayer = NULL;
    }

    _p_mplayer = libvlc_media_player_new_from_media(p_m,ex);
    if( _p_mplayer )
        set_player_window(ex);

    libvlc_media_release( p_m );
    return !libvlc_exception_raised(ex);

bad_unlock:
    libvlc_media_list_unlock(_p_mlist);
    return false;
}

void VLCPlugin::set_player_window(libvlc_exception_t *ex)
{
    // XXX FIXME no idea if this is correct or not
    libvlc_media_player_set_hwnd(_p_mplayer,getInPlaceWindow(),ex);
}

int  VLCPlugin::playlist_add_extended_untrusted(const char *mrl, int optc, const char **optv, libvlc_exception_t *ex)
{
    int item = -1;
    libvlc_media_t *p_m = libvlc_media_new(_p_libvlc,mrl,ex);
    if( libvlc_exception_raised(ex) )
        return -1;

    for( int i = 0; i < optc; ++i )
    {
        libvlc_media_add_option_untrusted(p_m, optv[i],ex);
        if( libvlc_exception_raised(ex) )
        {
            libvlc_media_release(p_m);
            return -1;
        }
    }

    libvlc_media_list_lock(_p_mlist);
    libvlc_media_list_add_media(_p_mlist,p_m,ex);
    if( !libvlc_exception_raised(ex) )
        item = libvlc_media_list_count(_p_mlist,ex)-1;
    libvlc_media_list_unlock(_p_mlist);
    libvlc_media_release(p_m);

    return item;
}


