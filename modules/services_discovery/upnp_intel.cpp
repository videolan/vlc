/*****************************************************************************
 * Upnp_intel.cpp :  UPnP discovery module (Intel SDK)
 *****************************************************************************
 * Copyright (C) 2004-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org> (original plugin)
 *          Christian Henz <henz # c-lab.de>
 *          Mirsal Ennaime <mirsal dot ennaime at gmail dot com>
 *
 * UPnP Plugin using the Intel SDK (libupnp) instead of CyberLink
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

/*
  \TODO: Debug messages: "__FILE__, __LINE__" ok ???, Wrn/Err ???
  \TODO: Change names to VLC standard ???
*/
#undef PACKAGE_NAME
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "upnp_intel.hpp"

#include <vlc_plugin.h>
#include <vlc_services_discovery.h>


// Constants
const char* MEDIA_SERVER_DEVICE_TYPE = "urn:schemas-upnp-org:device:MediaServer:1";
const char* CONTENT_DIRECTORY_SERVICE_TYPE = "urn:schemas-upnp-org:service:ContentDirectory:1";

// VLC handle
struct services_discovery_sys_t
{
    UpnpClient_Handle clientHandle;
    MediaServerList* serverList;
    Lockable* callbackLock;
};

// VLC callback prototypes
static int Open( vlc_object_t* );
static void Close( vlc_object_t* );

// Module descriptor

vlc_module_begin();
    set_shortname( "UPnP" );
    set_description( N_( "Universal Plug'n'Play discovery" ) );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );
    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );
vlc_module_end();


// More prototypes...

static int Callback( Upnp_EventType eventType, void* event, void* user_data );

const char* xml_getChildElementValue( IXML_Element* parent,
                                      const char*   tagName );

IXML_Document* parseBrowseResult( IXML_Document* doc );


// VLC callbacks...

static int Open( vlc_object_t *p_this )
{
    int res;
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = ( services_discovery_sys_t * )
            calloc( 1, sizeof( services_discovery_sys_t ) );

    if(!(p_sd->p_sys = p_sys))
        return VLC_ENOMEM;

    res = UpnpInit( 0, 0 );
    if( res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "%s", UpnpGetErrorMessage( res ) );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->serverList = new MediaServerList( p_sd );
    p_sys->callbackLock = new Lockable();

    res = UpnpRegisterClient( Callback, p_sd, &p_sys->clientHandle );
    if( res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "%s", UpnpGetErrorMessage( res ) );
        Close( (vlc_object_t*) p_sd );
        return VLC_EGENERIC;
    }

    res = UpnpSearchAsync( p_sys->clientHandle, 5,
            MEDIA_SERVER_DEVICE_TYPE, p_sd );
    
    if( res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "%s", UpnpGetErrorMessage( res ) );
        Close( (vlc_object_t*) p_sd );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;

    UpnpFinish();
    delete p_sd->p_sys->serverList;
    delete p_sd->p_sys->callbackLock;

    free( p_sd->p_sys );
}

// XML utility functions:

// Returns the value of a child element, or 0 on error
const char* xml_getChildElementValue( IXML_Element* parent,
                                      const char*   tagName )
{
    if ( !parent ) return 0;
    if ( !tagName ) return 0;

    char* s = strdup( tagName );
    IXML_NodeList* nodeList = ixmlElement_getElementsByTagName( parent, s );
    free( s );
    if ( !nodeList ) return 0;

    IXML_Node* element = ixmlNodeList_item( nodeList, 0 );
    ixmlNodeList_free( nodeList );
    if ( !element ) return 0;

    IXML_Node* textNode = ixmlNode_getFirstChild( element );
    if ( !textNode ) return 0;

    return ixmlNode_getNodeValue( textNode );
}

// Extracts the result document from a SOAP response
IXML_Document* parseBrowseResult( IXML_Document* doc )
{
    ixmlRelaxParser(1);

    if ( !doc ) return 0;

    IXML_NodeList* resultList = ixmlDocument_getElementsByTagName( doc,
                                                                   "Result" );

    if ( !resultList ) return 0;

    IXML_Node* resultNode = ixmlNodeList_item( resultList, 0 );

    ixmlNodeList_free( resultList );

    if ( !resultNode ) return 0;

    IXML_Node* textNode = ixmlNode_getFirstChild( resultNode );
    if ( !textNode ) return 0;

    const char* resultString = ixmlNode_getNodeValue( textNode );
    char* resultXML = strdup( resultString );

    IXML_Document* browseDoc = ixmlParseBuffer( resultXML );

    free( resultXML );

    return browseDoc;
}


