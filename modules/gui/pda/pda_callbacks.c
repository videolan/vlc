/*****************************************************************************
 * pda_callbacks.c : Callbacks for the pda Linux Gtk+ plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: pda_callbacks.c,v 1.16 2003/11/25 20:01:08 jpsaman Exp $
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
#include <pwd.h>
#include <grp.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "pda_callbacks.h"
#include "pda_interface.h"
#include "pda_support.h"
#include "pda.h"

#define VLC_MAX_MRL     256

static char *get_file_perms(struct stat st);

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

void PlaylistAddItem(GtkWidget *widget, gchar *name)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );
    playlist_t    *p_playlist;
    GtkTreeView   *p_tvplaylist = NULL;

    p_playlist = (playlist_t *)
             vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist ==  NULL)
    {   /* Bail out when VLC's playlist object is not found. */
        return;
    }

    /* Add to playlist object. */
    p_tvplaylist = (GtkTreeView *) lookup_widget( GTK_WIDGET(widget), "tvPlaylist");
    if (p_tvplaylist)
    {
        GtkTreeModel *p_play_model;
        GtkTreeIter   p_play_iter;

        p_play_model = gtk_tree_view_get_model(p_tvplaylist);

        if (p_play_model)
        {
            /* Add a new row to the playlist treeview model */
            gtk_list_store_append (GTK_LIST_STORE(p_play_model), &p_play_iter);
            gtk_list_store_set (GTK_LIST_STORE(p_play_model), &p_play_iter,
                                    0, name,   /* Add path to it !!! */
                                    1, "no info",
                                    -1 );

            msg_Dbg( p_intf, "Adding files to playlist ...");
            /* Add to VLC's playlist */
#if 0
            if (p_intf->p_sys->b_autoplayfile)
            {
                playlist_Add( p_playlist, (char*)name, 0, 0,
                              PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END);
            }
            else
            {
                playlist_Add( p_playlist, (char*)name, 0, 0,
                              PLAYLIST_APPEND, PLAYLIST_END );
            }
#endif
            msg_Dbg( p_intf, "done");
        }
    }
    vlc_object_release(  p_playlist );
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

/*****************************************************************
 * Read directory helper function.
 ****************************************************************/
void ReadDirectory(intf_thread_t *p_intf, GtkListStore *p_list, char *psz_dir )
{
    GtkTreeIter    iter;
    struct dirent **namelist;
    struct passwd *pw;
    struct group  *grp;
    struct stat    st;
    int n=-1, status=-1;

    msg_Dbg(p_intf, "Changing to dir %s", psz_dir);
    if (psz_dir)
    {
       status = chdir(psz_dir);
       if (status<0)
          msg_Dbg(p_intf, "permision denied" );
    }
    n = scandir(".", &namelist, 0, alphasort);

    if (n<0)
        perror("scandir");
    else
    {
        int i;
        gchar *ppsz_text[4];

        if (lstat("..", &st)==0)
        {
            /* user, group  */
            pw  = getpwuid(st.st_uid);
            grp = getgrgid(st.st_gid);

            /* XXX : kludge temporaire pour yopy */
            ppsz_text[0] = "..";
            ppsz_text[1] = get_file_perms(st);
            ppsz_text[2] = pw->pw_name;
            ppsz_text[3] = grp->gr_name;

            /* Add a new row to the model */
            gtk_list_store_append (p_list, &iter);
            gtk_list_store_set (p_list, &iter,
                                0, ppsz_text[0],
                                1, ppsz_text[1],
                                2, st.st_size,
                                3, ppsz_text[2],
                                4, ppsz_text[3],
                                -1);

            if (ppsz_text[1]) free(ppsz_text[1]);
        }
            /* kludge */
        for (i=0; i<n; i++)
        {           
            if ((namelist[i]->d_name[0] != '.') &&
                (lstat(namelist[i]->d_name, &st)==0))
            {
                /* user, group  */
                pw  = getpwuid(st.st_uid);
                grp = getgrgid(st.st_gid);

                /* This is a list of strings. */
                ppsz_text[0] = namelist[i]->d_name;
                ppsz_text[1] = get_file_perms(st);
                ppsz_text[2] = pw->pw_name;
                ppsz_text[3] = grp->gr_name;
#if 0
                msg_Dbg(p_intf, "(%d) file: %s permission: %s user: %s group: %s", i, ppsz_text[0], ppsz_text[1], ppsz_text[2], ppsz_text[3] );
#endif
                gtk_list_store_append (p_list, &iter);
                gtk_list_store_set (p_list, &iter,
                                    0, ppsz_text[0],
                                    1, ppsz_text[1],
                                    2, st.st_size,
                                    3, ppsz_text[2],
                                    4, ppsz_text[3],
                                    -1);

                if (ppsz_text[1]) free(ppsz_text[1]);
            }
        }
        free(namelist);
    }
}

