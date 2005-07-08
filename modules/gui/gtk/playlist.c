/*****************************************************************************
 * gtk_playlist.c : Interface for the playlist dialog
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale RÃ©seaux) and its contributors
 * $Id$
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
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <sys/types.h>          /* for readdir  and stat stuff */

#if (!defined( WIN32 ) || defined(__MINGW32__))
/* Mingw has its own version of dirent */
#   include <dirent.h>
#endif

#include <sys/stat.h>
#include <unistd.h>

#ifdef MODULE_NAME_IS_gnome
#   include <gnome.h>
#else
#   include <gtk/gtk.h>
#endif

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"

#include "playlist.h"
#include "common.h"

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static void UrlDecode       ( char * );
static GList * GtkReadFiles ( intf_thread_t *, gchar * );

/****************************************************************************
 * Playlist window management
 ****************************************************************************/
gboolean GtkPlaylistShow( GtkWidget       *widget,
                          gpointer         user_data )
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return FALSE;
    }

    if( GTK_WIDGET_VISIBLE( p_intf->p_sys->p_playwin ) )
    {
        gtk_widget_hide( p_intf->p_sys->p_playwin );
    }
    else
    {
        GtkCList * p_clist;

        p_clist = GTK_CLIST( gtk_object_get_data(
            GTK_OBJECT( p_intf->p_sys->p_playwin ), "playlist_clist" ) );
        GtkRebuildCList( p_clist , p_playlist );
        gtk_widget_show( p_intf->p_sys->p_playwin );
        gdk_window_raise( p_intf->p_sys->p_playwin->window );
    }

    vlc_object_release( p_playlist );

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
                          gpointer         user_data )
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return FALSE;
    }

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );

    return TRUE;
}


gboolean GtkPlaylistNext( GtkWidget       *widget,
                          gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return FALSE;
    }

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );

    return TRUE;
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

    intf_thread_t *  p_intf = GtkGetIntf( menuitem);
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );

    p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
        p_intf->p_sys->p_playwin ), "playlist_clist" ) );

    p_selection = p_clist->selection;

    if( g_list_length( p_selection ) )
    {
        /* reverse-sort so that we can delete from the furthest
         * to the closest item to delete...
         */
        p_selection = g_list_sort( p_selection, GtkCompareItems );
        g_list_foreach( p_selection, GtkDeleteGListItem, p_playlist );
        /* rebuild the CList */
        GtkRebuildCList( p_clist, p_playlist );
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    vlc_object_release( p_playlist );
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
    GtkCList *  p_clist;
    int *       pi_selected;
    int         i_length;
    int         i_dummy;

    /* catch the thread back */
    intf_thread_t *p_intf = GtkGetIntf( menuitem );

    /* lock the struct */
    vlc_mutex_lock( &p_intf->change_lock );

    p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
        p_intf->p_sys->p_playwin ), "playlist_clist" ) );

    gtk_clist_freeze( p_clist );

    /* have to copy the selection to an int *
       I wasn't able to copy the g_list to another g_list
       glib only does pointer copies, not real copies :( */

    i_length = g_list_length( p_clist->selection );
    pi_selected = malloc( sizeof(int) * i_length );

    for( i_dummy = 0 ; i_dummy < i_length ; i_dummy++ )
    {
        pi_selected[i_dummy] =
            GPOINTER_TO_UINT( g_list_nth_data( p_clist->selection, i_dummy ) );
    }

    gtk_clist_select_all( p_clist );

    for( i_dummy = 0; i_dummy < i_length; i_dummy++ )
    {
        gtk_clist_unselect_row( p_clist, pi_selected[i_dummy], 0 );
    }

    gtk_clist_thaw( p_clist );

    vlc_mutex_unlock( &p_intf->change_lock );

    free( pi_selected );
}

void GtkPlaylistSelect( GtkMenuItem * menuitem, gpointer user_data)
{

}

