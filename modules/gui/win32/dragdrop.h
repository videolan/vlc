/*****************************************************************************
 * dragdrop.h: drag and drop management
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

#ifndef dragdropH
#define dragdropH
//---------------------------------------------------------------------------
#include <ole2.h>

#define WM_OLEDROP WM_USER + 1
//---------------------------------------------------------------------------

class TDropTarget : public IDropTarget
{
public:
   __fastcall TDropTarget( HWND HForm );
   __fastcall ~TDropTarget();

protected:
    /* IUnknown methods */
    STDMETHOD(QueryInterface)( REFIID riid, void FAR* FAR* ppvObj );
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    /* IDropTarget methods */
    STDMETHOD(DragEnter)( LPDATAOBJECT pDataObj, DWORD grfKeyState,
                          POINTL pt, DWORD *pdwEffect );
    STDMETHOD(DragOver)( DWORD grfKeyState, POINTL pt, DWORD *pdwEffect );
    STDMETHOD(DragLeave)();
    STDMETHOD(Drop)( LPDATAOBJECT pDataObj, DWORD grfKeyState,
                     POINTL pt, DWORD *pdwEffect );

private:
    unsigned long References;
    HWND FormHandle;

    /* helper function */
    void __fastcall HandleDrop( HDROP HDrop );
};
//---------------------------------------------------------------------------
#endif