static char *get_file_perms(const struct stat st)
{
    char  *perm;

    perm = (char *) malloc(sizeof(char)*10);
    strncpy( perm, "----------", sizeof("----------"));

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

    if (p_intf->p_sys->p_input != NULL)
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
    }
}


void
onPause                                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if (p_intf->p_sys->p_input != NULL)
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

    if (p_playlist)
    {
        vlc_mutex_lock( &p_playlist->object_lock );
        if (p_playlist->i_size)
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
    if (p_playlist)
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
    }
}


void
onForward                              (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    if (p_intf->p_sys->p_input != NULL)
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
    gchar *filename;

    gtk_tree_model_get(model, iter, 0, &filename, -1);

    PlaylistAddItem(GTK_WIDGET(userdata), filename);
}

void
onFileListRow                          (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET(treeview) );
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);

    if (gtk_tree_selection_count_selected_rows(selection) == 1)
    {
        struct stat   st;
        GtkTreeModel *model;
        GtkTreeIter   iter;
        gchar        *filename;

        /* This might be a directory selection */
        model = gtk_tree_view_get_model(treeview);
        if (!model)
        {
            msg_Err(p_intf, "PDA: Filelist model contains a NULL pointer\n" );
            return;
        }
        if (!gtk_tree_model_get_iter(model, &iter, path))
        {
            msg_Err( p_intf, "PDA: Could not get iter from model" );
            return;
        }

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
                if (p_model)
                {
                    ReadDirectory(p_intf, p_model, filename);

                    /* Update TreeView with new model */
                    gtk_tree_view_set_model(treeview, (GtkTreeModel*) p_model);
                    g_object_unref(p_model);
                }
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
onAddFileToPlaylist                    (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkTreeView       *treeview = NULL;

    treeview = (GtkTreeView *) lookup_widget( GTK_WIDGET(button), "tvFileList");
    if (treeview)
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);

        gtk_tree_selection_selected_foreach(selection, (GtkTreeSelectionForeachFunc) &addSelectedToPlaylist, (gpointer) treeview);    
    }
}


void
NetworkBuildMRL                        (GtkEditable     *editable,
                                        gpointer         user_data)
{
    GtkSpinButton *networkPort = NULL;
    GtkEntry      *entryMRL = NULL;
    GtkEntry      *networkType = NULL;
    GtkEntry      *networkAddress = NULL;
    GtkEntry      *networkProtocol = NULL;
    const gchar   *mrlNetworkType;
    const gchar   *mrlAddress;
    const gchar   *mrlProtocol;
    gint           mrlPort;
    char           text[VLC_MAX_MRL];
    int            pos = 0;

    entryMRL = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryMRL" );

    networkType     = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkType" );
    networkAddress  = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkAddress" );
    networkPort     = (GtkSpinButton*) lookup_widget( GTK_WIDGET(editable), "entryNetworkPort" );
    networkProtocol = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkProtocolType" );

    mrlNetworkType = gtk_entry_get_text(GTK_ENTRY(networkType));
    mrlAddress     = gtk_entry_get_text(GTK_ENTRY(networkAddress));
    mrlPort        = gtk_spin_button_get_value_as_int(networkPort);
    mrlProtocol    = gtk_entry_get_text(GTK_ENTRY(networkProtocol));

    /* Build MRL from parts ;-) */
    pos = snprintf( &text[0], VLC_MAX_MRL, "%s://", (char*)mrlProtocol);
    if (strncasecmp( (char*)mrlNetworkType, "multicast",9)==0)
    {
        pos += snprintf( &text[pos], VLC_MAX_MRL - pos, "@" );
    }
    pos += snprintf( &text[pos], VLC_MAX_MRL - pos, "%s:%d", (char*)mrlAddress, (int)mrlPort );

    if (pos >= VLC_MAX_MRL)
        text[VLC_MAX_MRL-1]='\0';

    gtk_entry_set_text(entryMRL,text);
}

void
onAddNetworkPlaylist                   (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkEntry     *p_mrl = NULL;
    const gchar  *mrl_name;

    p_mrl = (GtkEntry*) lookup_widget(GTK_WIDGET(button),"entryMRL" );
    if (p_mrl)
    {
        mrl_name = gtk_entry_get_text(p_mrl);

        PlaylistAddItem(GTK_WIDGET(button), (gchar *)mrl_name);
    }
}


