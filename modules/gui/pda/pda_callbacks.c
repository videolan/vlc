/*****************************************************************************
 * pda_callbacks.c : Callbacks for the pda Linux Gtk+ plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
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

void PlaylistAddItem(GtkWidget *widget, gchar *name, char **ppsz_options, int i_size)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );
    playlist_t    *p_playlist;
    int           i_id , i_pos=0;
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
            int i;

            /* Add a new row to the playlist treeview model */
            gtk_list_store_append (GTK_LIST_STORE(p_play_model), &p_play_iter);
            gtk_list_store_set (GTK_LIST_STORE(p_play_model), &p_play_iter,
                                    0, name,   /* Add path to it !!! */
                                    1, "no info",
                                    2, p_playlist->i_size, /* Hidden index. */
                                    -1 );

            /* Add to VLC's playlist */
#if 0
            if (p_intf->p_sys->b_autoplayfile)
            {
                playlist_Add( p_playlist, (const char*)name, (const char**)ppsz_options, i_size,
                              PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END);
            }
            else
#endif
            {
                i_id = playlist_AddExt( p_playlist, (const char*)name,
                              (const char*)name,
                              PLAYLIST_APPEND, PLAYLIST_END,
                              (mtime_t) 0,
                              (const char **) ppsz_options, i_pos );
            }

            /* Cleanup memory */
            for (i=0; i<i_size; i++)
                free(ppsz_options[i]);
            free(ppsz_options);
        }
    }
    vlc_object_release( p_playlist );
}

