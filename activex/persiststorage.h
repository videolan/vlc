/*****************************************************************************
 * persiststorage.h: ActiveX control for VLC
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

#ifndef __PERSISTSTORAGE_H__
#define __PERSISTSTORAGE_H__

#include <ocidl.h>

class VLCPersistStorage : public IPersistStorage
{

public:

    VLCPersistStorage(VLCPlugin *p_instance) : _p_instance(p_instance) {};
    virtual ~VLCPersistStorage() {};

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( (NULL != ppv)
         && (IID_IUnknown == riid) 
         && (IID_IPersist == riid) 
         && (IID_IPersistStorage == riid) ) {
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

    // IPersistStorage methods
    STDMETHODIMP IsDirty(void);
    STDMETHODIMP InitNew(IStorage *);
    STDMETHODIMP Load(IStorage *);
    STDMETHODIMP Save(IStorage *, BOOL);
    STDMETHODIMP SaveCompleted(IStorage *);
    STDMETHODIMP HandsOffStorage(void);

private:

    VLCPlugin *_p_instance;
};

#endif

