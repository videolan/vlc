/*****************************************************************************
 * upnp.hpp :  UPnP discovery module (libupnp) header
 *****************************************************************************
 * Copyright (C) 2004-2010 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vector>
#include <string>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#include <vlc_common.h>

namespace SD
{
    class MediaServerList;
}

/*
 * libUpnp allows only one instance per process, so we have to share one for
 * both SD & Access module
 * Since the callback is bound to the UpnpClient_Handle, we have to register
 * a wrapper callback, in order for the access module to be able to initialize
 * libUpnp first.
 * When a SD wishes to use libUpnp, it will provide its own callback, that the
 * wrapper will forward.
 * This way, we always have a register callback & a client handle.
 */
class UpnpInstanceWrapper
{
public:
    // This increases the refcount before returning the instance
    static UpnpInstanceWrapper* get(vlc_object_t* p_obj, Upnp_FunPtr callback, SD::MediaServerList *opaque);
    void release(bool isSd);
    UpnpClient_Handle handle() const;

private:
    static int Callback( Upnp_EventType event_type, void* p_event, void* p_user_data );

    UpnpInstanceWrapper();
    ~UpnpInstanceWrapper();

private:
    static UpnpInstanceWrapper* s_instance;
    static vlc_mutex_t s_lock;
    UpnpClient_Handle handle_;
    SD::MediaServerList* opaque_;
    Upnp_FunPtr callback_;
    int refcount_;
};

namespace SD
{

struct MediaServerDesc
{
    MediaServerDesc(const std::string& udn, const std::string& fName, const std::string& loc);
    ~MediaServerDesc();
    std::string UDN;
    std::string friendlyName;
    std::string location;
    input_item_t* inputItem;
    bool isSatIp;
};


class MediaServerList
{
public:

    MediaServerList( services_discovery_t* p_sd );
    ~MediaServerList();

    bool addServer(MediaServerDesc *desc );
    void removeServer(const std::string &udn );
    MediaServerDesc* getServer( const std::string& udn );
    static int Callback( Upnp_EventType event_type, void* p_event, void* p_user_data );

private:
    void parseNewServer( IXML_Document* doc, const std::string& location );

private:
    services_discovery_t* p_sd_;
    std::vector<MediaServerDesc*> list_;
    vlc_mutex_t lock_;
};

}

namespace Access
{

class MediaServer
{
public:
    MediaServer( const char* psz_url, access_t* p_access );
    ~MediaServer();
    input_item_t* getNextItem();

private:
    MediaServer(const MediaServer&);
    MediaServer& operator=(const MediaServer&);

    void fetchContents();
    input_item_t* newItem(const char* objectID, const char* title);
    input_item_t* newItem(const char* title, const char* psz_objectID, const char* psz_subtitles, mtime_t duration, const char* psz_url );

    IXML_Document* _browseAction(const char*, const char*,
            const char*, const char*, const char* );

private:
    const std::string url_;
    access_t* access_;
    IXML_Document* xmlDocument_;
    IXML_NodeList* containerNodeList_;
    unsigned int   containerNodeIndex_;
    IXML_NodeList* itemNodeList_;
    unsigned int   itemNodeIndex_;
};

}
