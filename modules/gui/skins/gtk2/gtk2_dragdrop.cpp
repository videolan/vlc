/*****************************************************************************
 * gtk2_dragdrop.cpp: GTK2 implementation of the drag & drop
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_dragdrop.cpp,v 1.2 2003/04/13 19:09:59 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
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

#if !defined WIN32

//--- GTK2 -----------------------------------------------------------------
//#include <windows.h>

//--- SKIN ------------------------------------------------------------------
#include "event.h"
#include "gtk2_dragdrop.h"


/*
//---------------------------------------------------------------------------
GTK2DropObject::GTK2DropObject() : IDropTarget()
{
    References = 1;
}
//---------------------------------------------------------------------------
GTK2DropObject::~GTK2DropObject()
{
}
//---------------------------------------------------------------------------
void GTK2DropObject::HandleDrop( HDROP HDrop )
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
STDMETHODIMP GTK2DropObject::QueryInterface( REFIID iid, void FAR* FAR* ppv )
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
STDMETHODIMP_(ULONG) GTK2DropObject::AddRef()
{
    return ++References;
}
//---------------------------------------------------------------------------
STDMETHODIMP_(ULONG) GTK2DropObject::Release()
{
    if( --References == 0 )
    {
        delete this;
        return 0;
    }
    return References;
}
//---------------------------------------------------------------------------
STDMETHODIMP GTK2DropObject::DragEnter( LPDATAOBJECT pDataObj,
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
STDMETHODIMP GTK2DropObject::DragOver( DWORD grfKeyState, POINTL pt,
   DWORD *pdwEffect )
{
    // For visual feedback
    // TODO
    return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP GTK2DropObject::DragLeave()
{
    // Remove visual feedback
    // TODO
    return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP GTK2DropObject::Drop( LPDATAOBJECT pDataObj, DWORD grfKeyState,
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
*/

#endif
