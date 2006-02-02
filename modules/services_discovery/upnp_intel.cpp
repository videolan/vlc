/*****************************************************************************
 * Upnp_intell.cpp :  UPnP discovery module (Intel SDK)
 *****************************************************************************
 * Copyright (C) 2004-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org> (original plugin)
 *          Christian Henz <henz # c-lab.de> 
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

#include <stdlib.h>

#include <vector>
#include <string>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#undef PACKAGE_NAME
#include <vlc/vlc.h>
#include <vlc/intf.h>


// VLC handle

struct services_discovery_sys_t
{
    playlist_item_t *p_node;
    playlist_t *p_playlist;
};


// Constants

const char* MEDIA_SERVER_DEVICE_TYPE = "urn:schemas-upnp-org:device:MediaServer:1";
const char* CONTENT_DIRECTORY_SERVICE_TYPE = "urn:schemas-upnp-org:service:ContentDirectory:1";


// Classes 

class MediaServer;
class MediaServerList;
class Item;
class Container;

// Cookie that is passed to the callback

typedef struct 
{
    services_discovery_t* serviceDiscovery;
    UpnpClient_Handle clientHandle;
    MediaServerList* serverList; 
} Cookie;


// Class definitions...

class Lockable 
{  
public:

    Lockable( Cookie* c ) 
    {
	vlc_mutex_init( c->serviceDiscovery, &_mutex );
    }

    ~Lockable() 
    {
	vlc_mutex_destroy( &_mutex );
    }

    void lock() { vlc_mutex_lock( &_mutex ); }
    void unlock() { vlc_mutex_unlock( &_mutex ); }

private:
    
    vlc_mutex_t _mutex;
};


class Locker 
{
public:
    Locker( Lockable* l ) 
    {
	_lockable = l;
	_lockable->lock();
    }

    ~Locker() 
    {
	_lockable->unlock();
    }

private:
    Lockable* _lockable;
};


class MediaServer
{
public:

    static void parseDeviceDescription( IXML_Document* doc, const char* location, Cookie* cookie );
    
    MediaServer( const char* UDN, const char* friendlyName, Cookie* cookie );
    ~MediaServer();
    
    const char* getUDN() const;
    const char* getFriendlyName() const;

    void setContentDirectoryEventURL( const char* url );
    const char* getContentDirectoryEventURL() const;

    void setContentDirectoryControlURL( const char* url );
    const char* getContentDirectoryControlURL() const;

    void subscribeToContentDirectory();
    void fetchContents();

    void setPlaylistNode( playlist_item_t* node );

    bool compareSID( const char* sid );

private:

    bool _fetchContents( Container* parent );
    void _buildPlaylist( Container* container );
    IXML_Document* _browseAction( const char*, const char*, const char*, const char*, const char*, const char* );

    Cookie* _cookie;
  
    Container* _contents;
    playlist_item_t* _playlistNode;

    std::string _UDN;
    std::string _friendlyName;
  
    std::string _contentDirectoryEventURL;
    std::string _contentDirectoryControlURL;

    int _subscriptionTimeOut;
    Upnp_SID _subscriptionID;
};


class MediaServerList
{
public:

    MediaServerList( Cookie* cookie );
    ~MediaServerList();

    bool addServer( MediaServer* s );
    void removeServer( const char* UDN );

    MediaServer* getServer( const char* UDN );
    MediaServer* getServerBySID( const char* );

private:

    Cookie* _cookie;

    std::vector<MediaServer*> _list;
};


class Item 
{
public:

    Item( Container* parent, const char* objectID, const char* title, const char* resource );

    const char* getObjectID() const;
    const char* getTitle() const;
    const char* getResource() const;

    void setPlaylistNode( playlist_item_t* node );
    playlist_item_t* getPlaylistNode() const ;

private:

    playlist_item_t* _playlistNode;
 
    Container* _parent;
    std::string _objectID;
    std::string _title;
    std::string _resource;
};


class Container 
{
public:

    Container( Container* parent, const char* objectID, const char* title );
    ~Container();

    void addItem( Item* item );
    void addContainer( Container* container );

    const char* getObjectID() const;
    const char* getTitle() const;

    unsigned int getNumItems() const;
    unsigned int getNumContainers() const;

    Item* getItem( unsigned int i ) const;
    Container* getContainer( unsigned int i ) const;

    void setPlaylistNode( playlist_item_t* node );
    playlist_item_t* getPlaylistNode() const;

private:

    playlist_item_t* _playlistNode;

    Container* _parent;

    std::string _objectID;
    std::string _title;
    std::vector<Item*> _items;
    std::vector<Container*> _containers;
};


// VLC callback prototypes

static int Open( vlc_object_t* );
static void Close( vlc_object_t* );
static void Run( services_discovery_t *p_sd );

// Module descriptor

vlc_module_begin();
set_shortname( "UPnP" );
set_description( _( "Universal Plug'n'Play discovery ( Intel SDK )" ) );
set_category( CAT_PLAYLIST );
set_subcategory( SUBCAT_PLAYLIST_SD );
set_capability( "services_discovery", 0 );
set_callbacks( Open, Close );
vlc_module_end();


// More prototypes...

static Lockable* CallbackLock;
static int Callback( Upnp_EventType eventType, void* event, void* pCookie );

char* xml_makeSpecialChars( const char* in );
const char* xml_getChildElementValue( IXML_Element* parent, const char* tagName );
IXML_Document* parseBrowseResult( IXML_Document* doc );


// VLC callbacks...

static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = ( services_discovery_sys_t * )
	malloc( sizeof( services_discovery_sys_t ) );
    
    playlist_view_t *p_view;
    vlc_value_t val;

    p_sd->pf_run = Run;
    p_sd->p_sys = p_sys;

    /* Create our playlist node */
    p_sys->p_playlist = ( playlist_t * )vlc_object_find( p_sd,
							 VLC_OBJECT_PLAYLIST,
							 FIND_ANYWHERE );
    if( !p_sys->p_playlist )
    {
	msg_Warn( p_sd, "unable to find playlist, cancelling UPnP listening" );
	return VLC_EGENERIC;
    }

    p_view = playlist_ViewFind( p_sys->p_playlist, VIEW_CATEGORY );
    p_sys->p_node = playlist_NodeCreate( p_sys->p_playlist, VIEW_CATEGORY,
                                         "UPnP", p_view->p_root );
    p_sys->p_node->i_flags |= PLAYLIST_RO_FLAG;
    p_sys->p_node->i_flags &= ~PLAYLIST_SKIP_FLAG;
    val.b_bool = VLC_TRUE;
    var_Set( p_sys->p_playlist, "intf-change", val );

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    if( p_sys->p_playlist )
    {
	playlist_NodeDelete( p_sys->p_playlist, p_sys->p_node, VLC_TRUE,
			     VLC_TRUE );
	vlc_object_release( p_sys->p_playlist );
    }

    free( p_sys );
}

