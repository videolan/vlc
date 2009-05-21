/*****************************************************************************
 * viewobject.cpp: ActiveX control for VLC
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "plugin.h"
#include "dataobject.h"

#include "utils.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////////

static const FORMATETC _metaFileFormatEtc =
{
    CF_METAFILEPICT,
    NULL,
    DVASPECT_CONTENT,
    -1,
    TYMED_MFPICT,
};
static const FORMATETC _enhMetaFileFormatEtc =
{
    CF_ENHMETAFILE,
    NULL,
    DVASPECT_CONTENT,
    -1,
    TYMED_ENHMF,
};

class VLCEnumFORMATETC : public VLCEnumIterator<IID_IEnumFORMATETC,
    IEnumFORMATETC,
    FORMATETC,
    vector<FORMATETC>::iterator>
{
public:
    VLCEnumFORMATETC(vector<FORMATETC> v) :
        VLCEnumIterator<IID_IEnumFORMATETC,
        IEnumFORMATETC,
        FORMATETC,
        vector<FORMATETC>::iterator>(v.begin(), v.end())
    {};
};

//////////////////////////////////////////////////////////////////////////////

VLCDataObject::VLCDataObject(VLCPlugin *p_instance) : _p_instance(p_instance)
{
    _v_formatEtc.push_back(_enhMetaFileFormatEtc);
    _v_formatEtc.push_back(_metaFileFormatEtc);
    CreateDataAdviseHolder(&_p_adviseHolder);
};

VLCDataObject::~VLCDataObject()
{
    _p_adviseHolder->Release();
};

//////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VLCDataObject::DAdvise(LPFORMATETC pFormatEtc, DWORD padvf,
                              LPADVISESINK pAdviseSink, LPDWORD pdwConnection)
{
    return _p_adviseHolder->Advise(this,
            pFormatEtc, padvf,pAdviseSink, pdwConnection);
};

STDMETHODIMP VLCDataObject::DUnadvise(DWORD dwConnection)
{
    return _p_adviseHolder->Unadvise(dwConnection);
};

STDMETHODIMP VLCDataObject::EnumDAdvise(IEnumSTATDATA **ppenumAdvise)
{
    return _p_adviseHolder->EnumAdvise(ppenumAdvise);
};

STDMETHODIMP VLCDataObject::EnumFormatEtc(DWORD dwDirection,
                                          IEnumFORMATETC **ppEnum)
{
    if( NULL == ppEnum )
        return E_POINTER;

    *ppEnum = new VLCEnumFORMATETC(_v_formatEtc);

    return (NULL != *ppEnum ) ? S_OK : E_OUTOFMEMORY;
};

STDMETHODIMP VLCDataObject::GetCanonicalFormatEtc(LPFORMATETC pFormatEtcIn,
                                                  LPFORMATETC pFormatEtcOut)
{
    HRESULT result = QueryGetData(pFormatEtcIn);
    if( FAILED(result) )
        return result;

    if( NULL == pFormatEtcOut )
        return E_POINTER;

    *pFormatEtcOut = *pFormatEtcIn;
    pFormatEtcOut->ptd = NULL;

    return DATA_S_SAMEFORMATETC;
};

STDMETHODIMP VLCDataObject::GetData(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium)
{
    if( NULL == pMedium )
        return E_POINTER;

    HRESULT result = QueryGetData(pFormatEtc);
    if( SUCCEEDED(result) )
    {
        switch( pFormatEtc->cfFormat )
        {
            case CF_METAFILEPICT:
                pMedium->tymed = TYMED_MFPICT;
                pMedium->hMetaFilePict = NULL;
                pMedium->pUnkForRelease = NULL;
                result = getMetaFileData(pFormatEtc, pMedium);
                break;
            case CF_ENHMETAFILE:
                pMedium->tymed = TYMED_ENHMF;
                pMedium->hEnhMetaFile = NULL;
                pMedium->pUnkForRelease = NULL;
                result = getEnhMetaFileData(pFormatEtc, pMedium);
                break;
            default:
                result = DV_E_FORMATETC;
        }
    }
    return result;
};

STDMETHODIMP VLCDataObject::GetDataHere(LPFORMATETC pFormatEtc,
                                        LPSTGMEDIUM pMedium)
{
    if( NULL == pMedium )
        return E_POINTER;

    return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////

HRESULT VLCDataObject::getMetaFileData(LPFORMATETC pFormatEtc,
                                       LPSTGMEDIUM pMedium)
{
    HDC hicTargetDev = CreateDevDC(pFormatEtc->ptd);
    if( NULL == hicTargetDev )
        return E_FAIL;

    HDC hdcMeta = CreateMetaFile(NULL);
    if( NULL != hdcMeta )
    {
        LPMETAFILEPICT pMetaFilePict =
                         (LPMETAFILEPICT)CoTaskMemAlloc(sizeof(METAFILEPICT));
        if( NULL != pMetaFilePict )
        {
            SIZEL size = _p_instance->getExtent();
            RECTL wBounds = { 0L, 0L, size.cx, size.cy };

            pMetaFilePict->mm   = MM_ANISOTROPIC;
            pMetaFilePict->xExt = size.cx;
            pMetaFilePict->yExt = size.cy;

            DPFromHimetric(hicTargetDev, (LPPOINT)&size, 1);

            SetMapMode(hdcMeta, MM_ANISOTROPIC);
            SetWindowExtEx(hdcMeta, size.cx, size.cy, NULL);

            RECTL bounds = { 0L, 0L, size.cx, size.cy };

            _p_instance->onDraw(pFormatEtc->ptd, hicTargetDev, hdcMeta,
                                &bounds, &wBounds);
            pMetaFilePict->hMF = CloseMetaFile(hdcMeta);
            if( NULL != pMetaFilePict->hMF )
                pMedium->hMetaFilePict = pMetaFilePict;
            else
                CoTaskMemFree(pMetaFilePict);
        }
    }
    DeleteDC(hicTargetDev);
    return (NULL != pMedium->hMetaFilePict) ? S_OK : E_FAIL;
};

HRESULT VLCDataObject::getEnhMetaFileData(LPFORMATETC pFormatEtc,
                                          LPSTGMEDIUM pMedium)
{
    HDC hicTargetDev = CreateDevDC(pFormatEtc->ptd);
    if( NULL == hicTargetDev )
        return E_FAIL;

    SIZEL size = _p_instance->getExtent();

    HDC hdcMeta = CreateEnhMetaFile(hicTargetDev, NULL, NULL, NULL);
    if( NULL != hdcMeta )
    {
        RECTL wBounds = { 0L, 0L, size.cx, size.cy };

        DPFromHimetric(hicTargetDev, (LPPOINT)&size, 1);

        RECTL bounds = { 0L, 0L, size.cx, size.cy };

        _p_instance->onDraw(pFormatEtc->ptd, hicTargetDev,
                            hdcMeta, &bounds, &wBounds);
        pMedium->hEnhMetaFile = CloseEnhMetaFile(hdcMeta);
    }
    DeleteDC(hicTargetDev);

    return (NULL != pMedium->hEnhMetaFile) ? S_OK : E_FAIL;
};

STDMETHODIMP VLCDataObject::QueryGetData(LPFORMATETC pFormatEtc)
{
    if( NULL == pFormatEtc )
        return E_POINTER;

    const FORMATETC *formatEtc;

    switch( pFormatEtc->cfFormat )
    {
        case CF_METAFILEPICT:
            formatEtc = &_metaFileFormatEtc;
            break;
        case CF_ENHMETAFILE:
            formatEtc = &_enhMetaFileFormatEtc;
            break;
        default:
            return DV_E_FORMATETC;
    }
 
    if( pFormatEtc->dwAspect != formatEtc->dwAspect )
        return DV_E_DVASPECT;

    if( pFormatEtc->lindex != formatEtc->lindex )
        return DV_E_LINDEX;

    if( pFormatEtc->tymed != formatEtc->tymed )
        return DV_E_TYMED;

    return S_OK;
};

STDMETHODIMP VLCDataObject::SetData(LPFORMATETC pFormatEtc,
                                    LPSTGMEDIUM pMedium, BOOL fRelease)
{
    return E_NOTIMPL;
};

/*void VLCDataObject::onDataChange(void)
{
    _p_adviseHolder->SendOnDataChange(this, 0, 0);
};*/

void VLCDataObject::onClose(void)
{
    _p_adviseHolder->SendOnDataChange(this, 0, ADVF_DATAONSTOP);
    if( S_OK == OleIsCurrentClipboard(dynamic_cast<LPDATAOBJECT>(this)) )
        OleFlushClipboard();
};

