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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "plugin.h"
#include "connectioncontainer.h"

#include "utils.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////

/* this function object is used to return the value from a map pair */
struct VLCEnumConnectionsDereference
{
    CONNECTDATA operator()(const map<DWORD,LPUNKNOWN>::iterator& i)
    {
        CONNECTDATA cd;

        i->second->AddRef();

        cd.dwCookie = i->first;
        cd.pUnk     = i->second;
        return cd;
    };
};

class VLCEnumConnections : public VLCEnumIterator<IID_IEnumConnections,
    IEnumConnections,
    CONNECTDATA,
    map<DWORD,LPUNKNOWN>::iterator,
    VLCEnumConnectionsDereference>
{
public:
    VLCEnumConnections(map<DWORD,LPUNKNOWN> &m) :
        VLCEnumIterator<IID_IEnumConnections,
            IEnumConnections,
            CONNECTDATA,
            map<DWORD,LPUNKNOWN>::iterator,
            VLCEnumConnectionsDereference> (m.begin(), m.end())
    {};
};

////////////////////////////////////////////////////////////////////////////////////////////////

/* this function object is used to retain the dereferenced iterator value */
struct VLCEnumConnectionPointsDereference
{
    LPCONNECTIONPOINT operator()(const vector<LPCONNECTIONPOINT>::iterator& i)
    {
        LPCONNECTIONPOINT cp = *i;
        cp->AddRef();
        return cp;
    }
};

class VLCEnumConnectionPoints: public VLCEnumIterator<IID_IEnumConnectionPoints,
    IEnumConnectionPoints,
    LPCONNECTIONPOINT,
    vector<LPCONNECTIONPOINT>::iterator,
    VLCEnumConnectionPointsDereference>
{
public:
    VLCEnumConnectionPoints(vector<LPCONNECTIONPOINT>& v) :
        VLCEnumIterator<IID_IEnumConnectionPoints,
            IEnumConnectionPoints,
            LPCONNECTIONPOINT,
            vector<LPCONNECTIONPOINT>::iterator,
            VLCEnumConnectionPointsDereference> (v.begin(), v.end())
    {};
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
    static DWORD dwCookieCounter = 0;

    if( (NULL == pUnk) || (NULL == pdwCookie) )
        return E_POINTER;

    if( SUCCEEDED(pUnk->QueryInterface(_iid, (LPVOID *)&pUnk)) )
    {
        *pdwCookie = ++dwCookieCounter;
        _connections[*pdwCookie] = pUnk;
        return S_OK;
    }
    return CONNECT_E_CANNOTCONNECT;
};

STDMETHODIMP VLCConnectionPoint::Unadvise(DWORD pdwCookie)
{
    map<DWORD,LPUNKNOWN>::iterator pcd = _connections.find((DWORD)pdwCookie);
    if( pcd != _connections.end() )
    {
        pcd->second->Release();

        _connections.erase(pdwCookie);
        return S_OK;
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

void VLCConnectionPoint::fireEvent(DISPID dispId, DISPPARAMS *pDispParams)
{
    map<DWORD,LPUNKNOWN>::iterator end = _connections.end();
    map<DWORD,LPUNKNOWN>::iterator iter = _connections.begin();

    while( iter != end )
    {
        LPUNKNOWN pUnk = iter->second;
        if( NULL != pUnk )
        {
            IDispatch *pDisp;
            if( SUCCEEDED(pUnk->QueryInterface(_iid, (LPVOID *)&pDisp)) )
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
    map<DWORD,LPUNKNOWN>::iterator end = _connections.end();
    map<DWORD,LPUNKNOWN>::iterator iter = _connections.begin();

    while( iter != end )
    {
        LPUNKNOWN pUnk = iter->second;
        if( NULL != pUnk )
        {
            IPropertyNotifySink *pPropSink;
            if( SUCCEEDED(pUnk->QueryInterface(IID_IPropertyNotifySink, (LPVOID *)&pPropSink)) )
            {
                pPropSink->OnChanged(dispId);
                pPropSink->Release();
            }
        }
        ++iter;
    }
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
            _q_events.pop();
            _p_events->fireEvent(ev->_dispId, &ev->_dispParams);
            delete ev;
        }
    }
    _b_freeze = freeze;
};

void VLCConnectionPointContainer::fireEvent(DISPID dispId, DISPPARAMS* pDispParams)
{
    if( _b_freeze )
    {
        // queue event for later use when container is ready
        _q_events.push(new VLCDispatchEvent(dispId, *pDispParams));
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
    }
};

void VLCConnectionPointContainer::firePropChangedEvent(DISPID dispId)
{
    if( ! _b_freeze )
        _p_props->firePropChangedEvent(dispId);
};

