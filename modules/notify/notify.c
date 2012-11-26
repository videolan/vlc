/*****************************************************************************
 * notify.c : libnotify notification plugin
 *****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Mutricy <xtophe -at- videolan -dot- org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_url.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnotify/notify.h>

#ifndef NOTIFY_CHECK_VERSION
# define NOTIFY_CHECK_VERSION(x,y,z) 0
#endif

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

#define APPLICATION_NAME "VLC media player"

#define TIMEOUT_TEXT N_("Timeout (ms)")
#define TIMEOUT_LONGTEXT N_("How long the notification will be displayed ")

vlc_module_begin ()
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_shortname( N_( "Notify" ) )
    set_description( N_("LibNotify Notification Plugin") )

    add_integer( "notify-timeout", 4000,
                 TIMEOUT_TEXT, TIMEOUT_LONGTEXT, true )

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );
static int Notify( vlc_object_t *, const char *, GdkPixbuf *, intf_thread_t * );
#define MAX_LENGTH 256

struct intf_sys_t
{
    NotifyNotification *notification;
    vlc_mutex_t     lock;
    bool            b_has_actions;
};

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf = (intf_thread_t *)p_this;
    intf_sys_t      *p_sys  = malloc( sizeof( *p_sys ) );

    if( !p_sys )
        return VLC_ENOMEM;

    if( !notify_init( APPLICATION_NAME ) )
    {
        free( p_sys );
        msg_Err( p_intf, "can't find notification daemon" );
        return VLC_EGENERIC;
    }
    p_intf->p_sys = p_sys;

    vlc_mutex_init( &p_sys->lock );
    p_sys->notification = NULL;
    p_sys->b_has_actions = false;

    GList *p_caps = notify_get_server_caps ();
    if( p_caps )
    {
        for( GList *c = p_caps; c != NULL; c = c->next )
        {
            if( !strcmp( (char*)c->data, "actions" ) )
            {
                p_sys->b_has_actions = true;
                break;
            }
        }
        g_list_foreach( p_caps, (GFunc)g_free, NULL );
        g_list_free( p_caps );
    }

    /* */
    var_AddCallback( pl_Get( p_intf ), "activity", ItemChange, p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf = ( intf_thread_t* ) p_this;
    intf_sys_t      *p_sys  = p_intf->p_sys;

    var_DelCallback( pl_Get( p_this ), "activity", ItemChange, p_this );

    if( p_sys->notification )
    {
        GError *p_error = NULL;
        notify_notification_close( p_sys->notification, &p_error );
        g_object_unref( p_sys->notification );
    }

    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
    notify_uninit();
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(newval);
    char           psz_tmp[MAX_LENGTH];
    char           psz_notify[MAX_LENGTH];
    char           *psz_title;
    char           *psz_artist;
    char           *psz_album;
    char           *psz_arturl;
    input_thread_t *p_input = playlist_CurrentInput( (playlist_t*)p_this );
    intf_thread_t  *p_intf  = param;
    intf_sys_t     *p_sys   = p_intf->p_sys;

    if( !p_input )
        return VLC_SUCCESS;

    if( p_input->b_dead )
    {
        /* Not playing anything ... */
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Wait a tad so the meta has been fetched
     * FIXME that's awfully wrong */
    msleep( 10000 );

    /* Playing something ... */
    input_item_t *p_input_item = input_GetItem( p_input );
    psz_title = input_item_GetTitleFbName( p_input_item );

    /* We need at least a title */
    if( EMPTY_STR( psz_title ) )
    {
        free( psz_title );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    psz_artist = input_item_GetArtist( p_input_item );
    psz_album = input_item_GetAlbum( p_input_item );

    if( !EMPTY_STR( psz_artist ) )
    {
        if( !EMPTY_STR( psz_album ) )
            snprintf( psz_tmp, MAX_LENGTH, "<b>%s</b>\n%s\n[%s]",
                      psz_title, psz_artist, psz_album );
        else
            snprintf( psz_tmp, MAX_LENGTH, "<b>%s</b>\n%s",
                      psz_title, psz_artist );
    }
    else
        snprintf( psz_tmp, MAX_LENGTH, "<b>%s</b>", psz_title );

    free( psz_title );
    free( psz_artist );
    free( psz_album );

    GdkPixbuf *pix = NULL;
    psz_arturl = input_item_GetArtURL( p_input_item );
    vlc_object_release( p_input );

    if( psz_arturl )
    {
        char *psz = make_path( psz_arturl );
        free( psz_arturl );
        psz_arturl = psz;
    }

    if( psz_arturl )
    { /* scale the art to show it in notify popup */
        GError *p_error = NULL;
        pix = gdk_pixbuf_new_from_file_at_scale( psz_arturl,
                                                 72, 72, TRUE, &p_error );
    }
    else /* else we show state-of-the art logo */
    {
        /* First try to get an icon from the current theme. */
        GtkIconTheme* p_theme = gtk_icon_theme_get_default();
        pix = gtk_icon_theme_load_icon( p_theme, "vlc", 72, 0, NULL);

        if( !pix )
        {
        /* Load icon from share/ */
            GError *p_error = NULL;
            char *psz_pixbuf;
            char *psz_data = config_GetDataDir();
            if( asprintf( &psz_pixbuf, "%s/icons/48x48/vlc.png", psz_data ) >= 0 )
            {
                pix = gdk_pixbuf_new_from_file( psz_pixbuf, &p_error );
                free( psz_pixbuf );
            }
            free( psz_data );
        }
    }

    free( psz_arturl );

    /* we need to replace '&' with '&amp;' because '&' is a keyword of
     * notification-daemon parser */
    const int i_len = strlen( psz_tmp );
    int i_notify = 0;
    for( int i = 0; i < i_len && i_notify < ( MAX_LENGTH - 5 ); i++ )
    { /* we use MAX_LENGTH - 5 because if the last char of psz_tmp is '&'
       * we will need 5 more characters: 'amp;\0' .
       * however that's unlikely to happen because the last char is '\0' */
        if( psz_tmp[i] != '&' )
        {
            psz_notify[i_notify] = psz_tmp[i];
        }
        else
        {
            strcpy( &psz_notify[i_notify], "&amp;" );
            i_notify += 4;
        }
        i_notify++;
    }
    psz_notify[i_notify] = '\0';

    vlc_mutex_lock( &p_sys->lock );

    Notify( p_this, psz_notify, pix, p_intf );

    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/* libnotify callback, called when the "Next" button is pressed */
static void Next( NotifyNotification *notification, gchar *psz, gpointer p )
{
    vlc_object_t *p_object = (vlc_object_t*)p;

    VLC_UNUSED(psz);
    notify_notification_close( notification, NULL );
    playlist_Next( pl_Get( p_object ) );
}

/* libnotify callback, called when the "Previous" button is pressed */
static void Prev( NotifyNotification *notification, gchar *psz, gpointer p )
{
    vlc_object_t *p_object = (vlc_object_t*)p;

    VLC_UNUSED(psz);
    notify_notification_close( notification, NULL );
    playlist_Prev( pl_Get( p_object ) );
}

static int Notify( vlc_object_t *p_this, const char *psz_temp, GdkPixbuf *pix,
                   intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    NotifyNotification * notification;

    /* Close previous notification if still active */
    if( p_sys->notification )
    {
        GError *p_error = NULL;
        notify_notification_close( p_sys->notification, &p_error );
        g_object_unref( p_sys->notification );
    }

    notification = notify_notification_new( _("Now Playing"),
                                            psz_temp, NULL
#if NOTIFY_CHECK_VERSION (0, 7, 0)
                                          );
#else
                                          , NULL );
#endif
    notify_notification_set_timeout( notification,
                                var_InheritInteger(p_this, "notify-timeout") );
    notify_notification_set_urgency( notification, NOTIFY_URGENCY_LOW );
    if( pix )
    {
        notify_notification_set_icon_from_pixbuf( notification, pix );
        gdk_pixbuf_unref( pix );
    }

    /* Adds previous and next buttons in the notification if actions are supported. */
    if( p_sys->b_has_actions )
    {
      notify_notification_add_action( notification, "previous", _("Previous"), Prev,
                                      (gpointer*)p_intf, NULL );
      notify_notification_add_action( notification, "next", _("Next"), Next,
                                      (gpointer*)p_intf, NULL );
    }

    notify_notification_show( notification, NULL);

    /* Stores the notification to be able to close it */
    p_sys->notification = notification;
    return VLC_SUCCESS;
}

