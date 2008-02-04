/*****************************************************************************
 * persiststorage.cpp: ActiveX control for VLC
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
#include "persiststorage.h"

using namespace std;

STDMETHODIMP VLCPersistStorage::GetClassID(LPCLSID pClsID)
{
    if( NULL == pClsID )
        return E_POINTER;

    *pClsID = _p_instance->getClassID();

    return S_OK;
};

STDMETHODIMP VLCPersistStorage::IsDirty(void)
{
    return _p_instance->isDirty() ? S_OK : S_FALSE;
};

STDMETHODIMP VLCPersistStorage::InitNew(LPSTORAGE pStg)
{
    return _p_instance->onInit();
};

STDMETHODIMP VLCPersistStorage::Load(LPSTORAGE pStg)
{
    if( NULL == pStg )
        return E_INVALIDARG;

    LPSTREAM pStm = NULL;
    HRESULT result = pStg->OpenStream(L"VideoLAN ActiveX Plugin Data", NULL,
                        STGM_READ|STGM_SHARE_EXCLUSIVE, 0, &pStm);

    if( FAILED(result) )
        return result;

    LPPERSISTSTREAMINIT pPersistStreamInit;
    if( SUCCEEDED(QueryInterface(IID_IPersistStreamInit, (void **)&pPersistStreamInit)) )
    {
        result = pPersistStreamInit->Load(pStm);
        pPersistStreamInit->Release();
    }

    pStm->Release();

    return result;
};

STDMETHODIMP VLCPersistStorage::Save(LPSTORAGE pStg, BOOL fSameAsLoad)
{
    if( NULL == pStg )
        return E_INVALIDARG;

    if( fSameAsLoad && (S_FALSE == IsDirty()) )
        return S_OK;

    LPSTREAM pStm = NULL;
    HRESULT result = pStg->CreateStream(L"VideoLAN ActiveX Plugin Data",
                        STGM_CREATE|STGM_READWRITE|STGM_SHARE_EXCLUSIVE, 0, 0, &pStm);

    if( FAILED(result) )
        return result;

    LPPERSISTSTREAMINIT pPersistStreamInit;
    if( SUCCEEDED(QueryInterface(IID_IPersistStreamInit, (void **)&pPersistStreamInit)) )
    {
        result = pPersistStreamInit->Save(pStm, fSameAsLoad);
        pPersistStreamInit->Release();
    }

    pStm->Release();

    return result;
};

STDMETHODIMP VLCPersistStorage::SaveCompleted(IStorage *pStg)
{
    return S_OK;
};

STDMETHODIMP VLCPersistStorage::HandsOffStorage(void)
{
    return S_OK;
};