// Handles all UPnP events
static int Callback( Upnp_EventType eventType, void* event, void* user_data )
{
    services_discovery_t *p_sd = ( services_discovery_t* ) user_data;
    services_discovery_sys_t* p_sys = p_sd->p_sys;
    Locker locker( p_sys->callbackLock );

    switch( eventType ) {

    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        struct Upnp_Discovery* discovery = ( struct Upnp_Discovery* )event;

        IXML_Document *descriptionDoc = 0;

        int res;
        res = UpnpDownloadXmlDoc( discovery->Location, &descriptionDoc );
        if ( res != UPNP_E_SUCCESS )
        {
            msg_Dbg( p_sd,
                    "%s:%d: Could not download device description!",
                    __FILE__, __LINE__ );
            return res;
        }

        MediaServer::parseDeviceDescription( descriptionDoc,
                discovery->Location, p_sd );

        ixmlDocument_free( descriptionDoc );
    }
    break;

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        struct Upnp_Discovery* discovery = ( struct Upnp_Discovery* )event;

        p_sys->serverList->removeServer( discovery->DeviceId );
    }
    break;

    case UPNP_EVENT_RECEIVED:
    {
        Upnp_Event* e = ( Upnp_Event* )event;

        MediaServer* server = p_sys->serverList->getServerBySID( e->Sid );
        if ( server ) server->fetchContents();
    }
    break;

    case UPNP_EVENT_AUTORENEWAL_FAILED:
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
    {
        // Re-subscribe...

        Upnp_Event_Subscribe* s = ( Upnp_Event_Subscribe* )event;

        MediaServer* server = p_sys->serverList->getServerBySID( s->Sid );
        if ( server ) server->subscribeToContentDirectory();
    }
    break;

    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
        msg_Warn( p_sd, "subscription complete" );
        break;
 
    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
        msg_Warn( p_sd, "search timeout" );
        break;
 
    default:
    msg_Dbg( p_sd,
            "%s:%d: DEBUG: UNHANDLED EVENT ( TYPE=%d )",
            __FILE__, __LINE__, eventType );
    break;
    }

    return UPNP_E_SUCCESS;
}


// Class implementations...

// MediaServer...

