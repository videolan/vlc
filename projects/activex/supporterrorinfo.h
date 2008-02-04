/*****************************************************************************
 * supporterrorinfo.h: ActiveX control for VLC
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

#ifndef __SUPPORTERRORINFO_H__
#define __SUPPORTERRORINFO_H__

#include <oaidl.h>

class VLCSupportErrorInfo : public ISupportErrorInfo
{

public:

    VLCSupportErrorInfo(VLCPlugin *p_instance) :
        _p_instance(p_instance)
    {};
    virtual ~VLCSupportErrorInfo()
    {};

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
            return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_ISupportErrorInfo == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->pUnkOuter->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->pUnkOuter->Release(); };

    // ISupportErrorInfo methods
    STDMETHODIMP InterfaceSupportsErrorInfo(REFIID  riid);

    // VLCSupportErrorInfo methods
    void setErrorInfo(LPCOLESTR progid, REFIID riid, const char *description);

private:
    VLCPlugin *_p_instance;
};

#endif

