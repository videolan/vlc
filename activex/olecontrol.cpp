/*****************************************************************************
 * olecontrol.cpp: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN (Centrale Réseaux) and its contributors
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
#include "olecontrol.h"

#include "utils.h"

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
    return E_NOTIMPL;
};

static HRESULT getAmbientProperty(VLCPlugin& instance, DISPID dispID, VARIANT& v)
{
    HRESULT hr;
    IOleObject *oleObj;

    hr = instance.QueryInterface(IID_IOleObject, (LPVOID *)&oleObj);
    if( SUCCEEDED(hr) )
    {
        IOleClientSite *clientSite;

        hr = oleObj->GetClientSite(&clientSite);
        if( SUCCEEDED(hr) && (NULL != clientSite) )
        {
            hr = GetObjectProperty(clientSite, dispID, v);
            clientSite->Release();
        }
        oleObj->Release();
    }
    return hr;
};

STDMETHODIMP VLCOleControl::OnAmbientPropertyChange(DISPID dispID)
{
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
            VARIANT v;
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(getAmbientProperty(*_p_instance, dispID, v)) )
            {
                _p_instance->setCodePage(V_I4(&v));
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
        default:
            break;
    }
    return S_OK;
};

STDMETHODIMP VLCOleControl::FreezeEvents(BOOL bFreeze)
{
    _p_instance->setSendEvents(! bFreeze);
    return S_OK;
};

