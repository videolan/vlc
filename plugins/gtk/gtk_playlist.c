/*****************************************************************************
 * gtk_playlist.c : Interface for the playlist dialog
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: gtk_playlist.c,v 1.23 2001/12/29 11:36:00 lool Exp $
 *
 * Authors: Pierre Baillet <oct@zoy.org>
 *          Stéphane Borel <stef@via.ecp.fr>
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
#include "defs.h"

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>          /* for readdir  and stat stuff */

#ifndef WIN32
#   include <dirent.h>
#endif

#include <sys/stat.h>
#include <unistd.h>

#define gtk 12
#define gnome 42
#if ( MODULE_NAME == gtk )
#   include <gtk/gtk.h>
#elif ( MODULE_NAME == gnome )
#   include <gnome.h>
#endif
#undef gtk
#undef gnome

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_playlist.h"
#include "intf_gtk.h"

#include "modules_export.h"

/****************************************************************************
 * Playlist window management
 ****************************************************************************/
gboolean GtkPlaylistShow( GtkWidget       *widget,
                          GdkEventButton  *event,
                          gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( GTK_WIDGET_VISIBLE( p_intf->p_sys->p_playlist ) )
    {
        gtk_widget_hide( p_intf->p_sys->p_playlist );
    } 
    else 
    {        
        GtkCList * p_clist;

        p_clist = GTK_CLIST( gtk_object_get_data(
            GTK_OBJECT( p_intf->p_sys->p_playlist ), "playlist_clist" ) );
        GtkRebuildCList( p_clist , p_main->p_playlist );
        gtk_widget_show( p_intf->p_sys->p_playlist );
        gdk_window_raise( p_intf->p_sys->p_playlist->window );
    }

    return TRUE;
}


void GtkPlaylistOk( GtkButton * button, gpointer user_data )
{
     gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


void GtkPlaylistCancel( GtkButton * button, gpointer user_data )
{
     gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}



gboolean GtkPlaylistPrev( GtkWidget       *widget,
                          GdkEventButton  *event,
                          gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( p_intf->p_input != NULL )
    {
        /* FIXME: temporary hack */
        intf_PlaylistPrev( p_main->p_playlist );
        intf_PlaylistPrev( p_main->p_playlist );
        p_intf->p_input->b_eof = 1;
    }

    return TRUE;
}


gboolean GtkPlaylistNext( GtkWidget       *widget,
                          GdkEventButton  *event,
                          gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( p_intf->p_input != NULL )
    {
        /* FIXME: temporary hack */
        p_intf->p_input->b_eof = 1;
    }

    return TRUE;
}

/****************************************************************************
 * Menu callbacks for playlist functions
 ****************************************************************************/
void GtkPlaylistActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkPlaylistShow( GTK_WIDGET( menuitem ), NULL, user_data );
}


void GtkNextActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkPlaylistNext( GTK_WIDGET( menuitem ), NULL, user_data );
}


void GtkPrevActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkPlaylistPrev( GTK_WIDGET( menuitem ), NULL, user_data );
}


/****************************************************************************
 * Playlist core functions
 ****************************************************************************/
void GtkPlaylistAddUrl( GtkMenuItem * menuitem, gpointer user_data )
{

}


void GtkPlaylistDeleteAll( GtkMenuItem * menuitem, gpointer user_data )
{

}


void GtkPlaylistDeleteSelected( GtkMenuItem * menuitem, gpointer user_data )
{
    /* user wants to delete a file in the queue */
    GList *     p_selection;
    GtkCList *  p_clist;
    playlist_t *p_playlist;
    
    /* catch the thread back */
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), /*(char*)user_data*/"intf_playlist" );

    p_playlist = p_main->p_playlist;
    
    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );

    p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
        p_intf->p_sys->p_playlist ), "playlist_clist" ) );
    
    /* I use UNDOCUMENTED features to retrieve the selection... */
    p_selection = p_clist->selection;
    
    if( g_list_length( p_selection ) > 0 )
    {
        /* reverse-sort so that we can delete from the furthest
         * to the closest item to delete...
         */
        p_selection = g_list_sort( p_selection, GtkCompareItems );
        g_list_foreach( p_selection, GtkDeleteGListItem, p_intf );
        /* rebuild the CList */
        GtkRebuildCList( p_clist, p_playlist );
    }
    
    vlc_mutex_unlock( &p_intf->change_lock );
}

