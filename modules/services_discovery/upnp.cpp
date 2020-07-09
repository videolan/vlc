/*****************************************************************************
 * upnp.cpp :  UPnP discovery module (libupnp)
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Rémi Denis-Courmont (original plugin)
 *          Christian Henz <henz # c-lab.de>
 *          Mirsal Ennaime <mirsal dot ennaime at gmail dot com>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "upnp.hpp"

#include <vlc_access.h>
#include <vlc_plugin.h>
#include <vlc_interrupt.h>
#include <vlc_services_discovery.h>
#include <vlc_charset.h>

#include <assert.h>
#include <limits.h>
#include <algorithm>
#include <set>
#include <string>

#if UPNP_VERSION < 10623
/*
 * Compat functions and typedefs for libupnp prior to 1.8
 */

typedef Upnp_Discovery UpnpDiscovery;
typedef Upnp_Action_Complete UpnpActionComplete;

static const char* UpnpDiscovery_get_Location_cstr( const UpnpDiscovery* p_discovery )
{
  return p_discovery->Location;
}

static const char* UpnpDiscovery_get_DeviceID_cstr( const UpnpDiscovery* p_discovery )
{
  return p_discovery->DeviceId;
}

static IXML_Document* UpnpActionComplete_get_ActionResult( const UpnpActionComplete* p_result )
{
  return p_result->ActionResult;
}
#endif

/*
 * Constants
*/
const char* MEDIA_SERVER_DEVICE_TYPE = "urn:schemas-upnp-org:device:MediaServer:1";
const char* CONTENT_DIRECTORY_SERVICE_TYPE = "urn:schemas-upnp-org:service:ContentDirectory:1";
const char* SATIP_SERVER_DEVICE_TYPE = "urn:ses-com:device:SatIPServer:1";

#define SATIP_CHANNEL_LIST N_("SAT>IP channel list")
#define SATIP_CHANNEL_LIST_URL N_("Custom SAT>IP channel list URL")
static const char *const ppsz_satip_channel_lists[] = {
    "Auto", "ASTRA_19_2E", "ASTRA_28_2E", "ASTRA_23_5E", "MasterList", "ServerList", "CustomList"
};
static const char *const ppsz_readible_satip_channel_lists[] = {
    N_("Auto"), "Astra 19.2°E", "Astra 28.2°E", "Astra 23.5°E", N_("Master List"), N_("Server List"), N_("Custom List")
};

/*
 * VLC handle
 */
struct services_discovery_sys_t
{
    UpnpInstanceWrapper* p_upnp;
    vlc_thread_t         thread;
};

struct access_sys_t
{
    UpnpInstanceWrapper* p_upnp;
};

UpnpInstanceWrapper* UpnpInstanceWrapper::s_instance;
vlc_mutex_t UpnpInstanceWrapper::s_lock = VLC_STATIC_MUTEX;
SD::MediaServerList *UpnpInstanceWrapper::p_server_list = NULL;

/*
 * VLC callback prototypes
 */
namespace SD
{
    static int Open( vlc_object_t* );
    static void Close( vlc_object_t* );
}

namespace Access
{
    static int Open( vlc_object_t* );
    static void Close( vlc_object_t* );
}

VLC_SD_PROBE_HELPER( "upnp", N_("Universal Plug'n'Play"), SD_CAT_LAN )

/*
 * Module descriptor
 */
vlc_module_begin()
    set_shortname( "UPnP" );
    set_description( N_( "Universal Plug'n'Play" ) );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );
    set_capability( "services_discovery", 0 );
    set_callbacks( SD::Open, SD::Close );

    add_string( "satip-channelist", "auto", SATIP_CHANNEL_LIST,
                SATIP_CHANNEL_LIST, false )
    change_string_list( ppsz_satip_channel_lists, ppsz_readible_satip_channel_lists )
    add_string( "satip-channellist-url", NULL, SATIP_CHANNEL_LIST_URL,
                SATIP_CHANNEL_LIST_URL, false )

    add_submodule()
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_ACCESS )
        set_callbacks( Access::Open, Access::Close )
        set_capability( "access", 0 )

    VLC_SD_PROBE_SUBMODULE
vlc_module_end()


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
 * Extracts the result document from a SOAP response
 */
IXML_Document* parseBrowseResult( IXML_Document* p_doc )
{
    assert( p_doc );

    // ixml*_getElementsByTagName will ultimately only case the pointer to a Node
    // pointer, and pass it to a private function. Don't bother have a IXML_Document
    // version of getChildElementValue
    const char* psz_raw_didl = xml_getChildElementValue( (IXML_Element*)p_doc, "Result" );

    if( !psz_raw_didl )
        return NULL;

    /* First, try parsing the buffer as is */
    IXML_Document* p_result_doc = ixmlParseBuffer( psz_raw_didl );
    if( !p_result_doc ) {
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
        if( -1 == asprintf( &psz_xml_result_string,
                             psz_xml_result_fmt,
                             psz_raw_didl) )
            return NULL;

        p_result_doc = ixmlParseBuffer( psz_xml_result_string );
        free( psz_xml_result_string );
    }

    if( !p_result_doc )
        return NULL;

    IXML_NodeList *p_elems = ixmlDocument_getElementsByTagName( p_result_doc,
                                                                "DIDL-Lite" );

    IXML_Node *p_node = ixmlNodeList_item( p_elems, 0 );
    ixmlNodeList_free( p_elems );

    return (IXML_Document*)p_node;
}

namespace SD
{

static void *
SearchThread( void *p_data )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_data;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;

    /* Search for media servers */
    int i_res = UpnpSearchAsync( p_sys->p_upnp->handle(), 5,
            MEDIA_SERVER_DEVICE_TYPE, p_sys->p_upnp );
    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "Error sending search request: %s", UpnpGetErrorMessage( i_res ) );
        return NULL;
    }

    /* Search for Sat Ip servers*/
    i_res = UpnpSearchAsync( p_sys->p_upnp->handle(), 5,
            SATIP_SERVER_DEVICE_TYPE, p_sys->p_upnp );
    if( i_res != UPNP_E_SUCCESS )
        msg_Err( p_sd, "Error sending search request: %s", UpnpGetErrorMessage( i_res ) );
    return NULL;
}

/*
 * Initializes UPNP instance.
 */
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = ( services_discovery_sys_t * )
            calloc( 1, sizeof( services_discovery_sys_t ) );

    if( !( p_sd->p_sys = p_sys ) )
        return VLC_ENOMEM;

    p_sd->description = _("Universal Plug'n'Play");

    p_sys->p_upnp = UpnpInstanceWrapper::get( p_this, p_sd );
    if ( !p_sys->p_upnp )
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    /* XXX: Contrary to what the libupnp doc states, UpnpSearchAsync is
     * blocking (select() and send() are called). Therefore, Call
     * UpnpSearchAsync from an other thread. */
    if ( vlc_clone( &p_sys->thread, SearchThread, p_this,
                    VLC_THREAD_PRIORITY_LOW ) )
    {
        p_sys->p_upnp->release( true );
        free(p_sys);
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
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    vlc_join( p_sys->thread, NULL );
    p_sys->p_upnp->release( true );
    free( p_sys );
}

