/*****************************************************************************
 * pda_callbacks.c : Callbacks for the pda Linux Gtk+ plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: pda_callbacks.c,v 1.7 2003/11/08 16:04:05 jpsaman Exp $
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

#include "pda_callbacks.h"
#include "pda_interface.h"
#include "pda_support.h"
#include "pda.h"

static char* get_file_stat(const char *path, uid_t *uid, gid_t *gid, off_t *size);

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

void PlaylistRebuildListStore( GtkListStore * p_list, playlist_t * p_playlist )
{
    GtkTreeIter iter;
    int         i_dummy;
    gchar *     ppsz_text[2];
    GdkColor    red;
    red.red     = 65535;
    red.blue    = 0;
    red.green   = 0;

    vlc_mutex_lock( &p_playlist->object_lock );
    for( i_dummy = p_playlist->i_size ; i_dummy-- ; )
    {
        ppsz_text[0] = p_playlist->pp_items[i_dummy]->psz_name;
        ppsz_text[1] = "no info";
        gtk_list_store_append (p_list, &iter);
        gtk_list_store_set (p_list, &iter,
                            0, ppsz_text[0],
                            1, ppsz_text[1],
                            -1);
    }
    vlc_mutex_unlock( &p_playlist->object_lock );
}


/*****************************************************************************
 * Helper functions for URL changes in Media and Preferences notebook pages.
 ****************************************************************************/
void MediaURLOpenChanged( GtkWidget *widget, gchar *psz_url )
{
    intf_thread_t *p_intf = GtkGetIntf( widget );
    playlist_t   *p_playlist;
    GtkTreeView  *p_tvplaylist;

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

        p_tvplaylist = (GtkTreeView*) lookup_widget( GTK_WIDGET(widget), "tvPlaylist" );
        if (p_tvplaylist)
        {
            GtkListStore *p_liststore;
            p_liststore = (GtkListStore *) gtk_tree_view_get_model(p_tvplaylist);
            PlaylistRebuildListStore(p_liststore, p_playlist);
        }
    }
}

/*****************************************************************
 * Read directory helper function.
 ****************************************************************/
void ReadDirectory(GtkListStore *p_list, char *psz_dir )
{
    GtkTreeIter iter;
    struct dirent **namelist;
    int n=-1, status=-1;

    g_print("changing to dir %s\n", psz_dir);
    if (psz_dir)
    {
       status = chdir(psz_dir);
       if (status<0)
          g_print( "permision denied\n" );
    }
    n = scandir(".", &namelist, 0, alphasort);

    if (n<0)
        perror("scandir");
    else
    {
        int i;
        uint32_t uid;
        uint32_t gid;
        off_t  size;
        gchar *ppsz_text[5];

        /* XXX : kludge temporaire pour yopy */
        ppsz_text[0]="..";
        ppsz_text[1] = get_file_stat("..", &uid, &gid, &size);
        ppsz_text[2] = "";
        ppsz_text[3] = "";
        ppsz_text[4] = "";

        /* Add a new row to the model */
        gtk_list_store_append (p_list, &iter);
        gtk_list_store_set (p_list, &iter,
                            0, ppsz_text[0],
                            1, ppsz_text[1],
                            2, size,
                            3, ppsz_text[3],
                            4, ppsz_text[4],
                            -1);

        if (ppsz_text[1]) free(ppsz_text[1]);

        /* kludge */
        for (i=0; i<n; i++)
        {
            if (namelist[i]->d_name[0] != '.')
            {
                /* This is a list of strings. */
                ppsz_text[0] = namelist[i]->d_name;
                ppsz_text[1] = get_file_stat(namelist[i]->d_name, &uid, &gid, &size);
                ppsz_text[2] = "";
                ppsz_text[3] = "";
                ppsz_text[4] = "";
#if 0
                g_print( "(%d) file: %s permission: %s user: %ull group: %ull size: %ull\n", i, ppsz_text[0], ppsz_text[1], uid, gid, size );
#endif
                gtk_list_store_append (p_list, &iter);
                gtk_list_store_set (p_list, &iter,
                                    0, ppsz_text[0],
                                    1, ppsz_text[1],
                                    2, size,
                                    3, ppsz_text[3],
                                    4, ppsz_text[4],
                                    -1);

                if (ppsz_text[1]) free(ppsz_text[1]);
            }
        }
        free(namelist);
    }
}

static char* get_file_stat(const char *path, uid_t *uid, gid_t *gid, off_t *size)
{
    struct stat st;
    char *perm;

    perm = (char *) malloc(sizeof(char)*10);
    strncpy( perm, "----------", sizeof("----------"));
    if (lstat(path, &st)==0)
    {
        /* user, group, filesize */
        *uid = st.st_uid;
        *gid = st.st_gid;
        *size = st.st_size;
        /* determine permission modes */
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

gboolean
onPDADeleteEvent                       (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    msg_Dbg( p_intf, "about to exit vlc ... " );
    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_vlc->b_die = VLC_TRUE;
    vlc_mutex_unlock( &p_intf->change_lock );
    msg_Dbg( p_intf, "about to exit vlc ... signalled" );

    return TRUE;
}


void
onRewind                               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
    }
}


