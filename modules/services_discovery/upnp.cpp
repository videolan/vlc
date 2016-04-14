/*****************************************************************************
 * upnp.cpp :  UPnP discovery module (libupnp)
 *****************************************************************************
 * Copyright (C) 2004-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org> (original plugin)
 *          Christian Henz <henz # c-lab.de>
 *          Mirsal Ennaime <mirsal dot ennaime at gmail dot com>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#include "upnp.hpp"

#include <vlc_access.h>
#include <vlc_plugin.h>
#include <vlc_interrupt.h>
#include <vlc_services_discovery.h>

#include <assert.h>
#include <limits.h>
#include <algorithm>

/*
 * Constants
*/
const char* MEDIA_SERVER_DEVICE_TYPE = "urn:schemas-upnp-org:device:MediaServer:1";
const char* CONTENT_DIRECTORY_SERVICE_TYPE = "urn:schemas-upnp-org:service:ContentDirectory:1";
const char* SATIP_SERVER_DEVICE_TYPE = "urn:ses-com:device:SatIPServer:1";

#define SATIP_SATELLITE N_("SAT>IP satellite")
#define SATIP_SATELLITE_LONG N_( "VLC will download the channel list for SAT>IP " \
"playback based on the chosen satellite.")
static const char *const ppsz_satip_satellites[] = {
    "ASTRA_19_2E", "ASTRA_28_2E", "ASTRA_23_5E", "eutelsat_13_0E", "eutelsat_09_0E",
    "eutelsat_05_0W", "hispasat_30_0W"
};
static const char *const ppsz_readible_satip_satellites[] = {
    "Astra 19.2°E", "Astra 28.2°E", "Astra 23.5°E", "Eutelsat 13.0°E", "Eutelsat 09.0°E",
    "Eutelsat 05.0°W", "Hispasat 30.0°W"
};

/*
 * VLC handle
 */
struct services_discovery_sys_t
{
    UpnpInstanceWrapper* p_upnp;
    SD::MediaServerList* p_server_list;
    vlc_thread_t         thread;
};

struct access_sys_t
{
    UpnpInstanceWrapper* p_upnp;
    Access::MediaServer* p_server;
};

UpnpInstanceWrapper* UpnpInstanceWrapper::s_instance;
vlc_mutex_t UpnpInstanceWrapper::s_lock = VLC_STATIC_MUTEX;

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

VLC_SD_PROBE_HELPER( "upnp", "Universal Plug'n'Play", SD_CAT_LAN )

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

    set_description( N_("SAT>IP") )
    add_string( "satip-satellite", "ASTRA_19_2E", SATIP_SATELLITE,
                SATIP_SATELLITE_LONG, false )
    change_string_list( ppsz_satip_satellites, ppsz_readible_satip_satellites )
    change_safe ()

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

    p_sys->p_server_list = new(std::nothrow) SD::MediaServerList( p_sd );
    if ( unlikely( p_sys->p_server_list == NULL ) )
    {
        free(p_sys);
        return VLC_ENOMEM;
    }

    p_sys->p_upnp = UpnpInstanceWrapper::get( p_this, SD::MediaServerList::Callback, p_sys->p_server_list );
    if ( !p_sys->p_upnp )
    {
        delete p_sys->p_server_list;
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
        delete p_sys->p_server_list;
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
    delete p_sys->p_server_list;
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
        vlc_gc_decref( inputItem );
}

/*
 * MediaServerList class
 */
MediaServerList::MediaServerList( services_discovery_t* p_sd )
    : p_sd_( p_sd )
{
    vlc_mutex_init( &lock_ );
}

MediaServerList::~MediaServerList()
{
    vlc_delete_all(list_);
    vlc_mutex_destroy( &lock_ );
}