void GtkPlaylistCrop( GtkMenuItem * menuitem, gpointer user_data )
{
    /* Ok, this is a really small thing, but, hey, it works and
       might be useful, who knows ? */
    GtkPlaylistInvert( menuitem, user_data );
    GtkPlaylistDeleteSelected( menuitem, user_data );
}

void GtkPlaylistInvert( GtkMenuItem * menuitem, gpointer user_data )
{
    playlist_t *p_playlist;
    GtkCList *  p_clist;
    int *       pi_selected;
    int         i_sel_l;
    int         i_dummy;
    
    /* catch the thread back */
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), (char*)user_data );

    p_playlist = p_main->p_playlist;
    
    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );

    p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
        p_intf->p_sys->p_playlist ), "playlist_clist" ) );
    
    /* have to copy the selection to an int *
       I wasn't able to copy the g_list to another g_list
       glib only does pointer copies, not real copies :( */
    
    pi_selected = malloc( sizeof(int) *g_list_length( p_clist->selection ) );
    i_sel_l = g_list_length( p_clist->selection );

    for( i_dummy = 0 ; i_dummy < i_sel_l ; i_dummy++)
    {
        pi_selected[i_dummy] = (long)g_list_nth_data( p_clist->selection,
                                                      i_dummy );
    }
    
    gtk_clist_freeze( p_clist );
    gtk_clist_select_all( p_clist );

    for( i_dummy = 0; i_dummy < i_sel_l; i_dummy++)
    {
        gtk_clist_unselect_row( p_clist, pi_selected[i_dummy], 0 );
        gtk_clist_unselect_row( p_clist, pi_selected[i_dummy], 1 );
    }

    free( pi_selected );
    gtk_clist_thaw( p_clist );

    vlc_mutex_unlock( &p_intf->change_lock );
}

void GtkPlaylistSelect( GtkMenuItem * menuitem, gpointer user_data)
{

}

gboolean GtkPlaylistEvent( GtkWidget * widget,
                           GdkEvent  * event,
                           gpointer    user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( ( event->button ).type == GDK_2BUTTON_PRESS )
    {
        GtkCList *  p_clist;
        gint        i_row;
        gint        i_col;

        p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
            p_intf->p_sys->p_playlist ), "playlist_clist" ) );
        
        if( gtk_clist_get_selection_info( p_clist, (event->button).x, 
                    (event->button).y, &i_row, &i_col ) == 1 )
        {
            /* clicked is in range. */
            if( p_intf->p_input != NULL )
            {
                /* FIXME: temporary hack */
                p_intf->p_input->b_eof = 1;
            }

            intf_PlaylistJumpto( p_main->p_playlist, i_row - 1 );
        }
        return TRUE;
    }

    return FALSE;
}

void GtkPlaylistDragData( GtkWidget       *widget,
                          GdkDragContext  *drag_context,
                          gint             x,
                          gint             y,
                          GtkSelectionData *data,
                          guint            info,
                          guint            time,
                          gpointer         user_data )
{
    intf_thread_t * p_intf;
    GtkCList *      p_clist;
    gint            i_row;
    gint            i_col;
    int             i_end = p_main->p_playlist->i_size;

    /* catch the interface back */
    p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
        p_intf->p_sys->p_playlist ), "playlist_clist" ) );
   
    if( gtk_clist_get_selection_info( p_clist, x, y, &i_row, &i_col ) == 1 )
    {
        /* we are dropping somewhere into the clist items */
        GtkDropDataReceived( p_intf, data, info, i_row );
    } 
    else 
    {
        /* else, put that at the end of the playlist */
        GtkDropDataReceived( p_intf, data, info, PLAYLIST_END );
    }

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
}


