/*****************************************************************************
 * win32_dragdrop.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifdef WIN32_SKINS

#include <windows.h>
#include "win32_dragdrop.hpp"
#include "../commands/cmd_add_item.hpp"


Win32DragDrop::Win32DragDrop( intf_thread_t *pIntf, bool playOnDrop ):
    SkinObject( pIntf ), IDropTarget(), m_references( 1 ),
    m_playOnDrop( playOnDrop )
{
}


STDMETHODIMP Win32DragDrop::QueryInterface( REFIID iid, void FAR* FAR* ppv )
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


STDMETHODIMP_(ULONG) Win32DragDrop::AddRef()
{
    return ++m_references;
}


STDMETHODIMP_(ULONG) Win32DragDrop::Release()
{
    if( --m_references == 0 )
    {
        delete this;
        return 0;
    }
    return m_references;
}


STDMETHODIMP Win32DragDrop::DragEnter( LPDATAOBJECT pDataObj,
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
    {
        *pdwEffect = DROPEFFECT_COPY;
    }
    else
    {
        *pdwEffect = DROPEFFECT_NONE;
    }

    return S_OK;
}


STDMETHODIMP Win32DragDrop::DragOver( DWORD grfKeyState, POINTL pt,
                                      DWORD *pdwEffect )
{
    // For visual feedback
    return S_OK;
}


STDMETHODIMP Win32DragDrop::DragLeave()
{
    // Remove visual feedback
    return S_OK;
}


STDMETHODIMP Win32DragDrop::Drop( LPDATAOBJECT pDataObj, DWORD grfKeyState,
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

        // Notify VLC of the drop
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


void Win32DragDrop::HandleDrop( HDROP HDrop )
{
    // Get the number of dropped files
    int nbFiles = DragQueryFile( HDrop, 0xFFFFFFFF, NULL, 0 );

    // For each dropped file
    for( int i = 0; i < nbFiles; i++ )
    {
        // Get the name of the file
        int nameLength = DragQueryFile( HDrop, i, NULL, 0 ) + 1;
        char *psz_fileName = new char[nameLength];
        DragQueryFile( HDrop, i, psz_fileName, nameLength );

        // Add the file
        CmdAddItem cmd( getIntf(), psz_fileName, m_playOnDrop );
        cmd.execute();

        delete[] psz_fileName;
    }

    DragFinish( HDrop );
}


#endif