MediaServerDesc::MediaServerDesc( const std::string& udn, const std::string& fName,
                                  const std::string& loc, const std::string& iconUrl )
    : UDN( udn )
    , friendlyName( fName )
    , location( loc )
    , iconUrl( iconUrl )
    , inputItem( NULL )
    , isSatIp( false )
{
}

MediaServerDesc::~MediaServerDesc()
{
    if (inputItem)
        input_item_Release( inputItem );
}

/*
 * MediaServerList class
 */
MediaServerList::MediaServerList( services_discovery_t* p_sd )
    : m_sd( p_sd )
{
}

MediaServerList::~MediaServerList()
{
    vlc_delete_all(m_list);
}

bool MediaServerList::addServer( MediaServerDesc* desc )
{
    input_item_t* p_input_item = NULL;
    if ( getServer( desc->UDN ) )
        return false;

    msg_Dbg( m_sd, "Adding server '%s' with uuid '%s'", desc->friendlyName.c_str(), desc->UDN.c_str() );

    if ( desc->isSatIp )
    {
        p_input_item = input_item_NewDirectory( desc->location.c_str(),
                                                desc->friendlyName.c_str(),
                                                ITEM_NET );
        if ( !p_input_item )
            return false;

        input_item_SetSetting( p_input_item, SATIP_SERVER_DEVICE_TYPE );

        char *psz_playlist_option;

        if (asprintf( &psz_playlist_option, "satip-host=%s",
                     desc->satIpHost.c_str() ) >= 0 ) {
            input_item_AddOption( p_input_item, psz_playlist_option, 0 );
            free( psz_playlist_option );
        }
    } else {
        char* psz_mrl;
        // We might already have some options specified in the location.
        char opt_delim = desc->location.find( '?' ) == 0 ? '?' : '&';
        if( asprintf( &psz_mrl, "upnp://%s%cObjectID=0", desc->location.c_str(), opt_delim ) < 0 )
            return false;

        p_input_item = input_item_NewDirectory( psz_mrl,
                                                desc->friendlyName.c_str(),
                                                ITEM_NET );
        free( psz_mrl );

        if ( !p_input_item )
            return false;

        input_item_SetSetting( p_input_item, MEDIA_SERVER_DEVICE_TYPE );
    }

    if ( desc->iconUrl.empty() == false )
        input_item_SetArtworkURL( p_input_item, desc->iconUrl.c_str() );
    desc->inputItem = p_input_item;
    input_item_SetDescription( p_input_item, desc->UDN.c_str() );
    services_discovery_AddItem( m_sd, p_input_item );
    m_list.push_back( desc );

    return true;
}

MediaServerDesc* MediaServerList::getServer( const std::string& udn )
{
    std::vector<MediaServerDesc*>::const_iterator it = m_list.begin();
    std::vector<MediaServerDesc*>::const_iterator ite = m_list.end();

    for ( ; it != ite; ++it )
    {
        if( udn == (*it)->UDN )
        {
            return *it;
        }
    }
    return NULL;
}

void MediaServerList::parseNewServer( IXML_Document *doc, const std::string &location )
{
    if ( !doc )
    {
        msg_Err( m_sd, "Null IXML_Document" );
        return;
    }

    if ( location.empty() )
    {
        msg_Err( m_sd, "Empty location" );
        return;
    }

    const char* psz_base_url = location.c_str();

    /* Try to extract baseURL */
    IXML_NodeList* p_url_list = ixmlDocument_getElementsByTagName( doc, "URLBase" );
    if ( p_url_list )
    {
        if ( IXML_Node* p_url_node = ixmlNodeList_item( p_url_list, 0 ) )
        {
            IXML_Node* p_text_node = ixmlNode_getFirstChild( p_url_node );
            if ( p_text_node )
                psz_base_url = ixmlNode_getNodeValue( p_text_node );
        }
        ixmlNodeList_free( p_url_list );
    }

    /* Get devices */
    IXML_NodeList* p_device_list = ixmlDocument_getElementsByTagName( doc, "device" );

    if ( !p_device_list )
        return;

    for ( unsigned int i = 0; i < ixmlNodeList_length( p_device_list ); i++ )
    {
        IXML_Element* p_device_element = ( IXML_Element* ) ixmlNodeList_item( p_device_list, i );

        if( !p_device_element )
            continue;

        const char* psz_device_type = xml_getChildElementValue( p_device_element, "deviceType" );

        if ( !psz_device_type )
        {
            msg_Warn( m_sd, "No deviceType found!" );
            continue;
        }

        if ( strncmp( MEDIA_SERVER_DEVICE_TYPE, psz_device_type,
                strlen( MEDIA_SERVER_DEVICE_TYPE ) - 1 )
                && strncmp( SATIP_SERVER_DEVICE_TYPE, psz_device_type,
                        strlen( SATIP_SERVER_DEVICE_TYPE ) - 1 ) )
            continue;

        const char* psz_udn = xml_getChildElementValue( p_device_element,
                                                        "UDN" );
        if ( !psz_udn )
        {
            msg_Warn( m_sd, "No UDN!" );
            continue;
        }

        /* Check if server is already added */
        if ( getServer( psz_udn ) )
        {
            msg_Warn( m_sd, "Server with uuid '%s' already exists.", psz_udn );
            continue;
        }

        const char* psz_friendly_name =
                   xml_getChildElementValue( p_device_element,
                                             "friendlyName" );

        if ( !psz_friendly_name )
        {
            msg_Dbg( m_sd, "No friendlyName!" );
            continue;
        }

        std::string iconUrl = getIconURL( p_device_element, psz_base_url );

        // We now have basic info, we need to get the content browsing url
        // so the access module can browse without fetching the manifest again
        if ( !strncmp( SATIP_SERVER_DEVICE_TYPE, psz_device_type,
                strlen( SATIP_SERVER_DEVICE_TYPE ) - 1 ) ) {
            parseSatipServer( p_device_element, psz_base_url, psz_udn, psz_friendly_name, iconUrl );
        }

        /* Check for ContentDirectory service. */
        IXML_NodeList* p_service_list = ixmlElement_getElementsByTagName( p_device_element, "service" );
        if ( !p_service_list )
            continue;
        for ( unsigned int j = 0; j < ixmlNodeList_length( p_service_list ); j++ )
        {
            IXML_Element* p_service_element = (IXML_Element*)ixmlNodeList_item( p_service_list, j );

            const char* psz_service_type = xml_getChildElementValue( p_service_element, "serviceType" );
            if ( !psz_service_type )
            {
                msg_Warn( m_sd, "No service type found." );
                continue;
            }

            int k = strlen( CONTENT_DIRECTORY_SERVICE_TYPE ) - 1;
            if ( strncmp( CONTENT_DIRECTORY_SERVICE_TYPE,
                        psz_service_type, k ) )
                continue;

            const char* psz_control_url = xml_getChildElementValue( p_service_element,
                                          "controlURL" );
            if ( !psz_control_url )
            {
                msg_Warn( m_sd, "No control url found." );
                continue;
            }

            /* Try to browse content directory. */
            char* psz_url = NULL;
            if ( UpnpResolveURL2( psz_base_url, psz_control_url, &psz_url ) == UPNP_E_SUCCESS )
            {
                SD::MediaServerDesc* p_server = new(std::nothrow) SD::MediaServerDesc( psz_udn,
                    psz_friendly_name, psz_url, iconUrl );
                free( psz_url );
                if ( unlikely( !p_server ) )
                    break;

                if ( !addServer( p_server ) )
                {
                    delete p_server;
                    continue;
                }
            }
        }
        ixmlNodeList_free( p_service_list );
    }
    ixmlNodeList_free( p_device_list );
}

