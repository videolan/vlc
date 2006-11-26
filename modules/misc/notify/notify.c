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
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>

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
static int Notify( vlc_object_t *, const char * );
#define MAX_LENGTH 256

struct intf_sys_t
{
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
    input_thread_t      *p_input        = NULL;
    playlist_t          * p_playlist    = pl_Yield( p_this );
    intf_thread_t       *p_intf         = ( intf_thread_t* ) param;
    intf_sys_t          *p_sys          = p_intf->p_sys;

    p_input = p_playlist->p_input;
    vlc_object_release( p_playlist );

    if( !p_input ) return VLC_SUCCESS;
    vlc_object_yield( p_input );

    if( p_input->b_dead || !input_GetItem(p_input)->psz_name )
    {
        /* Not playing anything ... */
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Playing something ... */
    psz_artist = input_GetItem(p_input)->p_meta->psz_artist ?
                  strdup( input_GetItem(p_input)->p_meta->psz_artist ) :
                  strdup( _("no artist") );
    psz_album = input_GetItem(p_input)->p_meta->psz_album ?
                  strdup( input_GetItem(p_input)->p_meta->psz_album ) :
                  strdup( _("no album") );
    psz_title = strdup( input_GetItem(p_input)->psz_name );

    vlc_object_release( p_input );

    if( psz_title == NULL ) psz_title = strdup( N_("(no title)") );
    snprintf( psz_tmp, MAX_LENGTH, "<b>%s</b>\n%s - %s",
              psz_title, psz_artist, psz_album );
    free( psz_title );
    free( psz_artist );
    free( psz_album );

    vlc_mutex_lock( &p_sys->lock );

    Notify( p_this, psz_tmp );

    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

static int Notify( vlc_object_t *p_this, const char *psz_temp )
{
    NotifyNotification * notification;
    notification = notify_notification_new( _("Now Playing"),
                             psz_temp,
                             DATA_PATH "/vlc48x48.png",NULL);
    notify_notification_set_timeout( notification,
                                     config_GetInt(p_this, "notify-timeout") );
    notify_notification_set_urgency( notification, NOTIFY_URGENCY_LOW );
    notify_notification_show( notification, NULL);
    return VLC_SUCCESS;
}