gboolean GtkPlaylistEvent( GtkWidget * widget,
                           GdkEvent  * event,
                           gpointer    user_data)
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

        p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
            p_intf->p_sys->p_playwin ), "playlist_clist" ) );

        if( gtk_clist_get_selection_info( p_clist, (event->button).x,
                    (event->button).y, &i_row, &i_col ) == 1 )
        {
            playlist_Goto( p_playlist, i_row );
        }

        vlc_object_release( p_playlist );
        return TRUE;
    }

    vlc_object_release( p_playlist );
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
    intf_thread_t * p_intf = GtkGetIntf( widget );
    GtkCList *      p_clist;
    gint            i_row;
    gint            i_col;

    p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
        p_intf->p_sys->p_playwin ), "playlist_clist" ) );

    if( gtk_clist_get_selection_info( p_clist, x, y, &i_row, &i_col ) == 1 )
    {
        /* we are dropping somewhere into the clist items */
        GtkDropDataReceived( p_intf, data, info, i_row - 1 );
    }
    else
    {
        /* otherwise, put that at the end of the playlist */
        GtkDropDataReceived( p_intf, data, info, PLAYLIST_END );
    }
}


gboolean GtkPlaylistDragMotion( GtkWidget       *widget,
                                GdkDragContext  *drag_context,
                                gint             x,
                                gint             y,
                                guint            time,
                                gpointer         user_data )
{
    GtkCList *  p_clist;
    gint        i_row;
    gint        i_col;
    int         i_dummy;
    GdkColor    color;

    intf_thread_t *  p_intf = GtkGetIntf( widget );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return FALSE;
    }

    p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
        p_intf->p_sys->p_playwin ), "playlist_clist" ) );

    if( !GTK_WIDGET_TOPLEVEL(widget) )
    {
        gdk_window_raise( p_intf->p_sys->p_playwin->window );
    }

    color.red =   0xffff;
    color.blue =  0xffff;
    color.green = 0xffff;

    gtk_clist_freeze( p_clist );

    for( i_dummy = 0; i_dummy < p_clist->rows; i_dummy++)
    {
        gtk_clist_set_background( p_clist, i_dummy , &color );
    }

    color.red = 0;
    color.blue = 0xf000;
    color.green = 0x9000;
    if( gtk_clist_get_selection_info( p_clist, x, y, &i_row, &i_col ) == 1 )
    {
        gtk_clist_set_background ( p_clist, i_row - 1, &color );
        gtk_clist_set_background ( p_clist, i_row, &color );
    }
    else
    {
        gtk_clist_set_background ( p_clist, p_clist->rows - 1, &color );
    }

    color.red = 0xffff;
    color.blue = 0;
    color.green = 0;
    vlc_mutex_lock( &p_playlist->object_lock );
    gtk_clist_set_background( p_clist, p_playlist->i_index, &color );
    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );

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
    gchar *     p_string = (gchar *)p_data->data;
    GList *     p_files = NULL;
    GtkCList *  p_clist;

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* if this has been URLencoded, decode it
     *
     * Is it a good thing to do it in place ?
     * probably not...
     */
    if( i_info == DROP_ACCEPT_TEXT_URI_LIST )
    {
        UrlDecode( p_string );
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
            msg_Dbg( p_intf, "playlist protocol '%s', target '%s'",
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
            p_files = g_list_concat( p_files,
                                     GtkReadFiles( p_intf, p_string ) );
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

        msg_Dbg( p_intf, "adding %d elements", g_list_length( p_files ) );
        GtkAppendList( p_playlist, i_position, p_files );

        /* get the CList  and rebuild it. */
        p_clist = GTK_CLIST( lookup_widget( p_intf->p_sys->p_playwin,
                                            "playlist_clist" ) );
        GtkRebuildCList( p_clist , p_playlist );

        /* unlock the interface */
        vlc_mutex_unlock( &p_intf->change_lock );
    }

    vlc_object_release( p_playlist );
}


