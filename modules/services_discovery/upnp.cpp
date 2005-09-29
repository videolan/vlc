/*****************************************************************************
 * upnp.cpp :  UPnP discovery module
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id: sap.c 11664 2005-07-09 06:17:09Z courmisch $
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
 * 
 * Based on original wxWindows patch for VLC, and dependant on CyberLink
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Includes
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <cybergarage/upnp/media/player/MediaPlayer.h>

#undef PACKAGE_NAME
#include <vlc/vlc.h>
#include <vlc/intf.h>

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

vlc_module_begin();
    set_shortname( _("UPnP"));
    set_description( _("Universal Plug'n'Play discovery") );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

vlc_module_end();


/*****************************************************************************
 * Local structures
 *****************************************************************************/

struct services_discovery_sys_t
{
    /* playlist node */
    playlist_item_t *p_node;
    playlist_t *p_playlist;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Main functions */
    static void Run    ( services_discovery_t *p_sd );

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = (services_discovery_sys_t *)
                                malloc( sizeof( services_discovery_sys_t ) );

    playlist_view_t     *p_view;
    vlc_value_t         val;

    p_sd->pf_run = Run;
    p_sd->p_sys  = p_sys;

    /* Create our playlist node */
    p_sys->p_playlist = (playlist_t *)vlc_object_find( p_sd,
                                                       VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( !p_sys->p_playlist )
    {
        msg_Warn( p_sd, "unable to find playlist, cancelling UPnP listening");
        return VLC_EGENERIC;
    }

    p_view = playlist_ViewFind( p_sys->p_playlist, VIEW_CATEGORY );
    p_sys->p_node = playlist_NodeCreate( p_sys->p_playlist, VIEW_CATEGORY,
                                         _("UPnP"), p_view->p_root );
    p_sys->p_node->i_flags |= PLAYLIST_RO_FLAG;
    p_sys->p_node->i_flags &= ~PLAYLIST_SKIP_FLAG;
    val.b_bool = VLC_TRUE;
    var_Set( p_sys->p_playlist, "intf-change", val );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;

    if( p_sys->p_playlist )
    {
        playlist_NodeDelete( p_sys->p_playlist, p_sys->p_node, VLC_TRUE,
                             VLC_TRUE );
        vlc_object_release( p_sys->p_playlist );
    }

    free( p_sys );
}

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
        services_discovery_sys_t *p_sys;

        Device *GetDeviceFromUSN( const string& usn )
        {
            return getDevice( usn.substr( 0, usn.find( "::" ) ).c_str() );
        }

        playlist_item_t *FindDeviceNode( Device *dev )
        {
            return playlist_ChildSearchName( p_sys->p_node, dev->getFriendlyName() );
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
            : p_sd( p_this ), p_sys( p_this->p_sys )
        {
            addDeviceChangeListener( this );
            addSearchResponseListener( this );
            //addEventListener( this );
        }

};

static void Run( services_discovery_t *p_sd )
{
    UPnPHandler u( p_sd );

    u.start();

    msg_Dbg( p_sd, "UPnP discovery started" );
    /* read SAP packets */
    while( !p_sd->b_die )
    {
        msleep( 500 );
    }

    u.stop();
    msg_Dbg( p_sd, "UPnP discovery stopped" );
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

    p_item = playlist_NodeCreate( p_sys->p_playlist, VIEW_CATEGORY,
                                  str, p_sys->p_node );
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
    
    if( !node->isContainerNode() )
    {
        msg_Dbg( p_sd, "not a container" );
        return;
    }

    ContainerNode *conNode = (ContainerNode *)node;
    ItemNode *iNode = (ItemNode *)node;

    playlist_item_t *p_item;
    p_item = playlist_ItemNew( p_sd, iNode->getResource(), title );
    playlist_NodeAddItem( p_sys->p_playlist, p_item, VIEW_CATEGORY,
                          p_parent, PLAYLIST_APPEND, PLAYLIST_END );

    /*if( !cnode->hasContainerNodes() )
        return;*/

    unsigned nContentNodes = conNode->getNContentNodes();

    for( unsigned n = 0; n < nContentNodes; n++ )
        AddContent( p_item, conNode->getContentNode( n ) );
}


void UPnPHandler::RemoveDevice( Device *dev )
{
    playlist_item_t *p_item = FindDeviceNode( dev );

    if( p_item != NULL )
        playlist_NodeDelete( p_sys->p_playlist, p_item, VLC_TRUE, VLC_TRUE );
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

    Device *dev = GetDeviceFromUSN( usn );
    if( packet->isByeBye() )
        RemoveDevice( dev );
    else
        AddDeviceContent( dev );
}

/*void UPnPHandler::eventNotifyReceived( const char *uuid, long seq,
                                       const char *name, const char *value )
{
    msg_Dbg( p_sd, "event notify received" );
    msg_Dbg( p_sd, "uuid = %s, name = %s, value = %s", uuid, name, value );
}*/