void PlaylistRebuildListStore( GtkListStore * p_list, playlist_t * p_playlist )
{
    GtkTreeIter iter;
    int         i_dummy;
    gchar *     ppsz_text[2];
#if 0
    GdkColor    red;
    red.red     = 65535;
    red.blue    = 0;
    red.green   = 0;
#endif
    vlc_mutex_lock( &p_playlist->object_lock );
    for( i_dummy = 0; i_dummy < p_playlist->i_size ; i_dummy++ )
    {
        ppsz_text[0] = p_playlist->pp_items[i_dummy]->input.psz_name;
        ppsz_text[1] = "no info";
        gtk_list_store_append (p_list, &iter);
        gtk_list_store_set (p_list, &iter,
                            0, ppsz_text[0],
                            1, ppsz_text[1],
                            2, i_dummy, /* Hidden index */
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
    struct dirent **pp_namelist;
    struct passwd *p_pw;
    struct group  *p_grp;
    struct stat    st;
    int n=-1, status=-1;

    msg_Dbg(p_intf, "Changing to dir %s", psz_dir);
    if (psz_dir)
    {
       status = chdir(psz_dir);
       if (status<0)
          msg_Dbg(p_intf, "permision denied" );
    }
    n = scandir(".", &pp_namelist, 0, alphasort);

    if (n<0)
        perror("scandir");
    else
    {
        int i;
        gchar *ppsz_text[4];

        if (lstat("..", &st)==0)
        {
            /* user, group  */
            p_pw  = getpwuid(st.st_uid);
            p_grp = getgrgid(st.st_gid);

            /* XXX : kludge temporaire pour yopy */
            ppsz_text[0] = "..";
            ppsz_text[1] = get_file_perms(st);
            ppsz_text[2] = p_pw->pw_name;
            ppsz_text[3] = p_grp->gr_name;

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
            if ((pp_namelist[i]->d_name[0] != '.') &&
                (lstat(pp_namelist[i]->d_name, &st)==0))
            {
                /* user, group  */
                p_pw  = getpwuid(st.st_uid);
                p_grp = getgrgid(st.st_gid);

                /* This is a list of strings. */
                ppsz_text[0] = pp_namelist[i]->d_name;
                ppsz_text[1] = get_file_perms(st);
                ppsz_text[2] = p_pw->pw_name;
                ppsz_text[3] = p_grp->gr_name;
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
        free(pp_namelist);
    }
}

static char *get_file_perms(const struct stat st)
{
    char  *psz_perm;

    psz_perm = (char *) malloc(sizeof(char)*10);
    strncpy( psz_perm, "----------", sizeof("----------"));

    /* determine permission modes */
    if (S_ISLNK(st.st_mode))
        psz_perm[0]= 'l';
    else if (S_ISDIR(st.st_mode))
        psz_perm[0]= 'd';
    else if (S_ISCHR(st.st_mode))
        psz_perm[0]= 'c';
    else if (S_ISBLK(st.st_mode))
        psz_perm[0]= 'b';
    else if (S_ISFIFO(st.st_mode))
        psz_perm[0]= 'f';
    else if (S_ISSOCK(st.st_mode))
        psz_perm[0]= 's';
    else if (S_ISREG(st.st_mode))
        psz_perm[0]= '-';
    else /* Unknown type is an error */
        psz_perm[0]= '?';
    /* Get file permissions */
    /* User */
    if (st.st_mode & S_IRUSR)
        psz_perm[1]= 'r';
    if (st.st_mode & S_IWUSR)
        psz_perm[2]= 'w';
    if (st.st_mode & S_IXUSR)
    {
        if (st.st_mode & S_ISUID)
            psz_perm[3] = 's';
        else
            psz_perm[3]= 'x';
    }
    else if (st.st_mode & S_ISUID)
        psz_perm[3] = 'S';
    /* Group */
    if (st.st_mode & S_IRGRP)
        psz_perm[4]= 'r';
    if (st.st_mode & S_IWGRP)
        psz_perm[5]= 'w';
    if (st.st_mode & S_IXGRP)
    {
        if (st.st_mode & S_ISGID)
            psz_perm[6] = 's';
        else
            psz_perm[6]= 'x';
    }
    else if (st.st_mode & S_ISGID)
        psz_perm[6] = 'S';
    /* Other */
    if (st.st_mode & S_IROTH)
        psz_perm[7]= 'r';
    if (st.st_mode & S_IWOTH)
        psz_perm[8]= 'w';
    if (st.st_mode & S_IXOTH)
    {
        /* 'sticky' bit */
        if (st.st_mode &S_ISVTX)
            psz_perm[9] = 't';
        else
            psz_perm[9]= 'x';
    }
    else if (st.st_mode &S_ISVTX)
        psz_perm[9]= 'T';

    return psz_perm;
}

/*
 * Main interface callbacks
 */

gboolean onPDADeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_vlc->b_die = VLC_TRUE;
    vlc_mutex_unlock( &p_intf->change_lock );
    msg_Dbg( p_intf, "about to exit vlc ... signaled" );

    return TRUE;
}


void onRewind(GtkButton *button, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    if (p_intf->p_sys->p_input != NULL)
    {
        var_SetVoid( p_intf->p_sys->p_input, "rate-slower" );
    }
}


void onPause(GtkButton *button, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    if (p_intf->p_sys->p_input != NULL)
    {
        var_SetInteger( p_intf->p_sys->p_input, "state", PAUSE_S );
    }
}


void onPlay(GtkButton *button, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET( button ) );
    playlist_t *p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if (p_playlist)
    {
        vlc_mutex_lock( &p_playlist->object_lock );
        if (p_playlist->i_size)
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
            playlist_Play( p_playlist );
            gdk_window_lower( p_intf->p_sys->p_window->window );
        }
        else
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
        }
        vlc_object_release( p_playlist );
    }
}


void onStop(GtkButton *button, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET( button ) );
    playlist_t *p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if (p_playlist)
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}


void onForward(GtkButton *button, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    if (p_intf->p_sys->p_input != NULL)
    {
        var_SetVoid( p_intf->p_sys->p_input, "rate-faster" );
    }
}


void onAbout(GtkButton *button, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET(button) );

    /* Toggle notebook */
    if (p_intf->p_sys->p_notebook)
    {
        gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        gtk_notebook_set_page(p_intf->p_sys->p_notebook,6);
    }
}


