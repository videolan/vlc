/*****************************************************************************
 * notify.c : libnotify notification plugin
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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
#include <vlc_meta.h>

#include <errno.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnotify/notify.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );
static int Notify( vlc_object_t *, const char *, GdkPixbuf *, intf_thread_t * );
#define MAX_LENGTH 256

struct intf_sys_t
{
    NotifyNotification *notification;
    vlc_mutex_t     lock;
};

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/

#define APPLICATION_NAME "VLC media player"

#define TIMEOUT_TEXT N_("Timeout (ms)")
#define TIMEOUT_LONGTEXT N_("How long the notification will be displayed ")

vlc_module_begin ()
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_shortname( N_( "Notify" ) )
    set_description( N_("LibNotify Notification Plugin") )

    add_integer( "notify-timeout", 4000,NULL,
                TIMEOUT_TEXT, TIMEOUT_LONGTEXT, true )

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf = (intf_thread_t *)p_this;
    playlist_t      *p_playlist;
    intf_sys_t      *p_sys  = malloc( sizeof( intf_sys_t ) );

    if( !p_sys )
        return VLC_ENOMEM;

    if( !notify_init( APPLICATION_NAME ) )
    {
        free( p_sys );
        msg_Err( p_intf, "can't find notification daemon" );
        return VLC_EGENERIC;
    }

    vlc_mutex_init( &p_sys->lock );

    p_intf->p_sys = p_sys;

    p_intf->p_sys->notification = NULL;

    p_playlist = pl_Hold( p_intf );
    var_AddCallback( p_playlist, "item-current", ItemChange, p_intf );
    pl_Release( p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf = ( intf_thread_t* ) p_this;
    intf_sys_t      *p_sys  = p_intf->p_sys;

    playlist_t *p_playlist = pl_Hold( p_this );
    var_DelCallback( p_playlist, "item-current", ItemChange, p_this );
    pl_Release( p_this );

    if( p_intf->p_sys->notification )
        g_object_unref( p_intf->p_sys->notification );

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
    char                psz_tmp[MAX_LENGTH];
    char                psz_notify[MAX_LENGTH];
    char                *psz_title      = NULL;
    char                *psz_artist     = NULL;
    char                *psz_album      = NULL;
    char                *psz_arturl     = NULL;
    input_thread_t      *p_input        =  playlist_CurrentInput(
                                                    (playlist_t*) p_this );
    intf_thread_t       *p_intf         = ( intf_thread_t* ) param;
    intf_sys_t          *p_sys          = p_intf->p_sys;

    if( !p_input ) return VLC_SUCCESS;

    if( p_input->b_dead )
    {
        /* Not playing anything ... */
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    /*Wait a tad so the meta has been fetched*/
    msleep( 1000*4 );

    /* Playing something ... */
    psz_artist = input_item_GetArtist( input_GetItem( p_input ) );
    psz_album = input_item_GetAlbum( input_GetItem( p_input ) ) ;
    psz_title = input_item_GetTitle( input_GetItem( p_input ) );
    if( ( psz_title == NULL ) || EMPTY_STR( psz_title ) )
    {
        free( psz_title );
        psz_title = input_item_GetName( input_GetItem( p_input ) );
    }
    if( ( psz_title == NULL ) || EMPTY_STR( psz_title ) )
    {  /* Not enough metadata ... */
        free( psz_title );
        free( psz_artist );
        free( psz_album );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    if( EMPTY_STR( psz_artist ) )
    {
        free( psz_artist );
        psz_artist = NULL;
    }
    if( EMPTY_STR( psz_album ) )
    {
        free( psz_album );
        psz_album = NULL;
    }

    vlc_object_release( p_input );

    if( psz_artist && psz_album )
        snprintf( psz_tmp, MAX_LENGTH, "<b>%s</b>\n%s\n[%s]",
                  psz_title, psz_artist, psz_album );
    else if( psz_artist )
        snprintf( psz_tmp, MAX_LENGTH, "<b>%s</b>\n%s",
                  psz_title, psz_artist );
    else
        snprintf( psz_tmp, MAX_LENGTH, "<b>%s</b>", psz_title );

    free( psz_title );
    free( psz_artist );
    free( psz_album );

    GdkPixbuf *pix = NULL;
    GError *p_error = NULL;

    psz_arturl = input_item_GetArtURL( input_GetItem( p_input ) );
    if( psz_arturl && !strncmp( psz_arturl, "file://", 7 ) &&
                strlen( psz_arturl ) > 7 )
    { /* scale the art to show it in notify popup */
        gboolean b = TRUE;
        pix = gdk_pixbuf_new_from_file_at_scale(
                (psz_arturl + 7), 72, 72, b, &p_error );
        free( psz_arturl );
    }
    else /* else we show state-of-the art logo */
    {
        const char *data_path = config_GetDataDir ();
        char buf[strlen (data_path) + sizeof ("/vlc48x48.png")];

        snprintf (buf, sizeof (buf), "%s/vlc48x48.png", data_path);
        pix = gdk_pixbuf_new_from_file( buf, &p_error );
    }

    /* we need to replace '&' with '&amp;' because '&' is a keyword of
     * notification-daemon parser */
    int i_notify, i_len, i;
    i_len = strlen( psz_tmp );
    i_notify = 0;
    for( i = 0; ( ( i < i_len ) && ( i_notify < ( MAX_LENGTH - 5 ) ) ); i++ )
    { /* we use MAX_LENGTH - 5 because if the last char of psz_tmp is '&'
       * we will need 5 more characters: 'amp;\0' .
       * however that's unlikely to happen because the last char is '\0' */
        if( psz_tmp[i] != '&' )
            psz_notify[i_notify] = psz_tmp[i];
        else
        {
            snprintf( psz_notify + i_notify, 6, "&amp;" );
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

static void Next( NotifyNotification *notification, gchar *psz, gpointer p )
{ /* libnotify callback, called when the "Next" button is pressed */
    VLC_UNUSED(psz);
    notify_notification_close (notification, NULL);
    playlist_t *p_playlist = pl_Hold( ((vlc_object_t*) p) );
    playlist_Next( p_playlist );
    pl_Release( ((vlc_object_t*) p) );
}

static void Prev( NotifyNotification *notification, gchar *psz, gpointer p )
{ /* libnotify callback, called when the "Previous" button is pressed */
    VLC_UNUSED(psz);
    notify_notification_close (notification, NULL);
    playlist_t *p_playlist = pl_Hold( ((vlc_object_t*) p) );
    playlist_Prev( p_playlist );
    pl_Release( ((vlc_object_t*) p) );
}

static int Notify( vlc_object_t *p_this, const char *psz_temp, GdkPixbuf *pix,
                   intf_thread_t *p_intf )
{
    NotifyNotification * notification;
    GError *p_error = NULL;

    /* Close previous notification if still active */
    if( p_intf->p_sys->notification )
    {
        notify_notification_close( p_intf->p_sys->notification, &p_error );
        g_object_unref( p_intf->p_sys->notification );
    }

    notification = notify_notification_new( _("Now Playing"),
            psz_temp, NULL, NULL);
    notify_notification_set_timeout( notification,
                                     config_GetInt(p_this, "notify-timeout") );
    notify_notification_set_urgency( notification, NOTIFY_URGENCY_LOW );
    if( pix )
    {
        notify_notification_set_icon_from_pixbuf( notification, pix );
        gdk_pixbuf_unref( pix );
    }

    /* Adds previous and next buttons in the notification */
    notify_notification_add_action( notification, "previous", _("Previous"), Prev,
                                    (gpointer*) p_intf, NULL );
    notify_notification_add_action( notification, "next", _("Next"), Next,
                                    (gpointer*) p_intf, NULL );

    notify_notification_show( notification, NULL);

    /* Stores the notification to be able to close it */
    p_intf->p_sys->notification = notification;
    return VLC_SUCCESS;
}

