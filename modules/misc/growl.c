/*****************************************************************************
 * growl.c : growl notification plugin
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Jérôme Decoodt <djc -at- videolan -dot- org>
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

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc_meta.h>
#include <network.h>
#include <errno.h>
#include <vlc_md5.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );

static int RegisterToGrowl( vlc_object_t *p_this );
static int NotifyToGrowl( vlc_object_t *p_this, char *psz_type,
                            char *psz_title, char *psz_desc );
static int CheckAndSend( vlc_object_t *p_this, uint8_t* p_data, int i_offset );
#define GROWL_MAX_LENGTH 256

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/

/// \bug [String] REmove all "Growl" in short desc

#define SERVER_DEFAULT "127.0.0.1"
#define SERVER_TEXT N_("Growl server")
#define SERVER_LONGTEXT N_("This is the host to which Growl notifications " \
   "will be sent. By default, notifications are sent locally." )
#define PASS_DEFAULT ""
#define PASS_TEXT N_("Growl password")
/// \bug [String] Password on the Growl server.
#define PASS_LONGTEXT N_("Growl password on the server.")
#define PORT_TEXT N_("Growl UDP port")
/// \bug [String] UDP port on the Growl server
#define PORT_LONGTEXT N_("Growl UDP port on the server.")

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_shortname( N_( "Growl" ) );
    set_description( _("Growl Notification Plugin") );

    add_string( "growl-server", SERVER_DEFAULT, NULL,
                SERVER_TEXT, SERVER_LONGTEXT, VLC_FALSE );
    add_string( "growl-password", PASS_DEFAULT, NULL,
                PASS_TEXT, PASS_LONGTEXT, VLC_FALSE );
    add_integer( "growl-port", 9887, NULL,
                PORT_TEXT, PORT_LONGTEXT, VLC_TRUE );

    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    playlist_t *p_playlist = (playlist_t *)vlc_object_find(
        p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( !p_playlist )
    {
        msg_Err( p_intf, "could not find playlist object" );
        return VLC_ENOOBJ;
    }

    var_AddCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    vlc_object_release( p_playlist );

    RegisterToGrowl( p_this );
    p_intf->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t *)vlc_object_find(
        p_this, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist )
    {
        var_DelCallback( p_playlist, "playlist-current", ItemChange, p_this );
        vlc_object_release( p_playlist );
    }
}

/*****************************************************************************
 * Run
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    msleep( INTF_IDLE_SLEEP );
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    char psz_tmp[GROWL_MAX_LENGTH];
    playlist_t *p_playlist;
    char *psz_title = NULL;
    char *psz_artist = NULL;
    char *psz_album = NULL;
    input_thread_t *p_input;

    p_playlist = (playlist_t *)vlc_object_find( p_this, VLC_OBJECT_INPUT,
                                                FIND_ANYWHERE );
    if( !p_playlist ) return VLC_EGENERIC;

    p_input = p_playlist->p_input;
    vlc_object_release( p_playlist );
    if( !p_input ) return VLC_SUCCESS;
    vlc_object_yield( p_input );

    if( p_input->b_dead || !p_input->input.p_item->psz_name )
    {
        /* Not playing anything ... */
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Playing something ... */
    psz_artist = p_input->input.p_item->p_meta->psz_artist ?
                  strdup( p_input->input.p_item->p_meta->psz_artist ) :
                  strdup( "" );
    psz_album = p_input->input.p_item->p_meta->psz_album ?
                  strdup( p_input->input.p_item->p_meta->psz_album ) :
                  strdup( "" );
    psz_title = strdup( p_input->input.p_item->psz_name );
    if( psz_title == NULL ) psz_title = strdup( N_("(no title)") );
    if( psz_artist == NULL ) psz_artist = strdup( N_("(no artist)") );
    if( psz_album == NULL ) psz_album = strdup( N_("(no album)") );
    snprintf( psz_tmp, GROWL_MAX_LENGTH, "%s %s %s",
              psz_title, psz_artist, psz_album );
    free( psz_title );
    free( psz_artist );
    free( psz_album );

    NotifyToGrowl( p_this, "Now Playing", "Now Playing", psz_tmp );

    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Growl specific functions
 *****************************************************************************/
#define GROWL_PROTOCOL_VERSION (1)
#define GROWL_TYPE_REGISTRATION (0)
#define GROWL_TYPE_NOTIFICATION (1)
#define APPLICATION_NAME "VLC media player"

#define insertstrlen( psz ) \
{ \
    uint16_t i_size = strlen( psz ); \
    psz_encoded[i++] = (i_size>>8)&0xFF; \
    psz_encoded[i++] = i_size&0xFF; \
}
/*****************************************************************************
 * RegisterToGrowl
 *****************************************************************************/