gboolean SliderRelease(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    msg_Dbg( p_intf, "SliderButton Release" );
    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 1;
    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}


gboolean SliderPress(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    msg_Dbg( p_intf, "SliderButton Press" );
    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 0;
    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}

void SliderMove(GtkRange *range, GtkScrollType scroll, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( range );
    msg_Dbg( p_intf, "SliderButton Move" );
}


void addSelectedToPlaylist(GtkTreeModel *model, GtkTreePath *path,
                           GtkTreeIter *iter, gpointer *userdata)
{
    gchar *psz_filename;

    gtk_tree_model_get(model, iter, 0, &psz_filename, -1);

    PlaylistAddItem(GTK_WIDGET(userdata), psz_filename, 0, 0);
}

void onFileListRow(GtkTreeView *treeview, GtkTreePath *path,
                   GtkTreeViewColumn *column, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET(treeview) );
    GtkTreeSelection *p_selection = gtk_tree_view_get_selection(treeview);

    if (gtk_tree_selection_count_selected_rows(p_selection) == 1)
    {
        struct stat   st;
        GtkTreeModel *p_model;
        GtkTreeIter   iter;
        gchar        *psz_filename;

        /* This might be a directory selection */
        p_model = gtk_tree_view_get_model(treeview);
        if (!p_model)
        {
            msg_Err(p_intf, "PDA: Filelist model contains a NULL pointer\n" );
            return;
        }
        if (!gtk_tree_model_get_iter(p_model, &iter, path))
        {
            msg_Err( p_intf, "PDA: Could not get iter from model" );
            return;
        }

        gtk_tree_model_get(p_model, &iter, 0, &psz_filename, -1);
        if (stat((char*)psz_filename, &st)==0)
        {
            if (S_ISDIR(st.st_mode))
            {
                GtkListStore *p_store = NULL;

                /* Get new directory listing */
                p_store = gtk_list_store_new (5,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING,
                                           G_TYPE_UINT64,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING);
                if (p_store)
                {
                    ReadDirectory(p_intf, p_store, psz_filename);

                    /* Update TreeView with new model */
                    gtk_tree_view_set_model(treeview, (GtkTreeModel*) p_store);
                    g_object_unref(p_store);
                }
            }
        }
    }
}

void onAddFileToPlaylist(GtkButton *button, gpointer user_data)
{
    GtkTreeView       *p_treeview = NULL;

    p_treeview = (GtkTreeView *) lookup_widget( GTK_WIDGET(button), "tvFileList");
    if (p_treeview)
    {
        GtkTreeSelection *p_selection = gtk_tree_view_get_selection(p_treeview);

        gtk_tree_selection_selected_foreach(p_selection, (GtkTreeSelectionForeachFunc) &addSelectedToPlaylist, (gpointer) p_treeview);    
    }
}


