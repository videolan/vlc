/*****************************************************************************
 * gtk_playlist.c : Interface for the playlist dialog
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: gtk_playlist.c,v 1.10 2001/04/28 03:36:25 sam Exp $
 *
 * Authors: Pierre Baillet <oct@zoy.org>
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

#define MODULE_NAME gtk
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>

#include <gtk/gtk.h>

#include <string.h>

#include <sys/types.h>          /* for readdir  and stat stuff */
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"
#include "intf_msg.h"
#include "intf_urldecode.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_playlist.h"
#include "intf_gtk.h"


#include "main.h"

void
on_menubar_playlist_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );
    playlist_t * p_playlist ;
    GtkCList * list;
    
    if( !GTK_IS_WIDGET( p_intf->p_sys->p_playlist ) )
    {
        /* this shoud never happen */
        intf_ErrMsgImm("intf_playlist is not a widget !");
        p_intf->p_sys->p_playlist = create_intf_playlist();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_playlist ),
                             "p_intf", p_intf );
    }
    
    vlc_mutex_lock( &p_main->p_playlist->change_lock );
    if(p_main->p_playlist->i_size > 0 )
    {
        p_playlist = p_main->p_playlist;
        list = GTK_CLIST(lookup_widget( p_intf->p_sys->p_playlist, "playlist_clist" )) ;
        rebuildCList( list, p_playlist );
    }
    vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    
    gtk_widget_show( p_intf->p_sys->p_playlist );
    gdk_window_raise( p_intf->p_sys->p_playlist->window );
}


void
on_toolbar_playlist_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_playlist ) )
    {
        /* this should never happen */
        intf_ErrMsgImm("intf_playlist is not a widget !");

        p_intf->p_sys->p_playlist = create_intf_playlist();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_playlist ),
                             "p_intf", p_intf );
    }
    if( GTK_WIDGET_VISIBLE(p_intf->p_sys->p_playlist) ) {
        gtk_widget_hide( p_intf->p_sys->p_playlist);
    } 
    else 
    {        
        GtkCList * clist;
        gtk_widget_show( p_intf->p_sys->p_playlist );
        clist = GTK_CLIST(lookup_widget( p_intf->p_sys->p_playlist,"playlist_clist" ));
        gdk_window_raise( p_intf->p_sys->p_playlist->window );
        rebuildCList( clist , p_main->p_playlist );
    }
}

void
on_playlist_ok_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_playlist" );
    gtk_widget_hide( p_intf->p_sys->p_playlist );
}

void  deleteGListItem(gpointer data, gpointer param)
{
    int curRow = ( int )data;
    intf_thread_t * p_intf = param;    
    
    intf_PlaylistDelete( p_main->p_playlist, curRow );

    /* are we deleting the current played stream */
    if( p_intf->p_sys->i_playing == curRow )
    {
        /* next ! */
        p_intf->p_input->b_eof = 1;
        /* this has to set the slider to 0 */
        
        /* step minus one */
        p_intf->p_sys->i_playing-- ;
        p_main->p_playlist->i_index-- ;
    }
}
gint compareItems(gconstpointer a, gconstpointer b)
{
    return b - a;
}

void 
rebuildCList(GtkCList * clist, playlist_t * playlist_p)
{
    int dummy;
    gchar * text[2];
    GdkColor red;
    red.red = 65535;
    red.green = 0;
    red.blue = 0;

    
    gtk_clist_freeze( clist );
    gtk_clist_clear( clist );
   
    for( dummy=0; dummy < playlist_p->i_size; dummy++ )
    {
#ifdef WIN32 /* WIN32 HACK */
        text[0] = g_strdup( "" );
#else
        text[0] = g_strdup( rindex( (char *)(playlist_p->p_item[playlist_p->i_size -1 - dummy].psz_name ), '/' ) + 1 );
#endif
        text[1] = g_strdup( "no info");
        
        gtk_clist_insert( clist, 0, text );
        
        free(text[0]);
        free(text[1]);
    }
    gtk_clist_set_background (
      clist, 
      playlist_p->i_index, 
      &red);
    gtk_clist_thaw( clist );
}