std::string MediaServerList::getIconURL( IXML_Element* p_device_elem, const char* psz_base_url )
{
    std::string res;
    IXML_NodeList* p_icon_lists = ixmlElement_getElementsByTagName( p_device_elem, "iconList" );
    if ( p_icon_lists == NULL )
        return res;
    IXML_Element* p_icon_list = (IXML_Element*)ixmlNodeList_item( p_icon_lists, 0 );
    if ( p_icon_list != NULL )
    {
        IXML_NodeList* p_icons = ixmlElement_getElementsByTagName( p_icon_list, "icon" );
        if ( p_icons != NULL )
        {
            unsigned int maxWidth = 0;
            unsigned int maxHeight = 0;
            for ( unsigned int i = 0; i < ixmlNodeList_length( p_icons ); ++i )
            {
                IXML_Element* p_icon = (IXML_Element*)ixmlNodeList_item( p_icons, i );
                const char* widthStr = xml_getChildElementValue( p_icon, "width" );
                const char* heightStr = xml_getChildElementValue( p_icon, "height" );
                if ( widthStr == NULL || heightStr == NULL )
                    continue;
                unsigned int width = atoi( widthStr );
                unsigned int height = atoi( heightStr );
                if ( width <= maxWidth || height <= maxHeight )
                    continue;
                const char* iconUrl = xml_getChildElementValue( p_icon, "url" );
                if ( iconUrl == NULL )
                    continue;
                maxWidth = width;
                maxHeight = height;
                res = iconUrl;
            }
            ixmlNodeList_free( p_icons );
        }
    }
    ixmlNodeList_free( p_icon_lists );

    if ( res.empty() == false )
    {
        vlc_url_t url;
        vlc_UrlParse( &url, psz_base_url );
        char* psz_url;
        if ( asprintf( &psz_url, "%s://%s:%u%s", url.psz_protocol, url.psz_host, url.i_port, res.c_str() ) < 0 )
            res.clear();
        else
        {
            res = psz_url;
            free( psz_url );
        }
        vlc_UrlClean( &url );
    }
    return res;
}

void
MediaServerList::parseSatipServer( IXML_Element* p_device_element, const char *psz_base_url, const char *psz_udn, const char *psz_friendly_name, std::string iconUrl )
{
    SD::MediaServerDesc* p_server = NULL;

    char *psz_satip_channellist = config_GetPsz(m_sd, "satip-channelist");
    if( !psz_satip_channellist ) {
        psz_satip_channellist = strdup("Auto");
    }

    if( unlikely( !psz_satip_channellist ) )
        return;

    vlc_url_t url;
    vlc_UrlParse( &url, psz_base_url );

    /* Part 1: a user may have provided a custom playlist url */
    if (strncmp(psz_satip_channellist, "CustomList", 10) == 0) {
        char *psz_satip_playlist_url = config_GetPsz(m_sd, "satip-channellist-url" );
        if ( psz_satip_playlist_url ) {
            p_server = new(std::nothrow) SD::MediaServerDesc( psz_udn, psz_friendly_name, psz_satip_playlist_url, iconUrl );

            if( likely( p_server ) ) {
                p_server->satIpHost = url.psz_host;
                p_server->isSatIp = true;
                if( !addServer( p_server ) ) {
                    delete p_server;
                }
            }

            /* to comply with the SAT>IP specification, we don't fall back on another channel list if this path failed */
            free( psz_satip_channellist );
            free( psz_satip_playlist_url );
            vlc_UrlClean( &url );
            return;
        }
    }

    /* Part 2: device playlist
     * In Automatic mode, or if requested by the user, check for a SAT>IP m3u list on the device */
    if (strncmp(psz_satip_channellist, "ServerList", 10) == 0 ||
        strncmp(psz_satip_channellist, "Auto", strlen ("Auto")) == 0 ) {
        const char* psz_m3u_url = xml_getChildElementValue( p_device_element, "satip:X_SATIPM3U" );
        if ( psz_m3u_url ) {
            if ( strncmp( "http", psz_m3u_url, 4) )
            {
                char* psz_url = NULL;
                if ( UpnpResolveURL2( psz_base_url, psz_m3u_url, &psz_url ) == UPNP_E_SUCCESS )
                {
                    p_server = new(std::nothrow) SD::MediaServerDesc( psz_udn, psz_friendly_name, psz_url, iconUrl );
                    free(psz_url);
                }
            } else {
                p_server = new(std::nothrow) SD::MediaServerDesc( psz_udn, psz_friendly_name, psz_m3u_url, iconUrl );
            }

            if ( unlikely( !p_server ) )
            {
                free( psz_satip_channellist );
                vlc_UrlClean( &url );
                return;
            }

            p_server->satIpHost = url.psz_host;
            p_server->isSatIp = true;
            if ( !addServer( p_server ) )
                delete p_server;
        } else {
            msg_Dbg( m_sd, "SAT>IP server '%s' did not provide a playlist", url.psz_host);
        }

        if(strncmp(psz_satip_channellist, "ServerList", 10) == 0) {
            /* to comply with the SAT>IP specifications, we don't fallback on another channel list if this path failed,
             * but in Automatic mode, we continue */
            free(psz_satip_channellist);
            vlc_UrlClean( &url );
            return;
        }
    }

    /* Part 3: satip.info playlist
     * In the normal case, fetch a playlist from the satip website,
     * which will be processed by a lua script a bit later, to make it work sanely
     * MasterList is a list of usual Satellites */

    /* In Auto mode, default to MasterList list from satip.info */
    if( strncmp(psz_satip_channellist, "Auto", strlen ("Auto")) == 0 ) {
        free(psz_satip_channellist);
        psz_satip_channellist = strdup( "MasterList" );
    }

    char *psz_url;
    if (asprintf( &psz_url, "http://www.satip.info/Playlists/%s.m3u",
                psz_satip_channellist ) < 0 ) {
        vlc_UrlClean( &url );
        free( psz_satip_channellist );
        return;
    }

    p_server = new(std::nothrow) SD::MediaServerDesc( psz_udn,
            psz_friendly_name, psz_url, iconUrl );

    if( likely( p_server ) ) {
        p_server->satIpHost = url.psz_host;
        p_server->isSatIp = true;
        if( !addServer( p_server ) ) {
            delete p_server;
        }
    }
    free( psz_url );
    free( psz_satip_channellist );
    vlc_UrlClean( &url );
}

