/*****************************************************************************
 * gtk2_dragdrop.h: GTK2 implementation of the drag & drop
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_dragdrop.h,v 1.2 2003/04/19 11:16:17 asmax Exp $
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
#include <gdk/gdk.h>

#include <stdio.h>

//---------------------------------------------------------------------------
class GTK2DropObject
{

    intf_thread_t *p_intf;

    public:
       GTK2DropObject( intf_thread_t *_p_intf );
       virtual ~GTK2DropObject();

       void HandleDropStart( GdkDragContext *context );
};
//---------------------------------------------------------------------------
#endif
