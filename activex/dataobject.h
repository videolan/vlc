/*****************************************************************************
 * persiststorage.h: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
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

#ifndef __DATAOBJECT_H__
#define __DATAOBJECT_H__

#include <objidl.h>
#include <vector>

class VLCDataObject : public IDataObject
{

public:

    VLCDataObject(VLCPlugin *p_instance);
    virtual ~VLCDataObject();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv)
            return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDataObject == riid) ) {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->pUnkOuter->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->pUnkOuter->Release(); };

    // IDataObject methods
    STDMETHODIMP DAdvise(LPFORMATETC,DWORD,LPADVISESINK,LPDWORD);
    STDMETHODIMP DUnadvise(DWORD);
    STDMETHODIMP EnumDAdvise(IEnumSTATDATA**);
    STDMETHODIMP EnumFormatEtc(DWORD, IEnumFORMATETC**);
    STDMETHODIMP GetCanonicalFormatEtc(LPFORMATETC,LPFORMATETC);
    STDMETHODIMP GetData(LPFORMATETC,LPSTGMEDIUM);
    STDMETHODIMP GetDataHere(LPFORMATETC,LPSTGMEDIUM);
    STDMETHODIMP QueryGetData(LPFORMATETC);
    STDMETHODIMP SetData(LPFORMATETC,LPSTGMEDIUM,BOOL);

    void onClose(void);
private:

    HRESULT getMetaFileData(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium);
    HRESULT getEnhMetaFileData(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium);

    VLCPlugin *_p_instance;
    std::vector<FORMATETC> _v_formatEtc;
    IDataAdviseHolder *_p_adviseHolder;
};

#endif

