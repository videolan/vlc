/*****************************************************************************
 * oleinplaceactiveobject.cpp: ActiveX control for VLC
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
#include "oleinplaceactiveobject.h"

using namespace std;

STDMETHODIMP VLCOleInPlaceActiveObject::GetWindow(HWND *pHwnd)
{
    if( NULL == pHwnd )
        return E_POINTER;

    *pHwnd = NULL;
    if( _p_instance->isInPlaceActive() )
    {
        if( NULL != (*pHwnd = _p_instance->getInPlaceWindow()) )
            return S_OK;
    }
    return E_FAIL;
};

STDMETHODIMP VLCOleInPlaceActiveObject::EnableModeless(BOOL fEnable)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleInPlaceActiveObject::ContextSensitiveHelp(BOOL fEnterMode)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleInPlaceActiveObject::TranslateAccelerator(LPMSG lpmsg)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleInPlaceActiveObject::OnFrameWindowActivate(BOOL fActivate)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleInPlaceActiveObject::OnDocWindowActivate(BOOL fActivate)
{
    return E_NOTIMPL;
};

STDMETHODIMP VLCOleInPlaceActiveObject::ResizeBorder(LPCRECT prcBorder, LPOLEINPLACEUIWINDOW pUIWindow, BOOL fFrameWindow)
{
    return E_NOTIMPL;
};