void
onPause                                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );
    }
}


void
onPlay                                 (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( GTK_WIDGET( button ) );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist  )
    {
        vlc_mutex_lock( &p_playlist->object_lock );
        if( p_playlist->i_size )
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
            playlist_Play( p_playlist );
        }
        else
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
        }
        vlc_object_release( p_playlist );
    }
}


void
onStop                                 (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( GTK_WIDGET( button ) );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist)
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
    }
}


void
onForward                              (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_FASTER );
    }
}


void
onAbout                                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET(button) );

    // Toggle notebook
    if (p_intf->p_sys->p_notebook)
    {
        gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        gtk_notebook_set_page(p_intf->p_sys->p_notebook,6);
    }
}


gboolean
SliderRelease                          (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 1;
    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}


gboolean
SliderPress                            (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 0;
    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}

void addSelectedToPlaylist(GtkTreeModel *model,
                               GtkTreePath *path,
                               GtkTreeIter *iter,
                               gpointer *userdata)
{
    GtkTreeView  *p_tvplaylist = NULL;
    gchar *filename;

    gtk_tree_model_get(model, iter, 0, &filename, -1);

    /* Add to playlist object. */
    p_tvplaylist = (GtkTreeView *) lookup_widget( GTK_WIDGET(userdata), "tvPlaylist");
    if (p_tvplaylist)
    {
        GtkTreeModel *p_play_model;
        GtkTreeIter   p_play_iter;

        p_play_model = gtk_tree_view_get_model(p_tvplaylist);

        /* Add a new row to the playlist treeview model */
        gtk_list_store_append (GTK_LIST_STORE(p_play_model), &p_play_iter);
        gtk_list_store_set (GTK_LIST_STORE(p_play_model), &p_play_iter,
                                0, filename,   /* Add path to it !!! */
                                1, "no info",
                                -1 );
    }
    else
       g_print("Error obtaining pointer to Play List");
}

void
onFileListRow                          (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);

    if (gtk_tree_selection_count_selected_rows(selection) == 1)
    {
        struct stat st;
        GtkTreeModel *model;
        GtkTreeIter   iter;
        gchar        *filename;

        /* This might be a directory selection */
        model = gtk_tree_view_get_model(treeview);
        if (!model)
            g_print( "Error: model is a null pointer\n" );
        if (!gtk_tree_model_get_iter(model, &iter, path))
            g_print( "Error: could not get iter from model\n" );

        gtk_tree_model_get(model, &iter, 0, &filename, -1);

        if (stat((char*)filename, &st)==0)
        {
            if (S_ISDIR(st.st_mode))
            {
                GtkListStore *p_model = NULL;

                /* Get new directory listing */
                p_model = gtk_list_store_new (5,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING,
                                           G_TYPE_UINT64,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING);
                if (NULL == p_model)
                    g_print( "ERROR: model has a NULL pointer\n" );
                ReadDirectory(p_model, filename);

                /* Update TreeView with new model */
                gtk_tree_view_set_model(treeview, (GtkTreeModel*) p_model);
                g_object_unref(p_model);
            }
            else
            {
                gtk_tree_selection_selected_foreach(selection, (GtkTreeSelectionForeachFunc) &addSelectedToPlaylist, (gpointer) treeview);
            }
        }
    }
    else
    {
        gtk_tree_selection_selected_foreach(selection, (GtkTreeSelectionForeachFunc) &addSelectedToPlaylist, (gpointer) treeview);
    }
}


void
onFileListColumns                      (GtkTreeView     *treeview,
                                        gpointer         user_data)
{
    g_print("onFileListColumn\n");
}


gboolean
onFileListRowSelected                  (GtkTreeView     *treeview,
                                        gboolean         start_editing,
                                        gpointer         user_data)
{
    g_print("onFileListRowSelected\n");
    return FALSE;
}


void
onAddFileToPlaylist                    (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkTreeView       *treeview = NULL;

    treeview = (GtkTreeView *) lookup_widget( GTK_WIDGET(button), "tvFileList");
    if (treeview)
    {
        onFileListRow(treeview, NULL, NULL, NULL );
    }
}


void
onEntryMRLChanged                      (GtkEditable     *editable,
                                        gpointer         user_data)
{
}


