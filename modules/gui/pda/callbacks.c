/*****************************************************************************
 * callbacks.c : Callbacks for the pda Linux Gtk+ plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: callbacks.c,v 1.1 2003/07/23 22:02:56 jpsaman Exp $
 *
 * Authors: Jean-Paul Saman <jpsaman@wxs.nl>
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

#include "playlist.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "pda.h"

static char* get_file_perm(const char *path);

/*****************************************************************************
 * Useful function to retrieve p_intf
 ****************************************************************************/
void * E_(__GtkGetIntf)( GtkWidget * widget )
{
    void *p_data;

    if( GTK_IS_MENU_ITEM( widget ) )
    {
        /* Look for a GTK_MENU */
        while( widget->parent && !GTK_IS_MENU( widget ) )
        {
            widget = widget->parent;
        }

        /* Maybe this one has the data */
        p_data = gtk_object_get_data( GTK_OBJECT( widget ), "p_intf" );
        if( p_data )
        {
            return p_data;
        }

        /* Otherwise, the parent widget has it */
        widget = gtk_menu_get_attach_widget( GTK_MENU( widget ) );
    }

    /* We look for the top widget */
    widget = gtk_widget_get_toplevel( GTK_WIDGET( widget ) );

    p_data = gtk_object_get_data( GTK_OBJECT( widget ), "p_intf" );

    return p_data;
}

/*****************************************************************************
 * Helper functions for URL changes in Media and Preferences notebook pages.
 ****************************************************************************/
void MediaURLOpenChanged( GtkWidget *widget, gchar *psz_url )
{
    intf_thread_t *p_intf = GtkGetIntf( widget );
    playlist_t *p_playlist;

    // Add p_url to playlist .... but how ?
    p_playlist = (playlist_t *)
             vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist ==  NULL)
    {
        return;
    }
    
    if( p_playlist )
    {
        if (p_intf->p_sys->b_autoplayfile)
        {
            playlist_Add( p_playlist, (char*)psz_url, 0, 0, 
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END);
        }
        else
        {
            playlist_Add( p_playlist, (char*)psz_url, 0, 0, 
                          PLAYLIST_APPEND, PLAYLIST_END );
        }
        vlc_object_release(  p_playlist );
        PDARebuildCList( p_intf->p_sys->p_clistplaylist, p_playlist);
    }
}

/*****************************************************************
 * Read directory helper function.
 ****************************************************************/
void ReadDirectory( GtkCList *clist, char *psz_dir )
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET(clist) );
    struct dirent **namelist;
    int n=-1, status=-1;

    msg_Dbg(p_intf, "changing to dir %s", psz_dir);
    if (psz_dir)
    {
       status = chdir(psz_dir);
       if (status<0)
          msg_Err( p_intf, "permision denied" );			
    }
    n = scandir(".", &namelist, 0, alphasort);
    
    if (n<0)
        perror("scandir");
    else
    {
        int i, ctr=0;
        gchar *ppsz_text[5];

        msg_Dbg( p_intf, "updating interface" );
        gtk_clist_freeze( clist );
        gtk_clist_clear( clist );

        /* XXX : kludge temporaire pour yopy */
        ppsz_text[0]="..";
        ppsz_text[1] = get_file_perm("..");
        ppsz_text[2] = "";
        ppsz_text[3] = "";
        ppsz_text[4] = "";
  	gtk_clist_insert( GTK_CLIST(clist), ctr++, ppsz_text );

        /* kludge */                
        for (i=0; i<n; i++)
        {
            if (namelist[i]->d_name[0] != '.')
            {
                /* This is a list of strings. */
                ppsz_text[0] = namelist[i]->d_name;
                ppsz_text[1] = get_file_perm(namelist[i]->d_name);
                ppsz_text[2] = "";
                ppsz_text[3] = "";
                ppsz_text[4] = "";
                //            msg_Dbg(p_intf, "(%d) file: %s permission: %s", i, ppsz_text[0], ppsz_text[1] );
                gtk_clist_insert( GTK_CLIST(clist), ctr++, ppsz_text );
            }
        }
        gtk_clist_thaw( clist );
        free(namelist);
    }

    /* now switch to the "file" tab */
    if (p_intf->p_sys->p_mediabook)
    {
       gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_mediabook) );
       gtk_notebook_set_page(p_intf->p_sys->p_mediabook,0);
    }
}

