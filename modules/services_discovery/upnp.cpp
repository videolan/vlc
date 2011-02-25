/*****************************************************************************
 * Upnp.cpp :  UPnP discovery module (libupnp)
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
*/
#undef PACKAGE_NAME
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "upnp.hpp"

#include <vlc_plugin.h>
#include <vlc_services_discovery.h>

#include <assert.h>

// Constants
const char* MEDIA_SERVER_DEVICE_TYPE = "urn:schemas-upnp-org:device:MediaServer:1";
const char* CONTENT_DIRECTORY_SERVICE_TYPE = "urn:schemas-upnp-org:service:ContentDirectory:1";

// VLC handle
struct services_discovery_sys_t
{
    UpnpClient_Handle client_handle;
    MediaServerList* p_server_list;
    vlc_mutex_t callback_lock;
};

// VLC callback prototypes
static int Open( vlc_object_t* );
static void Close( vlc_object_t* );
VLC_SD_PROBE_HELPER("upnp", "Universal Plug'n'Play", SD_CAT_LAN)

// Module descriptor

vlc_module_begin();
    set_shortname( "UPnP" );
    set_description( N_( "Universal Plug'n'Play" ) );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );
    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

    VLC_SD_PROBE_SUBMODULE
vlc_module_end();


// More prototypes...

static int Callback( Upnp_EventType event_type, void* p_event, void* p_user_data );

const char* xml_getChildElementValue( IXML_Element* p_parent,
                                      const char*   psz_tag_name );

IXML_Document* parseBrowseResult( IXML_Document* p_doc );


// VLC callbacks...

static int Open( vlc_object_t *p_this )
{
    int i_res;
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = ( services_discovery_sys_t * )
            calloc( 1, sizeof( services_discovery_sys_t ) );

    if(!(p_sd->p_sys = p_sys))
        return VLC_ENOMEM;

    i_res = UpnpInit( 0, 0 );
    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "%s", UpnpGetErrorMessage( i_res ) );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->p_server_list = new MediaServerList( p_sd );
    vlc_mutex_init( &p_sys->callback_lock );

    i_res = UpnpRegisterClient( Callback, p_sd, &p_sys->client_handle );
    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "%s", UpnpGetErrorMessage( i_res ) );
        Close( (vlc_object_t*) p_sd );
        return VLC_EGENERIC;
    }

    i_res = UpnpSearchAsync( p_sys->client_handle, 5,
            MEDIA_SERVER_DEVICE_TYPE, p_sd );

    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "%s", UpnpGetErrorMessage( i_res ) );
        Close( (vlc_object_t*) p_sd );
        return VLC_EGENERIC;
    }

    i_res = UpnpSetMaxContentLength( 262144 );
    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "%s", UpnpGetErrorMessage( i_res ) );
        Close( (vlc_object_t*) p_sd );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;

    UpnpFinish();
    delete p_sd->p_sys->p_server_list;
    vlc_mutex_destroy( &p_sd->p_sys->callback_lock );

    free( p_sd->p_sys );
}

// XML utility functions:

// Returns the value of a child element, or 0 on error
const char* xml_getChildElementValue( IXML_Element* p_parent,
                                      const char*   psz_tag_name_ )
{
    if ( !p_parent ) return 0;
    if ( !psz_tag_name_ ) return 0;

    char* psz_tag_name = strdup( psz_tag_name_ );
    IXML_NodeList* p_node_list = ixmlElement_getElementsByTagName( p_parent, psz_tag_name );
    free( psz_tag_name );
    if ( !p_node_list ) return 0;

    IXML_Node* p_element = ixmlNodeList_item( p_node_list, 0 );
    ixmlNodeList_free( p_node_list );
    if ( !p_element ) return 0;

    IXML_Node* p_text_node = ixmlNode_getFirstChild( p_element );
    if ( !p_text_node ) return 0;

    return ixmlNode_getNodeValue( p_text_node );
}

