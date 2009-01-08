/*****************************************************************************
 * Upnp_intel.hpp :  UPnP discovery module (Intel SDK) header
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

#include <vector>
#include <string>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#include <vlc_common.h>

// Classes
class Container;


class Lockable
{
public:

    Lockable()
    {
        vlc_mutex_init( &_mutex );
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

    static void parseDeviceDescription( IXML_Document* doc,
                                        const char*    location,
                                        services_discovery_t* p_sd );

    MediaServer( const char* UDN,
                 const char* friendlyName,
                 services_discovery_t* p_sd );

    ~MediaServer();

    const char* getUDN() const;
    const char* getFriendlyName() const;

    void setContentDirectoryEventURL( const char* url );
    const char* getContentDirectoryEventURL() const;

    void setContentDirectoryControlURL( const char* url );
    const char* getContentDirectoryControlURL() const;

    void subscribeToContentDirectory();
    void fetchContents();

    void setInputItem( input_item_t* p_input_item );

    bool compareSID( const char* sid );

private:

    bool _fetchContents( Container* parent );
    void _buildPlaylist( Container* container );

    IXML_Document* _browseAction( const char*, const char*,
            const char*, const char*, const char*, const char* );

    services_discovery_t* _p_sd;

    Container* _contents;
    input_item_t* _inputItem;

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

    MediaServerList( services_discovery_t* p_sd );
    ~MediaServerList();

    bool addServer( MediaServer* s );
    void removeServer( const char* UDN );

    MediaServer* getServer( const char* UDN );
    MediaServer* getServerBySID( const char* );

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
          const char* resource );
    ~Item();

    const char* getObjectID() const;
    const char* getTitle() const;
    const char* getResource() const;

    void setInputItem( input_item_t* p_input_item );
    input_item_t* getInputItem() const ;

private:

    input_item_t* _inputItem;

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
    Container* getParent();

    void setInputItem( input_item_t* p_input_item );
    input_item_t* getInputItem() const;

private:

    input_item_t* _inputItem;

    Container* _parent;

    std::string _objectID;
    std::string _title;
    std::vector<Item*> _items;
    std::vector<Container*> _containers;
};