void
onEntryMRLEditingDone                  (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{
    g_print("onMRLEditingDone\n");
}


void
NetworkBuildMRL                        (GtkEditable     *editable,
                                        gpointer         user_data)
{
    GtkSpinButton *networkPort = NULL;
    GtkEntry      *entryMRL = NULL;
    GtkEntry      *networkMRLType = NULL;
    GtkEntry      *networkAddress = NULL;
    GtkEntry      *networkProtocol = NULL;
    GtkEntry      *networkType = NULL;
    const gchar   *mrlType;
    const gchar   *mrlAddress;
    gint     mrlPort;
    const gchar   *mrlProtocol;
    const gchar   *mrlNetworkType;
#define VLC_MAX_MRL     256
    char           text[VLC_MAX_MRL];
    int            pos = 0;

    entryMRL = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryMRL" );

    networkMRLType  = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkMRLType" );
    networkAddress  = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkAddress" );
    networkPort     = (GtkSpinButton*) lookup_widget( GTK_WIDGET(editable), "entryNetworkPort" );
    networkProtocol = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkProtocolType" );
    networkType     = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkType" );

    mrlType        = gtk_entry_get_text(GTK_ENTRY(networkMRLType));
    mrlAddress     = gtk_entry_get_text(GTK_ENTRY(networkAddress));
    mrlPort        = gtk_spin_button_get_value_as_int(networkPort);
    mrlProtocol    = gtk_entry_get_text(GTK_ENTRY(networkProtocol));
    mrlNetworkType = gtk_entry_get_text(GTK_ENTRY(networkType));

    /* Build MRL from parts ;-) */
    pos = snprintf( &text[0], VLC_MAX_MRL, "%s", (char*)mrlType);
    if (strncasecmp( (char*)mrlProtocol, "IPv6",4)==0 )
    {
        pos += snprintf( &text[pos], VLC_MAX_MRL - pos, "6://" );
        if (strncasecmp( (char*)mrlNetworkType, "multicast",9)==0)
        {
            pos += snprintf( &text[pos], VLC_MAX_MRL - pos, "@" );
        }
        pos += snprintf( &text[pos], VLC_MAX_MRL - pos, "[%s]:%d", (char*)mrlAddress, (int)mrlPort );
    }
    else
    {
        pos += snprintf( &text[pos], VLC_MAX_MRL - pos, "://" );
        if (strncasecmp( (char*)mrlNetworkType, "multicast",9)==0)
        {
            pos += snprintf( &text[pos], VLC_MAX_MRL - pos, "@" );
        }
        pos += snprintf( &text[pos], VLC_MAX_MRL - pos, "%s:%d", (char*)mrlAddress, (int)mrlPort );
    }

    if (pos >= VLC_MAX_MRL)
        text[VLC_MAX_MRL-1]='\0';

    gtk_entry_set_text(entryMRL,text);
#undef VLC_MAX_MRL
}

void
onAddNetworkPlaylist                   (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkEntry     *p_mrl = NULL;
    GtkTreeView  *p_tvplaylist = NULL;
    GtkTreeModel *p_play_model;
    GtkTreeIter   p_play_iter;
    const gchar  *mrl_name;

    p_mrl = (GtkEntry*) lookup_widget(GTK_WIDGET(button),"entryMRL" );
    if (NULL != p_mrl)
    {
        mrl_name = gtk_entry_get_text(p_mrl);

        p_tvplaylist = (GtkTreeView *) lookup_widget( GTK_WIDGET(button), "tvPlaylist");
        if (NULL != p_tvplaylist)
        {
            p_play_model = gtk_tree_view_get_model(p_tvplaylist);

            /* Add a new row to the playlist treeview model */
            gtk_list_store_append (GTK_LIST_STORE(p_play_model), &p_play_iter);
            gtk_list_store_set (GTK_LIST_STORE(p_play_model), &p_play_iter,
                                    0, mrl_name,   /* Add path to it !!! */
                                    1, "no info",
                                    -1 );
        }
    }
}


void
onV4LAudioChanged                      (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
onEntryV4LAudioEditingDone             (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{

}


void
onV4LVideoChanged                      (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
onEntryV4LVideoEditingDone             (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{

}


void
onAddCameraToPlaylist                  (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
onVideoDeviceChanged                   (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
onEntryVideoDeviceEditingDone          (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{

}


void
onVideoCodecChanged                    (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
onEntryVideoCodecEditingDone           (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{

}


void
onVideoBitrateChanged                  (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
onVideoBitrateEditingDone              (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{

}


void
onAudioDeviceChanged                   (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
onEntryAudioDeviceEditingDone          (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{

}


void
onAudioCodecChanged                    (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
onEntryAudioCodecEditingDone           (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{

}


void
onAudioBitrateChanged                  (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
onAudioBitrateEditingDone              (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{

}


void
onAddServerToPlaylist                  (GtkButton       *button,
                                        gpointer         user_data)
{

}


gboolean
PlaylistEvent                          (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    g_print("onPlaylistEvent\n");
    return FALSE;
}


void
onPlaylistColumnsChanged               (GtkTreeView     *treeview,
                                        gpointer         user_data)
{
    g_print("onPlaylistColumnsChanged\n");
}


gboolean
onPlaylistRowSelected                  (GtkTreeView     *treeview,
                                        gboolean         start_editing,
                                        gpointer         user_data)
{
  g_print("onPlaylistRowSelected\n");
  return FALSE;
}


void
onPlaylistRow                          (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data)
{
    g_print("onPlaylistRow\n");
}


void
onUpdatePlaylist                       (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
onDeletePlaylist                       (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
onClearPlaylist                        (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
onPreferenceSave                       (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
onPreferenceApply                      (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
onPreferenceCancel                     (GtkButton       *button,
                                        gpointer         user_data)
{

}



void
onNetworkMRLAdd                        (GtkContainer    *container,
                                        GtkWidget       *widget,
                                        gpointer         user_data)
{

}


