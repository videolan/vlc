/*****************************************************************************
 * gtk2_dragdrop.h: GTK2 implementation of the drag & drop
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_dragdrop.h,v 1.1 2003/04/12 21:43:27 asmax Exp $
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


#ifndef VLC_SKIN_GTK2_DRAGDROP
#define VLC_SKIN_GTK2_DRAGDROP

//--- GTK2 -----------------------------------------------------------------
//#include <shellapi.h>
//#include <ole2.h>

//---------------------------------------------------------------------------
/*
class GTK2DropObject : public IDropTarget
{
    public:
       GTK2DropObject();
       virtual ~GTK2DropObject();

    protected:
        // IUnknown methods
        STDMETHOD(QueryInterface)( REFIID riid, void FAR* FAR* ppvObj );
        STDMETHOD_(ULONG, AddRef)();
        STDMETHOD_(ULONG, Release)();

        // IDropTarget methods
        STDMETHOD(DragEnter)( LPDATAOBJECT pDataObj, DWORD grfKeyState,
                              POINTL pt, DWORD *pdwEffect );
        STDMETHOD(DragOver)( DWORD grfKeyState, POINTL pt, DWORD *pdwEffect );
        STDMETHOD(DragLeave)();
        STDMETHOD(Drop)( LPDATAOBJECT pDataObj, DWORD grfKeyState,
                         POINTL pt, DWORD *pdwEffect );

    private:
        unsigned long References;

        // Helper function
        void HandleDrop( HDROP HDrop );
};*/
//---------------------------------------------------------------------------
#endif
