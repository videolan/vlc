/*****************************************************************************
 * upnp.cpp :  UPnP discovery module (libupnp)
 *****************************************************************************
 * Copyright (C) 2004-2011 the VideoLAN team
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

#define __STDC_CONSTANT_MACROS 1

#undef PACKAGE_NAME
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "upnp.hpp"

#include <vlc_plugin.h>
#include <vlc_services_discovery.h>

#include <assert.h>
#include <limits.h>

/*
 * Constants
*/
const char* MEDIA_SERVER_DEVICE_TYPE = "urn:schemas-upnp-org:device:MediaServer:1";
const char* CONTENT_DIRECTORY_SERVICE_TYPE = "urn:schemas-upnp-org:service:ContentDirectory:1";

/*
 * VLC handle
 */
struct services_discovery_sys_t
{
    UpnpClient_Handle client_handle;
    MediaServerList* p_server_list;
    vlc_mutex_t callback_lock;
};

/*
 * VLC callback prototypes
 */
static int Open( vlc_object_t* );
static void Close( vlc_object_t* );
VLC_SD_PROBE_HELPER( "upnp", "Universal Plug'n'Play", SD_CAT_LAN )

/*
 * Module descriptor
 */
vlc_module_begin();
    set_shortname( "UPnP" );
    set_description( N_( "Universal Plug'n'Play" ) );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );
    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

    VLC_SD_PROBE_SUBMODULE
vlc_module_end();

/*
 * Local prototypes
 */
static int Callback( Upnp_EventType event_type, void* p_event, void* p_user_data );

const char* xml_getChildElementValue( IXML_Element* p_parent,
                                      const char*   psz_tag_name );

const char* xml_getChildElementValue( IXML_Document* p_doc,
                                      const char*    psz_tag_name );

const char* xml_getChildElementAttributeValue( IXML_Element* p_parent,
                                        const char* psz_tag_name,
                                        const char* psz_attribute );

int xml_getNumber( IXML_Document* p_doc,
                   const char*    psz_tag_name );

IXML_Document* parseBrowseResult( IXML_Document* p_doc );

/*
 * Initializes UPNP instance.
 */
static int Open( vlc_object_t *p_this )
{
    int i_res;
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = ( services_discovery_sys_t * )
            calloc( 1, sizeof( services_discovery_sys_t ) );

    if( !( p_sd->p_sys = p_sys ) )
        return VLC_ENOMEM;

#ifdef UPNP_ENABLE_IPV6
    char* psz_miface;
    psz_miface = var_InheritString( p_sd, "miface" );
    msg_Info( p_sd, "Initializing libupnp on '%s' interface", psz_miface );
    i_res = UpnpInit2( psz_miface, 0 );
    free( psz_miface );
#else
    /* If UpnpInit2 isnt available, initialize on first IPv4-capable interface */
    i_res = UpnpInit( 0, 0 );
#endif
    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "Initialization failed: %s", UpnpGetErrorMessage( i_res ) );
        free( p_sys );
        return VLC_EGENERIC;
    }

    ixmlRelaxParser( 1 );

    p_sys->p_server_list = new MediaServerList( p_sd );
    vlc_mutex_init( &p_sys->callback_lock );

    /* Register a control point */
    i_res = UpnpRegisterClient( Callback, p_sd, &p_sys->client_handle );
    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "Client registration failed: %s", UpnpGetErrorMessage( i_res ) );
        Close( (vlc_object_t*) p_sd );
        return VLC_EGENERIC;
    }

    /* Search for media servers */
    i_res = UpnpSearchAsync( p_sys->client_handle, 5,
            MEDIA_SERVER_DEVICE_TYPE, p_sd );
    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "Error sending search request: %s", UpnpGetErrorMessage( i_res ) );
        Close( (vlc_object_t*) p_sd );
        return VLC_EGENERIC;
    }

    /* libupnp does not treat a maximum content length of 0 as unlimited
     * until 64dedf (~ pupnp v1.6.7) and provides no sane way to discriminate
     * between versions */
    if( (i_res = UpnpSetMaxContentLength( INT_MAX )) != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "Failed to set maximum content length: %s",
                UpnpGetErrorMessage( i_res ));

        Close( (vlc_object_t*) p_sd );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*
 * Releases resources.
 */
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;

    UpnpUnRegisterClient( p_sd->p_sys->client_handle );
    UpnpFinish();

    delete p_sd->p_sys->p_server_list;
    vlc_mutex_destroy( &p_sd->p_sys->callback_lock );

    free( p_sd->p_sys );
}