void MediaServer::parseDeviceDescription( IXML_Document* doc,
                                          const char*    location,
                                          services_discovery_t* p_sd )
{
    if ( !doc )
    { 
        msg_Dbg( p_sd, "%s:%d: NULL", __FILE__, __LINE__ ); 
        return;
    }
    
    if ( !location )
    {
        msg_Dbg( p_sd, "%s:%d: NULL", __FILE__, __LINE__ ); 
        return;
    }

    const char* baseURL = location;

    // Try to extract baseURL

    IXML_NodeList* urlList = ixmlDocument_getElementsByTagName( doc, "baseURL" );
    if ( !urlList )
    {

        if ( IXML_Node* urlNode = ixmlNodeList_item( urlList, 0 ) )
        {
            IXML_Node* textNode = ixmlNode_getFirstChild( urlNode );
            if ( textNode ) baseURL = ixmlNode_getNodeValue( textNode );
        }

        ixmlNodeList_free( urlList );
    }

    // Get devices

    IXML_NodeList* deviceList =
                ixmlDocument_getElementsByTagName( doc, "device" );
    
    if ( deviceList )
    {
        for ( unsigned int i = 0; i < ixmlNodeList_length( deviceList ); i++ )
        {
            IXML_Element* deviceElement =
                   ( IXML_Element* ) ixmlNodeList_item( deviceList, i );

            const char* deviceType = xml_getChildElementValue( deviceElement,
                                                               "deviceType" );
            if ( !deviceType )
            {
                msg_Dbg( p_sd,
                        "%s:%d: no deviceType!",
                        __FILE__, __LINE__ );
                continue;
            }

            if ( strcmp( MEDIA_SERVER_DEVICE_TYPE, deviceType ) != 0 )
                continue;

            const char* UDN = xml_getChildElementValue( deviceElement, "UDN" );
            if ( !UDN )
            {
                msg_Dbg( p_sd, "%s:%d: no UDN!",
                        __FILE__, __LINE__ );
                continue;
            }
            
            if ( p_sd->p_sys->serverList->getServer( UDN ) != 0 )
                continue;

            const char* friendlyName =
                       xml_getChildElementValue( deviceElement, 
                                                 "friendlyName" );
            
            if ( !friendlyName )
            {
                msg_Dbg( p_sd, "%s:%d: no friendlyName!", __FILE__, __LINE__ );
                continue;
            }

            MediaServer* server = new MediaServer( UDN, friendlyName, p_sd );
            
            if ( !p_sd->p_sys->serverList->addServer( server ) )
            {

                delete server;
                server = 0;
                continue;
            }

            // Check for ContentDirectory service...
            IXML_NodeList* serviceList =
                       ixmlElement_getElementsByTagName( deviceElement,
                                                         "service" );
            if ( serviceList )
            {
                for ( unsigned int j = 0;
                      j < ixmlNodeList_length( serviceList ); j++ )
                {
                    IXML_Element* serviceElement =
                        ( IXML_Element* ) ixmlNodeList_item( serviceList, j );

                    const char* serviceType =
                        xml_getChildElementValue( serviceElement,
                                                  "serviceType" );
                    if ( !serviceType )
                        continue;

                    if ( strcmp( CONTENT_DIRECTORY_SERVICE_TYPE,
                                serviceType ) != 0 )
                        continue;

                    const char* eventSubURL =
                        xml_getChildElementValue( serviceElement,
                                                  "eventSubURL" );
                    if ( !eventSubURL )
                        continue;

                    const char* controlURL =
                        xml_getChildElementValue( serviceElement,
                                                  "controlURL" );
                    if ( !controlURL )
                        continue;

                    // Try to subscribe to ContentDirectory service

                    char* url = ( char* ) malloc( strlen( baseURL ) +
                            strlen( eventSubURL ) + 1 );
                    if ( url )
                    {
                        char* s1 = strdup( baseURL );
                        char* s2 = strdup( eventSubURL );

                        if ( UpnpResolveURL( s1, s2, url ) ==
                                UPNP_E_SUCCESS )
                        {
                            server->setContentDirectoryEventURL( url );
                            server->subscribeToContentDirectory();
                        }

                        free( s1 );
                        free( s2 );
                        free( url );
                    }

                    // Try to browse content directory...

                    url = ( char* ) malloc( strlen( baseURL ) +
                            strlen( controlURL ) + 1 );
                    if ( url )
                    {
                        char* s1 = strdup( baseURL );
                        char* s2 = strdup( controlURL );

                        if ( UpnpResolveURL( s1, s2, url ) ==
                                UPNP_E_SUCCESS )
                        {
                            server->setContentDirectoryControlURL( url );
                            server->fetchContents();
                        }

                        free( s1 );
                        free( s2 );
                        free( url );
                    }
               }
               ixmlNodeList_free( serviceList );
           }
       }
       ixmlNodeList_free( deviceList );
    }
}

MediaServer::MediaServer( const char* UDN,
                          const char* friendlyName,
                          services_discovery_t* p_sd )
{
    _p_sd = p_sd;

    _UDN = UDN;
    _friendlyName = friendlyName;

    _contents = NULL;
    _inputItem = NULL;
}

MediaServer::~MediaServer()
{
    delete _contents;
}

const char* MediaServer::getUDN() const
{
  const char* s = _UDN.c_str();
  return s;
}

const char* MediaServer::getFriendlyName() const
{
    const char* s = _friendlyName.c_str();
    return s;
}

void MediaServer::setContentDirectoryEventURL( const char* url )
{
    _contentDirectoryEventURL = url;
}

const char* MediaServer::getContentDirectoryEventURL() const
{
    const char* s =  _contentDirectoryEventURL.c_str();
    return s;
}

void MediaServer::setContentDirectoryControlURL( const char* url )
{
    _contentDirectoryControlURL = url;
}

const char* MediaServer::getContentDirectoryControlURL() const
{
    return _contentDirectoryControlURL.c_str();
}

