/*****************************************************************************
 * upnp_cc.cpp :  UPnP discovery module
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
 *
 * Based on original wxWindows patch for VLC, and dependent on CyberLink
 * UPnP library from :
 *          Satoshi Konno <skonno@cybergarage.org>
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
 * Includes
 *****************************************************************************/

#include <cybergarage/upnp/media/player/MediaPlayer.h>

#undef PACKAGE_NAME
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_services_discovery.h>

/* FIXME: thread-safety ?? */
/* FIXME: playlist locking */

/************************************************************************
 * Macros and definitions
 ************************************************************************/
using namespace std;
using namespace CyberLink;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
    static int  Open ( vlc_object_t * );
    static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( "UPnP")
    set_description( N_("Universal Plug'n'Play discovery") )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )

    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )

vlc_module_end ()

/*****************************************************************************
 * Run: main UPnP thread
 *****************************************************************************
 * Processes UPnP events
 *****************************************************************************/
class UPnPHandler : public MediaPlayer, public DeviceChangeListener,
                    /*public EventListener,*/ public SearchResponseListener
{
    private:
        services_discovery_t *p_sd;

        Device *GetDeviceFromUSN( const string& usn )
        {
            return getDevice( usn.substr( 0, usn.find( "::" ) ).c_str() );
        }

        playlist_item_t *FindDeviceNode( Device *dev )
        {
            return playlist_ChildSearchName( p_sd->p_cat, dev->getFriendlyName() );
        }

        playlist_item_t *FindDeviceNode( const string &usn )
        {
            return FindDeviceNode( GetDeviceFromUSN( usn ) );
        }

        playlist_item_t *AddDevice( Device *dev );
        void AddDeviceContent( Device *dev );
        void AddContent( playlist_item_t *p_parent, ContentNode *node );
        void RemoveDevice( Device *dev );

        /* CyberLink callbacks */
        virtual void deviceAdded( Device *dev );
        virtual void deviceRemoved( Device *dev );

        virtual void deviceSearchResponseReceived( SSDPPacket *packet );
        /*virtual void eventNotifyReceived( const char *uuid, long seq,
                                          const char *name,
                                          const char *value );*/

    public:
        UPnPHandler( services_discovery_t *p_this )
            : p_sd( p_this )
        {
            addDeviceChangeListener( this );
            addSearchResponseListener( this );
            //addEventListener( this );
        }
};

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;

    UPnPHandler *u = new UPnPHandler( p_sd );
    u->start( );
    msg_Dbg( p_sd, "upnp discovery started" );
    p_sd->p_private = u;

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    UPnPHandler *u = (UPnPHandler *)p_this->p_private;
    u->stop( );

    msg_Dbg( p_this, "upnp discovery started" );
}


playlist_item_t *UPnPHandler::AddDevice( Device *dev )
{
    if( dev == NULL )
        return NULL;

    /* We are not interested in IGD devices or whatever (at the moment) */
    if ( !dev->isDeviceType( MediaServer::DEVICE_TYPE ) )
        return NULL;

    playlist_item_t *p_item = FindDeviceNode( dev );
    if ( p_item != NULL )
        return p_item;

    /* FIXME:
     * Maybe one day, VLC API will make sensible use of the const keyword;
     * That day, you will no longer need this strdup().
     */
    char *str = strdup( dev->getFriendlyName( ) );

    p_item = playlist_NodeCreate( p_playlist, str, p_sd->p_cat, 0, NULL );
    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;
    msg_Dbg( p_sd, "device %s added", str );
    free( str );

    return p_item;
}

void UPnPHandler::AddDeviceContent( Device *dev )
{
    playlist_item_t *p_devnode = AddDevice( dev );

    if( p_devnode == NULL )
        return;

    AddContent( p_devnode, getContentDirectory( dev ) );
}

void UPnPHandler::AddContent( playlist_item_t *p_parent, ContentNode *node )
{
    if( node == NULL )
        return;

    const char *title = node->getTitle();
    if( title == NULL )
        return;

    msg_Dbg( p_sd, "title = %s", title );

    if ( node->isItemNode() )
    {
        ItemNode *iNode = (ItemNode *)node;
        input_item_t *p_input = input_item_New( p_sd, iNode->getResource(), title );
        /* FIXME: playlist_AddInput() can fail */
        playlist_BothAddInput( p_playlist, p_input, p_parent,
                               PLAYLIST_APPEND, PLAYLIST_END, NULL, NULL,
                               false );
        vlc_gc_decref( p_input );
    } else if ( node->isContainerNode() )
    {
        ContainerNode *conNode = (ContainerNode *)node;

        char* p_name = strdup(title); /* See other comment on strdup */
        playlist_item_t* p_node = playlist_NodeCreate( p_playlist, p_name,
                                                       p_parent, 0, NULL );
        free(p_name);

        unsigned nContentNodes = conNode->getNContentNodes();

        for( unsigned n = 0; n < nContentNodes; n++ )
           AddContent( p_node, conNode->getContentNode( n ) );
    }
}


void UPnPHandler::RemoveDevice( Device *dev )
{
    playlist_item_t *p_item = FindDeviceNode( dev );

    if( p_item != NULL )
        playlist_NodeDelete( p_playlist, p_item, true, true );
}


void UPnPHandler::deviceAdded( Device *dev )
{
    msg_Dbg( p_sd, "adding device" );
    AddDeviceContent( dev );
}


void UPnPHandler::deviceRemoved( Device *dev )
{
    msg_Dbg( p_sd, "removing device" );
    RemoveDevice( dev );
}


void UPnPHandler::deviceSearchResponseReceived( SSDPPacket *packet )
{
    if( !packet->isRootDevice() )
        return;

    string usn, nts, nt, udn;

    packet->getUSN( usn );
    packet->getNT( nt );
    packet->getNTS( nts );
    udn = usn.substr( 0, usn.find( "::" ) );

    /* Remove existing root device before adding updated one */

    Device *dev = GetDeviceFromUSN( usn );
    RemoveDevice( dev );

    if( !packet->isByeBye() )
        AddDeviceContent( dev );
}

/*void UPnPHandler::eventNotifyReceived( const char *uuid, long seq,
                                       const char *name, const char *value )
{
    msg_Dbg( p_sd, "event notify received" );
    msg_Dbg( p_sd, "uuid = %s, name = %s, value = %s", uuid, name, value );
}*/
