/*****************************************************************************
 * gtk2_dragdrop.cpp: GTK2 implementation of the drag & drop
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_dragdrop.cpp,v 1.6 2003/04/28 12:00:13 asmax Exp $
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

#ifdef GTK2_SKINS

//--- GTK2 -----------------------------------------------------------------
#include <gdk/gdk.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../src/event.h"
#include "../os_api.h"
#include "gtk2_dragdrop.h"


//---------------------------------------------------------------------------
GTK2DropObject::GTK2DropObject( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
}
//---------------------------------------------------------------------------
GTK2DropObject::~GTK2DropObject()
{
}
//---------------------------------------------------------------------------
void GTK2DropObject::HandleDropStart( GdkDragContext *context )
{
    GdkAtom atom = gdk_drag_get_selection( context );
    
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
    gdk_drop_finish( context, TRUE,OSAPI_GetTime() );
}

#endif