void NetworkBuildMRL(GtkEditable *editable, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET(editable) );
    GtkSpinButton *p_networkPort = NULL;
    GtkEntry      *p_entryMRL = NULL;
    GtkEntry      *p_networkType = NULL;
    GtkEntry      *p_networkAddress = NULL;
    GtkEntry      *p_networkProtocol = NULL;
    const gchar   *psz_mrlNetworkType;
    const gchar   *psz_mrlAddress;
    const gchar   *psz_mrlProtocol;
    gint           i_mrlPort;
    char           text[VLC_MAX_MRL];
    int            i_pos = 0;

    p_entryMRL = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryMRL" );

    p_networkType     = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkType" );
    p_networkAddress  = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkAddress" );
    p_networkPort     = (GtkSpinButton*) lookup_widget( GTK_WIDGET(editable), "entryNetworkPort" );
    p_networkProtocol = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryNetworkProtocolType" );

    psz_mrlNetworkType = gtk_entry_get_text(GTK_ENTRY(p_networkType));
    psz_mrlAddress     = gtk_entry_get_text(GTK_ENTRY(p_networkAddress));
    i_mrlPort          = gtk_spin_button_get_value_as_int(p_networkPort);
    psz_mrlProtocol    = gtk_entry_get_text(GTK_ENTRY(p_networkProtocol));

    /* Build MRL from parts ;-) */
    i_pos = snprintf( &text[0], VLC_MAX_MRL, "%s://", (char*)psz_mrlProtocol);
    if (strncasecmp( (char*)psz_mrlNetworkType, "multicast",9)==0)
    {
        i_pos += snprintf( &text[i_pos], VLC_MAX_MRL - i_pos, "@" );
    }
    i_pos += snprintf( &text[i_pos], VLC_MAX_MRL - i_pos, "%s:%d", (char*)psz_mrlAddress, (int)i_mrlPort );

    if (i_pos >= VLC_MAX_MRL)
    {
        text[VLC_MAX_MRL-1]='\0';
        msg_Err( p_intf, "Media Resource Locator is truncated to: %s", text);
    }

    gtk_entry_set_text(p_entryMRL,text);
}

void onAddNetworkPlaylist(GtkButton *button, gpointer user_data)
{
    intf_thread_t  *p_intf = GtkGetIntf( button );

    GtkEntry       *p_mrl = NULL;
    GtkCheckButton *p_network_transcode = NULL;
    gboolean        b_network_transcode;
    const gchar    *psz_mrl_name;

    p_mrl = (GtkEntry*) lookup_widget(GTK_WIDGET(button),"entryMRL" );
    psz_mrl_name = gtk_entry_get_text(p_mrl);

    p_network_transcode = (GtkCheckButton*) lookup_widget(GTK_WIDGET(button), "checkNetworkTranscode" );
    b_network_transcode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p_network_transcode));
    if (b_network_transcode)
    {
        msg_Dbg( p_intf, "Network transcode option selected." );
        onAddTranscodeToPlaylist(GTK_WIDGET(button), (gchar *)psz_mrl_name);
    }
    else
    {
        msg_Dbg( p_intf, "Network receiving selected." );
        PlaylistAddItem(GTK_WIDGET(button), (gchar *)psz_mrl_name, 0, 0);
    }
}


void onAddCameraToPlaylist(GtkButton *button, gpointer user_data)
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

    GtkCheckButton  *p_check_v4l_transcode = NULL;
    gboolean         b_v4l_transcode;
    
    char **ppsz_options = NULL; /* list of options */
    int  i_options=0;
    char v4l_mrl[6];
    int i_pos;
    int i;

    ppsz_options = (char **) malloc(11 *sizeof(char*));
    if (ppsz_options == NULL)
    {
        msg_Err(p_intf, "No memory to allocate for v4l options.");
        return;
    }
    for (i=0; i<11; i++)
    {
        ppsz_options[i] = (char *) malloc(VLC_MAX_MRL * sizeof(char));
        if (ppsz_options[i] == NULL)
        {
            msg_Err(p_intf, "No memory to allocate for v4l options string %i.", i);
            for (i-=1; i>=0; i--)
                free(ppsz_options[i]);
            free(ppsz_options);
            return;
        }
    }

    i_pos = snprintf( &v4l_mrl[0], 6, "v4l");
    v4l_mrl[5]='\0';

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

    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "%s", (char*)p_v4l_video_device );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "adev=%s", (char*)p_v4l_audio_device );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "norm=%s", (char*)p_v4l_norm );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "size=%s", (char*)p_v4l_size );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "%s", (char*)p_v4l_sound_direction );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';

    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "channel=%d", (int)i_v4l_channel );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "frequency=%d", (int)i_v4l_frequency );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "samplerate=%d", (int)i_v4l_samplerate );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "quality=%d", (int)i_v4l_quality );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "tuner=%d", (int)i_v4l_tuner );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';

    /* MJPEG only */
    checkV4LMJPEG      = (GtkCheckButton*) lookup_widget( GTK_WIDGET(button), "checkV4LMJPEG" );
    b_v4l_mjpeg = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkV4LMJPEG));
    if (b_v4l_mjpeg)
    {
        entryV4LDecimation = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryV4LDecimation" );
        i_v4l_decimation = gtk_spin_button_get_value_as_int(entryV4LDecimation);

        i_pos = snprintf( &ppsz_options[i_options++][0], VLC_MAX_MRL, "mjpeg:%d", (int)i_v4l_decimation );
        if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    }
    /* end MJPEG only */

    p_check_v4l_transcode = (GtkCheckButton*) lookup_widget(GTK_WIDGET(button), "checkV4LTranscode" );
    b_v4l_transcode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p_check_v4l_transcode));
    if (b_v4l_transcode)
    {
        msg_Dbg( p_intf, "Camera transcode option selected." );
        onAddTranscodeToPlaylist(GTK_WIDGET(button), (gchar *)v4l_mrl);
    }
    else
    {
        msg_Dbg( p_intf, "Camera reception option selected." );
        PlaylistAddItem(GTK_WIDGET(button), (gchar*) &v4l_mrl, ppsz_options, i_options);
    }
}


