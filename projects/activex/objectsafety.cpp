/*****************************************************************************
 * objectsafety.cpp: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005-2010 the VideoLAN team
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
#include "objectsafety.h"

#include "axvlc_idl.h"

#if 0
const GUID IID_IObjectSafety =
    {0xCB5BDC81,0x93C1,0x11cf,{0x8F,0x20,0x00,0x80,0x5F,0x2C,0xD0,0x64}};
#endif

using namespace std;

STDMETHODIMP VLCObjectSafety::GetInterfaceSafetyOptions(
    REFIID riid,
    DWORD *pdwSupportedOptions,
    DWORD *pdwEnabledOptions
)
{
    if( (NULL == pdwSupportedOptions) || (NULL == pdwEnabledOptions) )
        return E_POINTER;

    *pdwSupportedOptions = INTERFACESAFE_FOR_UNTRUSTED_DATA|INTERFACESAFE_FOR_UNTRUSTED_CALLER;

    if( (IID_IDispatch == riid)
     || (IID_IVLCControl == riid)
     || (IID_IVLCControl2 == riid) )
    {
        *pdwEnabledOptions = INTERFACESAFE_FOR_UNTRUSTED_CALLER;
        return NOERROR;
    }
    else if( (IID_IPersist == riid)
          || (IID_IPersistStreamInit == riid)
          || (IID_IPersistStorage == riid)
          || (IID_IPersistPropertyBag == riid) )
    {
        *pdwEnabledOptions = INTERFACESAFE_FOR_UNTRUSTED_DATA;
        return NOERROR;
    }
    *pdwEnabledOptions = 0;
    return E_NOINTERFACE;
};

STDMETHODIMP VLCObjectSafety::SetInterfaceSafetyOptions(
    REFIID riid,
    DWORD dwOptionSetMask,
    DWORD dwEnabledOptions
)
{
    if( (IID_IDispatch == riid)
     || (IID_IVLCControl == riid)
     || (IID_IVLCControl2 == riid) )
    {
        if( (INTERFACESAFE_FOR_UNTRUSTED_CALLER == dwOptionSetMask)
         && (INTERFACESAFE_FOR_UNTRUSTED_CALLER == dwEnabledOptions) )
        {
            return NOERROR;
        }
        return E_FAIL;
    }
    else if( (IID_IPersist == riid)
          || (IID_IPersistStreamInit == riid)
          || (IID_IPersistStorage == riid)
          || (IID_IPersistPropertyBag == riid) )
    {
        if( (INTERFACESAFE_FOR_UNTRUSTED_DATA == dwOptionSetMask)
         && (INTERFACESAFE_FOR_UNTRUSTED_DATA == dwEnabledOptions) )
        {
            return NOERROR;
        }
        return E_FAIL;
    }
    return E_FAIL;
};
