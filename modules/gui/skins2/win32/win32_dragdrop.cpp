/*****************************************************************************
 * win32_dragdrop.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#include "win32_dragdrop.hpp"
#include "../commands/cmd_add_item.hpp"
#include "../events/evt_dragndrop.hpp"
#include <list>
#include <shlobj.h>


Win32DragDrop::Win32DragDrop( intf_thread_t *pIntf,
                              bool playOnDrop, GenericWindow* pWin )
    : SkinObject( pIntf ), IDropTarget(), m_references( 1 ),
      m_playOnDrop( playOnDrop ), m_pWin( pWin), m_format( { 0, NULL} )
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
    return E_NOINTERFACE;
}


STDMETHODIMP_(ULONG) Win32DragDrop::AddRef()
{
    return InterlockedIncrement( &m_references );
}


STDMETHODIMP_(ULONG) Win32DragDrop::Release()
{
    LONG count = InterlockedDecrement( &m_references );
    if( count == 0 )
    {
        delete this;
        return 0;
    }
    return count;
}


STDMETHODIMP Win32DragDrop::DragEnter( LPDATAOBJECT pDataObj,
    DWORD grfKeyState, POINTL pt, DWORD *pdwEffect )
{
    (void)grfKeyState; (void)pt;

    // enumerate all data formats of HGLOBAL medium type
    // that the source is offering
    std::list<UINT> formats;
    IEnumFORMATETC *pEnum;
    if( pDataObj->EnumFormatEtc( DATADIR_GET, &pEnum ) == S_OK )
    {
        FORMATETC fe;
        while( pEnum->Next( 1, &fe, NULL ) == S_OK )
        {
            if( fe.tymed == TYMED_HGLOBAL )
                formats.push_back( fe.cfFormat );
        }
    }

    // List of all data formats that we are interested in
    // sorted by order of preference
    static UINT ft_url = RegisterClipboardFormat( CFSTR_INETURLW );
    static const struct {
        UINT format;
        const char* name;
    } preferred[] = {
        { ft_url, "CFSTR_INETURLW" },
        { CF_HDROP, "CF_HDROP" },
        { CF_TEXT, "CF_TEXT" },
    };

    // select the preferred format among those offered by the source
    m_format = { 0, NULL };
    for( unsigned i = 0; i < sizeof(preferred)/sizeof(preferred[0]); i++ )
    {
        for( std::list<UINT>::iterator it = formats.begin();
             it != formats.end(); ++it )
        {
            if( *it == preferred[i].format )
            {
                m_format = { preferred[i].format, preferred[i].name };
                msg_Dbg( getIntf(), "drag&drop selected format: %s",
                                     m_format.name );
                break;
            }
        }
        if( m_format.format )
            break;
    }

    // Check that the drag source provides the selected format
    FORMATETC fmtetc;
    fmtetc.cfFormat = m_format.format;
    fmtetc.ptd      = NULL;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex   = -1;
    fmtetc.tymed    = TYMED_HGLOBAL;
    if( m_format.format && pDataObj->QueryGetData( &fmtetc ) == S_OK )
        *pdwEffect = DROPEFFECT_COPY;
    else
        *pdwEffect = DROPEFFECT_NONE;

    // transmit DragEnter event
    EvtDragEnter evt( getIntf() );
    m_pWin->processEvent( evt );

    return S_OK;
}


STDMETHODIMP Win32DragDrop::DragOver( DWORD grfKeyState, POINTL pt,
                                      DWORD *pdwEffect )
{
    (void)grfKeyState; (void)pdwEffect;
    // transmit DragOver event
    EvtDragOver evt( getIntf(), pt.x, pt.y );
    m_pWin->processEvent( evt );

    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}


STDMETHODIMP Win32DragDrop::DragLeave()
{
    // transmit DragLeave event
    EvtDragLeave evt( getIntf() );
    m_pWin->processEvent( evt );

    // Remove visual feedback
    return S_OK;
}


STDMETHODIMP Win32DragDrop::Drop( LPDATAOBJECT pDataObj, DWORD grfKeyState,
    POINTL pt, DWORD *pdwEffect )
{
    (void)grfKeyState;

    // On Drop, retrieve and process the data
    FORMATETC fmtetc;
    fmtetc.cfFormat = m_format.format;
    fmtetc.ptd      = NULL;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex   = -1;
    fmtetc.tymed    = TYMED_HGLOBAL;

    STGMEDIUM medium;
    if( FAILED( pDataObj->GetData( &fmtetc, &medium ) ) )
    {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    std::list<std::string> files;
    if( !strcmp( m_format.name, "CFSTR_INETURLW" ) )
    {
        wchar_t* data = (wchar_t*)GlobalLock( medium.hGlobal );
        files.push_back( sFromWide(data) );
        GlobalUnlock( medium.hGlobal );
    }
    else if( m_format.format == CF_HDROP )
    {
        HDROP HDrop = (HDROP)GlobalLock( medium.hGlobal );
        // Get the number of dropped files
        int nbFiles = DragQueryFileW( HDrop, 0xFFFFFFFF, NULL, 0 );
        for( int i = 0; i < nbFiles; i++ )
        {
            // Get the name of the files
            int nameLength = DragQueryFileW( HDrop, i, NULL, 0 ) + 1;
            wchar_t *psz_fileName = new WCHAR[nameLength];
            DragQueryFileW( HDrop, i, psz_fileName, nameLength );

            files.push_back( sFromWide(psz_fileName) );
            delete[] psz_fileName;
         }
         GlobalUnlock( medium.hGlobal );
    }
    else if( m_format.format == CF_TEXT )
    {
        char* data = (char*)GlobalLock( medium.hGlobal );
        files.push_back( data );
        GlobalUnlock( medium.hGlobal );
    }

    // Release the pointer to the memory
    ReleaseStgMedium( &medium );

    // transmit DragDrop event
    EvtDragDrop evt( getIntf(), pt.x, pt.y, files );
    m_pWin->processEvent( evt );

    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}


#endif
