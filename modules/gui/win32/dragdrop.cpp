/*****************************************************************************
 * dragdrop.cpp: drag and drop management
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
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

#include <vcl.h>
#pragma hdrstop

#include "dragdrop.h"

//---------------------------------------------------------------------------
__fastcall TDropTarget::TDropTarget( HWND HForm ) : IDropTarget()
{
    FormHandle = HForm;
    References = 1;
}
//---------------------------------------------------------------------------
__fastcall TDropTarget::~TDropTarget()
{
}
//---------------------------------------------------------------------------
/* helper routine to notify Form of drop on target */
void __fastcall TDropTarget::HandleDrop( HDROP HDrop )
{
    SendMessage( FormHandle, WM_OLEDROP, (WPARAM)HDrop, 0 );
}
//---------------------------------------------------------------------------
STDMETHODIMP TDropTarget::QueryInterface( REFIID iid, void FAR* FAR* ppv )
{
    /* tell other objects about our capabilities */
    if( iid == IID_IUnknown || iid == IID_IDropTarget )
    {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return ResultFromScode( E_NOINTERFACE );
}
//---------------------------------------------------------------------------
STDMETHODIMP_(ULONG) TDropTarget::AddRef()
{
    return ++References;
}
//---------------------------------------------------------------------------
STDMETHODIMP_(ULONG) TDropTarget::Release()
{
    if( --References == 0 )
    {
        delete this;
        return 0;
    }
    return References;
}
//---------------------------------------------------------------------------
/* Indicates whether a drop can be accepted, and, if so,
 * the effect of the drop */
STDMETHODIMP TDropTarget::DragEnter( LPDATAOBJECT pDataObj, DWORD grfKeyState,
   POINTL pt, DWORD *pdwEffect )
{
    FORMATETC fmtetc;

    fmtetc.cfFormat = CF_HDROP;
    fmtetc.ptd      = NULL;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex   = -1;
    fmtetc.tymed    = TYMED_HGLOBAL;

    /* Check that the drag source provides CF_HDROP,
     * which is the only format we accept */
    if( pDataObj->QueryGetData( &fmtetc ) == S_OK )
        *pdwEffect = DROPEFFECT_COPY;
    else
        *pdwEffect = DROPEFFECT_NONE;

    return S_OK;
}
//---------------------------------------------------------------------------
/* for visual feedback */
STDMETHODIMP TDropTarget::DragOver( DWORD grfKeyState, POINTL pt,
   DWORD *pdwEffect )
{
    return S_OK;
}
//---------------------------------------------------------------------------
/* remove visual feedback */
STDMETHODIMP TDropTarget::DragLeave()
{
    return S_OK;
}
//---------------------------------------------------------------------------
/* something has been dropped */
STDMETHODIMP TDropTarget::Drop( LPDATAOBJECT pDataObj, DWORD grfKeyState,
   POINTL pt, DWORD *pdwEffect )
{
    /* user has dropped on us -- get the CF_HDROP data from drag source */
    FORMATETC fmtetc;
    fmtetc.cfFormat = CF_HDROP;
    fmtetc.ptd      = NULL;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex   = -1;
    fmtetc.tymed    = TYMED_HGLOBAL;

    STGMEDIUM medium;
    HRESULT hr = pDataObj->GetData( &fmtetc, &medium );

    if( !FAILED(hr) )
    {
        /* grab a pointer to the data */
        HGLOBAL HFiles = medium.hGlobal;
        HDROP HDrop = (HDROP)GlobalLock( HFiles );

        /* call the helper routine which will notify the Form of the drop */
        HandleDrop( HDrop );

        /* release the pointer to the memory */
        GlobalUnlock( HFiles );
        ReleaseStgMedium( &medium );
    }
    else
    {
        *pdwEffect = DROPEFFECT_NONE;
        return hr;
    }
    return S_OK;
}