static int RegisterToGrowl( vlc_object_t *p_this )
{
    uint8_t *psz_encoded = malloc(100);
    uint8_t i_defaults = 0;
    char *psz_notifications[] = {"Now Playing", NULL};
    vlc_bool_t pb_defaults[] = {VLC_TRUE, VLC_FALSE};
    int i = 0, j;
    if( psz_encoded == NULL )
        return VLC_FALSE;

    memset( psz_encoded, 0, sizeof(psz_encoded) );
    psz_encoded[i++] = GROWL_PROTOCOL_VERSION;
    psz_encoded[i++] = GROWL_TYPE_REGISTRATION;
    insertstrlen(APPLICATION_NAME);
    i+=2;
    strcpy( (char*)(psz_encoded+i), APPLICATION_NAME );
    i += strlen(APPLICATION_NAME);
    for( j = 0 ; psz_notifications[j] != NULL ; j++)
    {
        insertstrlen(psz_notifications[j]);
        strcpy( (char*)(psz_encoded+i), psz_notifications[j] );
        i += strlen(psz_notifications[j]);
    }
    psz_encoded[4] = j;
    for( j = 0 ; psz_notifications[j] != NULL ; j++)
        if(pb_defaults[j] == VLC_TRUE)
        {
            psz_encoded[i++] = (uint8_t)j;
            i_defaults++;
        }
    psz_encoded[5] = i_defaults;

    CheckAndSend(p_this, psz_encoded, i);
    free( psz_encoded );
    return VLC_SUCCESS;
}

static int NotifyToGrowl( vlc_object_t *p_this, char *psz_type,
                            char *psz_title, char *psz_desc )
{
    uint8_t *psz_encoded = malloc(GROWL_MAX_LENGTH + 42);
    uint16_t flags;
    int i = 0;
    if( psz_encoded == NULL )
        return VLC_FALSE;

    memset( psz_encoded, 0, sizeof(psz_encoded) );
    psz_encoded[i++] = GROWL_PROTOCOL_VERSION;
    psz_encoded[i++] = GROWL_TYPE_NOTIFICATION;
    flags = 0;
    psz_encoded[i++] = (flags>>8)&0xFF;
    psz_encoded[i++] = flags&0xFF;
    insertstrlen(psz_type);
    insertstrlen(psz_title);
    insertstrlen(psz_desc);
    insertstrlen(APPLICATION_NAME);
    strcpy( (char*)(psz_encoded+i), psz_type );
    i += strlen(psz_type);
    strcpy( (char*)(psz_encoded+i), psz_title );
    i += strlen(psz_title);
    strcpy( (char*)(psz_encoded+i), psz_desc );
    i += strlen(psz_desc);
    strcpy( (char*)(psz_encoded+i), APPLICATION_NAME );
    i += strlen(APPLICATION_NAME);

    CheckAndSend(p_this, psz_encoded, i);
    free( psz_encoded );
    return VLC_SUCCESS;
}

static int CheckAndSend( vlc_object_t *p_this, uint8_t* p_data, int i_offset )
{
    int i, i_handle;
    struct md5_s md5;
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    char *psz_password = config_GetPsz( p_intf, "growl-password" );
    char *psz_server = config_GetPsz( p_intf, "growl-server" );
    int i_port = config_GetInt( p_intf, "growl-port" );
    strcpy( (char*)(p_data+i_offset), psz_password );
    i = i_offset + strlen(psz_password);

    InitMD5( &md5 );
    AddMD5( &md5, p_data, i );
    EndMD5( &md5 );

    for( i = 0 ; i < 4 ; i++ )
    {
        md5.p_digest[i] = md5.p_digest[i];
        p_data[i_offset++] =  md5.p_digest[i]     &0xFF;
        p_data[i_offset++] = (md5.p_digest[i]>> 8)&0xFF;
        p_data[i_offset++] = (md5.p_digest[i]>>16)&0xFF;
        p_data[i_offset++] = (md5.p_digest[i]>>24)&0xFF;
    }

    i_handle = net_ConnectUDP( p_this, psz_server, i_port, 0 );
    if( i_handle == -1 )
    {
         msg_Err( p_this, "failed to open a connection (udp)" );
         free( psz_password);
         free( psz_server);
         return VLC_EGENERIC;
    }

    net_StopRecv( i_handle );
    if( send( i_handle, p_data, i_offset, 0 )
          == -1 )
    {
        msg_Warn( p_this, "send error: %s", strerror(errno) );
    }
    net_Close( i_handle );

    free( psz_password);
    free( psz_server);
    return VLC_SUCCESS;
}

#undef GROWL_PROTOCOL_VERSION
#undef GROWL_TYPE_REGISTRATION
#undef GROWL_TYPE_NOTIFICATION
#undef APPLICATION_NAME
#undef insertstrlen