/* XML utility functions */

/*
 * Returns the value of a child element, or NULL on error
 */
const char* xml_getChildElementValue( IXML_Element* p_parent,
                                      const char*   psz_tag_name )
{
    assert( p_parent );
    assert( psz_tag_name );

    IXML_NodeList* p_node_list;
    p_node_list = ixmlElement_getElementsByTagName( p_parent, psz_tag_name );
    if ( !p_node_list ) return NULL;

    IXML_Node* p_element = ixmlNodeList_item( p_node_list, 0 );
    ixmlNodeList_free( p_node_list );
    if ( !p_element )   return NULL;

    IXML_Node* p_text_node = ixmlNode_getFirstChild( p_element );
    if ( !p_text_node ) return NULL;

    return ixmlNode_getNodeValue( p_text_node );
}

/*
 * Returns the value of a child element's attribute, or NULL on error
 */
const char* xml_getChildElementAttributeValue( IXML_Element* p_parent,
                                        const char* psz_tag_name,
                                        const char* psz_attribute )
{
    assert( p_parent );
    assert( psz_tag_name );
    assert( psz_attribute );

    IXML_NodeList* p_node_list;
    p_node_list = ixmlElement_getElementsByTagName( p_parent, psz_tag_name );
    if ( !p_node_list )   return NULL;

    IXML_Node* p_element = ixmlNodeList_item( p_node_list, 0 );
    ixmlNodeList_free( p_node_list );
    if ( !p_element )     return NULL;

    return ixmlElement_getAttribute( (IXML_Element*) p_element, psz_attribute );
}

/*
 * Returns the value of a child element, or NULL on error
 */
const char* xml_getChildElementValue( IXML_Document*  p_doc,
                                      const char*     psz_tag_name )
{
    assert( p_doc );
    assert( psz_tag_name );

    IXML_NodeList* p_node_list;
    p_node_list = ixmlDocument_getElementsByTagName( p_doc, psz_tag_name );
    if ( !p_node_list )  return NULL;

    IXML_Node* p_element = ixmlNodeList_item( p_node_list, 0 );
    ixmlNodeList_free( p_node_list );
    if ( !p_element )    return NULL;

    IXML_Node* p_text_node = ixmlNode_getFirstChild( p_element );
    if ( !p_text_node )  return NULL;

    return ixmlNode_getNodeValue( p_text_node );
}

/*
 * Extracts the result document from a SOAP response
 */
IXML_Document* parseBrowseResult( IXML_Document* p_doc )
{
    assert( p_doc );

    /* Missing namespaces confuse the ixml parser. This is a very ugly
     * hack but it is needeed until devices start sending valid XML.
     *
     * It works that way:
     *
     * The DIDL document is extracted from the Result tag, then wrapped into
     * a valid XML header and a new root tag which contains missing namespace
     * definitions so the ixml parser understands it.
     *
     * If you know of a better workaround, please oh please fix it */
    const char* psz_xml_result_fmt = "<?xml version=\"1.0\" ?>"
        "<Result xmlns:sec=\"urn:samsung:metadata:2009\">%s</Result>";

    char* psz_xml_result_string = NULL;
    const char* psz_raw_didl = xml_getChildElementValue( p_doc, "Result" );

    if( !psz_raw_didl )
        return NULL;

    if( -1 == asprintf( &psz_xml_result_string,
                         psz_xml_result_fmt,
                         psz_raw_didl) )
        return NULL;


    IXML_Document* p_result_doc = ixmlParseBuffer( psz_xml_result_string );
    free( psz_xml_result_string );

    if( !p_result_doc )
        return NULL;

    IXML_NodeList *p_elems = ixmlDocument_getElementsByTagName( p_result_doc,
                                                                "DIDL-Lite" );

    IXML_Node *p_node = ixmlNodeList_item( p_elems, 0 );
    ixmlNodeList_free( p_elems );

    return (IXML_Document*)p_node;
}