bool MediaServerList::addServer( MediaServerDesc* desc )
{
    vlc_mutex_locker lock( &lock_ );
    input_item_t* p_input_item = NULL;
    if ( getServer( desc->UDN ) )
        return false;

    msg_Dbg( p_sd_, "Adding server '%s' with uuid '%s'", desc->friendlyName.c_str(), desc->UDN.c_str() );

    if ( desc->isSatIp )
    {
        p_input_item = input_item_NewDirectory( desc->location.c_str(),
                                                desc->friendlyName.c_str(),
                                                ITEM_NET );
    } else {
        char* psz_mrl;
        if( asprintf(&psz_mrl, "upnp://%s?ObjectID=0", desc->location.c_str() ) < 0 )
            return false;

        p_input_item = input_item_NewDirectory( psz_mrl,
                                                desc->friendlyName.c_str(),
                                                ITEM_NET );
        free( psz_mrl );
    }
    if ( !p_input_item )
        return false;

    if ( desc->iconUrl.empty() == false )
        input_item_SetArtworkURL( p_input_item, desc->iconUrl.c_str() );
    desc->inputItem = p_input_item;
    input_item_SetDescription( p_input_item, desc->UDN.c_str() );
    services_discovery_AddItem( p_sd_, p_input_item, NULL );
    list_.push_back( desc );

    return true;
}

