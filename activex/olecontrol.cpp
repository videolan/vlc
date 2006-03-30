/*****************************************************************************
 * olecontrol.cpp: ActiveX control for VLC
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
#include "olecontrol.h"

using namespace std;

STDMETHODIMP VLCOleControl::GetControlInfo(CONTROLINFO *pCI)
{
    if( NULL == pCI )
        return E_POINTER;

    pCI->cb      = sizeof(CONTROLINFO);
    pCI->hAccel  = NULL;
    pCI->cAccel  = 0;
    pCI->dwFlags = 0;

    return S_OK;
};

STDMETHODIMP VLCOleControl::OnMnemonic(LPMSG pMsg)
{
    return S_OK;
};

STDMETHODIMP VLCOleControl::OnAmbientPropertyChange(DISPID dispID)
{
    HRESULT hr;
    LPOLEOBJECT oleObj;

    hr = QueryInterface(IID_IOleObject, (LPVOID *)&oleObj);
    if( SUCCEEDED(hr) )
    {
        LPOLECLIENTSITE clientSite;

        hr = oleObj->GetClientSite(&clientSite);
        if( SUCCEEDED(hr) && (NULL != clientSite) )
        {
            _p_instance->onAmbientChanged(clientSite, dispID);
            clientSite->Release();
        }
        oleObj->Release();
    }
    return S_OK;
};

STDMETHODIMP VLCOleControl::FreezeEvents(BOOL bFreeze)
{
    _p_instance->freezeEvents(bFreeze);
    return S_OK;
};

