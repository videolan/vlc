/*****************************************************************************
 * upnp.cpp :  UPnP discovery module (libupnp)
 *****************************************************************************
 * Copyright (C) 2004-2018 VLC authors and VideoLAN
 *
 * Authors: Rémi Denis-Courmont (original plugin)
 *          Christian Henz <henz # c-lab.de>
 *          Mirsal Ennaime <mirsal dot ennaime at gmail dot com>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *          Shaleen Jain <shaleen@jain.sh>
 *          William Ung <william1.ung@epitech.eu>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "upnp.hpp"

#include <vlc_access.h>
#include <vlc_plugin.h>
#include <vlc_interrupt.h>
#include <vlc_services_discovery.h>
#include <vlc_renderer_discovery.h>

#include <assert.h>
#include <limits.h>
#include <algorithm>
#include <set>
#include <string>

/*
 * Constants
*/
const char* MEDIA_SERVER_DEVICE_TYPE = "urn:schemas-upnp-org:device:MediaServer:1";
const char* MEDIA_RENDERER_DEVICE_TYPE = "urn:schemas-upnp-org:device:MediaRenderer:1";
const char* CONTENT_DIRECTORY_SERVICE_TYPE = "urn:schemas-upnp-org:service:ContentDirectory:1";
const char* SATIP_SERVER_DEVICE_TYPE = "urn:ses-com:device:SatIPServer:1";

#define UPNP_SEARCH_TIMEOUT_SECONDS 15
#define SATIP_CHANNEL_LIST N_("SAT>IP channel list")
#define SATIP_CHANNEL_LIST_URL N_("Custom SAT>IP channel list URL")

#define HTTP_PORT         7070

#define HTTP_PORT_TEXT N_("HTTP port")
#define HTTP_PORT_LONGTEXT N_("This sets the HTTP port of the local server used to stream the media to the UPnP Renderer.")
#define HAS_VIDEO_TEXT N_("Video")
#define HAS_VIDEO_LONGTEXT N_("The UPnP Renderer can receive video.")

#define IP_ADDR_TEXT N_("IP Address")
#define IP_ADDR_LONGTEXT N_("IP Address of the UPnP Renderer.")
#define PORT_TEXT N_("UPnP Renderer port")
#define PORT_LONGTEXT N_("The port used to talk to the UPnP Renderer.")
#define BASE_URL_TEXT N_("base URL")
#define BASE_URL_LONGTEXT N_("The base Url relative to which all other UPnP operations must be called")
#define URL_TEXT N_("description URL")
#define URL_LONGTEXT N_("The Url used to get the xml descriptor of the UPnP Renderer")

static const char *const ppsz_satip_channel_lists[] = {
    "Auto", "ASTRA_19_2E", "ASTRA_28_2E", "ASTRA_23_5E", "MasterList", "ServerList", "CustomList"
};
static const char *const ppsz_readible_satip_channel_lists[] = {
    N_("Auto"), "Astra 19.2°E", "Astra 28.2°E", "Astra 23.5°E", N_("SAT>IP Main List"), N_("Device List"), N_("Custom List")
};

namespace {

/*
 * VLC handle
 */
struct services_discovery_sys_t
{
    UpnpInstanceWrapper* p_upnp;
    std::shared_ptr<SD::MediaServerList> p_server_list;
    vlc_thread_t         thread;
};


struct renderer_discovery_sys_t
{
    UpnpInstanceWrapper* p_upnp;
    std::shared_ptr<RD::MediaRendererList> p_renderer_list;
    vlc_thread_t thread;
};

struct access_sys_t
{
    UpnpInstanceWrapper* p_upnp;
};

} // namespace

/*
 * VLC callback prototypes
 */
namespace SD
{
    static int OpenSD( vlc_object_t* );
    static void CloseSD( vlc_object_t* );
}

namespace Access
{
    static int OpenAccess( vlc_object_t* );
    static void CloseAccess( vlc_object_t* );
}

namespace RD
{
    static int OpenRD( vlc_object_t*);
    static void CloseRD( vlc_object_t* );
}

VLC_SD_PROBE_HELPER( "upnp", N_("Universal Plug'n'Play"), SD_CAT_LAN )
VLC_RD_PROBE_HELPER( "upnp_renderer", N_("UPnP Renderer Discovery") )

/*
 * Module descriptor
 */
