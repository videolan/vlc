/*****************************************************************************
 * connectioncontainer.cpp: ActiveX control for VLC
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

#include "plugin.h"
#include "connectioncontainer.h"

#include "utils.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////

class VLCEnumConnections : public IEnumConnections
{
public:
    VLCEnumConnections(vector<CONNECTDATA> &v) :
        e(VLCEnum<CONNECTDATA>(IID_IEnumConnections, v))
    { e.setRetainOperation((VLCEnum<CONNECTDATA>::retainer)&retain); };

    VLCEnumConnections(const VLCEnumConnections &vlcEnum) : e(vlcEnum.e) {};

    virtual ~VLCEnumConnections() {};

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
        { return e.QueryInterface(riid, ppv); };
    STDMETHODIMP_(ULONG) AddRef(void)
        { return e.AddRef(); };
    STDMETHODIMP_(ULONG) Release(void)
        {return e.Release(); };

    //IEnumConnectionPoints
    STDMETHODIMP Next(ULONG celt, LPCONNECTDATA rgelt, ULONG *pceltFetched)
        { return e.Next(celt, rgelt, pceltFetched); };
    STDMETHODIMP Skip(ULONG celt)
        { return e.Skip(celt);};
    STDMETHODIMP Reset(void)
        { return e.Reset();};
    STDMETHODIMP Clone(LPENUMCONNECTIONS *ppenum)
        { if( NULL == ppenum ) return E_POINTER;
          *ppenum = dynamic_cast<LPENUMCONNECTIONS>(new VLCEnumConnections(*this));
          return (NULL != *ppenum) ? S_OK : E_OUTOFMEMORY;
        };

private:

    static void retain(CONNECTDATA cd)
    {
        cd.pUnk->AddRef();
    };

    VLCEnum<CONNECTDATA> e;
};

////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VLCConnectionPoint::GetConnectionInterface(IID *iid)
{
    if( NULL == iid )
        return E_POINTER;

    *iid = _iid;
    return S_OK;
};

STDMETHODIMP VLCConnectionPoint::GetConnectionPointContainer(LPCONNECTIONPOINTCONTAINER *ppCPC)
{
    if( NULL == ppCPC )
        return E_POINTER;

    _p_cpc->AddRef();
    *ppCPC = _p_cpc;
    return S_OK;
};

STDMETHODIMP VLCConnectionPoint::Advise(IUnknown *pUnk, DWORD *pdwCookie)
{
    if( (NULL == pUnk) || (NULL == pdwCookie) )
        return E_POINTER;

    CONNECTDATA cd;

    pUnk->AddRef();
    cd.pUnk = pUnk;
    *pdwCookie = cd.dwCookie = _connections.size();

    _connections.push_back(cd);

    return S_OK;
};

STDMETHODIMP VLCConnectionPoint::Unadvise(DWORD pdwCookie)
{
    if( pdwCookie < _connections.size() )
    {
        CONNECTDATA cd = _connections[pdwCookie];
        if( NULL != cd.pUnk )
        {
            cd.pUnk->Release();
            cd.pUnk = NULL;
            return S_OK;
        }
    }
    return CONNECT_E_NOCONNECTION;
};

STDMETHODIMP VLCConnectionPoint::EnumConnections(IEnumConnections **ppEnum)
{
    if( NULL == ppEnum )
        return E_POINTER;

    *ppEnum = dynamic_cast<LPENUMCONNECTIONS>(new VLCEnumConnections(_connections));

    return (NULL != *ppEnum ) ? S_OK : E_OUTOFMEMORY;
};

void VLCConnectionPoint::fireEvent(DISPID dispId, DISPPARAMS* pDispParams)
{
    vector<CONNECTDATA>::iterator end = _connections.end();
    vector<CONNECTDATA>::iterator iter = _connections.begin();

    while( iter != end )
    {
        CONNECTDATA cd = *iter;
        if( NULL != cd.pUnk )
        {
            IDispatch *pDisp;
            if( SUCCEEDED(cd.pUnk->QueryInterface(IID_IDispatch, (LPVOID *)&pDisp)) )
            {
                pDisp->Invoke(dispId, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, pDispParams, NULL, NULL, NULL);
                pDisp->Release();
            }
        }
        ++iter;
    }
};

void VLCConnectionPoint::firePropChangedEvent(DISPID dispId)
{
    vector<CONNECTDATA>::iterator end = _connections.end();
    vector<CONNECTDATA>::iterator iter = _connections.begin();

    while( iter != end )
    {
        CONNECTDATA cd = *iter;
        if( NULL != cd.pUnk )
        {
            IPropertyNotifySink *pPropSink;
            if( SUCCEEDED(cd.pUnk->QueryInterface(IID_IPropertyNotifySink, (LPVOID *)&pPropSink)) )
            {
                pPropSink->OnChanged(dispId);
                pPropSink->Release();
            }
        }
        ++iter;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////

class VLCEnumConnectionPoints : public IEnumConnectionPoints
{
public:
    VLCEnumConnectionPoints(vector<LPCONNECTIONPOINT> &v) :
        e(VLCEnum<LPCONNECTIONPOINT>(IID_IEnumConnectionPoints, v))
    { e.setRetainOperation((VLCEnum<LPCONNECTIONPOINT>::retainer)&retain); };

    VLCEnumConnectionPoints(const VLCEnumConnectionPoints &vlcEnum) : e(vlcEnum.e) {};

    virtual ~VLCEnumConnectionPoints() {};

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
        { return e.QueryInterface(riid, ppv); };
    STDMETHODIMP_(ULONG) AddRef(void)
        { return e.AddRef(); };
    STDMETHODIMP_(ULONG) Release(void)
        {return e.Release(); };

    //IEnumConnectionPoints
    STDMETHODIMP Next(ULONG celt, LPCONNECTIONPOINT *rgelt, ULONG *pceltFetched)
        { return e.Next(celt, rgelt, pceltFetched); };
    STDMETHODIMP Skip(ULONG celt)
        { return e.Skip(celt);};
    STDMETHODIMP Reset(void)
        { return e.Reset();};
    STDMETHODIMP Clone(LPENUMCONNECTIONPOINTS *ppenum)
        { if( NULL == ppenum ) return E_POINTER;
          *ppenum = dynamic_cast<LPENUMCONNECTIONPOINTS>(new VLCEnumConnectionPoints(*this));
          return (NULL != *ppenum) ? S_OK : E_OUTOFMEMORY;
        };

private:

    static void retain(LPCONNECTIONPOINT cp)
    {
        cp->AddRef();
    };

    VLCEnum<LPCONNECTIONPOINT> e;
};

////////////////////////////////////////////////////////////////////////////////////////////////

VLCDispatchEvent::~VLCDispatchEvent()
{
    //clear event arguments
    if( NULL != _dispParams.rgvarg )
    {
        for(unsigned int c=0; c<_dispParams.cArgs; ++c)
            VariantClear(_dispParams.rgvarg+c);
        CoTaskMemFree(_dispParams.rgvarg);
    }
    if( NULL != _dispParams.rgdispidNamedArgs )
        CoTaskMemFree(_dispParams.rgdispidNamedArgs);
};

////////////////////////////////////////////////////////////////////////////////////////////////

VLCConnectionPointContainer::VLCConnectionPointContainer(VLCPlugin *p_instance) :
    _p_instance(p_instance), _b_freeze(FALSE)
{
    _p_events = new VLCConnectionPoint(dynamic_cast<LPCONNECTIONPOINTCONTAINER>(this),
            _p_instance->getDispEventID());

    _v_cps.push_back(dynamic_cast<LPCONNECTIONPOINT>(_p_events));

    _p_props = new VLCConnectionPoint(dynamic_cast<LPCONNECTIONPOINTCONTAINER>(this),
            IID_IPropertyNotifySink);

    _v_cps.push_back(dynamic_cast<LPCONNECTIONPOINT>(_p_props));
};

VLCConnectionPointContainer::~VLCConnectionPointContainer()
{
    delete _p_props;
    delete _p_events;
};

STDMETHODIMP VLCConnectionPointContainer::EnumConnectionPoints(LPENUMCONNECTIONPOINTS *ppEnum)
{
    if( NULL == ppEnum )
        return E_POINTER;

    *ppEnum = dynamic_cast<LPENUMCONNECTIONPOINTS>(new VLCEnumConnectionPoints(_v_cps));

    return (NULL != *ppEnum ) ? S_OK : E_OUTOFMEMORY;
};

STDMETHODIMP VLCConnectionPointContainer::FindConnectionPoint(REFIID riid, IConnectionPoint **ppCP)
{
    if( NULL == ppCP )
        return E_POINTER;

    *ppCP = NULL;

    if( IID_IPropertyNotifySink == riid )
    {
        _p_props->AddRef();
        *ppCP = dynamic_cast<LPCONNECTIONPOINT>(_p_props);
    }
    else if( _p_instance->getDispEventID() == riid )
    {
        _p_events->AddRef();
        *ppCP = dynamic_cast<LPCONNECTIONPOINT>(_p_events);
    }
    else
        return CONNECT_E_NOCONNECTION;

    return NOERROR;
};

void VLCConnectionPointContainer::freezeEvents(BOOL freeze)
{
    if( ! freeze )
    {
        // release queued events
        while( ! _q_events.empty() )
        {
            VLCDispatchEvent *ev = _q_events.front();
            _p_events->fireEvent(ev->_dispId, &ev->_dispParams);
            delete ev;
            _q_events.pop();
        }
    }
    _b_freeze = freeze;
};

void VLCConnectionPointContainer::fireEvent(DISPID dispId, DISPPARAMS* pDispParams)
{
    VLCDispatchEvent *evt = new VLCDispatchEvent(dispId, *pDispParams);
    if( _b_freeze )
    {
        // queue event for later use when container is ready
        _q_events.push(evt);
        if( _q_events.size() > 10 )
        {
            // too many events in queue, get rid of older one
            delete _q_events.front();
            _q_events.pop();
        }
    }
    else
    {
        _p_events->fireEvent(dispId, pDispParams);
        delete evt;
    }
};

void VLCConnectionPointContainer::firePropChangedEvent(DISPID dispId)
{
    if( ! _b_freeze )
        _p_props->firePropChangedEvent(dispId);
};