gboolean GtkPlaylistDragMotion( GtkWidget       *widget,
                                GdkDragContext  *drag_context,
                                gint             x,
                                gint             y,
                                guint            time,
                                gpointer         user_data )
{
    intf_thread_t *p_intf;
    GtkCList *  p_clist;
    gint        i_row;
    gint        i_col;
    int         i_dummy;
    GdkColor    color;

    p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
        p_intf->p_sys->p_playlist ), "playlist_clist" ) );

    if( !GTK_WIDGET_TOPLEVEL(widget) )
    {
        gdk_window_raise( p_intf->p_sys->p_playlist->window );
    }

    color.red =   0xffff;
    color.blue =  0xffff;
    color.green = 0xffff;

    gtk_clist_freeze( p_clist );
    
    for( i_dummy = 0; i_dummy < p_clist->rows; i_dummy++)
    {
       gtk_clist_set_background ( p_clist, i_dummy , &color);
    }

    color.red = 0xffff;
    color.blue = 0;
    color.green = 0;
    gtk_clist_set_background( p_clist, p_main->p_playlist->i_index , &color );
        
    if( gtk_clist_get_selection_info( p_clist, x, y, &i_row, &i_col ) == 1)
    {
        color.red = 0;
        color.blue = 0xf000;
        color.green = 0x9000;
        gtk_clist_set_background ( p_clist, i_row - 1, &color);
        gtk_clist_set_background ( p_clist, i_row, &color);
    }

    gtk_clist_thaw( p_clist );
    
    return TRUE;
}

void GtkDropDataReceived( intf_thread_t * p_intf,
        GtkSelectionData * p_data, guint i_info, int i_position)
{
    /* first we'll have to split against all the '\n' we have */
    gchar *     p_protocol;
    gchar *     p_temp;
    gchar *     p_next;
    gchar *     p_string = p_data->data ;
    GList *     p_files = NULL;
    GtkCList *  p_clist;

    
    /* catch the playlist back */
    playlist_t * p_playlist = p_main->p_playlist;
   

    /* if this has been URLencoded, decode it
     * 
     * Is it a good thing to do it in place ?
     * probably not... 
     */
    if( i_info == DROP_ACCEPT_TEXT_URI_LIST )
    {
        intf_UrlDecode( p_string );
    }
    
    /* this cuts string into single file drops */
    /* this code was borrowed from xmms, thx guys :) */
    while( *p_string)
    {
        p_next = strchr( p_string, '\n' );
        if( p_next )
        {
            if( *( p_next - 1 ) == '\r' )
            {
                *( p_next - 1) = '\0';
            }
            *p_next = '\0';
        }

        /* do we have a protocol or something ? */
        p_temp = strstr( p_string, ":" );
        if( p_temp != NULL && p_temp[0] != '\0' )
        {
            char i_save;

	    i_save = p_temp[0];
	    p_temp[0] = '\0';
	    p_protocol = strdup( p_string );
	    p_temp[0] = i_save;
	    p_temp++;
	    
            /* Allowed things are proto: or proto:// */
            if( p_temp[0] == '/' && p_temp[1] == '/')
            {
                /* eat two '/'s */
                p_temp += 2;
            }
            intf_WarnMsg( 4, "playlist: protocol '%s', target '%s'",
                          p_protocol, p_temp );
        } 
        else 
        {
            p_protocol = strdup( "" );
        }
         
        /* if it uses the file protocol we can do something, else, sorry :( 
         * I think this is a good choice for now, as we don't have any
         * ability to read http:// or ftp:// files
         * what about adding dvd:// to the list of authorized proto ? */
        
        if( strcmp( p_protocol, "file:" ) == 0 )
        {
            p_files = g_list_concat( p_files, GtkReadFiles( p_string ) ); 
        }
	else
	{
            p_files = g_list_concat( p_files,
                      g_list_append( NULL, g_strdup( p_string ) ) );
	}
       
        /* free the malloc and go on... */
        free( p_protocol );

        if( p_next == NULL )
        {
            break;
        }
        p_string = p_next + 1;
    }
   
    /* At this point, we have a nice big list maybe NULL */
    if( p_files != NULL )
    {
        /* lock the interface */
        vlc_mutex_lock( &p_intf->change_lock );

        intf_WarnMsg( 4, "List has %d elements", g_list_length( p_files ) ); 
        GtkAppendList( p_playlist, i_position, p_files );

        /* get the CList  and rebuild it. */
        p_clist = GTK_CLIST( lookup_widget( p_intf->p_sys->p_playlist,
                                            "playlist_clist" ) ); 
        GtkRebuildCList( p_clist , p_playlist );
        
        /* unlock the interface */
        vlc_mutex_unlock( &p_intf->change_lock );
    }
}