void MediaServer::subscribeToContentDirectory()
{
    const char* url = getContentDirectoryEventURL();
    if ( !url || strcmp( url, "" ) == 0 )
    {
        msg_Dbg( _p_sd, "No subscription url set!" );
        return;
    }

    int timeOut = 1810;
    Upnp_SID sid;

    int res = UpnpSubscribe( _p_sd->p_sys->clientHandle, url, &timeOut, sid );

    if ( res == UPNP_E_SUCCESS )
    {
        _subscriptionTimeOut = timeOut;
        memcpy( _subscriptionID, sid, sizeof( Upnp_SID ) );
    }
    else
    {
        msg_Dbg( _p_sd,
                "%s:%d: WARNING: '%s': %s", __FILE__, __LINE__,
                getFriendlyName(), UpnpGetErrorMessage( res ) );
    }
}

IXML_Document* MediaServer::_browseAction( const char* pObjectID,
                                           const char* pBrowseFlag,
                                           const char* pFilter,
                                           const char* pStartingIndex,
                                           const char* pRequestedCount,
                                           const char* pSortCriteria )
{
    IXML_Document* action = 0;
    IXML_Document* response = 0;
    const char* url = getContentDirectoryControlURL();
    
    if ( !url || strcmp( url, "" ) == 0 )
    {
        msg_Dbg( _p_sd, "No subscription url set!" );
        return 0;
    }

    char* ObjectID = strdup( pObjectID );
    char* BrowseFlag = strdup( pBrowseFlag );
    char* Filter = strdup( pFilter );
    char* StartingIndex = strdup( pStartingIndex );
    char* RequestedCount = strdup( pRequestedCount );
    char* SortCriteria = strdup( pSortCriteria );
    char* serviceType = strdup( CONTENT_DIRECTORY_SERVICE_TYPE );

    int res;

    res = UpnpAddToAction( &action, "Browse",
            serviceType, "ObjectID", ObjectID );
    
    if ( res != UPNP_E_SUCCESS ) 
    {
        msg_Dbg( _p_sd,
                 "%s:%d: ERROR: %s", __FILE__, __LINE__,
                 UpnpGetErrorMessage( res ) );
        goto browseActionCleanup;
    }

    res = UpnpAddToAction( &action, "Browse",
            serviceType, "BrowseFlag", BrowseFlag );
    
    if ( res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
             "%s:%d: ERROR: %s", __FILE__, __LINE__,
             UpnpGetErrorMessage( res ) );
        goto browseActionCleanup;
    }

    res = UpnpAddToAction( &action, "Browse",
            serviceType, "Filter", Filter );
    
    if ( res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
             "%s:%d: ERROR: %s", __FILE__, __LINE__,
             UpnpGetErrorMessage( res ) );
        goto browseActionCleanup;
    }

    res = UpnpAddToAction( &action, "Browse",
            serviceType, "StartingIndex", StartingIndex );

    if ( res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
             "%s:%d: ERROR: %s", __FILE__, __LINE__,
             UpnpGetErrorMessage( res ) );
        goto browseActionCleanup;
    }

    res = UpnpAddToAction( &action, "Browse",
            serviceType, "RequestedCount", RequestedCount );

    if ( res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
                "%s:%d: ERROR: %s", __FILE__, __LINE__,
                UpnpGetErrorMessage( res ) ); goto browseActionCleanup; }

    res = UpnpAddToAction( &action, "Browse",
            serviceType, "SortCriteria", SortCriteria );
    
    if ( res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
             "%s:%d: ERROR: %s", __FILE__, __LINE__,
             UpnpGetErrorMessage( res ) );
        goto browseActionCleanup;
    }

    res = UpnpSendAction( _p_sd->p_sys->clientHandle,
              url,
              CONTENT_DIRECTORY_SERVICE_TYPE,
              0,
              action,
              &response );
    
    if ( res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
                "%s:%d: ERROR: %s when trying the send() action with URL: %s",
                __FILE__, __LINE__,
                UpnpGetErrorMessage( res ), url );

        ixmlDocument_free( response );
        response = 0;
    }

 browseActionCleanup:

    free( ObjectID );
    free( BrowseFlag );
    free( Filter );
    free( StartingIndex );
    free( RequestedCount );
    free( SortCriteria );

    free( serviceType );

    ixmlDocument_free( action );
    return response;
}

void MediaServer::fetchContents()
{
    Container* root = new Container( 0, "0", getFriendlyName() );
    _fetchContents( root );

    _contents = root;
    _contents->setInputItem( _inputItem );

    _buildPlaylist( _contents );
}

