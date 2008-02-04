/*****************************************************************************
 * oleobject.cpp: ActiveX control for VLC
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
    SetClientSite(NULL);
    Close(OLECLOSE_NOSAVE);
    _p_advise_holder->Release();
};

STDMETHODIMP VLCOleObject::Advise(IAdviseSink *pAdvSink, DWORD *dwConnection)
{
    return _p_advise_holder->Advise(pAdvSink, dwConnection);
};

STDMETHODIMP VLCOleObject::Close(DWORD dwSaveOption)
{
    if( _p_instance->isRunning() )
    {
        _p_advise_holder->SendOnClose();
        return _p_instance->onClose(dwSaveOption);
    }
    return S_OK;
};

STDMETHODIMP VLCOleObject::DoVerb(LONG iVerb, LPMSG lpMsg, LPOLECLIENTSITE pActiveSite,
                                    LONG lIndex, HWND hwndParent, LPCRECT lprcPosRect)
{
    switch( iVerb )
    {
        case OLEIVERB_PRIMARY:
        case OLEIVERB_SHOW:
        case OLEIVERB_OPEN:
            // force control to be visible when activating in place
            _p_instance->setVisible(TRUE);
            return doInPlaceActivate(lpMsg, pActiveSite, hwndParent, lprcPosRect, TRUE);

        case OLEIVERB_INPLACEACTIVATE:
            return doInPlaceActivate(lpMsg, pActiveSite, hwndParent, lprcPosRect, FALSE);

        case OLEIVERB_HIDE:
            _p_instance->setVisible(FALSE);
            return S_OK;

        case OLEIVERB_UIACTIVATE:
            // UI activate only if visible
            if( _p_instance->isVisible() )
                return doInPlaceActivate(lpMsg, pActiveSite, hwndParent, lprcPosRect, TRUE);
            return OLEOBJ_S_CANNOT_DOVERB_NOW;

        case OLEIVERB_DISCARDUNDOSTATE:
            return S_OK;

        default:
            if( iVerb > 0 ) {
                _p_instance->setVisible(TRUE);
                doInPlaceActivate(lpMsg, pActiveSite, hwndParent, lprcPosRect, TRUE);
                return OLEOBJ_S_INVALIDVERB;
            }
            return E_NOTIMPL;
    }
};

HRESULT VLCOleObject::doInPlaceActivate(LPMSG lpMsg, LPOLECLIENTSITE pActiveSite, HWND hwndParent, LPCRECT lprcPosRect, BOOL uiActivate)
{
    RECT posRect;
    RECT clipRect;
    LPCRECT lprcClipRect = lprcPosRect;

    if( pActiveSite )
    {
        LPOLEINPLACESITE p_inPlaceSite;
        IOleInPlaceSiteEx *p_inPlaceSiteEx;
        LPOLEINPLACEFRAME p_inPlaceFrame;
        LPOLEINPLACEUIWINDOW p_inPlaceUIWindow;

        if( SUCCEEDED(pActiveSite->QueryInterface(IID_IOleInPlaceSiteEx, reinterpret_cast<void**>(&p_inPlaceSiteEx))) )
        {
            p_inPlaceSite = p_inPlaceSiteEx;
            p_inPlaceSite->AddRef();
        }
        else if FAILED(pActiveSite->QueryInterface(IID_IOleInPlaceSite, reinterpret_cast<void**>(&p_inPlaceSite)) )
        {
            p_inPlaceSite = p_inPlaceSiteEx = NULL;
        }

        if( p_inPlaceSite )
        {
            OLEINPLACEFRAMEINFO oleFrameInfo;

            oleFrameInfo.cb = sizeof(OLEINPLACEFRAMEINFO);
            if( SUCCEEDED(p_inPlaceSite->GetWindowContext(&p_inPlaceFrame, &p_inPlaceUIWindow, &posRect, &clipRect, &oleFrameInfo)) )
            {
                lprcPosRect = &posRect;
                lprcClipRect = &clipRect;
            }

            if( (NULL == hwndParent) && FAILED(p_inPlaceSite->GetWindow(&hwndParent)) )
            {
                p_inPlaceSite->Release();
                if( p_inPlaceSiteEx )
                    p_inPlaceSiteEx->Release();
                if( p_inPlaceFrame )
                    p_inPlaceFrame->Release();
                if( p_inPlaceUIWindow )
                    p_inPlaceUIWindow->Release();

                return OLEOBJ_S_INVALIDHWND;
            }
        }
        else if( NULL == hwndParent )
        {
            return OLEOBJ_S_INVALIDHWND;
        }
        else if( NULL == lprcPosRect )
        {
            SetRect(&posRect, 0, 0, 0, 0);
            lprcPosRect = &posRect;
            lprcClipRect = &posRect;
        }

        // check if not already activated
        if( ! _p_instance->isInPlaceActive() )
        {
            if( ((NULL == p_inPlaceSite) || (S_OK == p_inPlaceSite->CanInPlaceActivate()))
             && SUCCEEDED(_p_instance->onActivateInPlace(lpMsg, hwndParent, lprcPosRect, lprcClipRect)) )
            {
                if( p_inPlaceSiteEx )
                {
                    BOOL needsRedraw;
                    p_inPlaceSiteEx->OnInPlaceActivateEx(&needsRedraw, 0);
                }
                else if( p_inPlaceSite )
                    p_inPlaceSite->OnInPlaceActivate();
            }
            else
            {
                if( p_inPlaceSite )
                {
                    p_inPlaceSite->Release();
                    if( p_inPlaceSiteEx )
                        p_inPlaceSiteEx->Release();
                    if( p_inPlaceFrame )
                        p_inPlaceFrame->Release();
                    if( p_inPlaceUIWindow )
                        p_inPlaceUIWindow->Release();
                }
                return OLEOBJ_S_CANNOT_DOVERB_NOW;
            }
        }

        if( p_inPlaceSite )
            p_inPlaceSite->OnPosRectChange(lprcPosRect);

        if( uiActivate )
        {
            if( (NULL == p_inPlaceSiteEx) || (S_OK == p_inPlaceSiteEx->RequestUIActivate()) )
            {
                if( p_inPlaceSite)
                {
                    p_inPlaceSite->OnUIActivate();

                    LPOLEINPLACEACTIVEOBJECT p_inPlaceActiveObject;
                    if( SUCCEEDED(QueryInterface(IID_IOleInPlaceActiveObject, reinterpret_cast<void**>(&p_inPlaceActiveObject))) )
                    {
                        if( p_inPlaceFrame )
                            p_inPlaceFrame->SetActiveObject(p_inPlaceActiveObject, NULL);
                        if( p_inPlaceUIWindow )
                            p_inPlaceUIWindow->SetActiveObject(p_inPlaceActiveObject, NULL);
                        p_inPlaceActiveObject->Release();
                    }
                    if( p_inPlaceFrame )
                        p_inPlaceFrame->RequestBorderSpace(NULL);

                    pActiveSite->ShowObject();
                }
                _p_instance->setFocus(TRUE);
            }
        }

        if( p_inPlaceSite )
        {
            p_inPlaceSite->Release();
            if( p_inPlaceSiteEx )
                p_inPlaceSiteEx->Release();
            if( p_inPlaceFrame )
                p_inPlaceFrame->Release();
            if( p_inPlaceUIWindow )
                p_inPlaceUIWindow->Release();
        }
        return S_OK;
    }
    return OLEOBJ_S_CANNOT_DOVERB_NOW;
};

STDMETHODIMP VLCOleObject::EnumAdvise(IEnumSTATDATA **ppEnumAdvise)
{
    return _p_advise_holder->EnumAdvise(ppEnumAdvise);
};

STDMETHODIMP VLCOleObject::EnumVerbs(IEnumOleVerb **ppEnumOleVerb)
{
    return OleRegEnumVerbs(_p_instance->getClassID(),
        ppEnumOleVerb);
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
    return _p_instance->pUnkOuter->QueryInterface(IID_IDataObject, (void **)ppDataObject);
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
    if( NULL == pdwStatus )
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
 
    *pClsid = _p_instance->getClassID();
    return S_OK;
};

STDMETHODIMP VLCOleObject::GetUserType(DWORD dwFormOfType, LPOLESTR *pszUserType)
{
    return OleRegGetUserType(_p_instance->getClassID(),
        dwFormOfType, pszUserType);
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
    if( NULL != _p_clientsite )
        _p_clientsite->Release();

    _p_clientsite = pClientSite;

    if( NULL != pClientSite )
    {
        pClientSite->AddRef();
        _p_instance->onAmbientChanged(pClientSite, DISPID_UNKNOWN);
    }
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

