/*****************************************************************************
 * utils.h: ActiveX control for VLC
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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <ole2.h>

#include <vector>

// utilities
extern char *CStrFromWSTR(UINT codePage, LPCWSTR wstr, UINT len);
extern char *CStrFromBSTR(UINT codePage, BSTR bstr);
extern BSTR BSTRFromCStr(UINT codePage, LPCSTR s);

// properties
extern HRESULT GetObjectProperty(LPUNKNOWN object, DISPID dispID, VARIANT& v);

// properties
extern HDC CreateDevDC(DVTARGETDEVICE *ptd);
extern void DPFromHimetric(HDC hdc, LPPOINT pt, int count);
extern void HimetricFromDP(HDC hdc, LPPOINT pt, int count);

// URL
extern LPWSTR CombineURL(LPCWSTR baseUrl, LPCWSTR url);

/**************************************************************************************************/

/* this function object is used to dereference the iterator into a value */
template <typename T, class Iterator>
struct VLCDereference
{
    T operator()(const Iterator& i) const
    {
        return *i;
    };
};

template<REFIID EnumeratorIID, class Enumerator, typename T, class Iterator, typename Dereference = VLCDereference<T, Iterator> >
class VLCEnumIterator : public Enumerator
{

public:

    VLCEnumIterator(const Iterator& from, const Iterator& to) :
        _refcount(1),
        _begin(from),
        _curr(from),
        _end(to)
    {};

    VLCEnumIterator(const VLCEnumIterator& e) :
        Enumerator(),
        _refcount(e._refcount),
        _begin(e._begin),
        _curr(e._curr),
        _end(e._end)
    {};

    virtual ~VLCEnumIterator()
    {};

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
            return E_POINTER;
        if( (IID_IUnknown == riid)
         || (EnumeratorIID == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        // standalone object
        return E_NOINTERFACE;
    };

    STDMETHODIMP_(ULONG) AddRef(void)
    {
        return InterlockedIncrement(&_refcount);
    };

    STDMETHODIMP_(ULONG) Release(void)
    {
        ULONG refcount = InterlockedDecrement(&_refcount);
        if( 0 == refcount )
        {
            delete this;
            return 0;
        }
        return refcount;
    };


    // IEnumXXXX methods
    STDMETHODIMP Next(ULONG celt, T *rgelt, ULONG *pceltFetched)
    {
        if( NULL == rgelt )
            return E_POINTER;

        if( (celt > 1) && (NULL == pceltFetched) )
            return E_INVALIDARG;

        ULONG c = 0;

        while( (c < celt) && (_curr != _end) )
        {
            rgelt[c] = dereference(_curr);
            ++_curr;
            ++c;
        }

        if( NULL != pceltFetched )
            *pceltFetched = c;

        return (c == celt) ? S_OK : S_FALSE;
    };

    STDMETHODIMP Skip(ULONG celt)
    {
        ULONG c = 0;

        while( (c < celt) && (_curr != _end) )
        {
            ++_curr;
            ++c;
        }
        return (c == celt) ? S_OK : S_FALSE;
    };

    STDMETHODIMP Reset(void)
    {
        _curr = _begin;
        return S_OK;
    };

    STDMETHODIMP Clone(Enumerator **ppEnum)
    {
        if( NULL == ppEnum )
            return E_POINTER;
        *ppEnum = dynamic_cast<Enumerator *>(new VLCEnumIterator(*this));
        return (NULL != *ppEnum ) ? S_OK : E_OUTOFMEMORY;
    };

private:

    LONG     _refcount;
    Iterator _begin, _curr, _end;

    Dereference dereference;

};

#endif