gboolean PlaylistEvent(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    return FALSE;
}


void onPlaylistColumnsChanged(GtkTreeView *treeview, gpointer user_data)
{
}


gboolean onPlaylistRowSelected(GtkTreeView *treeview, gboolean start_editing, gpointer user_data)
{
    return FALSE;
}


void onPlaylistRow(GtkTreeView *treeview, GtkTreePath *path,
                   GtkTreeViewColumn *column, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( GTK_WIDGET(treeview) );
    GtkTreeSelection *p_selection = gtk_tree_view_get_selection(treeview);
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return; // FALSE;
    }

    if (gtk_tree_selection_count_selected_rows(p_selection) == 1)
    {
        GtkTreeModel *p_model;
        GtkTreeIter   iter;
        int           i_row;

        /* This might be a directory selection */
        p_model = gtk_tree_view_get_model(treeview);
        if (!p_model)
        {
            msg_Err(p_intf, "PDA: Playlist model contains a NULL pointer\n" );
            return;
        }
        if (!gtk_tree_model_get_iter(p_model, &iter, path))
        {
            msg_Err( p_intf, "PDA: Playlist could not get iter from model" );
            return;
        }

        gtk_tree_model_get(p_model, &iter, 2, &i_row, -1);
        playlist_Goto( p_playlist, i_row );
    }
    vlc_object_release( p_playlist );
}


void onUpdatePlaylist(GtkButton *button, gpointer user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    GtkTreeView *p_tvplaylist = NULL;

    if( p_playlist == NULL )
    {
        return;
    }

    p_tvplaylist = (GtkTreeView*) lookup_widget( GTK_WIDGET(button), "tvPlaylist");
    if (p_tvplaylist)
    {
        GtkListStore *p_model = NULL;

        /* Rebuild the playlist then. */
        p_model = gtk_list_store_new (3,
                    G_TYPE_STRING, /* Filename */
                    G_TYPE_STRING, /* Time */
                    G_TYPE_UINT);  /* Hidden field */
        if (p_model)
        {
            PlaylistRebuildListStore(p_model, p_playlist);
            gtk_tree_view_set_model(GTK_TREE_VIEW(p_tvplaylist), GTK_TREE_MODEL(p_model));
            g_object_unref(p_model);
        }
    }
    vlc_object_release( p_playlist );
}

void deleteItemFromPlaylist(gpointer data, gpointer user_data)
{
    gtk_tree_path_free((GtkTreePath*) data); // removing an item.
}

