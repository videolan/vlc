/*****************************************************************************
 * x11_dragdrop.h: X11 implementation of the drag & drop
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_dragdrop.h,v 1.3 2003/06/09 00:07:09 asmax Exp $
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


#ifndef VLC_SKIN_X11_DRAGDROP
#define VLC_SKIN_X11_DRAGDROP

//--- X11 -----------------------------------------------------------------
#include <X11/Xlib.h>

//---------------------------------------------------------------------------

typedef long ldata_t[5];

class X11DropObject
{
    private:
        intf_thread_t *p_intf;
        Window Win;
        Display *display;
        Atom target;
        
    public:
       X11DropObject( intf_thread_t *_p_intf, Window win );
       virtual ~X11DropObject();

       void DndEnter( ldata_t data );
       void DndPosition( ldata_t data );
       void DndLeave( ldata_t data );
       void DndDrop( ldata_t data );
};
//---------------------------------------------------------------------------
#endif