static void Run( services_discovery_t* p_sd )
{
    int res;
  
    res = UpnpInit( 0, 0 );
    if( res != UPNP_E_SUCCESS ) 
    {
	msg_Err( p_sd, "%s", UpnpGetErrorMessage( res ) );
	return;
    }

    Cookie cookie;
    cookie.serviceDiscovery = p_sd;
    cookie.serverList = new MediaServerList( &cookie );

    CallbackLock = new Lockable( &cookie );

    res = UpnpRegisterClient( Callback, &cookie, &cookie.clientHandle );
    if( res != UPNP_E_SUCCESS ) 
    {
	msg_Err( p_sd, "%s", UpnpGetErrorMessage( res ) );
	goto shutDown;
    }

    res = UpnpSearchAsync( cookie.clientHandle, 5, MEDIA_SERVER_DEVICE_TYPE, &cookie );
    if( res != UPNP_E_SUCCESS ) 
    {
	msg_Err( p_sd, "%s", UpnpGetErrorMessage( res ) );
	goto shutDown;
    }
 
    msg_Dbg( p_sd, "UPnP discovery started" );
    while( !p_sd->b_die ) 
    {
	msleep( 500 );
    }

    msg_Dbg( p_sd, "UPnP discovery stopped" );

 shutDown:
    UpnpFinish();
    delete cookie.serverList;
    delete CallbackLock;
}