/*
 * Get the number value from a SOAP response
 */
int xml_getNumber( IXML_Document* p_doc,
                   const char* psz_tag_name )
{
    assert( p_doc );
    assert( psz_tag_name );

    const char* psz = xml_getChildElementValue( p_doc, psz_tag_name );

    if( !psz )
        return 0;

    char *psz_end;
    long l = strtol( psz, &psz_end, 10 );

    if( *psz_end || l < 0 || l > INT_MAX )
        return 0;

    return (int)l;
}

/*
 * Handles all UPnP events
 */
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
            msg_Warn( p_sd, "Could not download device description! "
                            "Fetching data from %s failed: %s",
                            p_discovery->Location, UpnpGetErrorMessage( i_res ) );
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
        /* Re-subscribe. */

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
        msg_Err( p_sd, "Unhandled event, please report ( type=%d )", event_type );
        break;
    }

    return UPNP_E_SUCCESS;
}


/*
 * Local class implementations.
 */

/*
 * MediaServer
 */

void MediaServer::parseDeviceDescription( IXML_Document* p_doc,
                                          const char*    p_location,
                                          services_discovery_t* p_sd )
{
    if ( !p_doc )
    {
        msg_Err( p_sd, "Null IXML_Document" );
        return;
    }

    if ( !p_location )
    {
        msg_Err( p_sd, "Null location" );
        return;
    }

    const char* psz_base_url = p_location;

    /* Try to extract baseURL */
    IXML_NodeList* p_url_list = ixmlDocument_getElementsByTagName( p_doc, "URLBase" );
    if ( p_url_list )
    {

        if ( IXML_Node* p_url_node = ixmlNodeList_item( p_url_list, 0 ) )
        {
            IXML_Node* p_text_node = ixmlNode_getFirstChild( p_url_node );
            if ( p_text_node ) psz_base_url = ixmlNode_getNodeValue( p_text_node );
        }

        ixmlNodeList_free( p_url_list );
    }

    /* Get devices */
    IXML_NodeList* p_device_list =
                ixmlDocument_getElementsByTagName( p_doc, "device" );

    if ( p_device_list )
    {
        for ( unsigned int i = 0; i < ixmlNodeList_length( p_device_list ); i++ )
        {
            IXML_Element* p_device_element =
                   ( IXML_Element* ) ixmlNodeList_item( p_device_list, i );

            if( !p_device_element )
                continue;

            const char* psz_device_type =
                xml_getChildElementValue( p_device_element, "deviceType" );

            if ( !psz_device_type )
            {
                msg_Warn( p_sd, "No deviceType found!" );
                continue;
            }

            if ( strncmp( MEDIA_SERVER_DEVICE_TYPE, psz_device_type,
                    strlen( MEDIA_SERVER_DEVICE_TYPE ) - 1 ) != 0 )
                continue;

            const char* psz_udn = xml_getChildElementValue( p_device_element,
                                                            "UDN" );
            if ( !psz_udn )
            {
                msg_Warn( p_sd, "No UDN!" );
                continue;
            }

            /* Check if server is already added */
            if ( p_sd->p_sys->p_server_list->getServer( psz_udn ) != 0 )
            {
                msg_Warn( p_sd, "Server with uuid '%s' already exists.", psz_udn );
                continue;
            }

            const char* psz_friendly_name =
                       xml_getChildElementValue( p_device_element,
                                                 "friendlyName" );

            if ( !psz_friendly_name )
            {
                msg_Dbg( p_sd, "No friendlyName!" );
                continue;
            }

            MediaServer* p_server = new MediaServer( psz_udn,
                    psz_friendly_name, p_sd );

            if ( !p_sd->p_sys->p_server_list->addServer( p_server ) )
            {
                delete p_server;
                p_server = 0;
                continue;
            }

            /* Check for ContentDirectory service. */
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
                    {
                        msg_Warn( p_sd, "No service type found." );
                        continue;
                    }

                    int k = strlen( CONTENT_DIRECTORY_SERVICE_TYPE ) - 1;
                    if ( strncmp( CONTENT_DIRECTORY_SERVICE_TYPE,
                                psz_service_type, k ) != 0 )
                        continue;

		    p_server->_i_content_directory_service_version =
			psz_service_type[k];

                    const char* psz_event_sub_url =
                        xml_getChildElementValue( p_service_element,
                                                  "eventSubURL" );
                    if ( !psz_event_sub_url )
                    {
                        msg_Warn( p_sd, "No event subscription url found." );
                        continue;
                    }

                    const char* psz_control_url =
                        xml_getChildElementValue( p_service_element,
                                                  "controlURL" );
                    if ( !psz_control_url )
                    {
                        msg_Warn( p_sd, "No control url found." );
                        continue;
                    }

                    /* Try to subscribe to ContentDirectory service */

                    char* psz_url = ( char* ) malloc( strlen( psz_base_url ) +
                            strlen( psz_event_sub_url ) + 1 );
                    if ( psz_url )
                    {
                        if ( UpnpResolveURL( psz_base_url, psz_event_sub_url, psz_url ) ==
                                UPNP_E_SUCCESS )
                        {
                            p_server->setContentDirectoryEventURL( psz_url );
                            p_server->subscribeToContentDirectory();
                        }

                        free( psz_url );
                    }

                    /* Try to browse content directory. */

                    psz_url = ( char* ) malloc( strlen( psz_base_url ) +
                            strlen( psz_control_url ) + 1 );
                    if ( psz_url )
                    {
                        if ( UpnpResolveURL( psz_base_url, psz_control_url, psz_url ) ==
                                UPNP_E_SUCCESS )
                        {
                            p_server->setContentDirectoryControlURL( psz_url );
                            p_server->fetchContents();
                        }

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
    _friendly_name = psz_friendly_name;

    _p_contents = NULL;
    _p_input_item = NULL;
    _i_content_directory_service_version = 1;
}

MediaServer::~MediaServer()
{
    delete _p_contents;
}

const char* MediaServer::getUDN() const
{
    return _UDN.c_str();
}

const char* MediaServer::getFriendlyName() const
{
    return _friendly_name.c_str();
}

void MediaServer::setContentDirectoryEventURL( const char* psz_url )
{
    _content_directory_event_url = psz_url;
}

const char* MediaServer::getContentDirectoryEventURL() const
{
    return _content_directory_event_url.c_str();
}

void MediaServer::setContentDirectoryControlURL( const char* psz_url )
{
    _content_directory_control_url = psz_url;
}

const char* MediaServer::getContentDirectoryControlURL() const
{
    return _content_directory_control_url.c_str();
}

/**
 * Subscribes current client handle to Content Directory Service.
 * CDS exports the server shares to clients.
 */
void MediaServer::subscribeToContentDirectory()
{
    const char* psz_url = getContentDirectoryEventURL();
    if ( !psz_url )
    {
        msg_Dbg( _p_sd, "No subscription url set!" );
        return;
    }

    int i_timeout = 1810;
    Upnp_SID sid;

    int i_res = UpnpSubscribe( _p_sd->p_sys->client_handle, psz_url, &i_timeout, sid );

    if ( i_res == UPNP_E_SUCCESS )
    {
        _i_subscription_timeout = i_timeout;
        memcpy( _subscription_id, sid, sizeof( Upnp_SID ) );
    }
    else
    {
        msg_Dbg( _p_sd, "Subscribe failed: '%s': %s",
                getFriendlyName(), UpnpGetErrorMessage( i_res ) );
    }
}
/*
 * Constructs UpnpAction to browse available content.
 */
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

    if ( !psz_url )
    {
        msg_Dbg( _p_sd, "No subscription url set!" );
        return 0;
    }

    char* psz_service_type = strdup( CONTENT_DIRECTORY_SERVICE_TYPE );

    psz_service_type[strlen( psz_service_type ) - 1] =
	_i_content_directory_service_version;

    int i_res;

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "ObjectID", psz_object_id_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd, "AddToAction 'ObjectID' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "BrowseFlag", psz_browser_flag_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd, "AddToAction 'BrowseFlag' failed: %s", 
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "Filter", psz_filter_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd, "AddToAction 'Filter' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "StartingIndex", psz_starting_index_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd, "AddToAction 'StartingIndex' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "RequestedCount", psz_requested_count_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd, "AddToAction 'RequestedCount' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            psz_service_type, "SortCriteria", psz_sort_criteria_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( _p_sd, "AddToAction 'SortCriteria' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpSendAction( _p_sd->p_sys->client_handle,
              psz_url,
              psz_service_type,
              0, /* ignored in SDK, must be NULL */
              p_action,
              &p_response );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( _p_sd, "%s when trying the send() action with URL: %s",
                UpnpGetErrorMessage( i_res ), psz_url );

        ixmlDocument_free( p_response );
        p_response = 0;
    }

browseActionCleanup:

    free( psz_service_type );

    ixmlDocument_free( p_action );
    return p_response;
}

void MediaServer::fetchContents()
{
    /* Delete previous contents to prevent duplicate entries */
    if ( _p_contents )
    {
        delete _p_contents;
        services_discovery_RemoveItem( _p_sd, _p_input_item );
        services_discovery_AddItem( _p_sd, _p_input_item, NULL );
    }

    Container* root = new Container( 0, "0", getFriendlyName() );

    _fetchContents( root, 0 );

    _p_contents = root;
    _p_contents->setInputItem( _p_input_item );

    _buildPlaylist( _p_contents, NULL );
}

/*
 * Fetches and parses the UPNP response
 */
bool MediaServer::_fetchContents( Container* p_parent, int i_offset )
{
    if (!p_parent)
    {
        msg_Err( _p_sd, "No parent" );
        return false;
    }

    char* psz_starting_index;
    if( asprintf( &psz_starting_index, "%d", i_offset ) < 0 )
    {
        msg_Err( _p_sd, "asprintf error:%d", i_offset );
        return false;
    }

    IXML_Document* p_response = _browseAction( p_parent->getObjectID(),
                                      "BrowseDirectChildren",
                                      "id,dc:title,res," /* Filter */
                                      "sec:CaptionInfo,sec:CaptionInfoEx",
                                      psz_starting_index, /* StartingIndex */
                                      "0", /* RequestedCount */
                                      "" /* SortCriteria */
                                      );
    free( psz_starting_index );
    if ( !p_response )
    {
        msg_Err( _p_sd, "No response from browse() action" );
        return false;
    }

    IXML_Document* p_result = parseBrowseResult( p_response );
    int i_number_returned = xml_getNumber( p_response, "NumberReturned" );
    int i_total_matches   = xml_getNumber( p_response , "TotalMatches" );

#ifndef NDEBUG
    msg_Dbg( _p_sd, "i_offset[%d]i_number_returned[%d]_total_matches[%d]\n",
             i_offset, i_number_returned, i_total_matches );
#endif

    ixmlDocument_free( p_response );

    if ( !p_result )
    {
        msg_Err( _p_sd, "browse() response parsing failed" );
        return false;
    }

#ifndef NDEBUG
    msg_Dbg( _p_sd, "Got DIDL document: %s", ixmlPrintDocument( p_result ) );
#endif

    IXML_NodeList* containerNodeList =
                ixmlDocument_getElementsByTagName( p_result, "container" );

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

            const char* title = xml_getChildElementValue( containerElement,
                                                          "dc:title" );

            if ( !title )
                continue;

            Container* container = new Container( p_parent, objectID, title );
            p_parent->addContainer( container );
            _fetchContents( container, 0 );
        }
        ixmlNodeList_free( containerNodeList );
    }

    IXML_NodeList* itemNodeList = ixmlDocument_getElementsByTagName( p_result,
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

            const char* psz_subtitles = xml_getChildElementValue( itemElement,
                    "sec:CaptionInfo" );

            if ( !psz_subtitles )
                psz_subtitles = xml_getChildElementValue( itemElement,
                        "sec:CaptionInfoEx" );

            /* Try to extract all resources in DIDL */
            IXML_NodeList* p_resource_list = ixmlDocument_getElementsByTagName( (IXML_Document*) itemElement, "res" );
            if ( p_resource_list )
            {
                int i_length = ixmlNodeList_length( p_resource_list );
                for ( int i = 0; i < i_length; i++ )
                {
                    mtime_t i_duration = -1;
                    int i_hours, i_minutes, i_seconds;
                    IXML_Element* p_resource = ( IXML_Element* ) ixmlNodeList_item( p_resource_list, i );
                    const char* psz_resource_url = xml_getChildElementValue( p_resource, "res" );
                    if( !psz_resource_url )
                        continue;
                    const char* psz_duration = ixmlElement_getAttribute( p_resource, "duration" );

                    if ( psz_duration )
                    {
                        if( sscanf( psz_duration, "%d:%02d:%02d",
                            &i_hours, &i_minutes, &i_seconds ) )
                            i_duration = INT64_C(1000000) * ( i_hours*3600 +
                                                              i_minutes*60 +
                                                              i_seconds );
                    }

                    Item* item = new Item( p_parent, objectID, title, psz_resource_url, psz_subtitles, i_duration );
                    p_parent->addItem( item );
                }
                ixmlNodeList_free( p_resource_list );
            }
            else continue;
        }
        ixmlNodeList_free( itemNodeList );
    }

    ixmlDocument_free( p_result );

    if( i_offset + i_number_returned < i_total_matches )
        return _fetchContents( p_parent, i_offset + i_number_returned );

    return true;
}