void onDeletePlaylist(GtkButton *button, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    GtkTreeView    *p_tvplaylist;

    /* Delete an arbitrary item from the playlist */
    p_tvplaylist = (GtkTreeView *) lookup_widget( GTK_WIDGET(button), "tvPlaylist" );
    if (p_tvplaylist != NULL)
    {
        GList *p_rows = NULL;
        GList *p_node;
        GtkTreeModel *p_model = NULL;
        GtkListStore *p_store = NULL;
        GtkTreeSelection *p_selection = gtk_tree_view_get_selection(p_tvplaylist);

        p_model = gtk_tree_view_get_model(p_tvplaylist);
        if (p_model)
        {
            p_rows = gtk_tree_selection_get_selected_rows(p_selection, &p_model);

            if( g_list_length( p_rows ) )
            {
                /* reverse-sort so that we can delete from the furthest
                 * to the closest item to delete...
                 */
                p_rows = g_list_reverse( p_rows );
            }

            for (p_node=p_rows; p_node!=NULL; p_node = p_node->next)
            {
                GtkTreeIter iter;
                GtkTreePath *p_path = NULL;

                p_path = (GtkTreePath *)p_node->data;
                if (p_path)
                {
                    if (gtk_tree_model_get_iter(p_model, &iter, p_path))
                    {
                        gint item;

                        gtk_tree_model_get(p_model, &iter, 2, &item, -1);
                        playlist_LockDelete(p_playlist, item);
                    }
                }
            }
#if 0 
            g_list_foreach (p_rows, (GFunc*)gtk_tree_path_free, NULL);
#endif /* Testing the next line */
            g_list_foreach (p_rows, deleteItemFromPlaylist, NULL);
            g_list_free (p_rows);
        }

        /* Rebuild the playlist then. */
        p_store = gtk_list_store_new (3,
                    G_TYPE_STRING, /* Filename */
                    G_TYPE_STRING, /* Time */
                    G_TYPE_UINT);  /* Hidden field */
        if (p_store)
        {
            PlaylistRebuildListStore(p_store, p_playlist);
            gtk_tree_view_set_model(GTK_TREE_VIEW(p_tvplaylist), GTK_TREE_MODEL(p_store));
            g_object_unref(p_store);
        }
    }
    vlc_object_release( p_playlist );
}


void onClearPlaylist(GtkButton *button, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    GtkTreeView    *p_tvplaylist;
    int item;

    if( p_playlist == NULL )
    {
        return;
    }

    for(item = p_playlist->i_size - 1; item >= 0 ;item-- )
    {
        playlist_LockDelete( p_playlist, item);
    }
    vlc_object_release( p_playlist );

    // Remove all entries from the Playlist widget.
    p_tvplaylist = (GtkTreeView*) lookup_widget( GTK_WIDGET(button), "tvPlaylist");
    if (p_tvplaylist)
    {
        GtkTreeModel *p_play_model;

        p_play_model = gtk_tree_view_get_model(p_tvplaylist);
        if (p_play_model)
        {
            gtk_list_store_clear(GTK_LIST_STORE(p_play_model));
        }
    }
}


void onPreferenceSave(GtkButton *button, gpointer user_data)
{
#if 0
    intf_thread_t *p_intf = GtkGetIntf( button );

    msg_Dbg(p_intf, "Preferences Save" );
    config_SaveConfigFile( p_intf, NULL );
#endif
}


void onPreferenceApply(GtkButton *button, gpointer user_data)
{
#if 0
    intf_thread_t *p_intf = GtkGetIntf( button );

    msg_Dbg(p_intf, "Preferences Apply" );
#endif
}


void onPreferenceCancel(GtkButton *button, gpointer user_data)
{
#if 0
    intf_thread_t *p_intf = GtkGetIntf( button );

    msg_Dbg(p_intf, "Preferences Cancel" );
    config_ResetAll( p_intf );
    /* Cancel interface changes. */
    config_SaveConfigFile( p_intf, NULL );
#endif
}