// XML utility functions:

// Returns the value of a child element, or 0 on error
const char* xml_getChildElementValue( IXML_Element* parent, const char* tagName ) 
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


// Replaces "&lt;" with "<" etc.
// Returns a newly created string that has to be freed by the caller.
// Returns 0 on error ( out of mem )
// \TODO: Probably not very robust!!!
char* xml_makeSpecialChars( const char* in ) 
{
    if ( !in ) return 0;

    char* result = ( char* )malloc( strlen( in ) + 1 );
    if ( !result ) return 0;

    char* out = result;

    while( *in ) 
    {
	if ( strncmp( "&amp;", in, 5 ) == 0 ) 
	{
	    *out = '&';

	    in += 5;
	    out++;
	} 
	else if ( strncmp( "&quot;", in, 6 ) == 0 ) 
	{
      	    *out = '"';

	    in += 6;
	    out++;
	} 
	else if ( strncmp( "&gt;", in, 4 ) == 0 ) 
	{
      	    *out = '>';
	    
	    in += 4;
	    out++;
	} 
	else if ( strncmp( "&lt;", in, 4 ) == 0 ) 
	{
	    *out = '<';

	    in += 4;
	    out++;
	} 
	else 
	{
	    *out = *in;

	    in++;
	    out++;
	}
    }

    *out = '\0';
    return result;
}


// Extracts the result document from a SOAP response
IXML_Document* parseBrowseResult( IXML_Document* doc ) 
{
    if ( !doc ) return 0;
  
    IXML_NodeList* resultList = ixmlDocument_getElementsByTagName( doc, "Result" );
    if ( !resultList ) return 0;
  
    IXML_Node* resultNode = ixmlNodeList_item( resultList, 0 );

    ixmlNodeList_free( resultList );

    if ( !resultNode ) return 0;

    IXML_Node* textNode = ixmlNode_getFirstChild( resultNode );
    if ( !textNode ) return 0;

    const char* resultString = ixmlNode_getNodeValue( textNode );
    char* resultXML = xml_makeSpecialChars( resultString );

    IXML_Document* browseDoc = ixmlParseBuffer( resultXML );

    free( resultXML );

    return browseDoc;
}


// Handles all UPnP events
static int Callback( Upnp_EventType eventType, void* event, void* pCookie ) 
{
    Locker locker( CallbackLock );

    Cookie* cookie = ( Cookie* )pCookie;

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
	      msg_Dbg( cookie->serviceDiscovery, "%s:%d: Could not download device description!", __FILE__, __LINE__ );
	      return res;
	    }

	    MediaServer::parseDeviceDescription( descriptionDoc, discovery->Location, cookie );

	    ixmlDocument_free( descriptionDoc );
	}
	break;
    
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	{
	    struct Upnp_Discovery* discovery = ( struct Upnp_Discovery* )event;

	    cookie->serverList->removeServer( discovery->DeviceId );
	}
	break;

    case UPNP_EVENT_RECEIVED:
	{
	    Upnp_Event* e = ( Upnp_Event* )event;

	    MediaServer* server = cookie->serverList->getServerBySID( e->Sid );
	    if ( server ) server->fetchContents(); 
	}
	break;

    case UPNP_EVENT_AUTORENEWAL_FAILED:
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
	{
	    // Re-subscribe...

	    Upnp_Event_Subscribe* s = ( Upnp_Event_Subscribe* )event;

	    MediaServer* server = cookie->serverList->getServerBySID( s->Sid );
	    if ( server ) server->subscribeToContentDirectory();
	}
	break;
	
    default:
	msg_Dbg( cookie->serviceDiscovery, "%s:%d: DEBUG: UNHANDLED EVENT ( TYPE=%d )", __FILE__, __LINE__, eventType );
	break;
    }

    return UPNP_E_SUCCESS;
}