void
on_invertselection_clicked (GtkMenuItem *item, gpointer user_data)
{
    int * selected, sel_l;
    GtkCList    * clist;
    playlist_t * playlist_p;
    int dummy;
    
    /* catch the thread back */
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(item), "intf_playlist" );
    playlist_p = p_main->p_playlist;
    
    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );
    clist = GTK_CLIST( lookup_widget(p_intf->p_sys->p_playlist,"playlist_clist") );
    
    /* have to copy the selection to an int *
       I wasn't able to copy the g_list to another g_list
       glib only does pointer copies, not real copies :( */
    
    selected = malloc(sizeof(int)* g_list_length(clist->selection));
    sel_l = g_list_length(clist->selection);
    for(dummy=0; dummy < sel_l; dummy++)
    {
        selected[dummy] = (int)g_list_nth_data(clist->selection,dummy);
    }
    
    gtk_clist_freeze( clist );
    gtk_clist_select_all( clist );
    for(dummy=0; dummy < sel_l; dummy++)
    {
        gtk_clist_unselect_row( clist, selected[dummy],0);
        gtk_clist_unselect_row( clist, selected[dummy],1);
    }
    free( selected );
    gtk_clist_thaw( clist );
    vlc_mutex_unlock( &p_intf->change_lock );
}    

void
on_crop_activate                       (GtkMenuItem     *menuitem,
                                       gpointer         user_data)
{
    /* Ok, this is a really small thing, but, hey, it works and
       might be useful, who knows ? */
    
    on_invertselection_clicked (menuitem, user_data);
    on_delete_clicked(menuitem, user_data);
}


void
on_delete_clicked                      (GtkMenuItem       *item,
                                        gpointer         user_data)
{
    /* user wants to delete a file in the queue */
    GList * selection;
    GtkCList    * clist;
    playlist_t * playlist_p;
    
    /* catch the thread back */
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(item), "intf_playlist" );
    playlist_p = p_main->p_playlist;
    
    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );
    clist = GTK_CLIST( lookup_widget(p_intf->p_sys->p_playlist,"playlist_clist") );
    
    /* I use UNDOCUMENTED features to retrieve the selection... */
    selection = clist->selection; 
    
    if( g_list_length(selection)>0 )
    {
        /* reverse-sort so that we can delete from the furthest to the 
           closest item to delete...
          */
        selection = g_list_sort( selection, compareItems );
        g_list_foreach( selection,
                        deleteGListItem, 
                        p_intf );
        /* rebuild the CList */
        rebuildCList( clist, playlist_p );
    }
    
    vlc_mutex_unlock( &p_intf->change_lock );
}

gboolean
on_intf_playlist_destroy_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    /* hide ! */
    gtk_widget_hide(widget);
    return TRUE;
}

void
on_intf_playlist_drag_data_received    (GtkWidget       *widget,
    GdkDragContext  *drag_context,
    gint             x,
    gint             y,
    GtkSelectionData *data,
    guint            info,
    guint            time,
    gpointer         user_data)
{
    /* catch the interface back */
    intf_thread_t * p_intf =  GetIntf( GTK_WIDGET(widget), "intf_playlist" );
    GtkCList *  clist;
    gint row, col;

    clist = GTK_CLIST(lookup_widget( p_intf->p_sys->p_playlist,"playlist_clist" ));
   
    /* are we dropping somewhere into the clist items ? */
    if( gtk_clist_get_selection_info( clist, 
                x, 
                y, 
                &row, 
                &col )== 1)
    {
        on_generic_drop_data_received( p_intf, data, info, row );
    } 
    /* else, put that at the end of the playlist */
    else 
    {
        on_generic_drop_data_received( p_intf, data, info, PLAYLIST_END);
    }
}
    