// TODO: Create a permanent fix for the item duplication bug. The current fix
// is essentially only a small hack. Although it fixes the problem, it introduces
// annoying cosmetic issues with the playlist. For example, when the UPnP Server
// rebroadcasts it's directory structure, the VLC Client deletes the old directory
// structure, causing the user to go back to the root node of the directory. The
// directory is then rebuilt, and the user is forced to traverse through the directory
// to find the item they were looking for. Some servers may not push the directory
// structure too often, but we cannot rely on this fix.
//
// I have thought up another fix, but this would require certain features to
// be present within the VLC services discovery. Currently, services_discovery_AddItem
// does not allow the programmer to nest items. It only allows a "2 deep" scope.
// An example of the limitation is below:
//
// Root Directory
// + Item 1
// + Item 2
//
// services_discovery_AddItem will not let the programmer specify a child-node to
// insert items into, so we would not be able to do the following:
//
// Root Directory
// + Item 1
//   + Sub Item 1
// + Item 2
//   + Sub Item 1 of Item 2
//     + Sub-Sub Item 1 of Sub Item 1
//
// This creates a HUGE limitation on what we are able to do. If we were able to do
// the above, we could simply preserve the old directory listing, and compare what items
// do not exist in the new directory listing, then remove them from the shown listing using
// services_discovery_RemoveItem. If new files were introduced within an already existing
// container, we could simply do so with services_discovery_AddItem.