void onAddTranscodeToPlaylist(GtkButton *button, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );

    GtkEntry       *p_entryVideoCodec = NULL;
    GtkSpinButton  *p_entryVideoBitrate = NULL;
    GtkSpinButton  *p_entryVideoBitrateTolerance = NULL;
    GtkSpinButton  *p_entryVideoKeyFrameInterval = NULL;
    GtkCheckButton *p_checkVideoDeinterlace = NULL;
    GtkEntry       *p_entryAudioCodec = NULL;
    GtkSpinButton  *p_entryAudioBitrate = NULL;
    const gchar    *p_video_codec;
    gint            i_video_bitrate;
    gint            i_video_bitrate_tolerance;
    gint            i_video_keyframe_interval;
    gboolean        b_video_deinterlace;
    const gchar    *p_audio_codec;
    gint            i_audio_bitrate;

    GtkEntry       *p_entryStdAccess = NULL;
    GtkEntry       *p_entryStdMuxer = NULL;
    GtkEntry       *p_entryStdURL = NULL;
    GtkEntry       *p_entryStdAnnounce = NULL;
    GtkSpinButton  *p_entryStdTTL = NULL;
    GtkCheckButton *p_checkSAP = NULL;
    GtkCheckButton *p_checkSLP = NULL;
    const gchar    *p_std_announce;
    const gchar    *p_std_access;
    const gchar    *p_std_muxer;
    const gchar    *p_std_url;
    gboolean        b_sap_announce;
    gboolean        b_slp_announce;
    gint            i_std_ttl;

    char **ppsz_options = NULL; /* list of options */
    int  i_options=0;
    int  i;

    gchar mrl[7];
    int   i_pos;

    ppsz_options = (char **) malloc(3 *sizeof(char*));
    if (ppsz_options == NULL)
    {
        msg_Err(p_intf, "No memory to allocate for v4l options.");
        return;
    }
    for (i=0; i<3; i++)
    {
        ppsz_options[i] = (char *) malloc(VLC_MAX_MRL * sizeof(char));
        if (ppsz_options[i] == NULL)
        {
            msg_Err(p_intf, "No memory to allocate for v4l options string %i.", i);
            for (i-=1; i>=0; i--)
                free(ppsz_options[i]);
            free(ppsz_options);
            return;
        }
    }

    /* Update the playlist */
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    /* Get all the options. */
    i_pos = snprintf( &mrl[0], VLC_MAX_MRL, "sout");
    mrl[6] = '\0';
    /* option 1 */
    i_pos = snprintf( &ppsz_options[i_options][0], VLC_MAX_MRL, "sout='#transcode{");
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';

    p_entryVideoCodec   = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryVideoCodec" );
    p_entryVideoBitrate = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryVideoBitrate" );
    p_entryVideoBitrateTolerance = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryVideoBitrateTolerance" );
    p_entryVideoKeyFrameInterval = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryVideoKeyFrameInterval" );
    
    p_video_codec = gtk_entry_get_text(GTK_ENTRY(p_entryVideoCodec));
    i_video_bitrate = gtk_spin_button_get_value_as_int(p_entryVideoBitrate);
    i_video_bitrate_tolerance = gtk_spin_button_get_value_as_int(p_entryVideoBitrateTolerance);
    i_video_keyframe_interval = gtk_spin_button_get_value_as_int(p_entryVideoKeyFrameInterval);
    
    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "vcodec=%s,", (char*)p_video_codec );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "vb=%d,", (int)i_video_bitrate );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "vt=%d,", (int)i_video_bitrate_tolerance );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "keyint=%d,", (int)i_video_keyframe_interval );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';

    p_checkVideoDeinterlace = (GtkCheckButton*) lookup_widget( GTK_WIDGET(button), "checkVideoDeinterlace" );
    b_video_deinterlace = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p_checkVideoDeinterlace));
    if (b_video_deinterlace)
    {
        i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "deinterlace," );
        if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    }
    p_entryAudioCodec   = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryAudioCodec" );
    p_entryAudioBitrate = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryAudioBitrate" );

    p_audio_codec = gtk_entry_get_text(GTK_ENTRY(p_entryAudioCodec));
    i_audio_bitrate = gtk_spin_button_get_value_as_int(p_entryAudioBitrate);

    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "acodec=%s,", (char*)p_audio_codec );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "ab=%d,", (int)i_audio_bitrate );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "channels=1}"/*, (int)i_audio_channels*/ );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';

    /* option 2 */
    i_pos = 0;
    i_pos = snprintf( &ppsz_options[i_options++][i_pos], VLC_MAX_MRL - i_pos, "#" );
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';

    p_entryStdAccess = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryStdAccess" );
    p_entryStdMuxer  = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryStdMuxer" );
    p_entryStdURL = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryStdURL" );
    p_entryStdAnnounce = (GtkEntry*) lookup_widget( GTK_WIDGET(button), "entryAnnounceChannel" );
    p_entryStdTTL = (GtkSpinButton*) lookup_widget( GTK_WIDGET(button), "entryStdTTL" );

    p_std_access = gtk_entry_get_text(GTK_ENTRY(p_entryStdAccess));
    p_std_muxer = gtk_entry_get_text(GTK_ENTRY(p_entryStdMuxer));
    p_std_url = gtk_entry_get_text(GTK_ENTRY(p_entryStdURL));
    p_std_announce = gtk_entry_get_text(GTK_ENTRY(p_entryStdAnnounce));
    b_sap_announce = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p_checkSAP));
    b_slp_announce = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p_checkSLP));

    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "std{access=%s,", (char*)p_std_access);
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "mux=%s,", (char*)p_std_muxer);
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
    i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "url=%s", (char*)p_std_url);
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';

    if (strncasecmp( (const char*)p_std_access, "udp", 3)==0)
    {
        if (b_sap_announce)
        {
            i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "sap=%s", (char*)p_std_announce);
            if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
        }
        if (b_slp_announce)
        {
            i_pos += snprintf( &ppsz_options[i_options][i_pos], VLC_MAX_MRL - i_pos, "slp=%s", (char*)p_std_announce);
            if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';
        }
    }

    i_std_ttl = gtk_spin_button_get_value_as_int(p_entryStdTTL);

    i_pos += snprintf( &ppsz_options[i_options++][i_pos], VLC_MAX_MRL - i_pos, "ttl=%d}", (int)i_std_ttl);
    if (i_pos>=VLC_MAX_MRL) ppsz_options[i_options][VLC_MAX_MRL-1] = '\0';

    if (user_data != NULL)
    {
      msg_Dbg(p_intf, "Adding transcoding options to playlist item." );
    }
    else
    {
      msg_Dbg(p_intf, "Adding --sout to playlist." );
      PlaylistAddItem(GTK_WIDGET(button), (gchar*) &mrl, ppsz_options, i_options);
    }
}

void onEntryStdAccessChanged(GtkEditable *editable, gpointer user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( editable );

    GtkCheckButton *p_checkSAP = NULL;
    GtkCheckButton *p_checkSLP = NULL;
    GtkEntry       *p_entryStdAccess = NULL;
    const gchar    *p_std_access = NULL;    
    gboolean        b_announce = FALSE;

    p_entryStdAccess = (GtkEntry*) lookup_widget( GTK_WIDGET(editable), "entryStdAccess" );
    p_checkSAP = (GtkCheckButton*) lookup_widget( GTK_WIDGET(editable), "checkSAP" );
    p_checkSLP = (GtkCheckButton*) lookup_widget( GTK_WIDGET(editable), "checkSLP" );

    if ( (p_std_access == NULL) || (p_checkSAP == NULL) || (p_checkSLP == NULL))
    {
        msg_Err( p_intf, "Access, SAP and SLP widgets not found." );
        return;
    }
    p_std_access = gtk_entry_get_text(GTK_ENTRY(p_entryStdAccess));

    b_announce = (strncasecmp( (const char*)p_std_access, "udp", 3) == 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_checkSAP), b_announce);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_checkSLP), b_announce);
}