// Extracts the result document from a SOAP response
IXML_Document* parseBrowseResult( IXML_Document* p_doc )
{
    ixmlRelaxParser(1);

    if ( !p_doc ) return 0;

    IXML_NodeList* p_result_list = ixmlDocument_getElementsByTagName( p_doc,
                                                                   "Result" );

    if ( !p_result_list ) return 0;

    IXML_Node* p_result_node = ixmlNodeList_item( p_result_list, 0 );

    ixmlNodeList_free( p_result_list );

    if ( !p_result_node ) return 0;

    IXML_Node* p_text_node = ixmlNode_getFirstChild( p_result_node );
    if ( !p_text_node ) return 0;

    const char* psz_result_string = ixmlNode_getNodeValue( p_text_node );
    char* psz_result_xml = strdup( psz_result_string );

    IXML_Document* p_browse_doc = ixmlParseBuffer( psz_result_xml );

    free( psz_result_xml );

    return p_browse_doc;
}


// Handles all UPnP events
static int Callback( Upnp_EventType event_type, void* p_event, void* p_user_data )
{
    services_discovery_t* p_sd = ( services_discovery_t* ) p_user_data;
    services_discovery_sys_t* p_sys = p_sd->p_sys;
    vlc_mutex_locker locker( &p_sys->callback_lock );

    switch( event_type )
    {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        struct Upnp_Discovery* p_discovery = ( struct Upnp_Discovery* )p_event;

        IXML_Document *p_description_doc = 0;

        int i_res;
        i_res = UpnpDownloadXmlDoc( p_discovery->Location, &p_description_doc );
        if ( i_res != UPNP_E_SUCCESS )
        {
            msg_Dbg( p_sd,
                    "%s:%d: Could not download device description!",
                    __FILE__, __LINE__ );
            return i_res;
        }

        MediaServer::parseDeviceDescription( p_description_doc,
                p_discovery->Location, p_sd );

        ixmlDocument_free( p_description_doc );
    }
    break;

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        struct Upnp_Discovery* p_discovery = ( struct Upnp_Discovery* )p_event;

        p_sys->p_server_list->removeServer( p_discovery->DeviceId );
    }
    break;

    case UPNP_EVENT_RECEIVED:
    {
        Upnp_Event* p_e = ( Upnp_Event* )p_event;

        MediaServer* p_server = p_sys->p_server_list->getServerBySID( p_e->Sid );
        if ( p_server ) p_server->fetchContents();
    }
    break;

    case UPNP_EVENT_AUTORENEWAL_FAILED:
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
    {
        // Re-subscribe...

        Upnp_Event_Subscribe* p_s = ( Upnp_Event_Subscribe* )p_event;

        MediaServer* p_server = p_sys->p_server_list->getServerBySID( p_s->Sid );
        if ( p_server ) p_server->subscribeToContentDirectory();
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
            __FILE__, __LINE__, event_type );
    break;
    }

    return UPNP_E_SUCCESS;
}


// Class implementations...

// MediaServer...