/*
 * Builds playlist based on available input items.
 */
void MediaServer::_buildPlaylist( Container* p_parent, input_item_node_t *p_input_node )
{
    bool b_send = p_input_node == NULL;
    if( b_send )
        p_input_node = input_item_node_Create( p_parent->getInputItem() );

    for ( unsigned int i = 0; i < p_parent->getNumContainers(); i++ )
    {
        Container* p_container = p_parent->getContainer( i );

        input_item_t* p_input_item = input_item_New( "vlc://nop",
                                                    p_container->getTitle() );
        input_item_node_t *p_new_node =
            input_item_node_AppendItem( p_input_node, p_input_item );

        p_container->setInputItem( p_input_item );
        _buildPlaylist( p_container, p_new_node );
    }

    for ( unsigned int i = 0; i < p_parent->getNumItems(); i++ )
    {
        Item* p_item = p_parent->getItem( i );

        char **ppsz_opts = NULL;
        char *psz_input_slave = p_item->buildInputSlaveOption();
        if( psz_input_slave )
        {
            ppsz_opts = (char**)malloc( 2 * sizeof( char* ) );
            ppsz_opts[0] = psz_input_slave;
            ppsz_opts[1] = p_item->buildSubTrackIdOption();
        }

        input_item_t* p_input_item = input_item_NewExt( p_item->getResource(),
                                           p_item->getTitle(),
                                           psz_input_slave ? 2 : 0,
                                           psz_input_slave ? ppsz_opts : NULL,
                                           VLC_INPUT_OPTION_TRUSTED, /* XXX */
                                           p_item->getDuration() );

        assert( p_input_item );
        if( ppsz_opts )
        {
            free( ppsz_opts[0] );
            free( ppsz_opts[1] );
            free( ppsz_opts );

            psz_input_slave = NULL;
        }

        input_item_node_AppendItem( p_input_node, p_input_item );
        p_item->setInputItem( p_input_item );
    }

    if( b_send )
        input_item_node_PostAndDelete( p_input_node );
}