MediaServerDesc* MediaServerList::getServer( const std::string& udn )
{
    std::vector<MediaServerDesc*>::const_iterator it = list_.begin();
    std::vector<MediaServerDesc*>::const_iterator ite = list_.end();

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
        msg_Err( p_sd_, "Null IXML_Document" );
        return;
    }

    if ( location.empty() )
    {
        msg_Err( p_sd_, "Empty location" );
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
            msg_Warn( p_sd_, "No deviceType found!" );
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
            msg_Warn( p_sd_, "No UDN!" );
            continue;
        }

        /* Check if server is already added */
        if ( p_sd_->p_sys->p_server_list->getServer( psz_udn ) )
        {
            msg_Warn( p_sd_, "Server with uuid '%s' already exists.", psz_udn );
            continue;
        }

        const char* psz_friendly_name =
                   xml_getChildElementValue( p_device_element,
                                             "friendlyName" );

        if ( !psz_friendly_name )
        {
            msg_Dbg( p_sd_, "No friendlyName!" );
            continue;
        }

        std::string iconUrl = getIconURL( p_device_element, psz_base_url );

        // We now have basic info, we need to get the content browsing url
        // so the access module can browse without fetching the manifest again

        if ( !strncmp( SATIP_SERVER_DEVICE_TYPE, psz_device_type,
                strlen( SATIP_SERVER_DEVICE_TYPE ) - 1 ) )
        {
            /* Check for SAT>IP m3u list, which is provided by some off-standard devices */
            const char* psz_m3u_url = xml_getChildElementValue( p_device_element, "satip:X_SATIPM3U" );
            SD::MediaServerDesc* p_server = NULL;
            if ( psz_m3u_url ) {

                if ( strncmp( "http://", psz_m3u_url, 7) && strncmp( "https://", psz_m3u_url, 8) )
                {
                    char* psz_url = NULL;
                    if ( UpnpResolveURL2( psz_base_url, psz_m3u_url, &psz_url ) == UPNP_E_SUCCESS )
                    {
                        p_server = new(std::nothrow) SD::MediaServerDesc( psz_udn, psz_friendly_name, psz_url, iconUrl );
                        free(psz_url);
                    }
                } else
                    p_server = new(std::nothrow) SD::MediaServerDesc( psz_udn, psz_friendly_name, psz_m3u_url, iconUrl );

                if ( unlikely( !p_server ) )
                    break;

                p_server->isSatIp = true;
                if ( !addServer( p_server ) )
                    delete p_server;
            } else {
                /* if no playlist is found, add a playlist from the web based on the chosen
                 * satellite, which will be processed by a lua script a bit later */
                char *psz_satellite = config_GetPsz(p_sd_, "satip-satellite");
                if( !psz_satellite ) {
                    break;
                }
                char *psz_url;
                vlc_url_t url;
                vlc_UrlParse( &url, psz_base_url );

                if (asprintf( &psz_url, "http/lua://www.satip.info/Playlists/%s.m3u?device=%s",
                             psz_satellite,
                             url.psz_host ) < 0 ) {
                    vlc_UrlClean( &url );
                    free( psz_satellite );
                    continue;
                }
                free( psz_satellite );
                vlc_UrlClean( &url );

                p_server = new(std::nothrow) SD::MediaServerDesc( psz_udn,
                                                                  psz_friendly_name, psz_url, iconUrl );

                p_server->isSatIp = true;
                if( !addServer( p_server ) ) {
                    delete p_server;
                }
                free( psz_url );
            }

            continue;
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
                msg_Warn( p_sd_, "No service type found." );
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
                msg_Warn( p_sd_, "No control url found." );
                continue;
            }

            /* Try to browse content directory. */
            char* psz_url = ( char* ) malloc( strlen( psz_base_url ) + strlen( psz_control_url ) + 1 );
            if ( psz_url )
            {
                if ( UpnpResolveURL( psz_base_url, psz_control_url, psz_url ) == UPNP_E_SUCCESS )
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
                else
                    free( psz_url );
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

void MediaServerList::removeServer( const std::string& udn )
{
    vlc_mutex_locker lock( &lock_ );

    MediaServerDesc* p_server = getServer( udn );
    if ( !p_server )
        return;

    msg_Dbg( p_sd_, "Removing server '%s'", p_server->friendlyName.c_str() );

    assert(p_server->inputItem);
    services_discovery_RemoveItem( p_sd_, p_server->inputItem );

    std::vector<MediaServerDesc*>::iterator it = std::find(list_.begin(), list_.end(), p_server);
    if (it != list_.end())
    {
        list_.erase( it );
    }
    delete p_server;
}

/*
 * Handles servers listing UPnP events
 */
int MediaServerList::Callback( Upnp_EventType event_type, void* p_event, void* p_user_data )
{
    MediaServerList* self = static_cast<MediaServerList*>( p_user_data );
    services_discovery_t* p_sd = self->p_sd_;

    switch( event_type )
    {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        struct Upnp_Discovery* p_discovery = ( struct Upnp_Discovery* )p_event;

        IXML_Document *p_description_doc = NULL;

        int i_res;
        i_res = UpnpDownloadXmlDoc( p_discovery->Location, &p_description_doc );
        if ( i_res != UPNP_E_SUCCESS )
        {
            msg_Warn( p_sd, "Could not download device description! "
                            "Fetching data from %s failed: %s",
                            p_discovery->Location, UpnpGetErrorMessage( i_res ) );
            return i_res;
        }
        self->parseNewServer( p_description_doc, p_discovery->Location );
        ixmlDocument_free( p_description_doc );
    }
    break;

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        struct Upnp_Discovery* p_discovery = ( struct Upnp_Discovery* )p_event;

        self->removeServer( p_discovery->DeviceId );
    }
    break;

    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
        msg_Warn( p_sd, "subscription complete" );
        break;

    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
        msg_Warn( p_sd, "search timeout" );
        break;

    case UPNP_EVENT_RECEIVED:
    case UPNP_EVENT_AUTORENEWAL_FAILED:
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
        // Those are for the access part
        break;

    default:
        msg_Err( p_sd, "Unhandled event, please report ( type=%d )", event_type );
        break;
    }

    return UPNP_E_SUCCESS;
}

}