void MediaServer::parseDeviceDescription( IXML_Document* p_doc,
                                          const char*    p_location,
                                          services_discovery_t* p_sd )
{
    if ( !p_doc )
    {
        msg_Dbg( p_sd, "%s:%d: NULL", __FILE__, __LINE__ );
        return;
    }

    if ( !p_location )
    {
        msg_Dbg( p_sd, "%s:%d: NULL", __FILE__, __LINE__ );
        return;
    }

    const char* psz_base_url = p_location;

    // Try to extract baseURL

    IXML_NodeList* p_url_list = ixmlDocument_getElementsByTagName( p_doc, "baseURL" );
    if ( !p_url_list )
    {

        if ( IXML_Node* p_url_node = ixmlNodeList_item( p_url_list, 0 ) )
        {
            IXML_Node* p_text_node = ixmlNode_getFirstChild( p_url_node );
            if ( p_text_node ) psz_base_url = ixmlNode_getNodeValue( p_text_node );
        }

        ixmlNodeList_free( p_url_list );
    }

    // Get devices

    IXML_NodeList* p_device_list =
                ixmlDocument_getElementsByTagName( p_doc, "device" );

    if ( p_device_list )
    {
        for ( unsigned int i = 0; i < ixmlNodeList_length( p_device_list ); i++ )
        {
            IXML_Element* p_device_element =
                   ( IXML_Element* ) ixmlNodeList_item( p_device_list, i );

            const char* psz_device_type = xml_getChildElementValue( p_device_element,
                                                               "deviceType" );
            if ( !psz_device_type )
            {
                msg_Dbg( p_sd,
                        "%s:%d: no deviceType!",
                        __FILE__, __LINE__ );
                continue;
            }

            if ( strcmp( MEDIA_SERVER_DEVICE_TYPE, psz_device_type ) != 0 )
                continue;

            const char* psz_udn = xml_getChildElementValue( p_device_element, "UDN" );
            if ( !psz_udn )
            {
                msg_Dbg( p_sd, "%s:%d: no UDN!",
                        __FILE__, __LINE__ );
                continue;
            }

            if ( p_sd->p_sys->p_server_list->getServer( psz_udn ) != 0 )
                continue;

            const char* psz_friendly_name =
                       xml_getChildElementValue( p_device_element,
                                                 "friendlyName" );

            if ( !psz_friendly_name )
            {
                msg_Dbg( p_sd, "%s:%d: no friendlyName!", __FILE__, __LINE__ );
                continue;
            }

            MediaServer* p_server = new MediaServer( psz_udn, psz_friendly_name, p_sd );

            if ( !p_sd->p_sys->p_server_list->addServer( p_server ) )
            {

                delete p_server;
                p_server = 0;
                continue;
            }

            // Check for ContentDirectory service...
            IXML_NodeList* p_service_list =
                       ixmlElement_getElementsByTagName( p_device_element,
                                                         "service" );
            if ( p_service_list )
            {
                for ( unsigned int j = 0;
                      j < ixmlNodeList_length( p_service_list ); j++ )
                {
                    IXML_Element* p_service_element =
                        ( IXML_Element* ) ixmlNodeList_item( p_service_list, j );

                    const char* psz_service_type =
                        xml_getChildElementValue( p_service_element,
                                                  "serviceType" );
                    if ( !psz_service_type )
                        continue;

                    if ( strcmp( CONTENT_DIRECTORY_SERVICE_TYPE,
                                psz_service_type ) != 0 )
                        continue;

                    const char* psz_event_sub_url =
                        xml_getChildElementValue( p_service_element,
                                                  "eventSubURL" );
                    if ( !psz_event_sub_url )
                        continue;

                    const char* psz_control_url =
                        xml_getChildElementValue( p_service_element,
                                                  "controlURL" );
                    if ( !psz_control_url )
                        continue;

                    // Try to subscribe to ContentDirectory service

                    char* psz_url = ( char* ) malloc( strlen( psz_base_url ) +
                            strlen( psz_event_sub_url ) + 1 );
                    if ( psz_url )
                    {
                        char* psz_s1 = strdup( psz_base_url );
                        char* psz_s2 = strdup( psz_event_sub_url );

                        if ( UpnpResolveURL( psz_s1, psz_s2, psz_url ) ==
                                UPNP_E_SUCCESS )
                        {
                            p_server->setContentDirectoryEventURL( psz_url );
                            p_server->subscribeToContentDirectory();
                        }

                        free( psz_s1 );
                        free( psz_s2 );
                        free( psz_url );
                    }

                    // Try to browse content directory...

                    psz_url = ( char* ) malloc( strlen( psz_base_url ) +
                            strlen( psz_control_url ) + 1 );
                    if ( psz_url )
                    {
                        char* psz_s1 = strdup( psz_base_url );
                        char* psz_s2 = strdup( psz_control_url );

                        if ( UpnpResolveURL( psz_s1, psz_s2, psz_url ) ==
                                UPNP_E_SUCCESS )
                        {
                            p_server->setContentDirectoryControlURL( psz_url );
                            p_server->fetchContents();
                        }

                        free( psz_s1 );
                        free( psz_s2 );
                        free( psz_url );
                    }
               }
               ixmlNodeList_free( p_service_list );
           }
       }
       ixmlNodeList_free( p_device_list );
    }
}