void MediaServerList::removeServer( const std::string& udn )
{
    MediaServerDesc* p_server = getServer( udn );
    if ( !p_server )
        return;

    msg_Dbg( m_sd, "Removing server '%s'", p_server->friendlyName.c_str() );

    assert(p_server->inputItem);
    services_discovery_RemoveItem( m_sd, p_server->inputItem );

    std::vector<MediaServerDesc*>::iterator it = std::find(m_list.begin(), m_list.end(), p_server);
    if (it != m_list.end())
    {
        m_list.erase( it );
    }
    delete p_server;
}

/*
 * Handles servers listing UPnP events
 */
int MediaServerList::Callback( Upnp_EventType event_type, UpnpEventPtr p_event )
{
    switch( event_type )
    {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        const UpnpDiscovery* p_discovery = ( const UpnpDiscovery* )p_event;

        IXML_Document *p_description_doc = NULL;

        int i_res;
        i_res = UpnpDownloadXmlDoc( UpnpDiscovery_get_Location_cstr( p_discovery ), &p_description_doc );

        MediaServerList *self = UpnpInstanceWrapper::lockMediaServerList();
        if ( !self )
        {
            UpnpInstanceWrapper::unlockMediaServerList();
            return UPNP_E_CANCELED;
        }

        if ( i_res != UPNP_E_SUCCESS )
        {
            msg_Warn( self->m_sd, "Could not download device description! "
                            "Fetching data from %s failed: %s",
                            UpnpDiscovery_get_Location_cstr( p_discovery ), UpnpGetErrorMessage( i_res ) );
            UpnpInstanceWrapper::unlockMediaServerList();
            return i_res;
        }
        self->parseNewServer( p_description_doc, UpnpDiscovery_get_Location_cstr( p_discovery ) );
        UpnpInstanceWrapper::unlockMediaServerList();
        ixmlDocument_free( p_description_doc );
    }
    break;

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        const UpnpDiscovery* p_discovery = ( const UpnpDiscovery* )p_event;

        MediaServerList *self = UpnpInstanceWrapper::lockMediaServerList();
        if ( self )
            self->removeServer( UpnpDiscovery_get_DeviceID_cstr( p_discovery ) );
        UpnpInstanceWrapper::unlockMediaServerList();
    }
    break;

    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
    {
        MediaServerList *self = UpnpInstanceWrapper::lockMediaServerList();
        if ( self )
            msg_Warn( self->m_sd, "subscription complete" );
        UpnpInstanceWrapper::unlockMediaServerList();
    }
        break;

    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
    {
        MediaServerList *self = UpnpInstanceWrapper::lockMediaServerList();
        if ( self )
            msg_Warn( self->m_sd, "search timeout" );
        UpnpInstanceWrapper::unlockMediaServerList();
    }
        break;

    case UPNP_EVENT_RECEIVED:
    case UPNP_EVENT_AUTORENEWAL_FAILED:
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
        // Those are for the access part
        break;

    default:
    {
        MediaServerList *self = UpnpInstanceWrapper::lockMediaServerList();
        if ( self )
            msg_Err( self->m_sd, "Unhandled event, please report ( type=%d )", event_type );
        UpnpInstanceWrapper::unlockMediaServerList();
    }
        break;
    }

    return UPNP_E_SUCCESS;
}

}

namespace Access
{

namespace
{
    class ItemDescriptionHolder
    {
    private:
        struct Slave : std::string
        {
            slave_type type;

            Slave(std::string const &url, slave_type type) :
                std::string(url), type(type)
            {
            }
        };

        std::set<Slave> slaves;

        const char* objectID,
            * title,
            * psz_artist,
            * psz_genre,
            * psz_album,
            * psz_date,
            * psz_orig_track_nb,
            * psz_album_artist,
            * psz_albumArt;

    public:
        enum MEDIA_TYPE
            {
                VIDEO = 0,
                AUDIO,
                IMAGE,
                CONTAINER
            };

        MEDIA_TYPE media_type;

        ItemDescriptionHolder()
        {
        }

        bool init(IXML_Element *itemElement)
        {
            objectID = ixmlElement_getAttribute( itemElement, "id" );
            if ( !objectID )
                return false;
            title = xml_getChildElementValue( itemElement, "dc:title" );
            if ( !title )
                return false;
            const char *psz_subtitles = xml_getChildElementValue( itemElement, "sec:CaptionInfo" );
            if ( !psz_subtitles &&
                 !(psz_subtitles = xml_getChildElementValue( itemElement, "sec:CaptionInfoEx" )) )
                psz_subtitles = xml_getChildElementValue( itemElement, "pv:subtitlefile" );
            addSlave(psz_subtitles, SLAVE_TYPE_SPU);
            psz_artist = xml_getChildElementValue( itemElement, "upnp:artist" );
            psz_genre = xml_getChildElementValue( itemElement, "upnp:genre" );
            psz_album = xml_getChildElementValue( itemElement, "upnp:album" );
            psz_date = xml_getChildElementValue( itemElement, "dc:date" );
            psz_orig_track_nb = xml_getChildElementValue( itemElement, "upnp:originalTrackNumber" );
            psz_album_artist = xml_getChildElementValue( itemElement, "upnp:albumArtist" );
            psz_albumArt = xml_getChildElementValue( itemElement, "upnp:albumArtURI" );
            const char *psz_media_type = xml_getChildElementValue( itemElement, "upnp:class" );
            if (strncmp(psz_media_type, "object.item.videoItem", 21) == 0)
                media_type = VIDEO;
            else if (strncmp(psz_media_type, "object.item.audioItem", 21) == 0)
                media_type = AUDIO;
            else if (strncmp(psz_media_type, "object.item.imageItem", 21) == 0)
                media_type = IMAGE;
            else if (strncmp(psz_media_type, "object.container", 16 ) == 0)
                media_type = CONTAINER;
            else
                return false;
            return true;
        }

        void addSlave(const char *psz_slave, slave_type type)
        {
            if (psz_slave)
                slaves.insert(Slave(psz_slave, type));
        }

        void addSubtitleSlave(IXML_Element* p_resource)
        {
            if (slaves.empty())
                addSlave(ixmlElement_getAttribute( p_resource, "pv:subtitleFileUri" ),
                         SLAVE_TYPE_SPU);
        }

        void setArtworkURL(IXML_Element* p_resource)
        {
            psz_albumArt = xml_getChildElementValue( p_resource, "res" );
        }

