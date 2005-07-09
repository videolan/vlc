/*****************************************************************************
 * persiststreaminit.h: ActiveX control for VLC
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef __PERSISTSTREAMINIT_H__
#define __PERSISTSTREAMINIT_H__

#include <ocidl.h>

class VLCPersistStreamInit : public IPersistStreamInit
{

public:

    VLCPersistStreamInit(VLCPlugin *p_instance) : _p_instance(p_instance) {};
    virtual ~VLCPersistStreamInit() {};

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( (NULL != ppv)
         && (IID_IUnknown == riid) 
         && (IID_IPersist == riid) 
         && (IID_IPersistStreamInit == riid) ) {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->Release(); };

    // IPersist methods
    STDMETHODIMP GetClassID(LPCLSID);

    // IPersistStreamInit methods
    STDMETHODIMP IsDirty(void);
    STDMETHODIMP InitNew(void);
    STDMETHODIMP Load(LPSTREAM);
    STDMETHODIMP Save(LPSTREAM, BOOL);
    STDMETHODIMP GetSizeMax(ULARGE_INTEGER *);

private:

    VLCPlugin *_p_instance;
};

#endif