bool MediaServer::_fetchContents( Container* parent )
{
    if (!parent)
    {
        msg_Dbg( _p_sd,
                "%s:%d: parent==NULL", __FILE__, __LINE__ );
        return false;
    }

    IXML_Document* response = _browseAction( parent->getObjectID(),
                                      "BrowseDirectChildren",
                                      "*", "0", "0", "" );
    if ( !response )
    {
        msg_Dbg( _p_sd,
                "%s:%d: ERROR! No response from browse() action",
                __FILE__, __LINE__ );
        return false;
    }

    IXML_Document* result = parseBrowseResult( response );
    ixmlDocument_free( response );
    
    if ( !result )
    {
        msg_Dbg( _p_sd,
                "%s:%d: ERROR! browse() response parsing failed",
                __FILE__, __LINE__ );
        return false;
    }

    IXML_NodeList* containerNodeList =
                ixmlDocument_getElementsByTagName( result, "container" );
    
    if ( containerNodeList )
    {
        for ( unsigned int i = 0;
                i < ixmlNodeList_length( containerNodeList ); i++ )
        {
            IXML_Element* containerElement =
                  ( IXML_Element* )ixmlNodeList_item( containerNodeList, i );

            const char* objectID = ixmlElement_getAttribute( containerElement,
                                                             "id" );
            if ( !objectID )
                continue;

            const char* childCountStr =
                    ixmlElement_getAttribute( containerElement, "childCount" );
            
            if ( !childCountStr )
                continue;
            
            int childCount = atoi( childCountStr );
            const char* title = xml_getChildElementValue( containerElement,
                                                          "dc:title" );
            
            if ( !title )
                continue;
            
            const char* resource = xml_getChildElementValue( containerElement,
                                                             "res" );

            if ( resource && childCount < 1 )
            {
                Item* item = new Item( parent, objectID, title, resource );
                parent->addItem( item );
            }

            else
            {
                Container* container = new Container( parent, objectID, title );
                parent->addContainer( container );

                if ( childCount > 0 )
                    _fetchContents( container );
            }
        }
        ixmlNodeList_free( containerNodeList );
    }

    IXML_NodeList* itemNodeList = ixmlDocument_getElementsByTagName( result,
                                                                     "item" );
    if ( itemNodeList )
    {
        for ( unsigned int i = 0; i < ixmlNodeList_length( itemNodeList ); i++ )
        {
            IXML_Element* itemElement =
                        ( IXML_Element* )ixmlNodeList_item( itemNodeList, i );

            const char* objectID =
                        ixmlElement_getAttribute( itemElement, "id" );
            
            if ( !objectID )
                continue;

            const char* title =
                        xml_getChildElementValue( itemElement, "dc:title" );
            
            if ( !title )
                continue;

            const char* resource =
                        xml_getChildElementValue( itemElement, "res" );
            
            if ( !resource )
                continue;

            Item* item = new Item( parent, objectID, title, resource );
            parent->addItem( item );
        }
        ixmlNodeList_free( itemNodeList );
    }

    ixmlDocument_free( result );
    return true;
}

void MediaServer::_buildPlaylist( Container* parent )
{
    for ( unsigned int i = 0; i < parent->getNumContainers(); i++ )
    {
        Container* container = parent->getContainer( i );

        input_item_t* p_input_item = input_item_New( _p_sd, "vlc://nop", parent->getTitle() ); 
        input_item_AddSubItem( parent->getInputItem(), p_input_item );

        container->setInputItem( p_input_item );
        _buildPlaylist( container );
    }

    for ( unsigned int i = 0; i < parent->getNumItems(); i++ )
    {
        Item* item = parent->getItem( i );

        input_item_t* p_input_item = input_item_New( _p_sd,
                                               item->getResource(),
                                               item->getTitle() );
        assert( p_input_item );
        input_item_AddSubItem( parent->getInputItem(), p_input_item );
        item->setInputItem( p_input_item );
    }
}

void MediaServer::setInputItem( input_item_t* p_input_item )
{
    if(_inputItem == p_input_item)
        return;

    if(_inputItem)
        vlc_gc_decref( _inputItem );

    vlc_gc_incref( p_input_item );
    _inputItem = p_input_item;
}

bool MediaServer::compareSID( const char* sid )
{
    return ( strncmp( _subscriptionID, sid, sizeof( Upnp_SID ) ) == 0 );
}


// MediaServerList...

MediaServerList::MediaServerList( services_discovery_t* p_sd )
{
    _p_sd = p_sd;
}

