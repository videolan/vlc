/*****************************************************************************
 * viewobject.cpp: ActiveX control for VLC
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
#include "viewobject.h"

#include "utils.h"

using namespace std;

STDMETHODIMP VLCViewObject::Draw(DWORD dwAspect, LONG lindex, PVOID pvAspect,
        DVTARGETDEVICE *ptd, HDC hicTargetDev, HDC hdcDraw, LPCRECTL lprcBounds,
        LPCRECTL lprcWBounds, BOOL(CALLBACK *pfnContinue)(DWORD), DWORD dwContinue)
{
    if( dwAspect & DVASPECT_CONTENT )
    {
        if( NULL == lprcBounds )
            return E_INVALIDARG;

        BOOL releaseDC = FALSE;
        SIZEL size = _p_instance->getExtent();

        if( NULL == ptd )
        {
            hicTargetDev = CreateDevDC(NULL);
            releaseDC = TRUE;
        }
        DPFromHimetric(hicTargetDev, (LPPOINT)&size, 1);

        RECTL bounds = { 0L, 0L, size.cx, size.cy };

        int sdc = SaveDC(hdcDraw);
        SetMapMode(hdcDraw, MM_ANISOTROPIC);
        SetWindowOrgEx(hdcDraw, 0, 0, NULL);
        SetWindowExtEx(hdcDraw, size.cx, size.cy, NULL);
        OffsetViewportOrgEx(hdcDraw, lprcBounds->left, lprcBounds->top, NULL);
        SetViewportExtEx(hdcDraw, lprcBounds->right-lprcBounds->left,
                lprcBounds->bottom-lprcBounds->top, NULL);

        _p_instance->onDraw(ptd, hicTargetDev, hdcDraw, &bounds, lprcWBounds);
        RestoreDC(hdcDraw, sdc);

        if( releaseDC )
            DeleteDC(hicTargetDev);

        return S_OK;
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCViewObject::Freeze(DWORD dwAspect, LONG lindex,
        PVOID pvAspect, LPDWORD pdwFreeze)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCViewObject::GetAdvise(LPDWORD pdwAspect, LPDWORD padvf,
        LPADVISESINK *ppAdviseSink)
{
    if( NULL != pdwAspect )
        *pdwAspect = _dwAspect;

    if( NULL != padvf )
        *padvf = _advf;

    if( NULL != ppAdviseSink )
    {
        *ppAdviseSink = _pAdvSink;
        if( NULL != _pAdvSink )
            _pAdvSink->AddRef();
    }

    return S_OK;
};

STDMETHODIMP VLCViewObject::GetColorSet(DWORD dwAspect, LONG lindex,
        PVOID pvAspect, DVTARGETDEVICE *ptd, HDC hicTargetDev, LPLOGPALETTE *ppColorSet)
{
    return S_FALSE;
};

STDMETHODIMP VLCViewObject::SetAdvise(DWORD dwAspect, DWORD advf,
        LPADVISESINK pAdvSink)
{
    if( NULL != pAdvSink )
        pAdvSink->AddRef();

    if( NULL != _pAdvSink )
        _pAdvSink->Release();

    _dwAspect = dwAspect;
    _advf = advf;
    _pAdvSink = pAdvSink;

    if( (dwAspect & DVASPECT_CONTENT) && (advf & ADVF_PRIMEFIRST) && (NULL != _pAdvSink) )
    {
        _pAdvSink->OnViewChange(DVASPECT_CONTENT, -1);
    }

    return S_OK;
};

STDMETHODIMP VLCViewObject::Unfreeze(DWORD dwFreeze)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCViewObject::GetExtent(DWORD dwAspect, LONG lindex,
        DVTARGETDEVICE *ptd, LPSIZEL lpSizel)
{
    if( dwAspect & DVASPECT_CONTENT )
    {
        *lpSizel = _p_instance->getExtent();
        return S_OK;
    }
    lpSizel->cx= 0L;
    lpSizel->cy= 0L;
    return E_NOTIMPL;
};

