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

#include "utils.h"

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
    switch( uMsg )
    {
        case WM_ERASEBKGND:
            return 1L;

        case WM_PAINT:
            PAINTSTRUCT ps;
            if( GetUpdateRect(hWnd, NULL, FALSE) )
            {
                BeginPaint(hWnd, &ps);
                EndPaint(hWnd, &ps);
            }
            return 0L;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
};

static LRESULT CALLBACK VLCVideoClassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

    if( ! GetClassInfo(hInstance, getVideoWndClassName(), &wClass) )
    {
        wClass.style          = CS_NOCLOSE;
        wClass.lpfnWndProc    = VLCVideoClassWndProc;
        wClass.cbClsExtra     = 0;
        wClass.cbWndExtra     = 0;
        wClass.hInstance      = hInstance;
        wClass.hIcon          = NULL;
        wClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wClass.hbrBackground  = NULL;
        wClass.lpszMenuName   = NULL;
        wClass.lpszClassName  = getVideoWndClassName();
       
        _video_wndclass_atom = RegisterClass(&wClass);
    }
    else
    {
        _video_wndclass_atom = 0;
    }

    HBITMAP hbitmap = (HBITMAP)LoadImage(getHInstance(), TEXT("INPLACE-PICT"), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
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

    if( 0 != _video_wndclass_atom )
        UnregisterClass(MAKEINTATOM(_video_wndclass_atom), _hinstance);

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
    _videownd(NULL),
    _p_class(p_class),
    _i_ref(1UL),
    _p_libvlc(NULL),
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

    // configure controlling IUnknown interface for implemented interfaces
    this->pUnkOuter = (NULL != pUnkOuter) ? pUnkOuter : dynamic_cast<LPUNKNOWN>(this);

    // default picure
    _p_pict = p_class->getInPlacePict();

    // make sure that persistable properties are initialized
    onInit();
};