namespace Access
{

Upnp_i11e_cb::Upnp_i11e_cb( Upnp_FunPtr callback, void *cookie )
    : refCount_( 2 ) /* 2: owned by the caller, and the Upnp Async function */
    , callback_( callback )
    , cookie_( cookie )

{
    vlc_mutex_init( &lock_ );
    vlc_sem_init( &sem_, 0 );
}

Upnp_i11e_cb::~Upnp_i11e_cb()
{
    vlc_mutex_destroy( &lock_ );
    vlc_sem_destroy( &sem_ );
}

void Upnp_i11e_cb::waitAndRelease( void )
{
    vlc_sem_wait_i11e( &sem_ );

    vlc_mutex_lock( &lock_ );
    if ( --refCount_ == 0 )
    {
        /* The run callback is processed, we can destroy this object */
        vlc_mutex_unlock( &lock_ );
        delete this;
    } else
    {
        /* Interrupted, let the run callback destroy this object */
        vlc_mutex_unlock( &lock_ );
    }
}

int Upnp_i11e_cb::run( Upnp_EventType eventType, void *p_event, void *p_cookie )
{
    Upnp_i11e_cb *self = static_cast<Upnp_i11e_cb*>( p_cookie );

    vlc_mutex_lock( &self->lock_ );
    if ( --self->refCount_ == 0 )
    {
        /* Interrupted, we can destroy self */
        vlc_mutex_unlock( &self->lock_ );
        delete self;
        return 0;
    }
    /* Process the user callback_ */
    self->callback_( eventType, p_event, self->cookie_);
    vlc_mutex_unlock( &self->lock_ );

    /* Signal that the callback is processed */
    vlc_sem_post( &self->sem_ );
    return 0;
}

MediaServer::MediaServer( access_t *p_access )
    : psz_root_( NULL )
    , psz_objectId_( NULL )
    , access_( p_access )
    , xmlDocument_( NULL )
    , containerNodeList_( NULL )
    , containerNodeIndex_( 0 )
    , itemNodeList_( NULL )
    , itemNodeIndex_( 0 )
{
    vlc_url_t url;
    vlc_UrlParse( &url, p_access->psz_location );
    if ( asprintf( &psz_root_, "%s://%s:%u%s", url.psz_protocol,
                  url.psz_host, url.i_port ? url.i_port : 80, url.psz_path ) < 0 )
        psz_root_ = NULL;

    if ( url.psz_option && !strncmp( url.psz_option, "ObjectID=", strlen( "ObjectID=" ) ) )
        psz_objectId_ = strdup( &url.psz_option[strlen( "ObjectID=" )] );
    vlc_UrlClean( &url );
}

MediaServer::~MediaServer()
{
    ixmlNodeList_free( containerNodeList_ );
    ixmlNodeList_free( itemNodeList_ );
    ixmlDocument_free( xmlDocument_ );
    free( psz_objectId_ );
    free( psz_root_ );
}

input_item_t* MediaServer::newItem( const char *objectID, const char *title )
{
    char* psz_url;

    if( asprintf( &psz_url, "upnp://%s?ObjectID=%s", psz_root_, objectID ) < 0 )
        return NULL;

    input_item_t* p_item = input_item_NewDirectory( psz_url, title, ITEM_NET );
    free( psz_url);
    return p_item;
}

input_item_t* MediaServer::newItem(const char* title, const char*,
                                   mtime_t duration, const char* psz_url)
{
    return input_item_NewFile( psz_url, title, duration, ITEM_NET );
}

int MediaServer::sendActionCb( Upnp_EventType eventType,
                               void *p_event, void *p_cookie )
{
    if( eventType != UPNP_CONTROL_ACTION_COMPLETE )
        return 0;
    IXML_Document** pp_sendActionResult = (IXML_Document** )p_cookie;
    Upnp_Action_Complete *p_result = (Upnp_Action_Complete *)p_event;

    /* The only way to dup the result is to print it and parse it again */
    DOMString tmpStr = ixmlPrintNode( ( IXML_Node * ) p_result->ActionResult );
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

    int i_res;

    if ( vlc_killed() )
        return NULL;

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "ObjectID", psz_object_id_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( access_, "AddToAction 'ObjectID' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "StartingIndex", "0" );
    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( access_, "AddToAction 'StartingIndex' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "BrowseFlag", psz_browser_flag_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( access_, "AddToAction 'BrowseFlag' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "Filter", psz_filter_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( access_, "AddToAction 'Filter' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "RequestedCount", psz_requested_count_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( access_, "AddToAction 'RequestedCount' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    i_res = UpnpAddToAction( &p_action, "Browse",
            CONTENT_DIRECTORY_SERVICE_TYPE, "SortCriteria", psz_sort_criteria_ );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Dbg( access_, "AddToAction 'SortCriteria' failed: %s",
                UpnpGetErrorMessage( i_res ) );
        goto browseActionCleanup;
    }

    /* Setup an interruptible callback that will call sendActionCb if not
     * interrupted by vlc_interrupt_kill */
    i11eCb = new Upnp_i11e_cb( sendActionCb, &p_response );
    i_res = UpnpSendActionAsync( access_->p_sys->p_upnp->handle(),
              psz_root_,
              CONTENT_DIRECTORY_SERVICE_TYPE,
              NULL, /* ignored in SDK, must be NULL */
              p_action,
              Upnp_i11e_cb::run, i11eCb );

    if ( i_res != UPNP_E_SUCCESS )
    {
        msg_Err( access_, "%s when trying the send() action with URL: %s",
                UpnpGetErrorMessage( i_res ), access_->psz_location );
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
void MediaServer::fetchContents()
{
    IXML_Document* p_response = _browseAction( psz_objectId_,
                                      "BrowseDirectChildren",
                                      "*",
                                      "0", /* RequestedCount */
                                      "" /* SortCriteria */
                                      );
    if ( !p_response )
    {
        msg_Err( access_, "No response from browse() action" );
        return;
    }

    xmlDocument_ = parseBrowseResult( p_response );

    ixmlDocument_free( p_response );

    if ( !xmlDocument_ )
    {
        msg_Err( access_, "browse() response parsing failed" );
        return;
    }

#ifndef NDEBUG
    msg_Dbg( access_, "Got DIDL document: %s", ixmlPrintDocument( xmlDocument_ ) );
#endif

    containerNodeList_ = ixmlDocument_getElementsByTagName( xmlDocument_, "container" );
    itemNodeList_ = ixmlDocument_getElementsByTagName( xmlDocument_, "item" );
}

input_item_t* MediaServer::getNextItem()
{
    input_item_t *p_item = NULL;

    if( !xmlDocument_ )
    {
        fetchContents();
        if( !xmlDocument_ )
            return NULL;
    }

    if ( containerNodeList_ )
    {
        for ( ; !p_item && containerNodeIndex_ < ixmlNodeList_length( containerNodeList_ )
              ; containerNodeIndex_++ )
        {
            IXML_Element* containerElement = (IXML_Element*)ixmlNodeList_item( containerNodeList_,
                                                                                containerNodeIndex_ );

            const char* objectID = ixmlElement_getAttribute( containerElement,
                                                             "id" );
            if ( !objectID )
                continue;

            const char* title = xml_getChildElementValue( containerElement,
                                                          "dc:title" );
            if ( !title )
                continue;
            p_item = newItem(objectID, title);
        }
    }

    if( itemNodeList_ )
    {
        for ( ; !p_item && itemNodeIndex_ < ixmlNodeList_length( itemNodeList_ ) ; itemNodeIndex_++ )
        {
            IXML_Element* itemElement = ( IXML_Element* )ixmlNodeList_item( itemNodeList_,
                                                                            itemNodeIndex_ );

            const char* objectID = ixmlElement_getAttribute( itemElement, "id" );
            if ( !objectID )
                continue;

            const char* title = xml_getChildElementValue( itemElement, "dc:title" );
            if ( !title )
                continue;

            const char* psz_subtitles = xml_getChildElementValue( itemElement, "sec:CaptionInfo" );

            if ( !psz_subtitles )
                psz_subtitles = xml_getChildElementValue( itemElement, "sec:CaptionInfoEx" );

            if ( !psz_subtitles )
                psz_subtitles = xml_getChildElementValue( itemElement, "pv:subtitlefile" );

            /* Try to extract all resources in DIDL */
            IXML_NodeList* p_resource_list = ixmlDocument_getElementsByTagName( (IXML_Document*) itemElement, "res" );
            if ( p_resource_list && ixmlNodeList_length( p_resource_list ) > 0 )
            {
                mtime_t i_duration = -1;
                IXML_Element* p_resource = ( IXML_Element* ) ixmlNodeList_item( p_resource_list, 0 );
                const char* psz_resource_url = xml_getChildElementValue( p_resource, "res" );
                if( !psz_resource_url )
                    continue;
                const char* psz_duration = ixmlElement_getAttribute( p_resource, "duration" );

                if ( psz_duration )
                {
                    int i_hours, i_minutes, i_seconds;
                    if( sscanf( psz_duration, "%d:%02d:%02d", &i_hours, &i_minutes, &i_seconds ) )
                        i_duration = INT64_C(1000000) * ( i_hours * 3600 +
                                                          i_minutes * 60 +
                                                          i_seconds );
                }

                p_item = newItem( title, objectID, i_duration,
                                  psz_resource_url );
                if ( p_item != NULL )
                {
                    const char* psz_artist = xml_getChildElementValue( itemElement, "upnp:artist" );
                    if ( psz_artist != NULL )
                        input_item_SetArtist( p_item, psz_artist );
                    const char* psz_genre = xml_getChildElementValue( itemElement, "upnp:genre" );
                    if ( psz_genre != NULL )
                        input_item_SetGenre( p_item, psz_genre );
                    const char* psz_album = xml_getChildElementValue( itemElement, "upnp:album" );
                    if ( psz_album != NULL )
                        input_item_SetAlbum( p_item, psz_album );
                    const char* psz_date = xml_getChildElementValue( itemElement, "dc:date" );
                    if ( psz_date != NULL )
                        input_item_SetDate( p_item, psz_date );
                    const char* psz_orig_track_nb = xml_getChildElementValue( itemElement, "upnp:originalTrackNumber" );
                    if ( psz_orig_track_nb != NULL )
                        input_item_SetTrackNumber( p_item, psz_orig_track_nb );
                    const char* psz_album_artist = xml_getChildElementValue( itemElement, "upnp:albumArtist" );
                    if ( psz_album_artist != NULL )
                        input_item_SetAlbumArtist( p_item, psz_album_artist );
                    const char* psz_albumArt = xml_getChildElementValue( itemElement, "upnp:albumArtURI" );
                    if ( psz_albumArt != NULL )
                        input_item_SetArtworkURL( p_item, psz_albumArt );
                }
            }
            ixmlNodeList_free( p_resource_list );
        }
    }

    return p_item;
}

static input_item_t* ReadDirectory( access_t *p_access )
{
    return p_access->p_sys->p_server->getNextItem();
}

static int ControlDirectory( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
    case ACCESS_IS_DIRECTORY:
        *va_arg( args, bool * ) = true; /* is sorted */
        *va_arg( args, bool * ) = true; /* might loop */
        break;
    default:
        return access_vaDirectoryControlHelper( p_access, i_query, args );
    }

    return VLC_SUCCESS;
}

static int Open( vlc_object_t *p_this )
{
    access_t* p_access = (access_t*)p_this;
    access_sys_t* p_sys = new(std::nothrow) access_sys_t;
    if ( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    p_access->p_sys = p_sys;
    p_sys->p_server = new(std::nothrow) MediaServer( p_access );
    if ( !p_sys->p_server )
    {
        delete p_sys;
        return VLC_EGENERIC;
    }
    p_sys->p_upnp = UpnpInstanceWrapper::get( p_this, NULL, NULL );
    if ( !p_sys->p_upnp )
    {
        delete p_sys->p_server;
        delete p_sys;
        return VLC_EGENERIC;
    }

    p_access->pf_readdir = ReadDirectory;
    p_access->pf_control = ControlDirectory;

    return VLC_SUCCESS;
}

static void Close( vlc_object_t* p_this )
{
    access_t* p_access = (access_t*)p_this;
    p_access->p_sys->p_upnp->release( false );
    delete p_access->p_sys->p_server;
    delete p_access->p_sys;
}

}

UpnpInstanceWrapper::UpnpInstanceWrapper()
    : handle_( -1 )
    , opaque_( NULL )
    , callback_( NULL )
    , refcount_( 0 )
{
    vlc_mutex_init( &callback_lock_ );
}

UpnpInstanceWrapper::~UpnpInstanceWrapper()
{
    UpnpUnRegisterClient( handle_ );
    UpnpFinish();
    vlc_mutex_destroy( &callback_lock_ );
}

UpnpInstanceWrapper *UpnpInstanceWrapper::get(vlc_object_t *p_obj, Upnp_FunPtr callback, SD::MediaServerList *opaque)
{
    vlc_mutex_locker lock( &s_lock );
    if ( s_instance == NULL )
    {
        UpnpInstanceWrapper* instance = new(std::nothrow) UpnpInstanceWrapper;
        if ( unlikely( !instance ) )
            return NULL;

    #ifdef UPNP_ENABLE_IPV6
        char* psz_miface = var_InheritString( p_obj, "miface" );
        msg_Info( p_obj, "Initializing libupnp on '%s' interface", psz_miface );
        int i_res = UpnpInit2( psz_miface, 0 );
        free( psz_miface );
    #else
        /* If UpnpInit2 isnt available, initialize on first IPv4-capable interface */
        int i_res = UpnpInit( 0, 0 );
    #endif
        if( i_res != UPNP_E_SUCCESS )
        {
            msg_Err( p_obj, "Initialization failed: %s", UpnpGetErrorMessage( i_res ) );
            delete instance;
            return NULL;
        }

        ixmlRelaxParser( 1 );

        /* Register a control point */
        i_res = UpnpRegisterClient( Callback, instance, &instance->handle_ );
        if( i_res != UPNP_E_SUCCESS )
        {
            msg_Err( p_obj, "Client registration failed: %s", UpnpGetErrorMessage( i_res ) );
            delete instance;
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
            return NULL;
        }
        s_instance = instance;
    }
    s_instance->refcount_++;
    // This assumes a single UPNP SD instance
    if (callback && opaque)
    {
        vlc_mutex_locker lock( &s_instance->callback_lock_ );
        assert(!s_instance->callback_ && !s_instance->opaque_);
        s_instance->opaque_ = opaque;
        s_instance->callback_ = callback;
    }
    return s_instance;
}

void UpnpInstanceWrapper::release(bool isSd)
{
    vlc_mutex_locker lock( &s_lock );
    if ( isSd )
    {
        vlc_mutex_locker lock( &callback_lock_ );
        callback_ = NULL;
        opaque_ = NULL;
    }
    if (--s_instance->refcount_ == 0)
    {
        delete s_instance;
        s_instance = NULL;
    }
}

UpnpClient_Handle UpnpInstanceWrapper::handle() const
{
    return handle_;
}

int UpnpInstanceWrapper::Callback(Upnp_EventType event_type, void *p_event, void *p_user_data)
{
    UpnpInstanceWrapper* self = static_cast<UpnpInstanceWrapper*>( p_user_data );
    vlc_mutex_locker lock( &self->callback_lock_ );
    if ( !self->callback_ )
        return 0;
    self->callback_( event_type, p_event, self->opaque_ );
    return 0;
}
