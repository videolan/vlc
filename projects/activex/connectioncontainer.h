/*****************************************************************************
 * connectioncontainer.h: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * Copyright (C) 2010 M2X BV
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef __CONNECTIONCONTAINER_H__
#define __CONNECTIONCONTAINER_H__

#include <ocidl.h>
#include <vector>
#include <queue>
#include <map>
#include <cguid.h>

class VLCConnectionPoint : public IConnectionPoint
{

public:

    VLCConnectionPoint(IConnectionPointContainer *p_cpc, REFIID iid);
    virtual ~VLCConnectionPoint();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
            return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IConnectionPoint == riid) )
        {
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
    IGlobalInterfaceTable *m_pGIT;
    IConnectionPointContainer *_p_cpc;
    std::map<DWORD, LPUNKNOWN> _connections;
};

//////////////////////////////////////////////////////////////////////////

class VLCDispatchEvent {

public:
    VLCDispatchEvent(DISPID dispId, DISPPARAMS dispParams) :
        _dispId(dispId), _dispParams(dispParams) {};
    VLCDispatchEvent(const VLCDispatchEvent&);
    ~VLCDispatchEvent();

    DISPID      _dispId;
    DISPPARAMS  _dispParams;
};

class VLCConnectionPointContainer : public IConnectionPointContainer
{

public:

    VLCConnectionPointContainer(VLCPlugin *p_instance);
    virtual ~VLCConnectionPointContainer();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv)
            return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IConnectionPointContainer == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->pUnkOuter->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->pUnkOuter->Release(); };

    // IConnectionPointContainer methods
    STDMETHODIMP EnumConnectionPoints(LPENUMCONNECTIONPOINTS *);
    STDMETHODIMP FindConnectionPoint(REFIID, LPCONNECTIONPOINT *);

    void freezeEvents(BOOL);
    void fireEvent(DISPID, DISPPARAMS*);
    void firePropChangedEvent(DISPID dispId);

public:
    CRITICAL_SECTION csEvents;
    HANDLE sEvents;

    VLCPlugin *_p_instance;
    BOOL isRunning;
    BOOL freeze;
    VLCConnectionPoint *_p_events;
    VLCConnectionPoint *_p_props;
    std::vector<LPCONNECTIONPOINT> _v_cps;
    std::queue<class VLCDispatchEvent *> _q_events;

private:
    HANDLE  hThread;
};

#endif