        void apply(input_item_t *p_item)
        {
            if ( psz_artist != NULL )
                input_item_SetArtist( p_item, psz_artist );
            if ( psz_genre != NULL )
                input_item_SetGenre( p_item, psz_genre );
            if ( psz_album != NULL )
                input_item_SetAlbum( p_item, psz_album );
            if ( psz_date != NULL )
                input_item_SetDate( p_item, psz_date );
            if ( psz_orig_track_nb != NULL )
                input_item_SetTrackNumber( p_item, psz_orig_track_nb );
            if ( psz_album_artist != NULL )
                input_item_SetAlbumArtist( p_item, psz_album_artist );
            if ( psz_albumArt != NULL )
                input_item_SetArtworkURL( p_item, psz_albumArt );
            for (std::set<Slave>::iterator it = slaves.begin(); it != slaves.end(); ++it)
            {
                input_item_slave *p_slave = input_item_slave_New( it->c_str(), it->type,
                                                                  SLAVE_PRIORITY_MATCH_ALL );
                if ( p_slave )
                    input_item_AddSlave( p_item, p_slave );
            }
        }

        input_item_t *createNewItem(IXML_Element *p_resource)
        {
            mtime_t i_duration = -1;
            const char* psz_resource_url = xml_getChildElementValue( p_resource, "res" );
            if( !psz_resource_url )
                return NULL;
            const char* psz_duration = ixmlElement_getAttribute( p_resource, "duration" );
            if ( psz_duration )
            {
                int i_hours, i_minutes, i_seconds;
                if( sscanf( psz_duration, "%d:%02d:%02d", &i_hours, &i_minutes, &i_seconds ) )
                    i_duration = INT64_C(1000000) * ( i_hours * 3600 + i_minutes * 60 +
                                                      i_seconds );
            }
            return input_item_NewExt( psz_resource_url, title, i_duration,
                                      ITEM_TYPE_FILE, ITEM_NET );
        }

