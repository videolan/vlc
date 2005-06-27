/*****************************************************************************
 * oleobject.cpp: ActiveX control for VLC
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

#include "plugin.h"
#include "oleobject.h"

#include "utils.h"

#include <docobj.h>

using namespace std;

VLCOleObject::VLCOleObject(VLCPlugin *p_instance) :
_p_clientsite(NULL), _p_instance(p_instance) 
{
    CreateOleAdviseHolder(&_p_advise_holder);
};

VLCOleObject::~VLCOleObject()
{
    _p_advise_holder->Release();
    SetClientSite(NULL); 
};

STDMETHODIMP VLCOleObject::Advise(IAdviseSink *pAdvSink, DWORD *dwConnection)
{
    return _p_advise_holder->Advise(pAdvSink, dwConnection);
};

STDMETHODIMP VLCOleObject::Close(DWORD dwSaveOption)
{
    _p_advise_holder->SendOnClose();
    OleFlushClipboard();
    return _p_instance->onClose(dwSaveOption);
};

STDMETHODIMP VLCOleObject::DoVerb(LONG iVerb, LPMSG lpMsg, LPOLECLIENTSITE pActiveSite,
                                    LONG lIndex, HWND hwndParent, LPCRECT lprcPosRect)
{
    switch( iVerb )
    {
        case OLEIVERB_PRIMARY:
        case OLEIVERB_SHOW:
        case OLEIVERB_OPEN:
        case OLEIVERB_INPLACEACTIVATE:
            return doInPlaceActivate(lpMsg, pActiveSite, hwndParent, lprcPosRect);

        case OLEIVERB_HIDE:
            _p_instance->setVisible(FALSE);
            return S_OK;

        case OLEIVERB_UIACTIVATE:
            return doUIActivate(lpMsg, pActiveSite, hwndParent, lprcPosRect);

        case OLEIVERB_DISCARDUNDOSTATE:
            return S_OK;

        default:
            return OLEOBJ_S_INVALIDVERB;
    }
};

HRESULT VLCOleObject::doInPlaceActivate(LPMSG lpMsg, LPOLECLIENTSITE pActiveSite, HWND hwndParent, LPCRECT lprcPosRect)
{
    RECT posRect;
    RECT clipRect;
    LPCRECT lprcClipRect = lprcPosRect;

    if( NULL != pActiveSite )
    {
        // check if already activated
        if( _p_instance->isInPlaceActive() )
        {
            // just attempt to show object then
            pActiveSite->ShowObject();
            _p_instance->setVisible(TRUE);
            return S_OK;
        }

        LPOLEINPLACESITE p_inPlaceSite;

        if( SUCCEEDED(pActiveSite->QueryInterface(IID_IOleInPlaceSite, (void**)&p_inPlaceSite)) )
        {
            if( S_OK != p_inPlaceSite->CanInPlaceActivate() )
            {
                return OLEOBJ_S_CANNOT_DOVERB_NOW;
            }

            LPOLEINPLACEFRAME p_inPlaceFrame;
            LPOLEINPLACEUIWINDOW p_inPlaceUIWindow;
            OLEINPLACEFRAMEINFO oleFrameInfo;

            if( SUCCEEDED(p_inPlaceSite->GetWindowContext(&p_inPlaceFrame, &p_inPlaceUIWindow, &posRect, &clipRect, &oleFrameInfo)) )
            {
                lprcPosRect = &posRect;
                lprcClipRect = &clipRect;

                if( NULL != p_inPlaceFrame )
                    p_inPlaceFrame->Release();
                if( NULL != p_inPlaceUIWindow )
                    p_inPlaceUIWindow->Release();
            }

            if( (NULL == hwndParent) && FAILED(p_inPlaceSite->GetWindow(&hwndParent)) )
            {
                p_inPlaceSite->Release();
                return OLEOBJ_S_INVALIDHWND;
            }
        }
        else if( NULL == hwndParent )
        {
            return OLEOBJ_S_INVALIDHWND;
        }

        if( FAILED(_p_instance->onActivateInPlace(lpMsg, hwndParent, lprcPosRect, lprcClipRect)) )
        {
            if( NULL != p_inPlaceSite )
                p_inPlaceSite->Release();
            return OLEOBJ_S_CANNOT_DOVERB_NOW;
        }

        if( NULL != p_inPlaceSite )
            p_inPlaceSite->OnPosRectChange(lprcPosRect);

        pActiveSite->ShowObject();
        _p_instance->setVisible(TRUE);

        if( NULL != p_inPlaceSite )
        {
            p_inPlaceSite->OnInPlaceActivate();
            p_inPlaceSite->Release();
        }

        if( NULL != lpMsg )
        {
            switch( lpMsg->message )
            {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                    doUIActivate(lpMsg, pActiveSite, hwndParent, lprcPosRect);
                    break;
                default:
                    break;
            }
        }
        return S_OK;
    }
    return OLEOBJ_S_CANNOT_DOVERB_NOW;
};

HRESULT VLCOleObject::doUIActivate(LPMSG lpMsg, LPOLECLIENTSITE pActiveSite, HWND hwndParent, LPCRECT lprcPosRect)
{
    if( NULL != pActiveSite )
    {
        // check if already activated
        if( ! _p_instance->isInPlaceActive() )
            return OLE_E_NOT_INPLACEACTIVE;

        LPOLEINPLACESITE p_inPlaceSite;

        if( SUCCEEDED(pActiveSite->QueryInterface(IID_IOleInPlaceSite, (void**)&p_inPlaceSite)) )
        {
            p_inPlaceSite->OnUIActivate();

            if( NULL != lprcPosRect )
            {
                p_inPlaceSite->OnPosRectChange(lprcPosRect);
            }
            p_inPlaceSite->Release();
        }

        pActiveSite->ShowObject();
        _p_instance->setVisible(TRUE);
        _p_instance->setFocus(TRUE);

        return S_OK;
    }
    return E_FAIL;
};

STDMETHODIMP VLCOleObject::EnumAdvise(IEnumSTATDATA **ppEnumAdvise)
{
    return _p_advise_holder->EnumAdvise(ppEnumAdvise);
};

STDMETHODIMP VLCOleObject::EnumVerbs(IEnumOleVerb **ppEnumOleVerb)
{
    return OLE_S_USEREG;
};

STDMETHODIMP VLCOleObject::GetClientSite(LPOLECLIENTSITE *ppClientSite)
{
    if( NULL == ppClientSite )
        return E_POINTER;
 
    if( NULL != _p_clientsite )
        _p_clientsite->AddRef(); 

    *ppClientSite = _p_clientsite;
    return S_OK;
};

STDMETHODIMP VLCOleObject::GetClipboardData(DWORD dwReserved, LPDATAOBJECT *ppDataObject)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleObject::GetExtent(DWORD dwDrawAspect, SIZEL *pSizel)
{
    if( NULL == pSizel )
        return E_POINTER;

    if( dwDrawAspect & DVASPECT_CONTENT )
    {
        *pSizel = _p_instance->getExtent();
        return S_OK;
    }
    pSizel->cx= 0L;
    pSizel->cy= 0L;
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleObject::GetMiscStatus(DWORD dwAspect, DWORD *pdwStatus)
{
    if( NULL != pdwStatus )
        return E_POINTER;

    switch( dwAspect )
    {
        case DVASPECT_CONTENT:
            *pdwStatus = OLEMISC_RECOMPOSEONRESIZE
                | OLEMISC_CANTLINKINSIDE
                | OLEMISC_INSIDEOUT
                | OLEMISC_ACTIVATEWHENVISIBLE
                | OLEMISC_SETCLIENTSITEFIRST;
            break;
        default:
            *pdwStatus = 0;
    }

    return S_OK;
};

STDMETHODIMP VLCOleObject::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, LPMONIKER *ppMoniker)
{
    if( NULL != _p_clientsite )
        return _p_clientsite->GetMoniker(dwAssign,dwWhichMoniker, ppMoniker);
 
    return E_UNEXPECTED;
};

STDMETHODIMP VLCOleObject::GetUserClassID(LPCLSID pClsid)
{
    if( NULL == pClsid )
        return E_POINTER;
 
    pClsid = const_cast<LPCLSID>(&_p_instance->getClassID()); 
    return S_OK;
};

STDMETHODIMP VLCOleObject::GetUserType(DWORD dwFormOfType, LPOLESTR *pszUserType)
{
    return OLE_S_USEREG;
};

STDMETHODIMP VLCOleObject::InitFromData(LPDATAOBJECT pDataObject, BOOL fCreation, DWORD dwReserved)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleObject::IsUpToDate(void)
{
    return S_OK;
};

STDMETHODIMP VLCOleObject::SetClientSite(LPOLECLIENTSITE pClientSite)
{
 
    if( NULL != pClientSite )
    {
        pClientSite->AddRef();

        /*
        ** retrieve container ambient properties
        */
        VARIANT v;
        VariantInit(&v);
        V_VT(&v) = VT_I4;
        if( SUCCEEDED(GetObjectProperty(pClientSite, DISPID_AMBIENT_CODEPAGE, v)) )
        {
            _p_instance->setCodePage(V_I4(&v));
            VariantClear(&v);
        }
    }

    if( NULL != _p_clientsite )
        _p_clientsite->Release();

    _p_clientsite = pClientSite;
    _p_instance->onClientSiteChanged(pClientSite);

    return S_OK;
};