void on_generic_drop_data_received( intf_thread_t * p_intf,
        GtkSelectionData *data, guint info, int position)
{
    /* first we'll have to split against all the '\n' we have */
    gchar * protocol;
    gchar * temp;
    gchar * string = data->data ;
    GList * files = NULL;
    GtkCList * clist;

    
    /* catch the playlist back */
    playlist_t * p_playlist = p_main->p_playlist ;
   

    /* if this has been URLencoded, decode it
     * 
     * Is it a good thing to do it in place ?
     * probably not... 
     */
    if(info == DROP_ACCEPT_TEXT_URI_LIST)
    {
        urldecode_path( string );
    }
    
    /* this cuts string into single file drops */
    /* this code was borrowed from xmms, thx guys :) */
    while(*string)
    {
        temp = strchr(string, '\n');
        if(temp)
        {
            if (*(temp - 1) == '\r')
                *(temp - 1) = '\0';
            *temp = '\0';
        }
       
        
        /* do we have a protocol or something ? */
        protocol = strstr( string, ":/" );
        if( protocol != NULL )
        {
            protocol = calloc( protocol - string + 2 , 
                            sizeof(char));
            protocol = strncpy( protocol, string, strstr( string, ":/") + 1 - string );

            intf_WarnMsg(1,"Protocol dropped is %s",protocol);
            string += strlen(protocol) ;

            /* Allowed things are proto: or proto:// */
            if(string[0]=='/' && string[1]=='/')
            {
                /* eat one '/' */
                string++;
            }
            intf_WarnMsg(1,"Dropped %s",string);

        } 
        else 
        {
            protocol = strdup("");
        }
         
        /* if it uses the file protocol we can do something, else, sorry :( 
         * I think this is a good choice for now, as we don't have any
         * ability to read http:// or ftp:// files
         * what about adding dvd:// to the list of authorized proto ? */
        
        if( strcmp(protocol,"file:")==0 )
        {
            files = g_list_concat( files, intf_readFiles( string ) ); 
        }
       
        /* free the malloc and go on... */
        free( protocol );
        if (!temp)
            break;
        string = temp + 1;
    }
   
    /* At this point, we have a nice big list maybe NULL */
    if(files != NULL)
    {
        /* lock the interface */
        vlc_mutex_lock( &p_intf->change_lock );
        intf_WarnMsg( 1, "List has %d elements",g_list_length( files ) ); 
        intf_AppendList( p_playlist, position, files );

        /* get the CList  and rebuild it. */
        clist = GTK_CLIST(
                lookup_widget( p_intf->p_sys->p_playlist,
                               "playlist_clist" ) ); 
        rebuildCList( clist , p_playlist );
        
        /* unlock the interface */
        vlc_mutex_unlock( &p_intf->change_lock );
    }
}

/* check a file (string) against supposed valid extension */
int 
hasValidExtension( gchar * filename )
{
    char * ext[6] = {"mpg","mpeg","vob","mp2","ts","ps"};
    int  i_ext = 6;
    int dummy;
    gchar * p_filename = strrchr( filename, '.' ) + sizeof( char );
    for( dummy=0; dummy<i_ext;dummy++ )
    {
        if( strcmp( p_filename,ext[dummy] )==0 )
            return 1;
    }
    return 0;
}

