/*****************************************************************************
 * notify.c : libnotify notification plugin
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
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
#include <errno.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnotify/notify.h>

#include <vlc/vlc.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );
static int Notify( vlc_object_t *, const char *, GdkPixbuf *, void * );
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

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_shortname( _( "Notify" ) );
    set_description( _("LibNotify Notification Plugin") );

    add_integer( "notify-timeout", 4000,NULL,
                TIMEOUT_TEXT, TIMEOUT_LONGTEXT, VLC_TRUE );

    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf = (intf_thread_t *)p_this;
    playlist_t      *p_playlist;
    intf_sys_t      *p_sys  = malloc( sizeof( intf_sys_t ) );
    
    if( !p_sys )
    {
        msg_Err( p_intf, "Out of memory" );
        return VLC_ENOMEM;
    }

    if( !notify_init( APPLICATION_NAME ) )
    {
        msg_Err( p_intf, "can't find notification daemon" );
        return VLC_EGENERIC;
    }

    vlc_mutex_init( p_this, &p_sys->lock );

    p_intf->p_sys = p_sys;

    p_intf->p_sys->notification = NULL;

    p_playlist = pl_Yield( p_intf );
    var_AddCallback( p_playlist, "playlist-current", ItemChange, p_intf );
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

    playlist_t *p_playlist = pl_Yield( p_this );
    var_DelCallback( p_playlist, "playlist-current", ItemChange, p_this );
    pl_Release( p_this );

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
    char                psz_tmp[MAX_LENGTH];
    char                *psz_title      = NULL;
    char                *psz_artist     = NULL;
    char                *psz_album      = NULL;
    char                *psz_arturl     = NULL;
    input_thread_t      *p_input        = NULL;
    playlist_t          * p_playlist    = pl_Yield( p_this );
    intf_thread_t       *p_intf         = ( intf_thread_t* ) param;
    intf_sys_t          *p_sys          = p_intf->p_sys;

    p_input = p_playlist->p_input;
    pl_Release( p_playlist );

    if( !p_input ) return VLC_SUCCESS;
    vlc_object_yield( p_input );

    if( p_input->b_dead )
    {
        /* Not playing anything ... */
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Playing something ... */
    psz_artist = input_item_GetArtist( input_GetItem( p_input ) );
    if( psz_artist == NULL ) psz_artist = strdup( _("no artist") );
    psz_album = input_item_GetAlbum( input_GetItem( p_input ) ) ;
    if( psz_album == NULL ) psz_album = strdup( _("no album") );
    psz_title = input_item_GetTitle( input_GetItem( p_input ) );
    if( psz_title == NULL )
        psz_title = input_item_GetName( input_GetItem( p_input ) );
    if( psz_title == NULL )
    {  /* Not enough metadata ... */
        free( psz_artist );
        free( psz_album );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    vlc_object_release( p_input );

    if( psz_title == NULL ) psz_title = strdup( N_("(no title)") );
    snprintf( psz_tmp, MAX_LENGTH, "<b>%s</b>\n%s - %s",
              psz_title, psz_artist, psz_album );
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
        pix = gdk_pixbuf_new_from_file( DATA_PATH "/vlc48x48.png", &p_error );

    vlc_mutex_lock( &p_sys->lock );

    Notify( p_this, psz_tmp, pix, param );

    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

static int Notify( vlc_object_t *p_this, const char *psz_temp, GdkPixbuf *pix,
                        void *param )
{
    intf_thread_t   *p_intf = (intf_thread_t *)param;
    NotifyNotification * notification;
    GError *p_error = NULL;

    /* Close previous notification if still active */
    if( p_intf->p_sys->notification )
        notify_notification_close( p_intf->p_sys->notification, &p_error );

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

    notify_notification_show( notification, NULL);

    /* Stores the notification to be able to close it */
    p_intf->p_sys->notification = notification;
    return VLC_SUCCESS;
}

