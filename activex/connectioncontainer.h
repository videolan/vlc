/*****************************************************************************
 * connectioncontainer.h: ActiveX control for VLC
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

#ifndef __CONNECTIONCONTAINER_H__
#define __CONNECTIONCONTAINER_H__

#include <ocidl.h>
#include <vector>

using namespace std;

class VLCConnectionPoint : public IConnectionPoint
{

public:

    VLCConnectionPoint(IConnectionPointContainer *p_cpc, REFIID iid) :
        _iid(iid), _p_cpc(p_cpc) {};
    virtual ~VLCConnectionPoint() {};

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv ) return E_POINTER;
        if( (IID_IUnknown == riid) 
         && (IID_IConnectionPoint == riid) ) {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        // must be a standalone object
        return E_NOINTERFACE;
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_cpc->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_cpc->Release(); };

    // IConnectionPoint methods
    STDMETHODIMP GetConnectionInterface(IID *);
    STDMETHODIMP GetConnectionPointContainer(LPCONNECTIONPOINTCONTAINER *);
    STDMETHODIMP Advise(IUnknown *, DWORD *);
    STDMETHODIMP Unadvise(DWORD);
    STDMETHODIMP EnumConnections(LPENUMCONNECTIONS *);

    void fireEvent(DISPID dispIdMember, DISPPARAMS* pDispParams);
    void firePropChangedEvent(DISPID dispId);

private:

    REFIID _iid;
    IConnectionPointContainer *_p_cpc;
    vector<CONNECTDATA> _connections;
};

//////////////////////////////////////////////////////////////////////////

class VLCConnectionPointContainer : public IConnectionPointContainer
{

public:

    VLCConnectionPointContainer(VLCPlugin *p_instance);
    virtual ~VLCConnectionPointContainer();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( (NULL != ppv)
         && (IID_IUnknown == riid) 
         && (IID_IConnectionPointContainer == riid) ) {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->Release(); };

    // IConnectionPointContainer methods
    STDMETHODIMP EnumConnectionPoints(LPENUMCONNECTIONPOINTS *);
    STDMETHODIMP FindConnectionPoint(REFIID, LPCONNECTIONPOINT *);

    void fireEvent(DISPID, DISPPARAMS*);
    void firePropChangedEvent(DISPID dispId);

private:

    VLCPlugin *_p_instance;
    VLCConnectionPoint *_p_events;
    VLCConnectionPoint *_p_props;
    vector<LPCONNECTIONPOINT> _v_cps;
};

#endif