void GtkDeleteGListItem( gpointer data, gpointer param )
{
    int i_cur_row = (long)data;
    playlist_t * p_playlist = param;

    playlist_LockDelete( p_playlist, i_cur_row );
}


gint GtkCompareItems( gconstpointer a, gconstpointer b )
{
    return (ptrdiff_t) ( (int *)b - (int *)a );
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
static GList * GtkReadFiles( intf_thread_t * p_intf, gchar * psz_fsname )
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
            msg_Dbg( p_intf, "%s is a valid file, stacking on the playlist",
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

        msg_Dbg( p_intf, "%s is a folder", psz_fsname );

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
                                           GtkReadFiles( p_intf, psz_newfs ) );

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
    int i_dummy;
    int i_length;

    i_length = g_list_length( p_list );

    for( i_dummy = 0; i_dummy < i_length ; i_dummy++ )
    {
        playlist_Add( p_playlist,
                /* ok; this is a really nasty trick to insert
                   the item where they are suppose to go but, hey
                   this works :P (btw, you are really nasty too) */
               g_list_nth_data( p_list, i_dummy ),
               g_list_nth_data( p_list, i_dummy ),
               i_dummy == 0 ? PLAYLIST_INSERT | PLAYLIST_GO : PLAYLIST_INSERT,
               i_pos == PLAYLIST_END ? PLAYLIST_END : ( i_pos + i_dummy ) );
    }

    return 0;
}

/* statis timeouted function */
void GtkPlayListManage( intf_thread_t * p_intf )
{
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    GtkCList *   p_clist;

    if( p_playlist == NULL )
    {
        return;
    }

    /* this thing really sucks for now :( */

    /* TODO speak more with src/playlist/playlist.c */
    if( GTK_IS_WIDGET( p_intf->p_sys->p_playwin ) )
    {
        p_clist = GTK_CLIST( gtk_object_get_data( GTK_OBJECT(
                       p_intf->p_sys->p_playwin ), "playlist_clist" ) );

        vlc_mutex_lock( &p_playlist->object_lock );

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

        vlc_mutex_unlock( &p_playlist->object_lock );
    }

    vlc_object_release( p_playlist );
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

    vlc_mutex_lock( &p_playlist->object_lock );
    for( i_dummy = p_playlist->i_size ; i_dummy-- ; )
    {
        char psz_duration[MSTRTIME_MAX_SIZE];
        mtime_t dur = p_playlist->pp_items[i_dummy]->input.i_duration;
        if ( dur != -1 )
        {
            secstotimestr( psz_duration, dur/1000000 );
        }
        else
        {
            memcpy( psz_duration ,"no info",sizeof("no info" ));
        }
        ppsz_text[0] = p_playlist->pp_items[i_dummy]->input.psz_name;
        ppsz_text[1] = strdup( psz_duration );
        gtk_clist_insert( p_clist, 0, ppsz_text );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    gtk_clist_set_background( p_clist, p_playlist->i_index, &red);
    gtk_clist_thaw( p_clist );
}

/* URL-decode a file: URL path, return NULL if it's not what we expect */
static void UrlDecode( char *encoded_path )
{
    char *tmp = NULL, *cur = NULL, *ext = NULL;
    int realchar;

    if( !encoded_path || *encoded_path == '\0' )
    {
        return;
    }

    cur = encoded_path ;

    tmp = calloc(strlen(encoded_path) + 1,  sizeof(char) );

    while ( ( ext = strchr(cur, '%') ) != NULL)
    {
        strncat(tmp, cur, (ext - cur) / sizeof(char));
        ext++;

        if (!sscanf(ext, "%2x", &realchar))
        {
            free(tmp);
            return;
        }

        tmp[strlen(tmp)] = (char)realchar;

        cur = ext + 2;
    }

    strcat(tmp, cur);
    strcpy(encoded_path,tmp);
}