vlc_module_begin()
    set_shortname( "UPnP" );
    set_description( N_( "Universal Plug'n'Play" ) );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );
    set_capability( "services_discovery", 0 );
    set_callbacks( SD::OpenSD, SD::CloseSD );

    add_string( "satip-channelist", "auto", SATIP_CHANNEL_LIST,
                SATIP_CHANNEL_LIST, false )
    change_string_list( ppsz_satip_channel_lists, ppsz_readible_satip_channel_lists )
    add_string( "satip-channellist-url", NULL, SATIP_CHANNEL_LIST_URL,
                SATIP_CHANNEL_LIST_URL, false )

    add_submodule()
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_ACCESS )
        set_callbacks( Access::OpenAccess, Access::CloseAccess )
        set_capability( "access", 0 )

    VLC_SD_PROBE_SUBMODULE

    add_submodule()
        set_description( N_( "UPnP Renderer Discovery" ) )
        set_category( CAT_SOUT )
        set_subcategory( SUBCAT_SOUT_RENDERER )
        set_callbacks( RD::OpenRD, RD::CloseRD )
        set_capability( "renderer_discovery", 0 )
        add_shortcut( "upnp_renderer" )

    VLC_RD_PROBE_SUBMODULE

    add_submodule()
        set_shortname("dlna")
        set_description(N_("UPnP/DLNA stream output"))
        set_capability("sout output", 0)
        add_shortcut("dlna")
        set_category(CAT_SOUT)
        set_subcategory(SUBCAT_SOUT_STREAM)
        set_callbacks(DLNA::OpenSout, DLNA::CloseSout)

        add_string(SOUT_CFG_PREFIX "ip", NULL, IP_ADDR_TEXT, IP_ADDR_LONGTEXT, false)
        add_integer(SOUT_CFG_PREFIX "port", NULL, PORT_TEXT, PORT_LONGTEXT, false)
        add_integer(SOUT_CFG_PREFIX "http-port", HTTP_PORT, HTTP_PORT_TEXT, HTTP_PORT_LONGTEXT, false)
        add_bool(SOUT_CFG_PREFIX "video", true, HAS_VIDEO_TEXT, HAS_VIDEO_LONGTEXT, false)
        add_string(SOUT_CFG_PREFIX "base_url", NULL, BASE_URL_TEXT, BASE_URL_LONGTEXT, false)
        add_string(SOUT_CFG_PREFIX "url", NULL, URL_TEXT, URL_LONGTEXT, false)
        add_renderer_opts(SOUT_CFG_PREFIX)
vlc_module_end()

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

/**
 * Reads the base URL from an XML device list
 *
 * \param services_discovery_t* p_sd This SD instance
 * \param IXML_Document* p_desc an XML device list document
 *
 * \return const char* The base URL
 */
static const char *parseBaseUrl( IXML_Document *p_desc )
{
    const char    *psz_base_url = nullptr;
    IXML_NodeList *p_url_list = nullptr;

    if( ( p_url_list = ixmlDocument_getElementsByTagName( p_desc, "URLBase" ) ) )
    {
        if ( IXML_Node* p_url_node = ixmlNodeList_item( p_url_list, 0 ) )
        {
            IXML_Node* p_text_node = ixmlNode_getFirstChild( p_url_node );
            if ( p_text_node )
                psz_base_url = ixmlNode_getNodeValue( p_text_node );
        }
        ixmlNodeList_free( p_url_list );
    }
    return psz_base_url;
}

namespace SD
{

static void *
SearchThread( void *p_data )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_data;
    services_discovery_sys_t *p_sys = reinterpret_cast<services_discovery_sys_t *>( p_sd->p_sys );

    /* Search for media servers */
    int i_res = UpnpSearchAsync( p_sys->p_upnp->handle(), 5,
            MEDIA_SERVER_DEVICE_TYPE, MEDIA_SERVER_DEVICE_TYPE );
    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_sd, "Error sending search request: %s", UpnpGetErrorMessage( i_res ) );
        return NULL;
    }

    /* Search for Sat Ip servers*/
    i_res = UpnpSearchAsync( p_sys->p_upnp->handle(), 5,
            SATIP_SERVER_DEVICE_TYPE, MEDIA_SERVER_DEVICE_TYPE );
    if( i_res != UPNP_E_SUCCESS )
        msg_Err( p_sd, "Error sending search request: %s", UpnpGetErrorMessage( i_res ) );
    return NULL;
}

/*
 * Initializes UPNP instance.
 */