void GtkDeleteGListItem( gpointer data, gpointer param )
{
    int i_cur_row = (long)data;
    intf_thread_t * p_intf = param;    
    
    intf_PlaylistDelete( p_main->p_playlist, i_cur_row );

    /* are we deleting the current played stream */
    if( p_intf->p_sys->i_playing == i_cur_row )
    {
        /* next ! */
        p_intf->p_input->b_eof = 1;
        /* this has to set the slider to 0 */
        
        /* step minus one */
        p_intf->p_sys->i_playing-- ;

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->i_index-- ;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }
}


gint GtkCompareItems( gconstpointer a, gconstpointer b )
{
    return b - a;
}


/* check a file (string) against supposed valid extension */
int GtkHasValidExtension( gchar * psz_filename )
{
    char * ppsz_ext[6] = { "mpg", "mpeg", "vob", "mp2", "ts", "ps" };
    int  i_ext = 6;
    int  i_dummy;

    gchar * psz_ext = strrchr( psz_filename, '.' ) + sizeof( char );

    for( i_dummy = 0 ; i_dummy < i_ext ; i_dummy++ )
    {
        if( strcmp( psz_ext, ppsz_ext[i_dummy] ) == 0 )
        {
            return 1;
        }
    }

    return 0;
}

/* recursive function: descend into folders and build a list of
 * valid filenames */
