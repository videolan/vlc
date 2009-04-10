/*****************************************************************************
 * persiststreaminit.cpp: ActiveX control for VLC
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
#include "persiststreaminit.h"

#include "utils.h"
#include <map>

#include <malloc.h>
#include <wchar.h>

using namespace std;

class AxVLCVariant
{

public:

    AxVLCVariant(void)
    {
        VariantInit(&_v);
    };

    ~AxVLCVariant(void)
    {
        VariantClear(&_v);
    }

    AxVLCVariant(VARIANTARG &v)
    {
        VariantInit(&_v);
        VariantCopy(&_v, &v);
    };

    AxVLCVariant(VARIANTARG *v)
    {
        VariantInit(&_v);
        VariantCopy(&_v, v);
    };

    AxVLCVariant(const AxVLCVariant &vv)
    {
        VariantInit(&_v);
        VariantCopy(&_v, const_cast<VARIANTARG *>(&(vv._v)));
    };

    AxVLCVariant(int i)
    {
        V_VT(&_v) = VT_I4;
        V_I4(&_v) = i;
    };

    AxVLCVariant(BSTR bstr)
    {
        VARIANT arg;
        V_VT(&arg) = VT_BSTR;
        V_BSTR(&arg) = bstr;
        VariantInit(&_v);
        VariantCopy(&_v, &arg);
    };

    inline const VARIANTARG *variantArg(void) const {
        return &_v;
    }

    inline void swap(AxVLCVariant &v1, AxVLCVariant &v2)
    {
        VARIANTARG tmp = v1._v;
        v1._v = v2._v;
        v2._v = tmp;
    };

private:

    VARIANTARG _v;
};

class AxVLCWSTR
{

public:

    AxVLCWSTR(void) : _data(NULL) {};

    virtual ~AxVLCWSTR()
    {
        if( NULL != _data )
        {
            ULONG refcount = InterlockedDecrement(&(_data->refcount));
            if( 0 == refcount )
                CoTaskMemFree(_data);
        }
    };

    AxVLCWSTR(LPCWSTR s)
    {
        if( NULL != s )
        {
            size_t len = wcslen(s);
            if( len > 0 )
            {
                size_t size = len*sizeof(WCHAR);
                _data = (struct data *)CoTaskMemAlloc(sizeof(struct data)+size);
                if( NULL != _data )
                {
                    _data->len = len;
                    _data->refcount = 1;
                    memcpy(_data->wstr, s, size);
                    _data->wstr[len]=L'\0';
                    return;
                }
            }
        }
        _data = NULL;
    };

    AxVLCWSTR(const AxVLCWSTR &s)
    {
        _data = s._data;
        if( NULL != _data )
            InterlockedIncrement(&(_data->refcount));
    };

    inline bool operator<(const AxVLCWSTR &s) const
    {
        return compareNoCase(s.wstr()) < 0;
    };

    inline bool operator<(LPCWSTR s) const
    {
        return compareNoCase(s) < 0;
    };

    inline bool operator==(const AxVLCWSTR &s) const
    {
        return size() == s.size() ?
                    (compareNoCase(s.wstr()) == 0) : false;
    };

    inline bool operator==(LPCWSTR s) const
    {
        return compareNoCase(s) == 0;
    };

    LPCWSTR wstr(void) const
    {
        return (NULL != _data) ? _data->wstr : NULL;
    };

    size_t size(void) const
    {
        return (NULL != _data) ? _data->len : 0;
    };

private:

    inline int compareNoCase(LPCWSTR s) const
    {
        if( NULL == _data )
        {
            return (NULL == s) ? 0 : -1;
        }
        if( NULL == s )
            return 1;

        return _wcsicmp(_data->wstr, s);
    };

    struct data {
        size_t  len;
        LONG    refcount;
        wchar_t wstr[1];
    } *_data;
};

typedef pair<class AxVLCWSTR, class AxVLCVariant> AxVLCPropertyPair;
typedef map<class AxVLCWSTR, class AxVLCVariant> AxVLCPropertyMap;

///////////////////////////

class VLCPropertyBag : public IPropertyBag
{

public:

    VLCPropertyBag(void) : _i_ref(1) {};
    virtual ~VLCPropertyBag() {};

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
            return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IPropertyBag == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        // standalone object
        return E_NOINTERFACE;
    };

    STDMETHODIMP_(ULONG) AddRef(void)
        { return InterlockedIncrement(&_i_ref); };

    STDMETHODIMP_(ULONG) Release(void)
    {
        ULONG refcount = InterlockedDecrement(&_i_ref);
        if( 0 == refcount )
        {
            delete this;
            return 0;
        }
        return refcount;
    };

    // IPropertyBag methods

    STDMETHODIMP Read(LPCOLESTR pszPropName, VARIANT *pVar, IErrorLog *pErrorLog)
    {
        if( (NULL == pszPropName) || (NULL == pVar) )
            return E_POINTER;

        AxVLCPropertyMap::const_iterator notfound = _pm.end();
        AxVLCPropertyMap::const_iterator iter = _pm.find(pszPropName);
        if( notfound != iter )
        {
            VARTYPE vtype = V_VT(pVar);
            VARIANTARG v;
            VariantInit(&v);
            VariantCopy(&v, const_cast<VARIANTARG*>((*iter).second.variantArg()));
            if( (V_VT(&v) != vtype) && FAILED(VariantChangeType(&v, &v, 0, vtype)) )
            {
                VariantClear(&v);
                return E_FAIL;
            }
            *pVar = v;
            return S_OK;
        }
        else
            return E_INVALIDARG;
    };
 
    STDMETHODIMP Write(LPCOLESTR pszPropName, VARIANT *pVar)
    {
        if( (NULL == pszPropName) || (NULL == pVar) )
            return E_POINTER;

        AxVLCPropertyPair val(pszPropName, pVar);
        pair<AxVLCPropertyMap::iterator, bool> p = _pm.insert(val);
        if( false == p.second )
            // replace existing key value
            (*p.first).second = val.second;
        return S_OK;
    };

    // custom methods

    HRESULT Load(LPSTREAM pStm)
    {
        if( NULL == pStm )
            return E_INVALIDARG;

        HRESULT result;

        AxVLCPropertyPair *val;
        result = ReadProperty(pStm, &val);
        if( SUCCEEDED(result) )
        {
            if( (val->first == L"(Count)") && (VT_I4 == V_VT(val->second.variantArg())) )
            {
                size_t count = V_I4(val->second.variantArg());
                delete val;
                while( count-- )
                {
                    result = ReadProperty(pStm, &val);
                    if( FAILED(result) )
                        return result;

                    pair<AxVLCPropertyMap::iterator, bool> p = _pm.insert(*val);
                    if( false == p.second )
                        // replace existing key value
                        (*p.first).second = val->second;
                    delete val;
                }
            }
        }
        return result;
    };

    HRESULT Save(LPSTREAM pStm)
    {
        if( NULL == pStm )
            return E_INVALIDARG;

        HRESULT result;

        AxVLCPropertyPair header(L"(Count)", _pm.size());
        result = WriteProperty(pStm, header);
        if( SUCCEEDED(result) )
        {
            AxVLCPropertyMap::const_iterator iter = _pm.begin();
            AxVLCPropertyMap::const_iterator end  = _pm.end();

            while( iter != end )
            {
                result = WriteProperty(pStm, *(iter++));
                if( FAILED(result) )
                    return result;
            }
        }
        return result;
    };

    BOOL IsEmpty()
    {
        return _pm.size() == 0;
    }

private:

    HRESULT WriteProperty(LPSTREAM pStm, const AxVLCPropertyPair &prop)
    {
        HRESULT result;

        const AxVLCWSTR propName = prop.first;

        ULONG len = propName.size();

        if( 0 == len )
            return E_INVALIDARG;

        result = pStm->Write(&len, sizeof(len), NULL);
        if( FAILED(result) )
            return result;

        result = pStm->Write(propName.wstr(), len*sizeof(WCHAR), NULL);
        if( FAILED(result) )
            return result;

        const VARIANTARG *propValue = prop.second.variantArg();
        VARTYPE vtype = V_VT(propValue);
        switch( vtype )
        {
            case VT_BOOL:
                result = pStm->Write(&vtype, sizeof(vtype), NULL);
                if( FAILED(result) )
                    return result;
                result = pStm->Write(&V_BOOL(propValue), sizeof(V_BOOL(propValue)), NULL);
                if( FAILED(result) )
                    return result;
                break;
            case VT_I4:
                result = pStm->Write(&vtype, sizeof(vtype), NULL);
                if( FAILED(result) )
                    return result;
                result = pStm->Write(&V_I4(propValue), sizeof(V_I4(propValue)), NULL);
                if( FAILED(result) )
                    return result;
                break;
            case VT_BSTR:
                result = pStm->Write(&vtype, sizeof(vtype), NULL);
                if( FAILED(result) )
                    return result;
                len = SysStringLen(V_BSTR(propValue));
                result = pStm->Write(&len, sizeof(len), NULL);
                if( FAILED(result) )
                    return result;
                if( len > 0 )
                {
                    result = pStm->Write(V_BSTR(propValue), len*sizeof(OLECHAR), NULL);
                    if( FAILED(result) )
                        return result;
                }
                break;
            default:
                vtype = VT_EMPTY;
                result = pStm->Write(&vtype, sizeof(vtype), NULL);
                if( FAILED(result) )
                    return result;
        }
        return result;
    };

    HRESULT ReadProperty(LPSTREAM pStm, AxVLCPropertyPair **prop)
    {
        HRESULT result;

        ULONG len;

        result = pStm->Read(&len, sizeof(len), NULL);
        if( FAILED(result) )
            return result;

        if( 0 == len )
            return E_INVALIDARG;

        WCHAR propName[len + 1];

        result = pStm->Read(propName, len*sizeof(WCHAR), NULL);
        if( FAILED(result) )
            return result;

        propName[len] = L'\0';

        VARIANTARG propValue;

        VARTYPE vtype;
        result = pStm->Read(&vtype, sizeof(vtype), NULL);
        if( FAILED(result) )
            return result;

        switch( vtype )
        {
            case VT_BOOL:
                V_VT(&propValue) = vtype;
                result = pStm->Read(&V_BOOL(&propValue), sizeof(V_BOOL(&propValue)), NULL);
                if( FAILED(result) )
                    return result;
                break;
            case VT_I4:
                V_VT(&propValue) = vtype;
                result = pStm->Read(&V_I4(&propValue), sizeof(V_I4(&propValue)), NULL);
                if( FAILED(result) )
                    return result;
                break;
            case VT_BSTR:
                V_VT(&propValue) = vtype;
                result = pStm->Read(&len, sizeof(len), NULL);
                if( FAILED(result) )
                    return result;

                V_BSTR(&propValue) = NULL;
                if( len > 0 )
                {
                    V_BSTR(&propValue) = SysAllocStringLen(NULL, len);
                    if( NULL == V_BSTR(&propValue) )
                        return E_OUTOFMEMORY;

                    result = pStm->Read(V_BSTR(&propValue), len*sizeof(OLECHAR), NULL);
                    if( FAILED(result) )
                    {
                        SysFreeString(V_BSTR(&propValue));
                        return result;
                    }
                }
                break;
            default:
                VariantInit(&propValue);
        }

        *prop = new AxVLCPropertyPair(propName, propValue);

        return S_OK;
    };

    AxVLCPropertyMap _pm;
    LONG _i_ref;
};

///////////////////////////

VLCPersistStreamInit::VLCPersistStreamInit(VLCPlugin *p_instance) : _p_instance(p_instance)
{
    _p_props = new VLCPropertyBag();
};

VLCPersistStreamInit::~VLCPersistStreamInit()
{
    _p_props->Release();
};

STDMETHODIMP VLCPersistStreamInit::GetClassID(LPCLSID pClsID)
{
    if( NULL == pClsID )
        return E_POINTER;

    *pClsID = _p_instance->getClassID();

    return S_OK;
};

STDMETHODIMP VLCPersistStreamInit::InitNew(void)
{
    return _p_instance->onInit();
};

STDMETHODIMP VLCPersistStreamInit::Load(LPSTREAM pStm)
{
    HRESULT result = _p_props->Load(pStm);
    if( FAILED(result) )
        return result;

    LPPERSISTPROPERTYBAG pPersistPropBag;
    if( FAILED(QueryInterface(IID_IPersistPropertyBag, (void**)&pPersistPropBag)) )
        return E_FAIL;

    result = pPersistPropBag->Load(_p_props, NULL);
    pPersistPropBag->Release();

    return result;
};

STDMETHODIMP VLCPersistStreamInit::Save(LPSTREAM pStm, BOOL fClearDirty)
{
    if( NULL == pStm )
        return E_INVALIDARG;

    LPPERSISTPROPERTYBAG pPersistPropBag;
    if( FAILED(QueryInterface(IID_IPersistPropertyBag, (void**)&pPersistPropBag)) )
        return E_FAIL;

    HRESULT result = pPersistPropBag->Save(_p_props, fClearDirty, _p_props->IsEmpty());
    pPersistPropBag->Release();
    if( FAILED(result) )
        return result;

    return _p_props->Save(pStm);
};

STDMETHODIMP VLCPersistStreamInit::IsDirty(void)
{
    return _p_instance->isDirty() ? S_OK : S_FALSE;
};

STDMETHODIMP VLCPersistStreamInit::GetSizeMax(ULARGE_INTEGER *pcbSize)
{
    pcbSize->HighPart = 0UL;
    pcbSize->LowPart  = 16384UL; // just a guess

    return S_OK;
};

