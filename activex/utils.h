/*****************************************************************************
 * utils.h: ActiveX control for VLC
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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <ole2.h>

#include <vector>

// utilities
extern char *CStrFromBSTR(int codePage, BSTR bstr);
extern BSTR BSTRFromCStr(int codePage, const char *s);

// properties
extern HRESULT GetObjectProperty(LPUNKNOWN object, DISPID dispID, VARIANT& v);

// properties
extern HDC CreateDevDC(DVTARGETDEVICE *ptd);

// enumeration
template<class T> class VLCEnum : IUnknown
{

public:

    VLCEnum(REFIID riid, std::vector<T> &);
    VLCEnum(const VLCEnum<T> &);
    virtual ~VLCEnum() {};

    VLCEnum<T>& operator=(const VLCEnum<T> &t);

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    // IEnumXXXX methods
    STDMETHODIMP Next(ULONG, T *, ULONG *);
    STDMETHODIMP Skip(ULONG);
    STDMETHODIMP Reset(void);
    // cloning is implemented by subclasses and must use copy constructor
    //STDMETHODIMP Clone(VLCEnum<T> **);

    typedef void (*retainer)(T);

    void setRetainOperation(retainer retain) { _retain = retain; };

private:

    LONG                                _refcount;
    std::vector<T>                      _v;
    typename std::vector<T>::iterator   _i;
    REFIID                              _riid;
    retainer                            _retain;
};

template<class T>
VLCEnum<T>::VLCEnum(REFIID riid, std::vector<T> &v) :
    _refcount(1),
    _v(v),
    _riid(riid),
    _retain(NULL)
{
    _i= v.begin();
};

template<class T>
VLCEnum<T>::VLCEnum(const VLCEnum<T> &e) :
    _refcount(1),
    _v(e._v),
    _riid(e._riid)
{
};

template<class T>
VLCEnum<T>& VLCEnum<T>::operator=(const VLCEnum<T> &e)
{
    this->_refcount = 1;
    this->_riid = e._riid;
    this->_v    = e._v;
    this->_i    = e._i;
};

template<class T>
STDMETHODIMP VLCEnum<T>::QueryInterface(REFIID riid, void **ppv)
{
    if( NULL == ppv ) return E_POINTER;
    if( (IID_IUnknown == riid) 
     && ( _riid == riid) ) {
        AddRef();
        *ppv = reinterpret_cast<LPVOID>(this);
        return NOERROR;
    }
    return E_NOINTERFACE;
};

template<class T>
STDMETHODIMP_(ULONG) VLCEnum<T>::AddRef(void)
{
    return InterlockedIncrement(&_refcount);
};

template<class T>
STDMETHODIMP_(ULONG) VLCEnum<T>::Release(void)
{
    ULONG refcount = InterlockedDecrement(&_refcount);
    if( 0 == refcount )
    {
        delete this;
        return 0;
    }
    return refcount;
};

template<class T>
STDMETHODIMP VLCEnum<T>::Next(ULONG celt, T *rgelt, ULONG *pceltFetched)
{
    if( NULL == rgelt )
        return E_POINTER;

    if( (celt > 1) && (NULL == pceltFetched) )
        return E_INVALIDARG;

    ULONG c = 0;
    typename std::vector<T>::iterator end = _v.end();

    while( (c < celt) && (_i != end) )
    {
        rgelt[c] = *_i;
        if( NULL != _retain ) _retain(rgelt[c]);
        ++_i;
        ++c;
    }

    if( NULL != pceltFetched )
        *pceltFetched = c;

    return (c == celt) ? S_OK : S_FALSE;
};

template<class T>
STDMETHODIMP VLCEnum<T>::Skip(ULONG celt)
{
    ULONG c = 0;
    typename std::vector<T>::iterator end = _v.end();

    while( (c < celt) && (_i != end) )
    {
        ++_i;
        ++c;
    }
    return (c == celt) ? S_OK : S_FALSE;
};

template<class T>
STDMETHODIMP VLCEnum<T>::Reset(void)
{
    _i= _v.begin();
    return S_OK;
};

#endif