GList * GtkReadFiles( gchar * psz_fsname )
{
    struct stat statbuf;
    GList  *    p_current = NULL;

    /* get the attributes of this file */
    stat( psz_fsname, &statbuf );
    
    /* is it a regular file ? */
    if( S_ISREG( statbuf.st_mode ) )
    {
        if( GtkHasValidExtension( psz_fsname ) )
        {
            intf_WarnMsg( 2, "%s is a valid file. Stacking on the playlist",
                          psz_fsname );
            return g_list_append( NULL, g_strdup( psz_fsname ) );
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
        DIR *           p_current_dir = opendir( psz_fsname );
        struct dirent * p_dir_content; 
        
        intf_WarnMsg( 2, "%s is a folder.", psz_fsname );
        
        if( p_current_dir == NULL )
        {
            /* something went bad, get out of here ! */
            return p_current;
        }
        p_dir_content = readdir( p_current_dir );

        /* while we still have entries in the directory */
        while( p_dir_content != NULL )
        {
            /* if it is "." or "..", forget it */
            if( ( strcmp( p_dir_content->d_name, "." ) != 0 ) &&
                ( strcmp( p_dir_content->d_name, ".." ) != 0 ) )
            {
                /* else build the new directory by adding
                   fsname "/" and the current entry name 
                   (kludgy :()
                  */
                char *  psz_newfs = malloc ( 2 + strlen( psz_fsname ) +
                            strlen( p_dir_content->d_name ) * sizeof(char) );
                strcpy( psz_newfs, psz_fsname );
                strcpy( psz_newfs + strlen( psz_fsname ) + 1,
                        p_dir_content->d_name );
                psz_newfs[strlen( psz_fsname )] = '/';
                
                p_current = g_list_concat( p_current,
                                           GtkReadFiles( psz_newfs ) );
                    
                g_free( psz_newfs );
            }
            p_dir_content = readdir( p_current_dir );
        }
        return p_current;
    }
    return NULL;
}

/* add items in a playlist 
 * when i_pos==-1 add to the end of the list... 
 */
int GtkAppendList( playlist_t * p_playlist, int i_pos, GList * p_list )
{
    guint i_dummy;
    guint i_length;

    i_length = g_list_length( p_list );

    for( i_dummy = 0; i_dummy < i_length ; i_dummy++ )
    {
        intf_PlaylistAdd( p_playlist, 
                /* ok; this is a really nasty trick to insert
                   the item where they are suppose to go but, hey
                   this works :P (btw, you are really nasty too) */
                i_pos==PLAYLIST_END?PLAYLIST_END:( i_pos + i_dummy ), 
                g_list_nth_data( p_list, i_dummy ) );
    }
    return 0;
}

/* statis timeouted function */
void GtkPlayListManage( intf_thread_t * p_intf )
{
    /* this thing really sucks for now :( */

    /* TODO speak more with interface/intf_playlist.c */

    playlist_t *    p_playlist = p_main->p_playlist ;
    GtkCList *      p_clist;

    if( GTK_IS_WIDGET( p_intf->p_sys->p_playlist ) )
    {
        p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
                       p_intf->p_sys->p_playlist ), "playlist_clist" ) );
    
        vlc_mutex_lock( &p_playlist->change_lock );
    
        if( p_intf->p_sys->i_playing != p_playlist->i_index )
        {
            GdkColor color;
    
            color.red = 0xffff;
            color.blue = 0;
            color.green = 0;
    
            gtk_clist_set_background( p_clist, p_playlist->i_index, &color );
    
            if( p_intf->p_sys->i_playing != -1 )
            {
                color.red = 0xffff;
                color.blue = 0xffff;
                color.green = 0xffff;
                gtk_clist_set_background( p_clist, p_intf->p_sys->i_playing,
                                          &color);
            }
            p_intf->p_sys->i_playing = p_playlist->i_index;
        }
    
        vlc_mutex_unlock( &p_playlist->change_lock );
    }
}

void GtkRebuildCList( GtkCList * p_clist, playlist_t * p_playlist )
{
    int         i_dummy;
    gchar *     ppsz_text[2];
    GdkColor    red;
    red.red     = 65535;
    red.blue    = 0;
    red.green   = 0;
    
    gtk_clist_freeze( p_clist );
    gtk_clist_clear( p_clist );
   
    for( i_dummy = 0; i_dummy < p_playlist->i_size ; i_dummy++ )
    {
#ifdef WIN32 /* WIN32 HACK */
        ppsz_text[0] = g_strdup( "" );
#else
        ppsz_text[0] = rindex( (char *)(p_playlist->p_item[
                p_playlist->i_size - 1 - i_dummy].psz_name), '/' );
        if ( ppsz_text[0] == NULL )
        {
            ppsz_text[0] = g_strdup( (char *)(p_playlist->p_item[
                    p_playlist->i_size - 1 - i_dummy].psz_name));
        }
        else
        {
            ppsz_text[0] = g_strdup( ppsz_text[0] + 1 );
        }
#endif
        ppsz_text[1] = g_strdup( "no info" );
        
        gtk_clist_insert( p_clist, 0, ppsz_text );
        
        free( ppsz_text[0] );
        free( ppsz_text[1] );
    }
    gtk_clist_set_background( p_clist, p_playlist->i_index, &red);
    gtk_clist_thaw( p_clist );
}