MediaServer::MediaServer( const char* psz_udn,
                          const char* psz_friendly_name,
                          services_discovery_t* p_sd )
{
    _p_sd = p_sd;

    _UDN = psz_udn;
    _friendlyName = psz_friendly_name;

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

void MediaServer::setContentDirectoryEventURL( const char* psz_url )
{
    _contentDirectoryEventURL = psz_url;
}

const char* MediaServer::getContentDirectoryEventURL() const
{
    const char* s =  _contentDirectoryEventURL.c_str();
    return s;
}

void MediaServer::setContentDirectoryControlURL( const char* psz_url )
{
    _contentDirectoryControlURL = psz_url;
}

const char* MediaServer::getContentDirectoryControlURL() const
{
    return _contentDirectoryControlURL.c_str();
}

void MediaServer::subscribeToContentDirectory()
{
    const char* psz_url = getContentDirectoryEventURL();
    if ( !psz_url || strcmp( psz_url, "" ) == 0 )
    {
        msg_Dbg( _p_sd, "No subscription url set!" );
        return;
    }

    int i_timeout = 1810;
    Upnp_SID sid;

    int i_res = UpnpSubscribe( _p_sd->p_sys->client_handle, psz_url, &i_timeout, sid );

    if ( i_res == UPNP_E_SUCCESS )
    {
        _subscriptionTimeOut = i_timeout;
        memcpy( _subscriptionID, sid, sizeof( Upnp_SID ) );
    }
    else
    {
        msg_Dbg( _p_sd,
                "%s:%d: WARNING: '%s': %s", __FILE__, __LINE__,
                getFriendlyName(), UpnpGetErrorMessage( i_res ) );
    }
}

IXML_Document* MediaServer::_browseAction( const char* psz_object_id_,
                                           const char* psz_browser_flag_,
                                           const char* psz_filter_,
                                           const char* psz_starting_index_,
                                           const char* psz_requested_count_,
                                           const char* psz_sort_criteria_ )
{
    IXML_Document* p_action = 0;
    IXML_Document* p_response = 0;
    const char* psz_url = getContentDirectoryControlURL();

    if ( !psz_url || strcmp( psz_url, "" ) == 0 )
    {
        msg_Dbg( _p_sd, "No subscription url set!" );
        return 0;
    }

    char* psz_object_id = strdup( psz_object_id_ );
    char* psz_browse_flag = strdup( psz_browser_flag_ );
    char* psz_filter = strdup( psz_filter_ );
    char* psz_starting_index = strdup( psz_starting_index_ );
    char* psz_requested_count = strdup( psz_requested_count_ );
    char* psz_sort_criteria = strdup( psz_sort_criteria_ );
    char* psz_service_type = strdup( CONTENT_DIRECTORY_SERVICE_TYPE );

    int i_res;

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "ObjectID", psz_object_id );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
                 "%s:%d: ERROR: %s", __FILE__, __LINE__,
                 UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "BrowseFlag", psz_browse_flag );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
             "%s:%d: ERROR: %s", __FILE__, __LINE__,
             UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "Filter", psz_filter );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
             "%s:%d: ERROR: %s", __FILE__, __LINE__,
             UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "StartingIndex", psz_starting_index );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
             "%s:%d: ERROR: %s", __FILE__, __LINE__,
             UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "RequestedCount", psz_requested_count );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
                "%s:%d: ERROR: %s", __FILE__, __LINE__,
                UpnpGetErrorMessage( i_res ) ); goto browseActionCleanup; }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "SortCriteria", psz_sort_criteria );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
             "%s:%d: ERROR: %s", __FILE__, __LINE__,
             UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpSendAction( _p_sd->p_sys->client_handle,
              psz_url,
              CONTENT_DIRECTORY_SERVICE_TYPE,
              0,
              p_action,
              &p_response );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd,
                "%s:%d: ERROR: %s when trying the send() action with URL: %s",
                __FILE__, __LINE__,
                UpnpGetErrorMessage( i_res ), psz_url );

        ixmlDocument_free( p_response );
        p_response = 0;
    }

 browseActionCleanup:

    free( psz_object_id );
    free( psz_browse_flag );
    free( psz_filter );
    free( psz_starting_index );
    free( psz_requested_count );
    free( psz_sort_criteria );

    free( psz_service_type );

    ixmlDocument_free( p_action );
    return p_response;
}

void MediaServer::fetchContents()
{
    Container* root = new Container( 0, "0", getFriendlyName() );
    _fetchContents( root );

    _contents = root;
    _contents->setInputItem( _inputItem );

    _buildPlaylist( _contents, NULL );
}