        input_item_t *createNewContainerItem( const char* psz_root )
        {
            if ( objectID == NULL || title == NULL )
                return NULL;

            char* psz_url;
            if( asprintf( &psz_url, "upnp://%s?ObjectID=%s", psz_root, objectID ) < 0 )
                return NULL;

            input_item_t* p_item = input_item_NewDirectory( psz_url, title, ITEM_NET );
            free( psz_url);
            return p_item;
        }
    };
}

Upnp_i11e_cb::Upnp_i11e_cb( Upnp_FunPtr callback, void *cookie )
    : m_refCount( 2 ) /* 2: owned by the caller, and the Upnp Async function */
    , m_callback( callback )
    , m_cookie( cookie )

{
    vlc_mutex_init( &m_lock );
    vlc_sem_init( &m_sem, 0 );
}

Upnp_i11e_cb::~Upnp_i11e_cb()
{
    vlc_mutex_destroy( &m_lock );
    vlc_sem_destroy( &m_sem );
}

void Upnp_i11e_cb::waitAndRelease( void )
{
    vlc_sem_wait_i11e( &m_sem );

    vlc_mutex_lock( &m_lock );
    if ( --m_refCount == 0 )
    {
        /* The run callback is processed, we can destroy this object */
        vlc_mutex_unlock( &m_lock );
        delete this;
    } else
    {
        /* Interrupted, let the run callback destroy this object */
        vlc_mutex_unlock( &m_lock );
    }
}

int Upnp_i11e_cb::run( Upnp_EventType eventType, UpnpEventPtr p_event, void *p_cookie )
{
    Upnp_i11e_cb *self = static_cast<Upnp_i11e_cb*>( p_cookie );

    vlc_mutex_lock( &self->m_lock );
    if ( --self->m_refCount == 0 )
    {
        /* Interrupted, we can destroy self */
        vlc_mutex_unlock( &self->m_lock );
        delete self;
        return 0;
    }
    /* Process the user callback_ */
    self->m_callback( eventType, p_event, self->m_cookie);
    vlc_mutex_unlock( &self->m_lock );

    /* Signal that the callback is processed */
    vlc_sem_post( &self->m_sem );
    return 0;
}

MediaServer::MediaServer( stream_t *p_access, input_item_node_t *node )
    : m_psz_objectId( NULL )
    , m_access( p_access )
    , m_node( node )

{
    m_psz_root = strdup( p_access->psz_location );
    char* psz_objectid = strstr( m_psz_root, "ObjectID=" );
    if ( psz_objectid != NULL )
    {
        // Remove this parameter from the URL, since it might cause some servers to fail
        // Keep in mind that we added a '&' or a '?' to the URL, so remove it as well
        *( psz_objectid - 1) = 0;
        m_psz_objectId = &psz_objectid[strlen( "ObjectID=" )];
    }
}

MediaServer::~MediaServer()
{
    free( m_psz_root );
}

bool MediaServer::addContainer( IXML_Element* containerElement )
{
    ItemDescriptionHolder holder;

    if ( holder.init( containerElement ) == false )
        return false;

    input_item_t* p_item = holder.createNewContainerItem( m_psz_root );
    if ( !p_item )
        return false;
    holder.apply( p_item );
    input_item_CopyOptions( p_item, m_node->p_item );
    input_item_node_AppendItem( m_node, p_item );
    input_item_Release( p_item );
    return true;
}

bool MediaServer::addItem( IXML_Element* itemElement )
{
    ItemDescriptionHolder holder;

    if (!holder.init(itemElement))
        return false;
    /* Try to extract all resources in DIDL */
    IXML_NodeList* p_resource_list = ixmlDocument_getElementsByTagName( (IXML_Document*) itemElement, "res" );
    if ( !p_resource_list)
        return false;
    int list_lenght = ixmlNodeList_length( p_resource_list );
    if (list_lenght <= 0 ) {
        ixmlNodeList_free( p_resource_list );
        return false;
    }
    input_item_t *p_item = NULL;

    for (int index = 0; index < list_lenght; index++)
    {
        IXML_Element* p_resource = ( IXML_Element* ) ixmlNodeList_item( p_resource_list, index );
        const char* rez_type = ixmlElement_getAttribute( p_resource, "protocolInfo" );

        if (strncmp(rez_type, "http-get:*:video/", 17) == 0 && holder.media_type == ItemDescriptionHolder::VIDEO)
        {
            if (!p_item)
                p_item = holder.createNewItem(p_resource);
            holder.addSubtitleSlave(p_resource);
        }
        else if (strncmp(rez_type, "http-get:*:image/", 17) == 0)
            switch (holder.media_type)
            {
            case ItemDescriptionHolder::IMAGE:
                if (!p_item) {
                    p_item = holder.createNewItem(p_resource);
                    break;
                }
            case ItemDescriptionHolder::VIDEO:
            case ItemDescriptionHolder::AUDIO:
                holder.setArtworkURL(p_resource);
                break;
            case ItemDescriptionHolder::CONTAINER:
                msg_Warn( m_access, "Unexpected object.container in item enumeration" );
                continue;
            }
        else if (strncmp(rez_type, "http-get:*:text/", 16) == 0)
            holder.addSlave(xml_getChildElementValue( p_resource, "res" ), SLAVE_TYPE_SPU);
        else if (strncmp(rez_type, "http-get:*:audio/", 17) == 0)
        {
            if (holder.media_type == ItemDescriptionHolder::AUDIO)
            {
                if (!p_item)
                    p_item = holder.createNewItem(p_resource);
            }
            else
                holder.addSlave(xml_getChildElementValue( p_resource, "res" ),
                                SLAVE_TYPE_AUDIO);
        }
    }
    ixmlNodeList_free( p_resource_list );
    if (!p_item)
        return false;
    holder.apply(p_item);
    input_item_CopyOptions( p_item, m_node->p_item );
    input_item_node_AppendItem( m_node, p_item );
    input_item_Release( p_item );
    return true;
}

int MediaServer::sendActionCb( Upnp_EventType eventType,
                               UpnpEventPtr p_event, void *p_cookie )
{
    if( eventType != UPNP_CONTROL_ACTION_COMPLETE )
        return 0;
    IXML_Document** pp_sendActionResult = (IXML_Document** )p_cookie;
    const UpnpActionComplete *p_result = (const UpnpActionComplete *)p_event;

    /* The only way to dup the result is to print it and parse it again */
    DOMString tmpStr = ixmlPrintNode( ( IXML_Node * ) UpnpActionComplete_get_ActionResult( p_result ) );
    if (tmpStr == NULL)
        return 0;

    *pp_sendActionResult = ixmlParseBuffer( tmpStr );
    ixmlFreeDOMString( tmpStr );
    return 0;
}

/* Access part */
IXML_Document* MediaServer::_browseAction( const char* psz_object_id_,
                                           const char* psz_browser_flag_,
                                           const char* psz_filter_,
                                           const char* psz_requested_count_,
                                           const char* psz_sort_criteria_ )
{
    IXML_Document* p_action = NULL;
    IXML_Document* p_response = NULL;
    Upnp_i11e_cb *i11eCb = NULL;
    access_sys_t *sys = (access_sys_t *)m_access->p_sys;

    int i_res;

    if ( vlc_killed() )
        return NULL;

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "ObjectID", psz_object_id_ ? psz_object_id_ : "0" );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( m_access, "AddToAction 'ObjectID' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "BrowseFlag", psz_browser_flag_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( m_access, "AddToAction 'BrowseFlag' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "Filter", psz_filter_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( m_access, "AddToAction 'Filter' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "StartingIndex", "0" );
    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( m_access, "AddToAction 'StartingIndex' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "RequestedCount", psz_requested_count_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( m_access, "AddToAction 'RequestedCount' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "SortCriteria", psz_sort_criteria_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( m_access, "AddToAction 'SortCriteria' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    /* Setup an interruptible callback that will call sendActionCb if not
     * interrupted by vlc_interrupt_kill */
    i11eCb = new Upnp_i11e_cb( sendActionCb, &p_response );
    i_res = UpnpSendActionAsync( sys->p_upnp->handle(),
              m_psz_root,
              CONTENT_DIRECTORY_SERVICE_TYPE,
              NULL, /* ignored in SDK, must be NULL */
              p_action,
              Upnp_i11e_cb::run, i11eCb );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( m_access, "%s when trying the send() action with URL: %s",
                UpnpGetErrorMessage( i_res ), m_access->psz_location );
    }
    /* Wait for the callback to fill p_response or wait for an interrupt */
    i11eCb->waitAndRelease();

browseActionCleanup:
    ixmlDocument_free( p_action );
    return p_response;
}

/*
 * Fetches and parses the UPNP response
 */
bool MediaServer::fetchContents()
{
    IXML_Document* p_response = _browseAction( m_psz_objectId,
                                      "BrowseDirectChildren",
                                      "*",
                                      // Some servers don't understand "0" as "no-limit"
                                      "5000", /* RequestedCount */
                                      "" /* SortCriteria */
                                      );
    if ( !p_response )
    {
        msg_Err( m_access, "No response from browse() action" );
        return false;
    }

    IXML_Document* p_result = parseBrowseResult( p_response );

    ixmlDocument_free( p_response );

    if ( !p_result )
    {
        msg_Err( m_access, "browse() response parsing failed" );
        return false;
    }

#ifndef NDEBUG
    msg_Dbg( m_access, "Got DIDL document: %s", ixmlPrintDocument( p_result ) );
#endif

    IXML_NodeList* containerNodeList =
                ixmlDocument_getElementsByTagName( p_result, "container" );

    if ( containerNodeList )
    {
        for ( unsigned int i = 0; i < ixmlNodeList_length( containerNodeList ); i++ )
            addContainer( (IXML_Element*)ixmlNodeList_item( containerNodeList, i ) );
        ixmlNodeList_free( containerNodeList );
    }

    IXML_NodeList* itemNodeList = ixmlDocument_getElementsByTagName( p_result,
                                                                     "item" );
    if ( itemNodeList )
    {
        for ( unsigned int i = 0; i < ixmlNodeList_length( itemNodeList ); i++ )
            addItem( (IXML_Element*)ixmlNodeList_item( itemNodeList, i ) );
        ixmlNodeList_free( itemNodeList );
    }

    ixmlDocument_free( p_result );
    return true;
}

static int ReadDirectory( stream_t *p_access, input_item_node_t* p_node )
{
    MediaServer server( p_access, p_node );

    if ( !server.fetchContents() )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int Open( vlc_object_t *p_this )
{
    stream_t* p_access = (stream_t*)p_this;
    access_sys_t* p_sys = new(std::nothrow) access_sys_t;
    if ( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    p_access->p_sys = p_sys;
    p_sys->p_upnp = UpnpInstanceWrapper::get( p_this, NULL );
    if ( !p_sys->p_upnp )
    {
        delete p_sys;
        return VLC_EGENERIC;
    }

    p_access->pf_readdir = ReadDirectory;
    p_access->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}

static void Close( vlc_object_t* p_this )
{
    stream_t* p_access = (stream_t*)p_this;
    access_sys_t *sys = (access_sys_t *)p_access->p_sys;

    sys->p_upnp->release( false );
    delete sys;
}

}

UpnpInstanceWrapper::UpnpInstanceWrapper()
    : m_handle( -1 )
    , m_refcount( 0 )
{
}

UpnpInstanceWrapper::~UpnpInstanceWrapper()
{
    UpnpUnRegisterClient( m_handle );
    UpnpFinish();
}

#ifdef _WIN32

static IP_ADAPTER_MULTICAST_ADDRESS* getMulticastAddress(IP_ADAPTER_ADDRESSES* p_adapter)
{
    const unsigned long i_broadcast_ip = inet_addr("239.255.255.250");

    IP_ADAPTER_MULTICAST_ADDRESS *p_multicast = p_adapter->FirstMulticastAddress;
    while (p_multicast != NULL)
    {
        if (((struct sockaddr_in *)p_multicast->Address.lpSockaddr)->sin_addr.S_un.S_addr == i_broadcast_ip)
            return p_multicast;
        p_multicast = p_multicast->Next;
    }
    return NULL;
}

static bool isAdapterSuitable(IP_ADAPTER_ADDRESSES* p_adapter, bool ipv6)
{
    if ( p_adapter->OperStatus != IfOperStatusUp )
        return false;
    if (p_adapter->Length == sizeof(IP_ADAPTER_ADDRESSES_XP))
    {
        IP_ADAPTER_ADDRESSES_XP* p_adapter_xp = reinterpret_cast<IP_ADAPTER_ADDRESSES_XP*>( p_adapter );
        // On Windows Server 2003 and Windows XP, this member is zero if IPv4 is not available on the interface.
        if (ipv6)
            return p_adapter_xp->Ipv6IfIndex != 0;
        return p_adapter_xp->IfIndex != 0;
    }
    IP_ADAPTER_ADDRESSES_LH* p_adapter_lh = reinterpret_cast<IP_ADAPTER_ADDRESSES_LH*>( p_adapter );
    if (p_adapter_lh->FirstGatewayAddress == NULL)
        return false;
    if (ipv6)
        return p_adapter_lh->Ipv6Enabled;
    return p_adapter_lh->Ipv4Enabled;
}

static IP_ADAPTER_ADDRESSES* ListAdapters()
{
    ULONG addrSize;
    const ULONG queryFlags = GAA_FLAG_INCLUDE_GATEWAYS|GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_DNS_SERVER;
    IP_ADAPTER_ADDRESSES* addresses = NULL;
    HRESULT hr;

    /**
     * https://msdn.microsoft.com/en-us/library/aa365915.aspx
     *
     * The recommended method of calling the GetAdaptersAddresses function is to pre-allocate a
     * 15KB working buffer pointed to by the AdapterAddresses parameter. On typical computers,
     * this dramatically reduces the chances that the GetAdaptersAddresses function returns
     * ERROR_BUFFER_OVERFLOW, which would require calling GetAdaptersAddresses function multiple
     * times. The example code illustrates this method of use.
     */
    addrSize = 15 * 1024;
    do
    {
        free(addresses);
        addresses = (IP_ADAPTER_ADDRESSES*)malloc( addrSize );
        if (addresses == NULL)
            return NULL;
        hr = GetAdaptersAddresses(AF_UNSPEC, queryFlags, NULL, addresses, &addrSize);
    } while (hr == ERROR_BUFFER_OVERFLOW);
    if (hr != NO_ERROR) {
        free(addresses);
        return NULL;
    }
    return addresses;
}

#ifdef UPNP_ENABLE_IPV6

static char* getPreferedAdapter()
{
    IP_ADAPTER_ADDRESSES *p_adapter, *addresses;

    addresses = ListAdapters();
    if (addresses == NULL)
        return NULL;

    /* find one with multicast capabilities */
    p_adapter = addresses;
    while (p_adapter != NULL)
    {
        if (isAdapterSuitable( p_adapter, true ))
        {
            /* make sure it supports 239.255.255.250 */
            IP_ADAPTER_MULTICAST_ADDRESS *p_multicast = getMulticastAddress( p_adapter );
            if (p_multicast != NULL)
            {
                char* res = FromWide( p_adapter->FriendlyName );
                free( addresses );
                return res;
            }
        }
        p_adapter = p_adapter->Next;
    }
    free(addresses);
    return NULL;
}

#else

static char *getIpv4ForMulticast()
{
    IP_ADAPTER_UNICAST_ADDRESS *p_best_ip = NULL;
    wchar_t psz_uri[32];
    DWORD strSize;
    IP_ADAPTER_ADDRESSES *p_adapter, *addresses;

    addresses = ListAdapters();
    if (addresses == NULL)
        return NULL;

    /* find one with multicast capabilities */
    p_adapter = addresses;
    while (p_adapter != NULL)
    {
        if (isAdapterSuitable( p_adapter, false ))
        {
            /* make sure it supports 239.255.255.250 */
            IP_ADAPTER_MULTICAST_ADDRESS *p_multicast = getMulticastAddress( p_adapter );
            if (p_multicast != NULL)
            {
                /* get an IPv4 address */
                IP_ADAPTER_UNICAST_ADDRESS *p_unicast = p_adapter->FirstUnicastAddress;
                while (p_unicast != NULL)
                {
                    strSize = sizeof( psz_uri ) / sizeof( wchar_t );
                    if( WSAAddressToString( p_unicast->Address.lpSockaddr,
                                            p_unicast->Address.iSockaddrLength,
                                            NULL, psz_uri, &strSize ) == 0 )
                    {
                        if ( p_best_ip == NULL ||
                             p_best_ip->ValidLifetime > p_unicast->ValidLifetime )
                        {
                            p_best_ip = p_unicast;
                        }
                    }
                    p_unicast = p_unicast->Next;
                }
            }
        }
        p_adapter = p_adapter->Next;
    }

    if ( p_best_ip != NULL )
        goto done;

    /* find any with IPv4 */
    p_adapter = addresses;
    while (p_adapter != NULL)
    {
        if (isAdapterSuitable(p_adapter, false))
        {
            /* get an IPv4 address */
            IP_ADAPTER_UNICAST_ADDRESS *p_unicast = p_adapter->FirstUnicastAddress;
            while (p_unicast != NULL)
            {
                strSize = sizeof( psz_uri ) / sizeof( wchar_t );
                if( WSAAddressToString( p_unicast->Address.lpSockaddr,
                                        p_unicast->Address.iSockaddrLength,
                                        NULL, psz_uri, &strSize ) == 0 )
                {
                    if ( p_best_ip == NULL ||
                         p_best_ip->ValidLifetime > p_unicast->ValidLifetime )
                    {
                        p_best_ip = p_unicast;
                    }
                }
                p_unicast = p_unicast->Next;
            }
        }
        p_adapter = p_adapter->Next;
    }

done:
    if (p_best_ip != NULL)
    {
        strSize = sizeof( psz_uri ) / sizeof( wchar_t );
        WSAAddressToString( p_best_ip->Address.lpSockaddr,
                            p_best_ip->Address.iSockaddrLength,
                            NULL, psz_uri, &strSize );
        free(addresses);
        return FromWide( psz_uri );
    }
    free(addresses);
    return NULL;
}
#endif /* UPNP_ENABLE_IPV6 */
#else /* _WIN32 */

#ifdef UPNP_ENABLE_IPV6

#ifdef __APPLE__
#include <TargetConditionals.h>

#if defined(TARGET_OS_OSX) && TARGET_OS_OSX
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "vlc_charset.h"

inline char *FromCFString(const CFStringRef cfString,
                          const CFStringEncoding cfStringEncoding)
{
    // Try the quick way to obtain the buffer
    const char *tmpBuffer = CFStringGetCStringPtr(cfString, cfStringEncoding);
    if (tmpBuffer != NULL) {
        return strdup(tmpBuffer);
    }

    // The quick way did not work, try the long way
    CFIndex length = CFStringGetLength(cfString);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, cfStringEncoding);

    // If result would exceed LONG_MAX, kCFNotFound is returned
    if (unlikely(maxSize == kCFNotFound)) {
        return NULL;
    }

    // Account for the null terminator
    maxSize++;

    char *buffer = (char *)malloc(maxSize);
    if (unlikely(buffer == NULL)) {
        return NULL;
    }

    // Copy CFString in requested encoding to buffer
    Boolean success = CFStringGetCString(cfString, buffer, maxSize, cfStringEncoding);

    if (!success)
        FREENULL(buffer);
    return buffer;
}

inline char *getPreferedAdapter()
{
    SCDynamicStoreRef session = SCDynamicStoreCreate(NULL, CFSTR("session"), NULL, NULL);
    if (session == NULL)
        return NULL;

    CFDictionaryRef q = (CFDictionaryRef) SCDynamicStoreCopyValue(session, CFSTR("State:/Network/Global/IPv4"));
    char *returnValue = NULL;

    if (q != NULL) {
        const void *val;
        if (CFDictionaryGetValueIfPresent(q, CFSTR("PrimaryInterface"), &val)) {
            returnValue = FromCFString((CFStringRef)val, kCFStringEncodingUTF8);
        }
        CFRelease(q);
    }
    CFRelease(session);

    return returnValue;
}

#else /* iOS and tvOS */

inline bool necessaryFlagsSetOnInterface(struct ifaddrs *anInterface)
{
    unsigned int flags = anInterface->ifa_flags;
    if( (flags & IFF_UP) && (flags & IFF_RUNNING) && !(flags & IFF_LOOPBACK) && !(flags & IFF_POINTOPOINT) ) {
        return true;
    }
    return false;
}

inline char *getPreferedAdapter()
{
    struct ifaddrs *listOfInterfaces;
    struct ifaddrs *anInterface;
    int ret = getifaddrs(&listOfInterfaces);
    char *adapterName = NULL;

    if (ret != 0) {
        return NULL;
    }

    anInterface = listOfInterfaces;
    while (anInterface != NULL) {
        bool ret = necessaryFlagsSetOnInterface(anInterface);
        if (ret) {
            adapterName = strdup(anInterface->ifa_name);
            break;
        }

        anInterface = anInterface->ifa_next;
    }
    freeifaddrs(listOfInterfaces);

    return adapterName;
}

#endif

#else /* *nix and Android */

inline char *getPreferedAdapter()
{
    return NULL;
}

#endif
#else

static char *getIpv4ForMulticast()
{
    return NULL;
}

#endif

#endif /* _WIN32 */

UpnpInstanceWrapper *UpnpInstanceWrapper::get(vlc_object_t *p_obj, services_discovery_t *p_sd)
{
    SD::MediaServerList *p_server_list = NULL;
    if (p_sd)
    {
        p_server_list = new(std::nothrow) SD::MediaServerList( p_sd );
        if ( unlikely( p_server_list == NULL ) )
        {
            msg_Err( p_sd, "Failed to create a MediaServerList");
            return NULL;
        }
    }

    vlc_mutex_locker lock( &s_lock );
    if ( s_instance == NULL )
    {
        UpnpInstanceWrapper* instance = new(std::nothrow) UpnpInstanceWrapper;
        if ( unlikely( !instance ) )
        {
            delete p_server_list;
            return NULL;
        }

    #ifdef UPNP_ENABLE_IPV6
        char* psz_miface = var_InheritString( p_obj, "miface" );
        if (psz_miface == NULL)
            psz_miface = getPreferedAdapter();
        msg_Info( p_obj, "Initializing libupnp on '%s' interface", psz_miface ? psz_miface : "default" );
        int i_res = UpnpInit2( psz_miface, 0 );
        free( psz_miface );
    #else
        /* If UpnpInit2 isnt available, initialize on first IPv4-capable interface */
        char *psz_hostip = getIpv4ForMulticast();
        int i_res = UpnpInit( psz_hostip, 0 );
        free(psz_hostip);
    #endif /* UPNP_ENABLE_IPV6 */
        if( i_res != UPNP_E_SUCCESS )
        {
            msg_Err( p_obj, "Initialization failed: %s", UpnpGetErrorMessage( i_res ) );
            delete instance;
            delete p_server_list;
            return NULL;
        }

        ixmlRelaxParser( 1 );

        /* Register a control point */
        i_res = UpnpRegisterClient( Callback, instance, &instance->m_handle );
        if( i_res != UPNP_E_SUCCESS )
        {
            msg_Err( p_obj, "Client registration failed: %s", UpnpGetErrorMessage( i_res ) );
            delete instance;
            delete p_server_list;
            return NULL;
        }

        /* libupnp does not treat a maximum content length of 0 as unlimited
         * until 64dedf (~ pupnp v1.6.7) and provides no sane way to discriminate
         * between versions */
        if( (i_res = UpnpSetMaxContentLength( INT_MAX )) != UPNP_E_SUCCESS )
        {
            msg_Err( p_obj, "Failed to set maximum content length: %s",
                    UpnpGetErrorMessage( i_res ));
            delete instance;
            delete p_server_list;
            return NULL;
        }
        s_instance = instance;
    }
    s_instance->m_refcount++;
    // This assumes a single UPNP SD instance
    if (p_server_list != NULL)
    {
        assert(!UpnpInstanceWrapper::p_server_list);
        UpnpInstanceWrapper::p_server_list = p_server_list;
    }
    return s_instance;
}

void UpnpInstanceWrapper::release(bool isSd)
{
    UpnpInstanceWrapper *p_delete = NULL;
    vlc_mutex_locker lock( &s_lock );
    if ( isSd )
    {
        delete UpnpInstanceWrapper::p_server_list;
        UpnpInstanceWrapper::p_server_list = NULL;
    }
    if (--s_instance->m_refcount == 0)
    {
        p_delete = s_instance;
        s_instance = NULL;
    }
    delete p_delete;
}

UpnpClient_Handle UpnpInstanceWrapper::handle() const
{
    return m_handle;
}

int UpnpInstanceWrapper::Callback(Upnp_EventType event_type, UpnpEventPtr p_event, void *p_user_data)
{
    VLC_UNUSED(p_user_data);
    vlc_mutex_lock( &s_lock );
    if ( !UpnpInstanceWrapper::p_server_list )
    {
        vlc_mutex_unlock( &s_lock );
        /* no MediaServerList available (anymore), do nothing */
        return 0;
    }
    vlc_mutex_unlock( &s_lock );
    SD::MediaServerList::Callback( event_type, p_event );
    return 0;
}

SD::MediaServerList *UpnpInstanceWrapper::lockMediaServerList()
{
    vlc_mutex_lock( &s_lock ); /* do not allow deleting the p_server_list while using it */
    return UpnpInstanceWrapper::p_server_list;
}

void UpnpInstanceWrapper::unlockMediaServerList()
{
    vlc_mutex_unlock( &s_lock );
}