/* recursive function: descend into folders and build a list of valid filenames */
GList * 
intf_readFiles( gchar * fsname )
{
    struct stat statbuf;
    GList  * current = NULL;

    /* get the attributes of this file */
    stat(fsname, &statbuf);
    
    /* is it a regular file ? */
    if( S_ISREG( statbuf.st_mode ) )
    {
        if( hasValidExtension(fsname) )
        {
            intf_WarnMsg( 3, "%s is a valid file. Stacking on the playlist", fsname );
            return g_list_append( NULL, g_strdup(fsname) );
        } 
        else
        {
            return NULL;
        }
    } 
    /* is it a directory (should we check for symlinks ?) */
    else if( S_ISDIR( statbuf.st_mode ) ) 
    {
        /* have to cd into this dir */
        DIR * currentDir = opendir( fsname );
        struct dirent * dirContent; 
        
        intf_WarnMsg( 3, "%s is a folder.", fsname );
        
        if( currentDir == NULL )
        {
            /* something went bad, get out of here ! */
            return current;
        }
        dirContent = readdir( currentDir );

        /* while we still have entries in the directory */
        while( dirContent != NULL )
        {
            /* if it is "." or "..", forget it */
            if(strcmp(dirContent->d_name,".") != 0
                    && strcmp(dirContent->d_name,"..") != 0)
            {
                /* else build the new directory by adding
                   fsname "/" and the current entry name 
                   (kludgy :()
                  */
                char * newfs = malloc ( 2 + 
                        strlen( fsname ) + 
                        strlen( dirContent->d_name ) * sizeof( char ) );
                strcpy( newfs, fsname );
                strcpy( newfs + strlen( fsname )+1, dirContent->d_name);
                newfs[strlen( fsname )] = '/';
                
                current = g_list_concat( current, intf_readFiles( newfs ) );
                    
                g_free( newfs );
            }
            dirContent = readdir( currentDir );
        }
        return current;
    }
    return NULL;
}

/* add items in a playlist 
  when i_pos==-1 add to the end of the list... 
 */
int intf_AppendList( playlist_t * p_playlist, int i_pos, GList * list )
{
    guint length, dummy;
    length = g_list_length( list );
    for( dummy=0; dummy<length; dummy++ )
    {
        intf_PlaylistAdd( p_playlist, 
                /* ok; this is a really nasty trick to insert
                   the item where they are suppose to go but, hey
                   this works :P (btw, you are really nasty too) */
                i_pos==PLAYLIST_END?PLAYLIST_END:( i_pos + dummy ), 
                g_list_nth_data(list, dummy));
    }
    return 0;
}
gboolean
on_playlist_clist_event (GtkWidget       *widget,
                            GdkEvent        *event,
                            gpointer         user_data)
{
    intf_thread_t * p_intf =  GetIntf( GTK_WIDGET( widget ), "intf_playlist" );

    if( ( event->button ).type == GDK_2BUTTON_PRESS )
    {
        GtkCList *  clist;
        gint row, col;

        clist = GTK_CLIST( 
                    lookup_widget( 
                        p_intf->p_sys->p_playlist,
                        "playlist_clist" ) );
        
        if( gtk_clist_get_selection_info( clist, 
                    (event->button).x, 
                    (event->button).y, 
                    &row, 
                    &col )== 1 )
        {

            /* clicked is in range. */
            if( p_intf->p_input != NULL )
            {
                /* FIXME: temporary hack */
                p_intf->p_input->b_eof = 1;
            }
            intf_PlaylistJumpto( p_main->p_playlist, row-1 );
        }
        return TRUE;
    }
    return FALSE;
}

/* statis timeouted function */
void GtkPlayListManage( gpointer p_data )
{
    /* this thing really sucks for now :( */

    /* TODO speak more with interface/intf_playlist.c */

    intf_thread_t *p_intf = (void *)p_data;
    playlist_t * p_playlist = p_main->p_playlist ;

    vlc_mutex_lock( &p_intf->change_lock );

    if( p_intf->p_sys->i_playing != p_playlist->i_index )
    {
        GdkColor color;

        color.red = 0xffff;
        color.green = 0;
        color.blue = 0;

        gtk_clist_set_background ( GTK_CLIST(
                    lookup_widget( p_intf->p_sys->p_playlist, 
                                   "playlist_clist" ) ),
                    p_playlist->i_index,
                    &color );

        if( p_intf->p_sys->i_playing != -1 )
        {
            color.red = 0xffff;
            color.green = 0xffff;
            color.blue = 0xffff;
            gtk_clist_set_background (
            GTK_CLIST(lookup_widget( p_intf->p_sys->p_playlist, "playlist_clist" ) ),
            p_intf->p_sys->i_playing,
            &color);
        }
        p_intf->p_sys->i_playing = p_playlist->i_index;
    }
    vlc_mutex_unlock( &p_intf->change_lock );
}