void
onAddCameraToPlaylist                  (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    GtkSpinButton *entryV4LChannel = NULL;
    GtkSpinButton *entryV4LFrequency = NULL;
    GtkSpinButton *entryV4LSampleRate = NULL;
    GtkSpinButton *entryV4LQuality = NULL;
    GtkSpinButton *entryV4LTuner = NULL;
    gint    i_v4l_channel;
    gint    i_v4l_frequency;
    gint    i_v4l_samplerate;
    gint    i_v4l_quality;
    gint    i_v4l_tuner;

    GtkEntry      *entryV4LVideoDevice = NULL;
    GtkEntry      *entryV4LAudioDevice = NULL;
    GtkEntry      *entryV4LNorm = NULL;
    GtkEntry      *entryV4LSize = NULL;
    GtkEntry      *entryV4LSoundDirection = NULL;
    const gchar   *p_v4l_video_device;
    const gchar   *p_v4l_audio_device;
    const gchar   *p_v4l_norm;
    const gchar   *p_v4l_size;
    const gchar   *p_v4l_sound_direction;

    /* MJPEG only */
    GtkCheckButton *checkV4LMJPEG = NULL;
    GtkSpinButton  *entryV4LDecimation = NULL;
    gboolean        b_v4l_mjpeg;
    gint            i_v4l_decimation;
    /* end MJPEG only */

    char v4l_mrl[VLC_MAX_MRL];
    int pos;

    pos = snprintf( &v4l_mrl[0], VLC_MAX_MRL, "v4l://");

    entryV4LChannel    = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryV4LChannel" );
    entryV4LFrequency  = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryV4LFrequency" );
    entryV4LSampleRate = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryV4LSampleRate" );
    entryV4LQuality    = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryV4LQuality" );
    entryV4LTuner      = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryV4LTuner" );

    entryV4LVideoDevice  = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryV4LVideoDevice" );
    entryV4LAudioDevice  = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryV4LAudioDevice" );
    entryV4LNorm  = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryV4LNorm" );
    entryV4LSize  = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryV4LSize" );
    entryV4LSoundDirection  = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryV4LSoundDirection" );

    i_v4l_channel = gtk_spin_button_get_value_as_int(entryV4LChannel);
    i_v4l_frequency = gtk_spin_button_get_value_as_int(entryV4LFrequency);
    i_v4l_samplerate = gtk_spin_button_get_value_as_int(entryV4LSampleRate);
    i_v4l_quality = gtk_spin_button_get_value_as_int(entryV4LQuality);
    i_v4l_tuner = gtk_spin_button_get_value_as_int(entryV4LTuner);

    p_v4l_video_device = gtk_entry_get_text(GTK_ENTRY(entryV4LVideoDevice));
    p_v4l_audio_device = gtk_entry_get_text(GTK_ENTRY(entryV4LAudioDevice));
    p_v4l_norm = gtk_entry_get_text(GTK_ENTRY(entryV4LNorm));
    p_v4l_size  = gtk_entry_get_text(GTK_ENTRY(entryV4LSize));
    p_v4l_sound_direction = gtk_entry_get_text(GTK_ENTRY(entryV4LSoundDirection));

    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":%s", (char*)p_v4l_video_device );
    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":adev=%s", (char*)p_v4l_audio_device );
    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":norm=%s", (char*)p_v4l_norm );
    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":size=%s", (char*)p_v4l_size );
    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":%s", (char*)p_v4l_sound_direction );

    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":channel=%d", (int)i_v4l_channel );
    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":frequency=%d", (int)i_v4l_frequency );
    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":samplerate=%d", (int)i_v4l_samplerate );
    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":quality=%d", (int)i_v4l_quality );
    pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":tuner=%d", (int)i_v4l_tuner );

    /* MJPEG only */
    checkV4LMJPEG      = (GtkCheckButton*) lookup_widget( GTK_WIDGET(button), "checkV4LMJPEG" );
    b_v4l_mjpeg = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkV4LMJPEG));
    if (b_v4l_mjpeg)
    {
        entryV4LDecimation = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryV4LDecimation" );
        i_v4l_decimation = gtk_spin_button_get_value_as_int(entryV4LDecimation);
        pos += snprintf( &v4l_mrl[pos], VLC_MAX_MRL - pos, ":mjpeg:%d", (int)i_v4l_decimation );
    }
    /* end MJPEG only */

    if (pos >= VLC_MAX_MRL)
    {
        v4l_mrl[VLC_MAX_MRL-1]='\0';
        msg_Err(p_intf, "Media Resource Locator is truncated to: %s", v4l_mrl);
    }

    PlaylistAddItem(GTK_WIDGET(button), (gchar*) &v4l_mrl);
}


gboolean
PlaylistEvent                          (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    return FALSE;
}


void
onPlaylistColumnsChanged               (GtkTreeView     *treeview,
                                        gpointer         user_data)
{
}


gboolean
onPlaylistRowSelected                  (GtkTreeView     *treeview,
                                        gboolean         start_editing,
                                        gpointer         user_data)
{
    return FALSE;
}