static char* get_file_perm(const char *path)
{
    struct stat st;
    char *perm;

    perm = (char *) malloc(sizeof(char)*10);
    strncpy( perm, "----------", sizeof("----------"));
    if (lstat(path, &st)==0)
    {
        if (S_ISLNK(st.st_mode))
            perm[0]= 'l';
        else if (S_ISDIR(st.st_mode))
            perm[0]= 'd';
        else if (S_ISCHR(st.st_mode))
            perm[0]= 'c';
        else if (S_ISBLK(st.st_mode))
            perm[0]= 'b';
        else if (S_ISFIFO(st.st_mode))
            perm[0]= 'f';
        else if (S_ISSOCK(st.st_mode))
            perm[0]= 's';
        else if (S_ISREG(st.st_mode))
            perm[0]= '-';
        else /* Unknown type is an error */
            perm[0]= '?';
        /* Get file permissions */
        /* User */
        if (st.st_mode & S_IRUSR)
            perm[1]= 'r';
        if (st.st_mode & S_IWUSR)
            perm[2]= 'w';
        if (st.st_mode & S_IXUSR)
        {
            if (st.st_mode & S_ISUID)
                perm[3] = 's';
            else
                perm[3]= 'x';
        }
        else if (st.st_mode & S_ISUID)
            perm[3] = 'S';
        /* Group */
        if (st.st_mode & S_IRGRP)
            perm[4]= 'r';
        if (st.st_mode & S_IWGRP)
            perm[5]= 'w';
        if (st.st_mode & S_IXGRP)
        {
            if (st.st_mode & S_ISGID)
                perm[6] = 's';
            else
                perm[6]= 'x';
        }
        else if (st.st_mode & S_ISGID)
            perm[6] = 'S';
        /* Other */
        if (st.st_mode & S_IROTH)
            perm[7]= 'r';
        if (st.st_mode & S_IWOTH)
            perm[8]= 'w';
        if (st.st_mode & S_IXOTH)
        {
            // 'sticky' bit
            if (st.st_mode &S_ISVTX)
                perm[9] = 't';
            else
                perm[9]= 'x';
        }
        else if (st.st_mode &S_ISVTX)
            perm[9]= 'T';
    }
    return perm;
}

/*
 * Main interface callbacks
 */
gboolean PDAExit( GtkWidget       *widget,
                       gpointer         user_data )
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_vlc->b_die = VLC_TRUE;
    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}

void
on_toolbar_open_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET( button ) );

    if (p_intf->p_sys->p_notebook)
    {
       gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
       gtk_notebook_set_page(p_intf->p_sys->p_notebook,0);
    }
    if (p_intf->p_sys->p_mediabook)
    {
       gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_mediabook) );
       gtk_notebook_set_page(p_intf->p_sys->p_mediabook,0);
    }
    gdk_window_raise( p_intf->p_sys->p_window->window );
    if (p_intf->p_sys->p_clist)
    {
       ReadDirectory(p_intf->p_sys->p_clist, ".");
    }
}

void
on_toolbar_preferences_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET( button ) );

    if (p_intf->p_sys->p_notebook)
    {
       gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
       gtk_notebook_set_page(p_intf->p_sys->p_notebook,2);
    }
    gdk_window_raise( p_intf->p_sys->p_window->window );
}

void
on_toolbar_rewind_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
    }
}

void
on_toolbar_pause_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );
    }
}

void
on_toolbar_play_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
     intf_thread_t *  p_intf = GtkGetIntf( GTK_WIDGET( button ) );
     playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

     if( p_playlist == NULL )
     {
         /* Display open page */
         on_toolbar_open_clicked(button,user_data);
     }

     /* If the playlist is empty, open a file requester instead */
     vlc_mutex_lock( &p_playlist->object_lock );
     if( p_playlist->i_size )
     {
         vlc_mutex_unlock( &p_playlist->object_lock );
         playlist_Play( p_playlist );
         vlc_object_release( p_playlist );
         gdk_window_lower( p_intf->p_sys->p_window->window );
     }
     else
     {
         vlc_mutex_unlock( &p_playlist->object_lock );
         vlc_object_release( p_playlist );
         /* Display open page */
         on_toolbar_open_clicked(button,user_data);
    }
}

void
on_toolbar_stop_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( GTK_WIDGET( button ) );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist)
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}

void
on_toolbar_forward_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_FASTER );
    }
}

void
on_toolbar_playlist_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET(button) );

    // Toggle notebook
    if (p_intf->p_sys->p_notebook)
    {
        gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        gtk_notebook_set_page(p_intf->p_sys->p_notebook,1);
    }
    gdk_window_raise( p_intf->p_sys->p_window->window );
}

void
on_toolbar_about_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET(button) );

    // Toggle notebook
    if (p_intf->p_sys->p_notebook)
    {
        gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        gtk_notebook_set_page(p_intf->p_sys->p_notebook,3);
    }
    gdk_window_raise( p_intf->p_sys->p_window->window );
}