static int OpenSD( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = new (std::nothrow) services_discovery_sys_t();

    if( !( p_sd->p_sys = p_sys ) )
        return VLC_ENOMEM;

    p_sd->description = _("Universal Plug'n'Play");

    p_sys->p_upnp = UpnpInstanceWrapper::get( p_this );
    if ( !p_sys->p_upnp )
    {
        delete p_sys;
        return VLC_EGENERIC;
    }

    try
    {
        p_sys->p_server_list = std::make_shared<SD::MediaServerList>( p_sd );
    }
    catch ( const std::bad_alloc& )
    {
        msg_Err( p_sd, "Failed to create a MediaServerList");
        p_sys->p_upnp->release();
        delete p_sys;
        return VLC_EGENERIC;
    }
    p_sys->p_upnp->addListener( p_sys->p_server_list );

    /* XXX: Contrary to what the libupnp doc states, UpnpSearchAsync is
     * blocking (select() and send() are called). Therefore, Call
     * UpnpSearchAsync from an other thread. */
    if ( vlc_clone( &p_sys->thread, SearchThread, p_this,
                    VLC_THREAD_PRIORITY_LOW ) )
    {
        p_sys->p_upnp->removeListener( p_sys->p_server_list );
        p_sys->p_upnp->release();
        delete p_sys;
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*
 * Releases resources.
 */
static void CloseSD( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = reinterpret_cast<services_discovery_sys_t *>( p_sd->p_sys );

    vlc_join( p_sys->thread, NULL );
    p_sys->p_upnp->removeListener( p_sys->p_server_list );
    p_sys->p_upnp->release();
    delete p_sys;
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

    char *psz_satip_channellist = config_GetPsz("satip-channelist");
    if( !psz_satip_channellist ) {
        psz_satip_channellist = strdup("Auto");
    }

    if( unlikely( !psz_satip_channellist ) )
        return;

    vlc_url_t url;
    vlc_UrlParse( &url, psz_base_url );

    /* Part 1: a user may have provided a custom playlist url */
    if (strncmp(psz_satip_channellist, "CustomList", 10) == 0) {
        char *psz_satip_playlist_url = config_GetPsz( "satip-channellist-url" );
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
int MediaServerList::onEvent( Upnp_EventType event_type, UpnpEventPtr p_event, void* p_user_data )
{
    if (p_user_data != MEDIA_SERVER_DEVICE_TYPE)
        return 0;

    switch( event_type )
    {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        const UpnpDiscovery* p_discovery = ( const UpnpDiscovery* )p_event;

        IXML_Document *p_description_doc = NULL;

        int i_res;
        i_res = UpnpDownloadXmlDoc( UpnpDiscovery_get_Location_cstr( p_discovery ), &p_description_doc );

        if ( i_res != UPNP_E_SUCCESS )
        {
            msg_Warn( m_sd, "Could not download device description! "
                            "Fetching data from %s failed: %s",
                            UpnpDiscovery_get_Location_cstr( p_discovery ), UpnpGetErrorMessage( i_res ) );
            return i_res;
        }
        parseNewServer( p_description_doc, UpnpDiscovery_get_Location_cstr( p_discovery ) );
        ixmlDocument_free( p_description_doc );
    }
    break;

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        const UpnpDiscovery* p_discovery = ( const UpnpDiscovery* )p_event;
        removeServer( UpnpDiscovery_get_DeviceID_cstr( p_discovery ) );
    }
    break;

    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
    {
        msg_Warn( m_sd, "subscription complete" );
    }
        break;

    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
    {
        msg_Warn( m_sd, "search timeout" );
    }
        break;

    case UPNP_EVENT_RECEIVED:
    case UPNP_EVENT_AUTORENEWAL_FAILED:
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
        // Those are for the access part
        break;

    default:
    {
        msg_Err( m_sd, "Unhandled event, please report ( type=%d )", event_type );
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
            vlc_tick_t i_duration = INPUT_DURATION_INDEFINITE;
            const char* psz_resource_url = xml_getChildElementValue( p_resource, "res" );
            if( !psz_resource_url )
                return NULL;
            const char* psz_duration = ixmlElement_getAttribute( p_resource, "duration" );
            if ( psz_duration )
            {
                int i_hours, i_minutes, i_seconds;
                if( sscanf( psz_duration, "%d:%02d:%02d", &i_hours, &i_minutes, &i_seconds ) )
                    i_duration = vlc_tick_from_sec( i_hours * 3600 + i_minutes * 60 +
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
}

void Upnp_i11e_cb::waitAndRelease( void )
{
    m_sem.wait_i11e();

    int refCount;
    {
        vlc::threads::mutex_locker lock( m_lock );
        refCount = --m_refCount;
    }
    if ( refCount == 0 )
    {
        /* The run callback is processed, we can destroy this object */
        delete this;
    }
    /* Otherwise interrupted, let the run callback destroy this object */
}

int Upnp_i11e_cb::run( Upnp_EventType eventType, UpnpEventPtr p_event, void *p_cookie )
{
    Upnp_i11e_cb *self = static_cast<Upnp_i11e_cb*>( p_cookie );

    self->m_lock.lock();
    if ( --self->m_refCount == 0 )
    {
        /* Interrupted, we can destroy self */
        self->m_lock.unlock();
        delete self;
        return 0;
    }
    /* Process the user callback_ */
    self->m_callback( eventType, p_event, self->m_cookie);
    self->m_lock.unlock();

    /* Signal that the callback is processed */
    self->m_sem.post();
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
                                           const char* psz_starting_index,
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
            CONTENT_DIRECTORY_SERVICE_TYPE, "StartingIndex", psz_starting_index );
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
    std::string StartingIndex = "0";
    std::string RequestedCount = "5000";
    const char* psz_TotalMatches = "0";
    const char* psz_NumberReturned = "0";
    long  l_reqCount = 0;

    do
    {
        IXML_Document* p_response = _browseAction( m_psz_objectId,
                                                  "BrowseDirectChildren",
                                                  "*",
                                                  StartingIndex.c_str(),
                                                  // Some servers don't understand "0" as "no-limit"
                                                  RequestedCount.c_str(), /* RequestedCount */
                                                  "" /* SortCriteria */
                                                  );
        if ( !p_response )
        {
            msg_Err( m_access, "No response from browse() action" );
            return false;
        }

        psz_TotalMatches = xml_getChildElementValue( (IXML_Element*)p_response, "TotalMatches" );
        psz_NumberReturned = xml_getChildElementValue( (IXML_Element*)p_response, "NumberReturned" );

        StartingIndex = std::to_string(  std::stol(psz_NumberReturned) + std::stol(StartingIndex) );
        l_reqCount = std::stol(psz_TotalMatches) - std::stol(StartingIndex) ;
        RequestedCount = std::to_string(l_reqCount);

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
            for ( unsigned int i = 0; i < ixmlNodeList_length( containerNodeList ); i++)
                addContainer( (IXML_Element*)ixmlNodeList_item( containerNodeList, i ) );
            ixmlNodeList_free( containerNodeList );
        }

        IXML_NodeList* itemNodeList = ixmlDocument_getElementsByTagName( p_result,
                                                                        "item" );
        if ( itemNodeList )
        {
            for ( unsigned int i = 0; i < ixmlNodeList_length( itemNodeList ); i++)
                addItem( (IXML_Element*)ixmlNodeList_item( itemNodeList, i ) );
            ixmlNodeList_free( itemNodeList );
        }

        ixmlDocument_free( p_result );
    }
    while( l_reqCount );
    return true;
}

static int ReadDirectory( stream_t *p_access, input_item_node_t* p_node )
{
    MediaServer server( p_access, p_node );

    if ( !server.fetchContents() )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int OpenAccess( vlc_object_t *p_this )
{
    stream_t* p_access = (stream_t*)p_this;
    access_sys_t* p_sys = new(std::nothrow) access_sys_t;
    if ( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    p_access->p_sys = p_sys;
    p_sys->p_upnp = UpnpInstanceWrapper::get( p_this );
    if ( !p_sys->p_upnp )
    {
        delete p_sys;
        return VLC_EGENERIC;
    }

    p_access->pf_readdir = ReadDirectory;
    p_access->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}

static void CloseAccess( vlc_object_t* p_this )
{
    stream_t* p_access = (stream_t*)p_this;
    access_sys_t *sys = (access_sys_t *)p_access->p_sys;

    sys->p_upnp->release();
    delete sys;
}

} // namespace Access

namespace RD
{

/**
 * Crafts an MRL with the 'dlna' stream out
 * containing the host and port.
 *
 * \param psz_location URL to the MediaRenderer device description doc
 */
const char *getUrl(const char *psz_location)
{
    char *psz_res;
    vlc_url_t url;

    vlc_UrlParse(&url, psz_location);
    if (asprintf(&psz_res, "dlna://%s:%d", url.psz_host, url.i_port) < 0)
    {
        vlc_UrlClean(&url);
        return NULL;
    }
    vlc_UrlClean(&url);
    return psz_res;
}

MediaRendererDesc::MediaRendererDesc( const std::string& udn,
                                      const std::string& fName,
                                      const std::string& base,
                                      const std::string& loc )
    : UDN( udn )
    , friendlyName( fName )
    , base_url( base )
    , location( loc )
    , inputItem( NULL )
{
}

MediaRendererDesc::~MediaRendererDesc()
{
    if (inputItem)
        vlc_renderer_item_release(inputItem);
}

MediaRendererList::MediaRendererList(vlc_renderer_discovery_t *p_rd)
    : m_rd( p_rd )
{
}

MediaRendererList::~MediaRendererList()
{
    vlc_delete_all(m_list);
}

bool MediaRendererList::addRenderer(MediaRendererDesc *desc)
{
    const char* psz_url = getUrl(desc->location.c_str());

    char *extra_sout;

    if (asprintf(&extra_sout, "base_url=%s,url=%s", desc->base_url.c_str(),
                        desc->location.c_str()) < 0)
        return false;
    desc->inputItem = vlc_renderer_item_new("stream_out_dlna",
                                            desc->friendlyName.c_str(),
                                            psz_url,
                                            extra_sout,
                                            NULL, "", 3);
    free(extra_sout);
    if ( !desc->inputItem )
        return false;
    msg_Dbg( m_rd, "Adding renderer '%s' with uuid %s",
                        desc->friendlyName.c_str(),
                        desc->UDN.c_str() );
    vlc_rd_add_item(m_rd, desc->inputItem);
    m_list.push_back(desc);
    return true;
}

MediaRendererDesc* MediaRendererList::getRenderer( const std::string& udn )
{
    std::vector<MediaRendererDesc*>::const_iterator it = m_list.begin();
    std::vector<MediaRendererDesc*>::const_iterator ite = m_list.end();

    for ( ; it != ite; ++it )
    {
        if( udn == (*it)->UDN )
            return *it;
    }
    return NULL;
}

void MediaRendererList::removeRenderer( const std::string& udn )
{
    MediaRendererDesc* p_renderer = getRenderer( udn );
    if ( !p_renderer )
        return;

    assert( p_renderer->inputItem );

    std::vector<MediaRendererDesc*>::iterator it =
                        std::find( m_list.begin(),
                                   m_list.end(),
                                   p_renderer );
    if( it != m_list.end() )
    {
        msg_Dbg( m_rd, "Removing renderer '%s' with uuid %s",
                            p_renderer->friendlyName.c_str(),
                            p_renderer->UDN.c_str() );
        m_list.erase( it );
    }
    delete p_renderer;
}

void MediaRendererList::parseNewRenderer( IXML_Document* doc,
                                          const std::string& location)
{
    assert(!location.empty());
    if( var_InheritInteger(m_rd, "verbose") >= 4 )
        msg_Dbg( m_rd , "Got device desc doc:\n%s", ixmlPrintDocument( doc ));

    const char* psz_base_url = nullptr;
    IXML_NodeList* p_device_nodes = nullptr;

    /* Fallback to the Device description URL basename
    * if no base URL is advertised */
    psz_base_url = parseBaseUrl( doc );
    if( !psz_base_url && !location.empty() )
    {
        psz_base_url = location.c_str();
    }

    p_device_nodes = ixmlDocument_getElementsByTagName( doc, "device" );
    if ( !p_device_nodes )
        return;

    for ( unsigned int i = 0; i < ixmlNodeList_length( p_device_nodes ); i++ )
    {
        IXML_Element* p_device_element = ( IXML_Element* ) ixmlNodeList_item( p_device_nodes, i );
        const char* psz_device_name = nullptr;
        const char* psz_udn = nullptr;

        if( !p_device_element )
            continue;

        psz_device_name = xml_getChildElementValue( p_device_element, "friendlyName");
        if (psz_device_name == nullptr)
            msg_Dbg( m_rd, "No friendlyName!" );

        psz_udn = xml_getChildElementValue( p_device_element, "UDN");
        if (psz_udn == nullptr)
        {
            msg_Err( m_rd, "No UDN" );
            continue;
        }

        /* Check if renderer is already added */
        if (getRenderer( psz_udn ))
        {
            msg_Warn( m_rd, "Renderer with UDN '%s' already exists.", psz_udn );
            continue;
        }

        MediaRendererDesc *p_renderer = new MediaRendererDesc(psz_udn,
                                                psz_device_name,
                                                psz_base_url,
                                                location);
        if (!addRenderer( p_renderer ))
            delete p_renderer;
    }
    ixmlNodeList_free( p_device_nodes );
}

int MediaRendererList::onEvent( Upnp_EventType event_type,
                                UpnpEventPtr Event,
                                void *p_user_data )
{
    if (p_user_data != MEDIA_RENDERER_DEVICE_TYPE)
        return 0;

    switch (event_type)
    {
        case UPNP_DISCOVERY_SEARCH_RESULT:
        {
            const UpnpDiscovery *p_discovery = (const UpnpDiscovery*)Event;
            IXML_Document *p_doc = NULL;
            int i_res;

            i_res = UpnpDownloadXmlDoc( UpnpDiscovery_get_Location_cstr( p_discovery ), &p_doc);
            if (i_res != UPNP_E_SUCCESS)
            {
                fprintf(stderr, "%s\n", UpnpDiscovery_get_Location_cstr( p_discovery ));
                return i_res;
            }
            parseNewRenderer(p_doc, UpnpDiscovery_get_Location_cstr( p_discovery ) );
            ixmlDocument_free(p_doc);
        }
        break;

        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
        {
            const UpnpDiscovery* p_discovery = ( const UpnpDiscovery* )Event;

            removeRenderer( UpnpDiscovery_get_DeviceID_cstr ( p_discovery ) );
        }
        break;

        case UPNP_DISCOVERY_SEARCH_TIMEOUT:
        {
                msg_Warn( m_rd, "search timeout" );
        }
        break;

        default:
        break;
    }
    return UPNP_E_SUCCESS;
}

void *SearchThread(void *data)
{
    vlc_renderer_discovery_t *p_rd = (vlc_renderer_discovery_t*)data;
    renderer_discovery_sys_t *p_sys = (renderer_discovery_sys_t*)p_rd->p_sys;
    int i_res;

    i_res = UpnpSearchAsync(p_sys->p_upnp->handle(), UPNP_SEARCH_TIMEOUT_SECONDS,
            MEDIA_RENDERER_DEVICE_TYPE, MEDIA_RENDERER_DEVICE_TYPE);
    if( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( p_rd, "Error sending search request: %s", UpnpGetErrorMessage( i_res ) );
        return NULL;
    }
    return data;
}

static int OpenRD( vlc_object_t *p_this )
{
    vlc_renderer_discovery_t *p_rd = ( vlc_renderer_discovery_t* )p_this;
    renderer_discovery_sys_t *p_sys  = new(std::nothrow) renderer_discovery_sys_t;

    if ( !p_sys )
        return VLC_ENOMEM;
    p_rd->p_sys = p_sys;
    p_sys->p_upnp = UpnpInstanceWrapper::get( p_this );

    if ( !p_sys->p_upnp )
    {
        delete p_sys;
        return VLC_EGENERIC;
    }

    try
    {
        p_sys->p_renderer_list = std::make_shared<RD::MediaRendererList>( p_rd );
    }
    catch ( const std::bad_alloc& )
    {
        msg_Err( p_rd, "Failed to create a MediaRendererList");
        p_sys->p_upnp->release();
        free(p_sys);
        return VLC_EGENERIC;
    }
    p_sys->p_upnp->addListener( p_sys->p_renderer_list );

    if( vlc_clone( &p_sys->thread, SearchThread, (void*)p_rd,
                    VLC_THREAD_PRIORITY_LOW ) )
        {
            msg_Err( p_rd, "Can't run the lookup thread" );
            p_sys->p_upnp->removeListener( p_sys->p_renderer_list );
            p_sys->p_upnp->release();
            delete p_sys;
            return VLC_EGENERIC;
        }
    return VLC_SUCCESS;
}

static void CloseRD( vlc_object_t *p_this )
{
    vlc_renderer_discovery_t *p_rd = ( vlc_renderer_discovery_t* )p_this;
    renderer_discovery_sys_t *p_sys = (renderer_discovery_sys_t*)p_rd->p_sys;

    vlc_join(p_sys->thread, NULL);
    p_sys->p_upnp->removeListener( p_sys->p_renderer_list );
    p_sys->p_upnp->release();
    delete p_sys;
}

} // namespace RD
