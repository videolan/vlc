/*****************************************************************************
 * supporterrorinfo.cpp: ActiveX control for VLC
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
#include "supporterrorinfo.h"

#include "utils.h"
#include "axvlc_idl.h"

using namespace std;

STDMETHODIMP VLCSupportErrorInfo::InterfaceSupportsErrorInfo(REFIID  riid)
{
    if( (riid == IID_IVLCAudio)
     || (riid == IID_IVLCInput)
     || (riid == IID_IVLCMarquee)
     || (riid == IID_IVLCLogo)
     || (riid == IID_IVLCPlaylist)
     || (riid == IID_IVLCPlaylistItems)
     || (riid == IID_IVLCSubtitle)
     || (riid == IID_IVLCVideo)
     || (riid == IID_IVLCControl2) )
    {
        return S_OK;
    }
    return S_FALSE;
};

void VLCSupportErrorInfo::setErrorInfo(LPCOLESTR progid, REFIID riid, const char *description)
{
    BSTR bstrDescription = BSTRFromCStr(CP_UTF8, description);
    if( NULL != bstrDescription )
    {
        ICreateErrorInfo* pcerrinfo;

        HRESULT hr = CreateErrorInfo(&pcerrinfo);
        if( SUCCEEDED(hr) )
        {
            IErrorInfo* perrinfo;

            pcerrinfo->SetSource((LPOLESTR)progid);
            pcerrinfo->SetGUID(riid);
            pcerrinfo->SetDescription((LPOLESTR)bstrDescription);
            hr = pcerrinfo->QueryInterface(IID_IErrorInfo, (LPVOID*) &perrinfo);
            if( SUCCEEDED(hr) )
            {
               ::SetErrorInfo(0, perrinfo);
               perrinfo->Release();
            }
            pcerrinfo->Release();
        }
        SysFreeString(bstrDescription);
    }
};

