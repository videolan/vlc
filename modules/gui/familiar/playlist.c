/*****************************************************************************
 * playlist.c : Playlist interface of the gtk-familiar plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: playlist.c,v 1.1 2003/03/13 15:50:17 marcari Exp $
 *
 * Authors: Marc Ariberti <marcari@videolan.org>
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


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <gtk/gtk.h>

#include "interface.h"
#include "support.h"
#include "familiar.h"
#include "playlist.h"

gboolean
FamiliarPlaylistEvent                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return FALSE;
    }

    if( ( event->button ).type == GDK_2BUTTON_PRESS )
    {
        GtkCList *  p_clist;
        gint        i_row;
        gint        i_col;

        p_clist = p_intf->p_sys->p_clistplaylist;

        if( gtk_clist_get_selection_info( p_clist, (event->button).x,
                    (event->button).y, &i_row, &i_col ) == 1 )
        {
            playlist_Goto( p_playlist, i_row );
        }

        vlc_object_release( p_playlist );
        FamiliarRebuildCList( p_clist, p_playlist );
        return TRUE;
    }

    vlc_object_release( p_playlist );

  return FALSE;
}

void
FamiliarPlaylistClear                  (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    int item;
    
    if( p_playlist == NULL )
    {
        return;
    }

    for(item = p_playlist->i_size - 1 ; item >= 0 ; item-- )
    {
        playlist_Delete( p_playlist, item);
    }
    
    vlc_object_release( p_playlist );
    FamiliarRebuildCList( p_intf->p_sys->p_clistplaylist, p_playlist);
}

void FamiliarRebuildCList( GtkCList * p_clist, playlist_t * p_playlist )
{
    int         i_dummy;
    gchar *     ppsz_text[2];
    GdkColor    red;
    red.red     = 65535;
    red.blue    = 0;
    red.green   = 0;

    gtk_clist_freeze( p_clist );
    gtk_clist_clear( p_clist );

    vlc_mutex_lock( &p_playlist->object_lock );
    for( i_dummy = p_playlist->i_size ; i_dummy-- ; )
    {
        ppsz_text[0] = p_playlist->pp_items[i_dummy]->psz_name;
        ppsz_text[1] = "no info";
        gtk_clist_insert( p_clist, 0, ppsz_text );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    gtk_clist_set_background( p_clist, p_playlist->i_index, &red);
    gtk_clist_thaw( p_clist );
}


void
FamiliarPlaylistUpdate                 (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }


    FamiliarRebuildCList( p_intf->p_sys->p_clistplaylist, p_playlist );
}

static void FamiliarDeleteGListItem( gpointer data, gpointer param )
{
    int i_cur_row = (long)data;
    playlist_t * p_playlist = param;

    playlist_Delete( p_playlist, i_cur_row );
}

static gint FamiliarCompareItems( gconstpointer a, gconstpointer b )
{
    return (ptrdiff_t) ( (int *)b - (int *)a );
}


void
FamiliarPlaylistDel                    (GtkButton       *button,
                                        gpointer         user_data)
{
    /* user wants to delete a file in the queue */
    GList *     p_selection;
    
    intf_thread_t *  p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    
    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );

    p_selection = p_intf->p_sys->p_clistplaylist->selection;

    if( g_list_length( p_selection ) )
    {
        /* reverse-sort so that we can delete from the furthest
         * to the closest item to delete...
         */
        p_selection = g_list_sort( p_selection, FamiliarCompareItems );
        g_list_foreach( p_selection, FamiliarDeleteGListItem, p_playlist );
    }

    vlc_mutex_unlock( &p_intf->change_lock );


    vlc_object_release( p_playlist );
    FamiliarRebuildCList( p_intf->p_sys->p_clistplaylist, p_playlist );
}