MediaServerList::~MediaServerList()
{
    for ( unsigned int i = 0; i < _list.size(); i++ )
    {
        delete _list[i];
    }
}

bool MediaServerList::addServer( MediaServer* s )
{
    input_item_t* p_input_item = NULL;
    if ( getServer( s->getUDN() ) != 0 ) return false;

    msg_Dbg( _p_sd, "Adding server '%s'",
            s->getFriendlyName() );

    services_discovery_t* p_sd = _p_sd;

    p_input_item = input_item_New( p_sd, "vlc://nop", s->getFriendlyName() ); 
    s->setInputItem( p_input_item );

    services_discovery_AddItem( p_sd, p_input_item, NULL );

    _list.push_back( s );

    return true;
}

MediaServer* MediaServerList::getServer( const char* UDN )
{
    MediaServer* result = 0;

    for ( unsigned int i = 0; i < _list.size(); i++ )
    {
        if( strcmp( UDN, _list[i]->getUDN() ) == 0 )
        {
            result = _list[i];
            break;
        }
    }

    return result;
}

MediaServer* MediaServerList::getServerBySID( const char* sid )
{
    MediaServer* server = 0;

    for ( unsigned int i = 0; i < _list.size(); i++ )
    {
        if ( _list[i]->compareSID( sid ) )
        {
            server = _list[i];
            break;
        }
    }

    return server;
}

void MediaServerList::removeServer( const char* UDN )
{
    MediaServer* server = getServer( UDN );
    if ( !server ) return;

    msg_Dbg( _p_sd,
            "Removing server '%s'", server->getFriendlyName() );

    std::vector<MediaServer*>::iterator it;
    for ( it = _list.begin(); it != _list.end(); it++ )
    {
        if ( *it == server )
        {
            _list.erase( it );
            delete server;
            break;
        }
    }
}


// Item...

Item::Item( Container* parent, const char* objectID, const char* title, const char* resource )
{
    _parent = parent;

    _objectID = objectID;
    _title = title;
    _resource = resource;

    _inputItem = NULL;
}

Item::~Item()
{
    if(_inputItem)
        vlc_gc_decref( _inputItem );
}

const char* Item::getObjectID() const
{
    return _objectID.c_str();
}

const char* Item::getTitle() const
{
    return _title.c_str();
}

const char* Item::getResource() const
{
    return _resource.c_str();
}

void Item::setInputItem( input_item_t* p_input_item )
{
    if(_inputItem == p_input_item)
        return;

    if(_inputItem)
        vlc_gc_decref( _inputItem );

    vlc_gc_incref( p_input_item );
    _inputItem = p_input_item;
}

input_item_t* Item::getInputItem() const
{
    return _inputItem;
}


// Container...

Container::Container( Container*  parent,
                      const char* objectID,
                      const char* title )
{
    _parent = parent;

    _objectID = objectID;
    _title = title;

    _inputItem = NULL;
}

Container::~Container()
{
    for ( unsigned int i = 0; i < _containers.size(); i++ )
    {
        delete _containers[i];
    }

    for ( unsigned int i = 0; i < _items.size(); i++ )
    {
        delete _items[i];
    }

    if(_inputItem )
        vlc_gc_decref( _inputItem );
}

void Container::addItem( Item* item )
{
    _items.push_back( item );
}

void Container::addContainer( Container* container )
{
    _containers.push_back( container );
}

const char* Container::getObjectID() const
{
    return _objectID.c_str();
}

const char* Container::getTitle() const
{
    return _title.c_str();
}

unsigned int Container::getNumItems() const
{
    return _items.size();
}

unsigned int Container::getNumContainers() const
{
    return _containers.size();
}

Item* Container::getItem( unsigned int i ) const
{
    if ( i < _items.size() ) return _items[i];
    return 0;
}

Container* Container::getContainer( unsigned int i ) const
{
    if ( i < _containers.size() ) return _containers[i];
    return 0;
}

Container* Container::getParent()
{
    return _parent;
}

void Container::setInputItem( input_item_t* p_input_item )
{
    if(_inputItem == p_input_item)
        return;

    if(_inputItem)
        vlc_gc_decref( _inputItem );

    vlc_gc_incref( p_input_item );
    _inputItem = p_input_item;
}

input_item_t* Container::getInputItem() const
{
    return _inputItem;
}