void
on_comboURL_entry_changed              (GtkEditable     *editable,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf = GtkGetIntf( GTK_WIDGET(editable) );
    gchar *       psz_url;
    struct stat st;
    
    psz_url = gtk_entry_get_text(GTK_ENTRY(editable));
/*    if( (strncmp("file://",(const char *) psz_url,7)==0) ||
        (strncmp("udp://",(const char *) psz_url,6)==0) ||
        (strncmp("udp4://",(const char *) psz_url,7)==0) ||
        (strncmp("udp6://",(const char *) psz_url,7)==0) ||
        (strncmp("udpstream://",(const char *) psz_url,12)==0) ||
        (strncmp("rtp://",(const char *) psz_url,6)==0) ||
        (strncmp("rtp4://",(const char *) psz_url,7)==0) ||
        (strncmp("rtp6://",(const char *) psz_url,7)==0) ||
        (strncmp("rtpstream://",(const char *) psz_url,12)==0) ||
        (strncmp("ftp://",(const char *) psz_url,6)==0) ||
        (strncmp("mms://",(const char *) psz_url,6)==0) ||
        (strncmp("http://",(const char *) psz_url,7)==0) )
    {
        MediaURLOpenChanged(GTK_WIDGET(editable), psz_url);
    }
    else */
    if (stat((char*)psz_url, &st)==0)
    {
        if (S_ISDIR(st.st_mode))
        {
            if (!p_intf->p_sys->p_clist)
                msg_Err(p_intf, "p_clist pointer invalid!!" );
            ReadDirectory(p_intf->p_sys->p_clist, psz_url);
        }
        else if( (S_ISLNK(st.st_mode)) || (S_ISCHR(st.st_mode)) ||
            (S_ISBLK(st.st_mode)) || (S_ISFIFO(st.st_mode))||
            (S_ISSOCK(st.st_mode))|| (S_ISREG(st.st_mode)) )
        {
            MediaURLOpenChanged(GTK_WIDGET(editable), psz_url);
        }
    }
}

void
on_clistmedia_click_column             (GtkCList        *clist,
                                        gint             column,
                                        gpointer         user_data)
{
    static GtkSortType sort_type = GTK_SORT_ASCENDING;

    // Should sort on column
    switch(sort_type)
    {
        case GTK_SORT_ASCENDING:
            sort_type = GTK_SORT_DESCENDING;
            break;
        case GTK_SORT_DESCENDING:
            sort_type = GTK_SORT_ASCENDING;
            break;
    }
    gtk_clist_freeze( clist );
    gtk_clist_set_sort_type( clist, sort_type );
    gtk_clist_sort( clist );
    gtk_clist_thaw( clist );
}

void
on_clistmedia_select_row               (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf = GtkGetIntf( GTK_WIDGET(clist) );
    gchar *text[2];
    gint ret;
    struct stat st;

    if (!p_intf->p_sys->p_clist)
		msg_Err(p_intf, "p_clist pointer is invalid.");
    ret = gtk_clist_get_text (p_intf->p_sys->p_clist, row, 0, text);
    if (ret)
    {
        if (stat((char*)text[0], &st)==0)
        {
            if (S_ISDIR(st.st_mode))
               ReadDirectory(p_intf->p_sys->p_clist, text[0]);
            else if( (S_ISLNK(st.st_mode)) || (S_ISCHR(st.st_mode)) ||
                     (S_ISBLK(st.st_mode)) || (S_ISFIFO(st.st_mode))||
                     (S_ISSOCK(st.st_mode))|| (S_ISREG(st.st_mode)) )
            {
               MediaURLOpenChanged(GTK_WIDGET(p_intf->p_sys->p_clist), text[0]);
            }
       }
    }
}

void
on_cbautoplay_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
}

gboolean
on_pda_delete_event               (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    PDAExit( GTK_WIDGET( widget ), user_data );
    return TRUE;
}

/* Slider Management */
gboolean
PDASliderRelease                  (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 1;
    vlc_mutex_unlock( &p_intf->change_lock );

    return FALSE;
}

gboolean
PDASliderPress                    (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 0;
    vlc_mutex_unlock( &p_intf->change_lock );

    return FALSE;
}

void
PDAMrlGo                          (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    MediaURLOpenChanged( GTK_WIDGET( button ),
            gtk_entry_get_text(p_intf->p_sys->p_mrlentry ) );
}

void
PDAPreferencesApply               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf = GtkGetIntf( GTK_WIDGET(button) );
    
    GtkToggleButton * p_autopl_button = GTK_GET( TOGGLE_BUTTON, "cbautoplay" );
    if (gtk_toggle_button_get_active(p_autopl_button))
    {
        p_intf->p_sys->b_autoplayfile = 1;
    }
    else
    {
        p_intf->p_sys->b_autoplayfile = 0;
    }
}