void MediaServer::setInputItem( input_item_t* p_input_item )
{
    if( _p_input_item == p_input_item )
        return;

    if( _p_input_item )
        vlc_gc_decref( _p_input_item );

    vlc_gc_incref( p_input_item );
    _p_input_item = p_input_item;
}

input_item_t* MediaServer::getInputItem() const
{
    return _p_input_item;
}

bool MediaServer::compareSID( const char* psz_sid )
{
    return ( strncmp( _subscription_id, psz_sid, sizeof( Upnp_SID ) ) == 0 );
}


/*
 * MediaServerList class
 */
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

bool MediaServerList::addServer( MediaServer* p_server )
{
    input_item_t* p_input_item = NULL;
    if ( getServer( p_server->getUDN() ) != 0 ) return false;

    msg_Dbg( _p_sd, "Adding server '%s' with uuid '%s'", p_server->getFriendlyName(), p_server->getUDN() );

    p_input_item = input_item_New( "vlc://nop", p_server->getFriendlyName() );

    input_item_SetDescription( p_input_item, p_server->getUDN() );

    p_server->setInputItem( p_input_item );

    services_discovery_AddItem( _p_sd, p_input_item, NULL );

    _list.push_back( p_server );

    return true;
}

MediaServer* MediaServerList::getServer( const char* psz_udn )
{
    MediaServer* p_result = 0;

    for ( unsigned int i = 0; i < _list.size(); i++ )
    {
        if( strcmp( psz_udn, _list[i]->getUDN() ) == 0 )
        {
            p_result = _list[i];
            break;
        }
    }

    return p_result;
}

