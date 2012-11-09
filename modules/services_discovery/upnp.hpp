/*****************************************************************************
 * upnp.hpp :  UPnP discovery module (libupnp) header
 *****************************************************************************
 * Copyright (C) 2004-2010 the VideoLAN team
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

#include <vector>
#include <string>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#include <vlc_common.h>

// Classes
class Container;

class MediaServer
{
public:

    static void parseDeviceDescription( IXML_Document* p_doc,
                                        const char*    psz_location,
                                        services_discovery_t* p_sd );

    MediaServer( const char* psz_udn,
                 const char* psz_friendly_name,
                 services_discovery_t* p_sd );

    ~MediaServer();

    const char* getUDN() const;
    const char* getFriendlyName() const;

    void setContentDirectoryEventURL( const char* psz_url );
    const char* getContentDirectoryEventURL() const;

    void setContentDirectoryControlURL( const char* psz_url );
    const char* getContentDirectoryControlURL() const;

    void subscribeToContentDirectory();
    void fetchContents();

    void setInputItem( input_item_t* p_input_item );
    input_item_t* getInputItem() const;

    bool compareSID( const char* psz_sid );

private:

    bool _fetchContents( Container* p_parent, int i_starting_index );
    void _buildPlaylist( Container* p_container, input_item_node_t *p_item_node );

    IXML_Document* _browseAction( const char*, const char*,
            const char*, const char*, const char*, const char* );

    services_discovery_t* _p_sd;

    Container* _p_contents;
    input_item_t* _p_input_item;

    std::string _UDN;
    std::string _friendly_name;

    std::string _content_directory_event_url;
    std::string _content_directory_control_url;

    int _i_subscription_timeout;
    int _i_content_directory_service_version;
    Upnp_SID _subscription_id;
};


class MediaServerList
{
public:

    MediaServerList( services_discovery_t* p_sd );
    ~MediaServerList();

    bool addServer( MediaServer* p_server );
    void removeServer( const char* psz_udn );

    MediaServer* getServer( const char* psz_udn );
    MediaServer* getServerBySID( const char* psz_sid );

private:

    services_discovery_t* _p_sd;

    std::vector<MediaServer*> _list;
};


class Item
{
public:

    Item( Container*  parent,
          const char* objectID,
          const char* title,
          const char* subtitles,
          const char* resource,
          mtime_t duration );
    ~Item();

    const char* getObjectID() const;
    const char* getTitle() const;
    const char* getResource() const;
    const char* getSubtitles() const;
    char* buildInputSlaveOption() const;
    char* buildSubTrackIdOption() const;
    mtime_t getDuration() const;

    void setInputItem( input_item_t* p_input_item );

private:

    input_item_t* _p_input_item;

    Container* _parent;
    std::string _objectID;
    std::string _title;
    std::string _resource;
    std::string _subtitles;
    mtime_t _duration;
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
    Container* getParent();

    void setInputItem( input_item_t* p_input_item );
    input_item_t* getInputItem() const;

private:

    input_item_t* _p_input_item;

    Container* _parent;

    std::string _objectID;
    std::string _title;
    std::vector<Item*> _items;
    std::vector<Container*> _containers;
};

