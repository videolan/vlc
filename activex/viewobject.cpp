/*****************************************************************************
 * viewobject.cpp: ActiveX control for VLC
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
#include "viewobject.h"

#include <iostream>

using namespace std;

STDMETHODIMP VLCViewObject::Draw(DWORD dwAspect, LONG lindex, PVOID pvAspect,
        DVTARGETDEVICE *ptd, HDC hicTargetDev, HDC hdcDraw, LPCRECTL lprcBounds,
        LPCRECTL lprcWBounds, BOOL(CALLBACK *pfnContinue)(DWORD), DWORD dwContinue)
{
    switch( dwAspect )
    {
        case DVASPECT_CONTENT:
            if( _p_instance->getVisible() )
            {
                RECT bounds;
                bounds.left   = lprcBounds->left;
                bounds.top    = lprcBounds->top;
                bounds.right  = lprcBounds->right;
                bounds.bottom = lprcBounds->bottom;
                _p_instance->onPaint(hdcDraw, bounds, bounds);
            }
            return S_OK;
        case DVASPECT_THUMBNAIL:
            break;
        case DVASPECT_ICON:
            break;
        case DVASPECT_DOCPRINT:
            break;
    }
    return E_NOTIMPL;
};

STDMETHODIMP VLCViewObject::Freeze(DWORD dwAspect, LONG lindex,
        PVOID pvAspect, LPDWORD pdwFreeze)
{
    if( NULL != pvAspect )
        return E_INVALIDARG;

    return OLE_E_BLANK;
};

STDMETHODIMP VLCViewObject::GetAdvise(LPDWORD pdwAspect, LPDWORD padvf,
        LPADVISESINK *ppAdviseSink)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCViewObject::GetColorSet(DWORD dwAspect, LONG lindex, 
        PVOID pvAspect, DVTARGETDEVICE *ptd, HDC hicTargetDev, LPLOGPALETTE *ppColorSet)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCViewObject::SetAdvise(DWORD dwAspect, DWORD advf,
        LPADVISESINK pAdvSink)
{
    return OLE_E_ADVISENOTSUPPORTED;
};

STDMETHODIMP VLCViewObject::Unfreeze(DWORD dwFreeze)
{
    return E_NOTIMPL;
};