MediaServer* MediaServerList::getServerBySID( const char* psz_sid )
{
    MediaServer* p_server = 0;

    for ( unsigned int i = 0; i < _list.size(); i++ )
    {
        if ( _list[i]->compareSID( psz_sid ) )
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

    msg_Dbg( _p_sd, "Removing server '%s'", p_server->getFriendlyName() );

    services_discovery_RemoveItem( _p_sd, p_server->getInputItem() );

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


/*
 * Item class
 */
Item::Item( Container* p_parent,
        const char* psz_object_id, const char* psz_title,
        const char* psz_resource, const char* psz_subtitles,
        mtime_t i_duration )
{
    _parent = p_parent;

    _objectID = psz_object_id;
    _title = psz_title;
    _resource = psz_resource;
    _subtitles = psz_subtitles ? psz_subtitles : "";
    _duration = i_duration;

    _p_input_item = NULL;
}

Item::~Item()
{
    if( _p_input_item )
        vlc_gc_decref( _p_input_item );
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

const char* Item::getSubtitles() const
{
    if( !_subtitles.size() )
        return NULL;

    return _subtitles.c_str();
}

mtime_t Item::getDuration() const
{
    return _duration;
}

char* Item::buildInputSlaveOption() const
{
    const char *psz_subtitles    = getSubtitles();

    const char *psz_scheme_delim = "://";
    const char *psz_sub_opt_fmt  = ":input-slave=%s/%s://%s";
    const char *psz_demux        = "subtitle";

    char       *psz_uri_scheme   = NULL;
    const char *psz_scheme_end   = NULL;
    const char *psz_uri_location = NULL;
    char       *psz_input_slave  = NULL;

    size_t i_scheme_len;

    if( !psz_subtitles )
        return NULL;

    psz_scheme_end = strstr( psz_subtitles, psz_scheme_delim );

    /* subtitles not being an URI would make no sense */
    if( !psz_scheme_end )
        return NULL;

    i_scheme_len   = psz_scheme_end - psz_subtitles;
    psz_uri_scheme = (char*)malloc( i_scheme_len + 1 );

    if( !psz_uri_scheme )
        return NULL;

    memcpy( psz_uri_scheme, psz_subtitles, i_scheme_len );
    psz_uri_scheme[i_scheme_len] = '\0';

    /* If the subtitles try to force a vlc demux,
     * then something is very wrong */
    if( strchr( psz_uri_scheme, '/' ) )
    {
        free( psz_uri_scheme );
        return NULL;
    }

    psz_uri_location = psz_scheme_end + strlen( psz_scheme_delim );

    if( -1 == asprintf( &psz_input_slave, psz_sub_opt_fmt,
            psz_uri_scheme, psz_demux, psz_uri_location ) )
        psz_input_slave = NULL;

    free( psz_uri_scheme );
    return psz_input_slave;
}

char* Item::buildSubTrackIdOption() const
{
    return strdup( ":sub-track-id=2" );
}

void Item::setInputItem( input_item_t* p_input_item )
{
    if( _p_input_item == p_input_item )
        return;

    if( _p_input_item )
        vlc_gc_decref( _p_input_item );

    vlc_gc_incref( p_input_item );
    _p_input_item = p_input_item;
}

/*
 * Container class
 */
Container::Container( Container*  p_parent,
                      const char* psz_object_id,
                      const char* psz_title )
{
    _parent = p_parent;

    _objectID = psz_object_id;
    _title = psz_title;

    _p_input_item = NULL;
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

    if( _p_input_item )
        vlc_gc_decref( _p_input_item );
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
    if( _p_input_item == p_input_item )
        return;

    if( _p_input_item )
        vlc_gc_decref( _p_input_item );

    vlc_gc_incref( p_input_item );
    _p_input_item = p_input_item;
}

input_item_t* Container::getInputItem() const
{
    return _p_input_item;
}
