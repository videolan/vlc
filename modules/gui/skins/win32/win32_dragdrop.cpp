/*****************************************************************************
 * win32_dragdrop.cpp: Win32 implementation of the drag & drop
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_dragdrop.cpp,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


//--- GENERAL ---------------------------------------------------------------
#include <list>
using namespace std;

//--- WIN32 -----------------------------------------------------------------
#include <windows.h>

//--- SKIN ------------------------------------------------------------------
#include "event.h"
#include "win32_dragdrop.h"



//---------------------------------------------------------------------------
Win32DropObject::Win32DropObject() : IDropTarget()
{
    References = 1;
}
//---------------------------------------------------------------------------
Win32DropObject::~Win32DropObject()
{
}
//---------------------------------------------------------------------------
void Win32DropObject::HandleDrop( HDROP HDrop )
{
    // Get number of files that are dropped into vlc
    int NbFiles = DragQueryFile( (HDROP)HDrop, 0xFFFFFFFF, NULL, 0 );

    // For each dropped files
    for( int i = 0; i < NbFiles; i++ )
    {
        // Get the name of the file
        int NameLength = DragQueryFile( (HDROP)HDrop, i, NULL, 0 ) + 1;
        char *FileName = new char[NameLength];
        DragQueryFile( (HDROP)HDrop, i, FileName, NameLength );

        // The pointer must not be deleted here because it will be deleted
        // in the VLC specific messages processing function
        PostMessage( NULL, VLC_DROP, (WPARAM)FileName, 0 );
    }

    DragFinish( (HDROP)HDrop );

}
//---------------------------------------------------------------------------
STDMETHODIMP Win32DropObject::QueryInterface( REFIID iid, void FAR* FAR* ppv )
{
    // Tell other objects about our capabilities
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
STDMETHODIMP_(ULONG) Win32DropObject::AddRef()
{
    return ++References;
}
//---------------------------------------------------------------------------
STDMETHODIMP_(ULONG) Win32DropObject::Release()
{
    if( --References == 0 )
    {
        delete this;
        return 0;
    }
    return References;
}
//---------------------------------------------------------------------------
STDMETHODIMP Win32DropObject::DragEnter( LPDATAOBJECT pDataObj,
    DWORD grfKeyState, POINTL pt, DWORD *pdwEffect )
{
    FORMATETC fmtetc;

    fmtetc.cfFormat = CF_HDROP;
    fmtetc.ptd      = NULL;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex   = -1;
    fmtetc.tymed    = TYMED_HGLOBAL;

    // Check that the drag source provides CF_HDROP,
    // which is the only format we accept
    if( pDataObj->QueryGetData( &fmtetc ) == S_OK )
        *pdwEffect = DROPEFFECT_COPY;
    else
        *pdwEffect = DROPEFFECT_NONE;

    return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP Win32DropObject::DragOver( DWORD grfKeyState, POINTL pt,
   DWORD *pdwEffect )
{
    // For visual feedback
    // TODO
    return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP Win32DropObject::DragLeave()
{
    // Remove visual feedback
    // TODO
    return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP Win32DropObject::Drop( LPDATAOBJECT pDataObj, DWORD grfKeyState,
   POINTL pt, DWORD *pdwEffect )
{
    // User has dropped on us -- get the CF_HDROP data from drag source
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
        // Grab a pointer to the data
        HGLOBAL HFiles = medium.hGlobal;
        HDROP HDrop = (HDROP)GlobalLock( HFiles );

        // Notify the Form of the drop
        HandleDrop( HDrop );

        // Release the pointer to the memory
        GlobalUnlock( HFiles );
//        ReleaseStgMedium( &medium );
    }
    else
    {
        *pdwEffect = DROPEFFECT_NONE;
        return hr;
    }
    return S_OK;
}