bool MediaServer::_fetchContents( Container* p_parent )
{
    if (!p_parent)
    {
        msg_Dbg( _p_sd,
                "%s:%d: parent==NULL", __FILE__, __LINE__ );
        return false;
    }

    IXML_Document* p_response = _browseAction( p_parent->getObjectID(),
                                      "BrowseDirectChildren",
                                      "*", "0", "0", "" );
    if ( !p_response )
    {
        msg_Dbg( _p_sd,
                "%s:%d: ERROR! No response from browse() action",
                __FILE__, __LINE__ );
        return false;
    }

    IXML_Document* result = parseBrowseResult( p_response );
    ixmlDocument_free( p_response );

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
                Item* item = new Item( p_parent, objectID, title, resource );
                p_parent->addItem( item );
            }

            else
            {
                Container* container = new Container( p_parent, objectID, title );
                p_parent->addContainer( container );

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

            Item* item = new Item( p_parent, objectID, title, resource );
            p_parent->addItem( item );
        }
        ixmlNodeList_free( itemNodeList );
    }

    ixmlDocument_free( result );
    return true;
}

void MediaServer::_buildPlaylist( Container* p_parent, input_item_node_t *p_input_node )
{
    bool send = p_input_node == NULL;
    if( send )
        p_input_node = input_item_node_Create( p_parent->getInputItem() );

    for ( unsigned int i = 0; i < p_parent->getNumContainers(); i++ )
    {
        Container* container = p_parent->getContainer( i );

        input_item_t* p_input_item = input_item_New( _p_sd, "vlc://nop", container->getTitle() );
        input_item_node_t *p_new_node =
            input_item_node_AppendItem( p_input_node, p_input_item );

        container->setInputItem( p_input_item );
        _buildPlaylist( container, p_new_node );
    }

    for ( unsigned int i = 0; i < p_parent->getNumItems(); i++ )
    {
        Item* item = p_parent->getItem( i );

        input_item_t* p_input_item = input_item_New( _p_sd,
                                               item->getResource(),
                                               item->getTitle() );
        assert( p_input_item );
        input_item_node_AppendItem( p_input_node, p_input_item );
        item->setInputItem( p_input_item );
    }

    if( send )
        input_item_node_PostAndDelete( p_input_node );
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

MediaServer* MediaServerList::getServer( const char* psz_udn )
{
    MediaServer* result = 0;

    for ( unsigned int i = 0; i < _list.size(); i++ )
    {
        if( strcmp( psz_udn, _list[i]->getUDN() ) == 0 )
        {
            result = _list[i];
            break;
        }
    }

    return result;
}

MediaServer* MediaServerList::getServerBySID( const char* sid )
{
    MediaServer* p_server = 0;

    for ( unsigned int i = 0; i < _list.size(); i++ )
    {
        if ( _list[i]->compareSID( sid ) )
        {
            p_server = _list[i];
            break;
        }
    }

    return p_server;
}

void MediaServerList::removeServer( const char* psz_udn )
{
    MediaServer* p_server = getServer( psz_udn );
    if ( !p_server ) return;

    msg_Dbg( _p_sd,
            "Removing server '%s'", p_server->getFriendlyName() );

    std::vector<MediaServer*>::iterator it;
    for ( it = _list.begin(); it != _list.end(); ++it )
    {
        if ( *it == p_server )
        {
            _list.erase( it );
            delete p_server;
            break;
        }
    }
}


// Item...

Item::Item( Container* p_parent, const char* psz_object_id, const char* psz_title,
	   const char* psz_resource )
{
    _parent = p_parent;

    _objectID = psz_object_id;
    _title = psz_title;
    _resource = psz_resource;

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

Container::Container( Container*  p_parent,
                      const char* psz_object_id,
                      const char* psz_title )
{
    _parent = p_parent;

    _objectID = psz_object_id;
    _title = psz_title;

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

void Container::addContainer( Container* p_container )
{
    _containers.push_back( p_container );
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

Item* Container::getItem( unsigned int i_index ) const
{
    if ( i_index < _items.size() ) return _items[i_index];
    return 0;
}

Container* Container::getContainer( unsigned int i_index ) const
{
    if ( i_index < _containers.size() ) return _containers[i_index];
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
