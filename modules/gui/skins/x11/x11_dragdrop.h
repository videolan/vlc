/*****************************************************************************
 * x11_dragdrop.h: X11 implementation of the drag & drop
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_dragdrop.h,v 1.1 2003/04/28 14:32:57 asmax Exp $
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
class X11DropObject
{
    private:
        intf_thread_t *p_intf;

    public:
       X11DropObject( intf_thread_t *_p_intf );
       virtual ~X11DropObject();

 //      void HandleDropStart( GdkDragContext *context );
};
//---------------------------------------------------------------------------
#endif