VLCPlugin::~VLCPlugin()
{
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

/*
** we use a window to represent plugin viewport,
** whose geometry is limited by the clipping rectangle
** all drawing within this window must follow must
** follow coordinates system described in lprPosRect
*/

static void getViewportCoords(LPRECT lprPosRect, LPRECT lprClipRect)
{
    RECT bounds;
    bounds.right  = lprPosRect->right-lprPosRect->left;

    if( lprClipRect->left <= lprPosRect->left )
    {
        // left side is not clipped out
        bounds.left = 0;

        if( lprClipRect->right >= lprPosRect->right )
        {
            // right side is not clipped out, no change
        }
        else if( lprClipRect->right >= lprPosRect->left )
        {
            // right side is clipped out
            lprPosRect->right = lprClipRect->right;
        }
        else
        {
            // outside of clipping rectange, not visible
            lprPosRect->right = lprPosRect->left;
        }
    }
    else
    {
        // left side is clipped out
        bounds.left = lprPosRect->left-lprClipRect->left;
        bounds.right += bounds.left;

        lprPosRect->left = lprClipRect->left;
        if( lprClipRect->right >= lprPosRect->right )
        {
            // right side is not clipped out
        }
        else
        {
            // right side is clipped out
            lprPosRect->right = lprClipRect->right;
        }
    }

    bounds.bottom = lprPosRect->bottom-lprPosRect->top;

    if( lprClipRect->top <= lprPosRect->top )
    {
        // top side is not clipped out
        bounds.top = 0;

        if( lprClipRect->bottom >= lprPosRect->bottom )
        {
            // bottom side is not clipped out, no change
        }
        else if( lprClipRect->bottom >= lprPosRect->top )
        {
            // bottom side is clipped out
            lprPosRect->bottom = lprClipRect->bottom;
        }
        else
        {
            // outside of clipping rectange, not visible
            lprPosRect->right = lprPosRect->left;
        }
    }
    else
    {
        bounds.top = lprPosRect->top-lprClipRect->top;
        bounds.bottom += bounds.top;

        lprPosRect->top = lprClipRect->top;
        if( lprClipRect->bottom >= lprPosRect->bottom )
        {
            // bottom side is not clipped out
        }
        else
        {
            // bottom side is clipped out
            lprPosRect->bottom = lprClipRect->bottom;
        }
    }
    *lprClipRect = *lprPosRect;
    *lprPosRect  = bounds;
};

HRESULT VLCPlugin::onInit(void)
{
    if( NULL == _p_libvlc )
    {
        // initialize persistable properties
        _b_autoplay = TRUE;
        _b_autoloop = FALSE;
        _bstr_baseurl = NULL;
        _bstr_mrl = NULL;
        _b_visible = TRUE;
        _b_mute = FALSE;
        _i_volume = 50;
        _i_time   = 0;
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

HRESULT VLCPlugin::getVLCObject(int* i_vlc)
{
    libvlc_instance_t *p_libvlc;
    HRESULT result = getVLC(&p_libvlc);
    if( SUCCEEDED(result) )
    {
        *i_vlc = libvlc_get_vlc_id(p_libvlc);
    }
    else
    {
        *i_vlc = 0;
    }
    return result;
}

HRESULT VLCPlugin::getVLC(libvlc_instance_t** pp_libvlc)
{
    if( ! isRunning() )
    {
        /*
        ** default initialization options
        */
        char *ppsz_argv[32] = { "vlc" };
        int   ppsz_argc = 1;

        HKEY h_key;
        DWORD i_type, i_data = MAX_PATH + 1;
        char p_data[MAX_PATH + 1];
        if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, "Software\\VideoLAN\\VLC",
                          0, KEY_READ, &h_key ) == ERROR_SUCCESS )
        {
             if( RegQueryValueEx( h_key, "InstallDir", 0, &i_type,
                                  (LPBYTE)p_data, &i_data ) == ERROR_SUCCESS )
             {
                 if( i_type == REG_SZ )
                 {
                     strcat( p_data, "\\vlc" );
                     ppsz_argv[0] = p_data;
                 }
             }
             RegCloseKey( h_key );
        }

#if 0
        ppsz_argv[0] = "C:\\cygwin\\home\\damienf\\vlc-trunk\\vlc";
#endif

        // make sure plugin isn't affected with VLC single instance mode
        ppsz_argv[ppsz_argc++] = "--no-one-instance";

        /* common settings */
        ppsz_argv[ppsz_argc++] = "-vv";
        ppsz_argv[ppsz_argc++] = "--no-stats";
        ppsz_argv[ppsz_argc++] = "--no-media-library";
        ppsz_argv[ppsz_argc++] = "--intf";
        ppsz_argv[ppsz_argc++] = "dummy";

        // loop mode is a configuration option only
        if( _b_autoloop )
            ppsz_argv[ppsz_argc++] = "--loop";

        // initial volume setting
        char volBuffer[16];
        ppsz_argv[ppsz_argc++] = "--volume";
        if( _b_mute )
        {
           ppsz_argv[ppsz_argc++] = "0";
        }
        else
        {
            snprintf(volBuffer, sizeof(volBuffer), "%d", _i_volume);
            ppsz_argv[ppsz_argc++] = volBuffer;
        }
            
        if( IsDebuggerPresent() )
        {
            /*
            ** VLC default threading mechanism is designed to be as compatible
            ** with POSIX as possible. However when debugged on win32, threads
            ** lose signals and eventually VLC get stuck during initialization.
            ** threading support can be configured to be more debugging friendly
            ** but it will be less compatible with POSIX.
            ** This is done by initializing with the following options:
            */
            ppsz_argv[ppsz_argc++] = "--fast-mutex";
            ppsz_argv[ppsz_argc++] = "--win9x-cv-method=1";
        }

        _p_libvlc = libvlc_new(ppsz_argc, ppsz_argv, NULL);
        if( NULL == _p_libvlc )
        {
            *pp_libvlc = NULL;
            return E_FAIL;
        }

        if( SysStringLen(_bstr_mrl) > 0 )
        {
            char *psz_mrl = NULL;

            if( SysStringLen(_bstr_baseurl) > 0 )
            {
                DWORD len = INTERNET_MAX_URL_LENGTH;
                LPOLESTR abs_url = (LPOLESTR)CoTaskMemAlloc(sizeof(OLECHAR)*len);
                if( NULL != abs_url )
                {
                    /*
                    ** if the MRL a relative URL, we should end up with an absolute URL
                    */
                    if( SUCCEEDED(UrlCombineW(_bstr_baseurl, _bstr_mrl, abs_url, &len,
                                    URL_ESCAPE_UNSAFE|URL_PLUGGABLE_PROTOCOL)) )
                    {
                        psz_mrl = CStrFromBSTR(CP_UTF8, abs_url);
                    }
                    else
                    {
                        psz_mrl = CStrFromBSTR(CP_UTF8, _bstr_mrl);
                    }
                    CoTaskMemFree(abs_url);
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
                libvlc_playlist_add_extended(_p_libvlc, psz_mrl, NULL, i_options, options, NULL);
                CoTaskMemFree(psz_mrl);
            }
        }
    }
    *pp_libvlc = _p_libvlc;
    return S_OK;
};

HRESULT VLCPlugin::onAmbientChanged(LPUNKNOWN pContainer, DISPID dispID)
{
    VARIANT v;
    switch( dispID )
    {
        case DISPID_AMBIENT_BACKCOLOR:
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

        _p_libvlc = NULL;
        vlcDataObject->onClose();

        libvlc_destroy(p_libvlc, NULL );
    }
    return S_OK;
};

BOOL VLCPlugin::isInPlaceActive(void)
{
    return (NULL != _inplacewnd);
};

HRESULT VLCPlugin::onActivateInPlace(LPMSG lpMesg, HWND hwndParent, LPCRECT lprcPosRect, LPCRECT lprcClipRect)
{
    RECT posRect = *lprcPosRect;
    RECT clipRect = *lprcClipRect;

    /*
    ** record keeping of control geometry within container
    */ 
    _posRect = posRect;

    /*
    ** convert posRect & clipRect to match control viewport coordinates
    */
    getViewportCoords(&posRect, &clipRect);

    /*
    ** Create a window for in place activated control.
    ** the window geometry matches the control viewport
    ** within container so that embedded video is always
    ** properly clipped.
    */
    _inplacewnd = CreateWindow(_p_class->getInPlaceWndClassName(),
            "VLC Plugin In-Place Window",
            WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
            clipRect.left,
            clipRect.top,
            clipRect.right-clipRect.left,
            clipRect.bottom-clipRect.top,
            hwndParent,
            0,
            _p_class->getHInstance(),
            NULL
           );

    if( NULL == _inplacewnd )
        return E_FAIL;

    SetWindowLongPtr(_inplacewnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    /*
    ** VLC embedded video automatically grows to cover client
    ** area of parent window.
    ** hence create a such a 'parent' window whose geometry
    ** is always correct relative to the viewport bounds
    */
    _videownd = CreateWindow(_p_class->getVideoWndClassName(),
            "VLC Plugin Video Window",
            WS_CHILD|WS_CLIPCHILDREN|WS_VISIBLE,
            posRect.left,
            posRect.top,
            posRect.right-posRect.left,
            posRect.bottom-posRect.top,
            _inplacewnd,
            0,
            _p_class->getHInstance(),
            NULL
           );

    if( NULL == _videownd )
    {
        DestroyWindow(_inplacewnd);
        return E_FAIL;
    }

    SetWindowLongPtr(_videownd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    if( _b_usermode )
    {
        /* will run vlc if not done already */
        libvlc_instance_t* p_libvlc;
        HRESULT result = getVLC(&p_libvlc);
        if( FAILED(result) )
            return result;

        /* set internal video width and height */
        libvlc_video_set_size(p_libvlc,
            posRect.right-posRect.left,
            posRect.bottom-posRect.top,
            NULL );

        /* set internal video parent window */
        libvlc_video_set_parent(p_libvlc,
            reinterpret_cast<libvlc_drawable_t>(_videownd), NULL);

        if( _b_autoplay & (libvlc_playlist_items_count(p_libvlc, NULL) > 0) )
        {
            libvlc_playlist_play(p_libvlc, 0, 0, NULL, NULL);
            fireOnPlayEvent();
        }
    }

    if( isVisible() )
        ShowWindow(_inplacewnd, SW_SHOW);

    return S_OK;
};

HRESULT VLCPlugin::onInPlaceDeactivate(void)
{
    if( isRunning() )
    {
        libvlc_playlist_stop(_p_libvlc, NULL);
        fireOnStopEvent();
    }

    DestroyWindow(_videownd);
    _videownd = NULL;
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
                InvalidateRect(_videownd, NULL, TRUE);
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

void VLCPlugin::setTime(int seconds)
{
    if( seconds < 0 )
        seconds = 0;

    if( seconds != _i_time )
    {
        setStartTime(_i_time);
        if( isRunning() )
        {
            libvlc_input_t *p_input = libvlc_playlist_get_input(_p_libvlc, NULL);
            if( NULL != p_input )
            {
                libvlc_input_set_time(p_input, _i_time, NULL);
                libvlc_input_free(p_input);
            }
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
};

void VLCPlugin::onPaint(HDC hdc, const RECT &bounds, const RECT &clipRect)
{
    if( isVisible() )
    {
        /** if VLC is playing, it may not display any VIDEO content 
        ** hence, draw control logo*/
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
    RECT posRect  = *lprcPosRect;

    //RedrawWindow(GetParent(_inplacewnd), &_posRect, NULL, RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);

    /*
    ** record keeping of control geometry within container
    */
    _posRect = posRect;

    /*
    ** convert posRect & clipRect to match control viewport coordinates
    */
    getViewportCoords(&posRect, &clipRect);

    /*
    ** change in-place window geometry to match clipping region
    */
    SetWindowPos(_inplacewnd, NULL,
            clipRect.left,
            clipRect.top,
            clipRect.right-clipRect.left,
            clipRect.bottom-clipRect.top,
            SWP_NOACTIVATE|
            SWP_NOCOPYBITS|
            SWP_NOZORDER|
            SWP_NOOWNERZORDER );

    /*
    ** change video window geometry to match object bounds within clipping region
    */
    SetWindowPos(_videownd, NULL,
            posRect.left,
            posRect.top,
            posRect.right-posRect.left,
            posRect.bottom-posRect.top,
            SWP_NOACTIVATE|
            SWP_NOCOPYBITS|
            SWP_NOZORDER|
            SWP_NOOWNERZORDER );

    //RedrawWindow(_videownd, &posRect, NULL, RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    if( isRunning() )
    {
        libvlc_video_set_size(_p_libvlc,
            posRect.right-posRect.left,
            posRect.bottom-posRect.top,
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

