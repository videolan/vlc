/*****************************************************************************
 * x11_dragdrop.cpp: X11 implementation of the drag & drop
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_dragdrop.cpp,v 1.2 2003/06/08 18:17:50 asmax Exp $
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

#ifdef X11_SKINS

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/event.h"
#include "../os_api.h"
#include "x11_dragdrop.h"


//---------------------------------------------------------------------------
X11DropObject::X11DropObject( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
}
//---------------------------------------------------------------------------
X11DropObject::~X11DropObject()
{
}
//---------------------------------------------------------------------------
void X11DropObject::DndEnter( ldata_t data )
{
    fprintf(stderr,"dnd enter\n");
}
//---------------------------------------------------------------------------
void X11DropObject::DndPosition( ldata_t data )
{
    fprintf(stderr,"dnd position\n");
}
//---------------------------------------------------------------------------
void X11DropObject::DndLeave( ldata_t data )
{
    fprintf(stderr,"dnd leave\n");
}
//---------------------------------------------------------------------------
void X11DropObject::DndDrop( ldata_t data )
{
    fprintf(stderr,"dnd drop\n");
}
#if 0
void X11DropObject::HandleDropStart( GdkDragContext *context )
{
/*    GdkAtom atom = gdk_drag_get_selection( context );
    
    guchar *buffer;
    GdkAtom prop_type;
    gint prop_format;
    
    // Get the owner of the selection
    GdkWindow *owner = gdk_selection_owner_get( atom ); 

    // Find the possible targets for the selection
    string target = "";
    gdk_selection_convert( owner, atom, gdk_atom_intern("TARGETS", FALSE), 
                           OSAPI_GetTime() );
    int len = gdk_selection_property_get( owner, &buffer, &prop_type, 
                                          &prop_format );
    for( int i = 0; i < len / sizeof(GdkAtom); i++ )
    {
        GdkAtom atom = ( (GdkAtom*)buffer )[i];
        gchar *name = gdk_atom_name( atom );
        if( name )
        {
            string curTarget = name;
            if( (curTarget == "text/plain" || curTarget == "STRING") )
            {
                target = curTarget;
                break;
            }
        }
    }

    if( target == "" )
    {
        msg_Warn( p_intf, "Drag&Drop: target not found\n" );
    }
    else
    {
        gdk_selection_convert( owner, atom, gdk_atom_intern(target.c_str(), 
                               FALSE), OSAPI_GetTime() );
        len = gdk_selection_property_get( owner, &buffer, &prop_type, 
                                          &prop_format);
        OSAPI_PostMessage( NULL, VLC_DROP, (unsigned int)buffer, 0 );
    }
*/
/*    // Get number of files that are dropped into vlc
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
    
*/
  //  gdk_drop_finish( context, TRUE,OSAPI_GetTime() );
}
#endif
#endif