void
onPlaylistRow                          (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data)
{
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
    intf_thread_t *p_intf = GtkGetIntf( button );

    msg_Dbg(p_intf, "Clear playlist" );
}


void
onPreferenceSave                       (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    msg_Dbg(p_intf, "Preferences Save" );
}


void
onPreferenceApply                      (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    msg_Dbg(p_intf, "Preferences Apply" );
}


void
onPreferenceCancel                     (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    msg_Dbg(p_intf, "Preferences Cancel" );
}


void
onAddTranscodeToPlaylist               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    GtkEntry       *entryVideoCodec = NULL;
    GtkSpinButton  *entryVideoBitrate = NULL;
    GtkSpinButton  *entryVideoBitrateTolerance = NULL;
    GtkSpinButton  *entryVideoKeyFrameInterval = NULL;
    GtkCheckButton *checkVideoDeinterlace = NULL;
    GtkEntry       *entryAudioCodec = NULL;
    GtkSpinButton  *entryAudioBitrate = NULL;
    const gchar    *p_video_codec;
    gint            i_video_bitrate;
    gint            i_video_bitrate_tolerance;
    gint            i_video_keyframe_interval;
    gboolean        b_video_deinterlace;
    const gchar    *p_audio_codec;
    gint            i_audio_bitrate;

    GtkEntry       *entryStdAccess = NULL;
    GtkEntry       *entryStdMuxer = NULL;
    GtkEntry       *entryStdURL = NULL;
    GtkSpinButton  *entryStdTTL = NULL;
    const gchar    *p_std_access;
    const gchar    *p_std_muxer;
    const gchar    *p_std_url;
    gint            i_std_ttl;

    gchar mrl[VLC_MAX_MRL];
    int   pos;

    pos = snprintf( &mrl[0], VLC_MAX_MRL, "--sout '#transcode{");

    entryVideoCodec   = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryVideoCodec" );
    entryVideoBitrate = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryVideoBitrate" );
    entryVideoBitrateTolerance = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryVideoBitrateTolerance" );
    entryVideoKeyFrameInterval = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryVideoKeyFrameInterval" );
    
    p_video_codec = gtk_entry_get_text(GTK_ENTRY(entryVideoCodec));
    i_video_bitrate = gtk_spin_button_get_value_as_int(entryVideoBitrate);
    i_video_bitrate_tolerance = gtk_spin_button_get_value_as_int(entryVideoBitrateTolerance);
    i_video_keyframe_interval = gtk_spin_button_get_value_as_int(entryVideoKeyFrameInterval);
    
    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "vcodec=%s,", (char*)p_video_codec );
    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "vb=%d,", (int)i_video_bitrate );
    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "vt=%d,", (int)i_video_bitrate_tolerance );
    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "keyint=%d,", (int)i_video_keyframe_interval );

    checkVideoDeinterlace = (GtkCheckButton*) lookup_widget( GTK_WIDGET(button), "checkVideoDeinterlace" );
    b_video_deinterlace = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkVideoDeinterlace));
    if (b_video_deinterlace)
    {
        pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "deinterlace," );
    }
    entryAudioCodec   = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryAudioCodec" );
    entryAudioBitrate = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryAudioBitrate" );

    p_audio_codec = gtk_entry_get_text(GTK_ENTRY(entryAudioCodec));
    i_audio_bitrate = gtk_spin_button_get_value_as_int(entryAudioBitrate);

    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "acodec=%s,", (char*)p_audio_codec );
    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "ab=%d", (int)i_audio_bitrate );

    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "}:std{" );

    entryStdAccess = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryStdAccess" );
    entryStdMuxer  = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryStdMuxer" );
    entryStdURL = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryStdURL" );
    entryStdTTL = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryStdTTL" );

    p_std_access = gtk_entry_get_text(GTK_ENTRY(entryStdAccess));
    p_std_muxer = gtk_entry_get_text(GTK_ENTRY(entryStdMuxer));
    p_std_url = gtk_entry_get_text(GTK_ENTRY(entryStdURL));

    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "access=%s,", (char*)p_std_access);
    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "mux=%s,", (char*)p_std_muxer);
    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "url=%s", (char*)p_std_url);
    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, "}'");

    i_std_ttl = gtk_spin_button_get_value_as_int(entryStdTTL);

    pos += snprintf( &mrl[pos], VLC_MAX_MRL - pos, " --ttl=%d", (int)i_std_ttl);

    if (pos >= VLC_MAX_MRL)
    {
        mrl[VLC_MAX_MRL-1]='\0';
        msg_Err(p_intf, "Media Resource Locator is truncated to: %s", mrl );
    }

    PlaylistAddItem(GTK_WIDGET(button), (gchar*) &mrl);
}