STDMETHODIMP VLCOleObject::SetColorScheme(LOGPALETTE *pLogpal)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleObject::SetExtent(DWORD dwDrawAspect, SIZEL *pSizel)
{
    if( NULL == pSizel )
        return E_POINTER;

    if( dwDrawAspect & DVASPECT_CONTENT )
    {
        _p_instance->setExtent(*pSizel);

        if( _p_instance->isInPlaceActive() )
        {
            LPOLEINPLACESITE p_inPlaceSite;

            if( SUCCEEDED(_p_clientsite->QueryInterface(IID_IOleInPlaceSite, (void**)&p_inPlaceSite)) )
            {
                HWND hwnd;

                if( SUCCEEDED(p_inPlaceSite->GetWindow(&hwnd)) )
                {
                    // use HIMETRIC to pixel transform 
                    RECT posRect = _p_instance->getPosRect();
                    HDC hDC = GetDC(hwnd);
                    posRect.right = (pSizel->cx*GetDeviceCaps(hDC, LOGPIXELSX)/2540L)+posRect.left;
                    posRect.bottom = (pSizel->cy*GetDeviceCaps(hDC, LOGPIXELSY)/2540L)+posRect.top;
                    DeleteDC(hDC);
                    p_inPlaceSite->OnPosRectChange(&posRect);
                }
                p_inPlaceSite->Release();
            }
        }
        return S_OK;
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleObject::SetHostNames(LPCOLESTR szContainerApp, LPCOLESTR szContainerObj)
{
    return S_OK;
};

STDMETHODIMP VLCOleObject::SetMoniker(DWORD dwWhichMoniker, LPMONIKER pMoniker)
{
    return _p_advise_holder->SendOnRename(pMoniker);
};

STDMETHODIMP VLCOleObject::Unadvise(DWORD dwConnection)
{
    return _p_advise_holder->Unadvise(dwConnection);
};

STDMETHODIMP VLCOleObject::Update(void)
{
    return S_OK;
};