// Class implementations...

// MediaServer...

void MediaServer::parseDeviceDescription( IXML_Document* doc, const char* location, Cookie* cookie ) 
{
    if ( !doc ) { msg_Dbg( cookie->serviceDiscovery, "%s:%d: NULL", __FILE__, __LINE__ ); return; }
    if ( !location ) { msg_Dbg( cookie->serviceDiscovery, "%s:%d: NULL", __FILE__, __LINE__ ); return; }
  
    const char* baseURL = location;

    // Try to extract baseURL

    IXML_NodeList* urlList = ixmlDocument_getElementsByTagName( doc, "baseURL" );
    if ( urlList ) 
    {  
	if ( IXML_Node* urlNode = ixmlNodeList_item( urlList, 0 ) ) 
	{
	    IXML_Node* textNode = ixmlNode_getFirstChild( urlNode );
	    if ( textNode ) baseURL = ixmlNode_getNodeValue( textNode );
	}
      
	ixmlNodeList_free( urlList );
    }
  
    // Get devices

    IXML_NodeList* deviceList = ixmlDocument_getElementsByTagName( doc, "device" );
    if ( deviceList )
    {
  
	for ( unsigned int i = 0; i < ixmlNodeList_length( deviceList ); i++ ) 
	{
	    IXML_Element* deviceElement = ( IXML_Element* )ixmlNodeList_item( deviceList, i );
    
	    const char* deviceType = xml_getChildElementValue( deviceElement, "deviceType" );
	    if ( !deviceType ) { msg_Dbg( cookie->serviceDiscovery, "%s:%d: no deviceType!", __FILE__, __LINE__ ); continue; }
	    if ( strcmp( MEDIA_SERVER_DEVICE_TYPE, deviceType ) != 0 ) continue;
   
	    const char* UDN = xml_getChildElementValue( deviceElement, "UDN" );
	    if ( !UDN ) { msg_Dbg( cookie->serviceDiscovery, "%s:%d: no UDN!", __FILE__, __LINE__ ); continue; }    
	    if ( cookie->serverList->getServer( UDN ) != 0 ) continue;
    
	    const char* friendlyName = xml_getChildElementValue( deviceElement, "friendlyName" );
	    if ( !friendlyName ) { msg_Dbg( cookie->serviceDiscovery, "%s:%d: no friendlyName!", __FILE__, __LINE__ ); continue; }
    
	    MediaServer* server = new MediaServer( UDN, friendlyName, cookie );
	    if ( !cookie->serverList->addServer( server ) ) {

		delete server;
		server = 0;
		continue;
	    }
	    
	    // Check for ContentDirectory service...
	    
	    IXML_NodeList* serviceList = ixmlElement_getElementsByTagName( deviceElement, "service" );
	    if ( serviceList ) 
	    {
    		for ( unsigned int j = 0; j < ixmlNodeList_length( serviceList ); j++ ) 
		{
		    IXML_Element* serviceElement = ( IXML_Element* )ixmlNodeList_item( serviceList, j );
		
		    const char* serviceType = xml_getChildElementValue( serviceElement, "serviceType" );
		    if ( !serviceType ) continue;
		    if ( strcmp( CONTENT_DIRECTORY_SERVICE_TYPE, serviceType ) != 0 ) continue;
		    
		    const char* eventSubURL = xml_getChildElementValue( serviceElement, "eventSubURL" );
		    if ( !eventSubURL ) continue;
		
		    const char* controlURL = xml_getChildElementValue( serviceElement, "controlURL" );
		    if ( !controlURL ) continue;

		    // Try to subscribe to ContentDirectory service
      
		    char* url = ( char* )malloc( strlen( baseURL ) + strlen( eventSubURL ) + 1 );
		    if ( url ) 
		    {
			    char* s1 = strdup( baseURL );
			    char* s2 = strdup( eventSubURL );
		    
			    if ( UpnpResolveURL( s1, s2, url ) == UPNP_E_SUCCESS ) 
			    {
				// msg_Dbg( cookie->serviceDiscovery, "CDS EVENT URL: %s", url );
				
				server->setContentDirectoryEventURL( url );
				server->subscribeToContentDirectory();
			    }

			    free( s1 );
			    free( s2 );
			    free( url );
		    }
		    
		    // Try to browse content directory...
		    
		    url = ( char* )malloc( strlen( baseURL ) + strlen( controlURL ) + 1 );
		    if ( url ) 
		    {
			char* s1 = strdup( baseURL );
			char* s2 = strdup( controlURL );
		    
			if ( UpnpResolveURL( s1, s2, url ) == UPNP_E_SUCCESS ) 
			{
			    // msg_Dbg( cookie->serviceDiscovery, "CDS CTRL URL: %s", url );
			    
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

MediaServer::MediaServer( const char* UDN, const char* friendlyName, Cookie* cookie ) 
{
    _cookie = cookie;

    _UDN = UDN;
    _friendlyName = friendlyName;

    _contents = 0;
    _playlistNode = 0;
}

MediaServer::~MediaServer() 
{ 
    if ( _contents ) 
    {
	playlist_NodeDelete( _cookie->serviceDiscovery->p_sys->p_playlist, 
			    _playlistNode,
			    true,
			    true );
    }

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
	msg_Dbg( _cookie->serviceDiscovery, "No subscription url set!" ); 
	return;
    }

    int timeOut = 1810;
    Upnp_SID sid;
  
    int res = UpnpSubscribe( _cookie->clientHandle, url, &timeOut, sid );

    if ( res == UPNP_E_SUCCESS ) 
    {
	_subscriptionTimeOut = timeOut;
	memcpy( _subscriptionID, sid, sizeof( Upnp_SID ) );
    } 
    else 
    {
	msg_Dbg( _cookie->serviceDiscovery, "%s:%d: WARNING: '%s': %s", __FILE__, __LINE__, getFriendlyName(), UpnpGetErrorMessage( res ) );
    }
}

IXML_Document* MediaServer::_browseAction( const char* pObjectID, const char* pBrowseFlag, const char* pFilter, 
					   const char* pStartingIndex, const char* pRequestedCount, const char* pSortCriteria ) 
{
    IXML_Document* action = 0;
    IXML_Document* response = 0;

    const char* url = getContentDirectoryControlURL();
    if ( !url || strcmp( url, "" ) == 0 ) { msg_Dbg( _cookie->serviceDiscovery, "No subscription url set!" ); return 0; }

    char* ObjectID = strdup( pObjectID );
    char* BrowseFlag = strdup( pBrowseFlag );
    char* Filter = strdup( pFilter );
    char* StartingIndex = strdup( pStartingIndex );
    char* RequestedCount = strdup( pRequestedCount );
    char* SortCriteria = strdup( pSortCriteria );

    char* serviceType = strdup( CONTENT_DIRECTORY_SERVICE_TYPE );

    int res;

    res = UpnpAddToAction( &action, "Browse", serviceType, "ObjectID", ObjectID );
    if ( res != UPNP_E_SUCCESS ) { /* msg_Dbg( _cookie->serviceDiscovery, "%s:%d: ERROR: %s", __FILE__, __LINE__, UpnpGetErrorMessage( res ) ); */ goto browseActionCleanup; }

    res = UpnpAddToAction( &action, "Browse", serviceType, "BrowseFlag", BrowseFlag );
    if ( res != UPNP_E_SUCCESS ) { /* msg_Dbg( _cookie->serviceDiscovery, "%s:%d: ERROR: %s", __FILE__, __LINE__, UpnpGetErrorMessage( res ) ); */ goto browseActionCleanup; }

    res = UpnpAddToAction( &action, "Browse", serviceType, "Filter", Filter );
    if ( res != UPNP_E_SUCCESS ) { /* msg_Dbg( _cookie->serviceDiscovery, "%s:%d: ERROR: %s", __FILE__, __LINE__, UpnpGetErrorMessage( res ) ); */ goto browseActionCleanup; }

    res = UpnpAddToAction( &action, "Browse", serviceType, "StartingIndex", StartingIndex );
    if ( res != UPNP_E_SUCCESS ) { /* msg_Dbg( _cookie->serviceDiscovery, "%s:%d: ERROR: %s", __FILE__, __LINE__, UpnpGetErrorMessage( res ) ); */ goto browseActionCleanup; }

    res = UpnpAddToAction( &action, "Browse", serviceType, "RequestedCount", RequestedCount );
    if ( res != UPNP_E_SUCCESS ) { /* msg_Dbg( _cookie->serviceDiscovery, "%s:%d: ERROR: %s", __FILE__, __LINE__, UpnpGetErrorMessage( res ) ); */ goto browseActionCleanup; }

    res = UpnpAddToAction( &action, "Browse", serviceType, "SortCriteria", SortCriteria );
    if ( res != UPNP_E_SUCCESS ) { /* msg_Dbg( _cookie->serviceDiscovery, "%s:%d: ERROR: %s", __FILE__, __LINE__, UpnpGetErrorMessage( res ) ); */ goto browseActionCleanup; }
  
    res = UpnpSendAction( _cookie->clientHandle, 
			  url,
			  CONTENT_DIRECTORY_SERVICE_TYPE, 
			  0, 
			  action, 
			  &response );
    if ( res != UPNP_E_SUCCESS ) 
    { 
	msg_Dbg( _cookie->serviceDiscovery, "%s:%d: ERROR: %s", __FILE__, __LINE__, UpnpGetErrorMessage( res ) ); 
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

    if ( _contents ) 
    {
	vlc_mutex_lock( &_cookie->serviceDiscovery->p_sys->p_playlist->object_lock );

	playlist_NodeEmpty( _cookie->serviceDiscovery->p_sys->p_playlist, 
			   _playlistNode,
			   true );

	vlc_mutex_unlock( &_cookie->serviceDiscovery->p_sys->p_playlist->object_lock );
    
	delete _contents;
    }

    _contents = root;
    _contents->setPlaylistNode( _playlistNode );

    _buildPlaylist( _contents );
}

bool MediaServer::_fetchContents( Container* parent ) 
{
    if (!parent) { msg_Dbg( _cookie->serviceDiscovery, "%s:%d: parent==NULL", __FILE__, __LINE__ ); return false; }

    IXML_Document* response = _browseAction( parent->getObjectID(), "BrowseDirectChildren", "*", "0", "0", "" );
    if ( !response ) { msg_Dbg( _cookie->serviceDiscovery, "%s:%d: ERROR!", __FILE__, __LINE__ ); return false; }

    IXML_Document* result = parseBrowseResult( response );
    ixmlDocument_free( response );
    if ( !result ) { msg_Dbg( _cookie->serviceDiscovery, "%s:%d: ERROR!", __FILE__, __LINE__ ); return false; }

    IXML_NodeList* containerNodeList = ixmlDocument_getElementsByTagName( result, "container" );
    if ( containerNodeList ) 
    {
	for ( unsigned int i = 0; i < ixmlNodeList_length( containerNodeList ); i++ ) 
	{
      	    IXML_Element* containerElement = ( IXML_Element* )ixmlNodeList_item( containerNodeList, i );
      
	    const char* objectID = ixmlElement_getAttribute( containerElement, "id" );
	    if ( !objectID ) continue;
      
	    const char* childCountStr = ixmlElement_getAttribute( containerElement, "childCount" );
	    if ( !childCountStr ) continue;
	    int childCount = atoi( childCountStr );
      
	    const char* title = xml_getChildElementValue( containerElement, "dc:title" );
	    if ( !title ) continue;
      
	    const char* resource = xml_getChildElementValue( containerElement, "res" );

	    if ( resource && childCount < 1 ) 
	    { 
		Item* item = new Item( parent, objectID, title, resource );
		parent->addItem( item );
	    } 
	    else 
	    {
		Container* container = new Container( parent, objectID, title );
		parent->addContainer( container );

		if ( childCount > 0 ) _fetchContents( container );
	    }
	}

	ixmlNodeList_free( containerNodeList );
    }

    IXML_NodeList* itemNodeList = ixmlDocument_getElementsByTagName( result, "item" );
    if ( itemNodeList ) 
    {
    	for ( unsigned int i = 0; i < ixmlNodeList_length( itemNodeList ); i++ ) 
	{
	    IXML_Element* itemElement = ( IXML_Element* )ixmlNodeList_item( itemNodeList, i );
	
	    const char* objectID = ixmlElement_getAttribute( itemElement, "id" );
	    if ( !objectID ) continue;
      
	    const char* title = xml_getChildElementValue( itemElement, "dc:title" );
	    if ( !title ) continue;
      
	    const char* resource = xml_getChildElementValue( itemElement, "res" );
	    if ( !resource ) continue;
      
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
	playlist_item_t* parentNode = parent->getPlaylistNode();

	char* title = strdup( container->getTitle() );
	playlist_item_t* node = playlist_NodeCreate( _cookie->serviceDiscovery->p_sys->p_playlist,
						     VIEW_CATEGORY,
						     title,
						     parentNode );
	free( title );
	
	container->setPlaylistNode( node );
	_buildPlaylist( container );
    }

    for ( unsigned int i = 0; i < parent->getNumItems(); i++ ) 
    {
	Item* item = parent->getItem( i );
	playlist_item_t* parentNode = parent->getPlaylistNode();

	playlist_item_t* node = playlist_ItemNew( _cookie->serviceDiscovery, 
						 item->getResource(), 
						 item->getTitle() );
    
	playlist_NodeAddItem( _cookie->serviceDiscovery->p_sys->p_playlist, 
			     node, 
			     VIEW_CATEGORY,
			     parentNode, PLAYLIST_APPEND, PLAYLIST_END );

	item->setPlaylistNode( node );
    }
}

void MediaServer::setPlaylistNode( playlist_item_t* playlistNode ) 
{
    _playlistNode = playlistNode;
}

bool MediaServer::compareSID( const char* sid ) 
{
    return ( strncmp( _subscriptionID, sid, sizeof( Upnp_SID ) ) == 0 );
}


// MediaServerList...

MediaServerList::MediaServerList( Cookie* cookie ) 
{
    _cookie = cookie;
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
    if ( getServer( s->getUDN() ) != 0 ) return false;

    msg_Dbg( _cookie->serviceDiscovery, "Adding server '%s'", s->getFriendlyName() );

    _list.push_back( s );

    char* name = strdup( s->getFriendlyName() );
    playlist_item_t* node = playlist_NodeCreate( _cookie->serviceDiscovery->p_sys->p_playlist,
						VIEW_CATEGORY,
						name,
						_cookie->serviceDiscovery->p_sys->p_node );
    free( name );
    s->setPlaylistNode( node );

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

    msg_Dbg( _cookie->serviceDiscovery, "Removing server '%s'", server->getFriendlyName() );  

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

    _playlistNode = 0;
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

void Item::setPlaylistNode( playlist_item_t* node ) 
{ 
    _playlistNode = node; 
}

playlist_item_t* Item::getPlaylistNode() const 
{ 
    return _playlistNode; 
}


// Container...

Container::Container( Container* parent, const char* objectID, const char* title ) 
{
    _parent = parent;
    
    _objectID = objectID;
    _title = title;

    _playlistNode = 0;
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

void Container::setPlaylistNode( playlist_item_t* node ) 
{ 
    _playlistNode = node; 
}

playlist_item_t* Container::getPlaylistNode() const 
{ 
    return _playlistNode; 
}
