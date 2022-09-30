/*****************************************************************************
 * upnp-wrapper.hpp :  UPnP Instance Wrapper class header
 *****************************************************************************
 * Copyright © 2004-2018 VLC authors and VideoLAN
 *
 * Authors: Rémi Denis-Courmont (original plugin)
 *          Christian Henz <henz # c-lab.de>
 *          Mirsal Ennaime <mirsal dot ennaime at gmail dot com>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *          Shaleen Jain <shaleen@jain.sh>
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

#ifndef UPNP_WRAPPER_H
#define UPNP_WRAPPER_H

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_cxx_helpers.hpp>

#include <memory>
#include <vector>
#include <algorithm>
#include <assert.h>

#include <upnp.h>
#include <upnptools.h>

typedef const void* UpnpEventPtr;

/**
 * libUpnp allows only one instance per process, so we create a wrapper
 * class around it that acts and behaves as a singleton. Letting us get
 * multiple references to it but only ever having a single instance in memory.
 * At the same time we let any module wishing to get a callback from the library
 * to register a UpnpInstanceWrapper::Listener to get the Listener#onEvent()
 * callback without having any hard dependencies.
 */
class UpnpInstanceWrapper
{
public:
    class Listener
    {
        public:
            virtual ~Listener() {}
            virtual int onEvent( Upnp_EventType event_type,
                                 UpnpEventPtr p_event,
                                 void* p_user_data ) = 0;
    };

private:
    static UpnpInstanceWrapper* s_instance;
    static vlc::threads::mutex s_lock;
    UpnpClient_Handle m_handle;
    int m_refcount;
    typedef std::shared_ptr<Listener> ListenerPtr;
    typedef std::vector<ListenerPtr> Listeners;
    static Listeners s_listeners;

public:
    // This increases the refcount before returning the instance
    static UpnpInstanceWrapper* get( vlc_object_t* p_obj );
    void release();
    UpnpClient_Handle handle() const;
    void addListener(ListenerPtr listener);
    void removeListener(ListenerPtr listener);

private:
    static int Callback( Upnp_EventType event_type, UpnpEventPtr p_event, void* p_user_data );

    UpnpInstanceWrapper();
    ~UpnpInstanceWrapper();
};

// **************************
// Helper functions
// **************************

/*
 * Returns the value of a child element, or NULL on error
 */
inline const char* xml_getChildElementValue( IXML_Element* p_parent,
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

#ifdef _WIN32

inline IP_ADAPTER_MULTICAST_ADDRESS* getMulticastAddress(IP_ADAPTER_ADDRESSES* p_adapter)
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

inline bool isAdapterSuitable(IP_ADAPTER_ADDRESSES* p_adapter)
{
    if ( p_adapter->OperStatus != IfOperStatusUp )
        return false;
    if (p_adapter->Length == sizeof(IP_ADAPTER_ADDRESSES_XP))
    {
        IP_ADAPTER_ADDRESSES_XP* p_adapter_xp = reinterpret_cast<IP_ADAPTER_ADDRESSES_XP*>( p_adapter );
        // On Windows Server 2003 and Windows XP, those members are zero if the IPv* implementation
        // is not available on the interface.
#if defined( UPNP_ENABLE_IPV6 )
        return p_adapter_xp->Ipv6IfIndex != 0 || p_adapter_xp->IfIndex != 0;
#else
        return p_adapter_xp->IfIndex != 0;
#endif
    }
    IP_ADAPTER_ADDRESSES_LH* p_adapter_lh = reinterpret_cast<IP_ADAPTER_ADDRESSES_LH*>( p_adapter );
    if (p_adapter_lh->FirstGatewayAddress == NULL)
        return false;
#if defined( UPNP_ENABLE_IPV6 )
    return p_adapter_lh->Ipv6Enabled || p_adapter_lh->Ipv4Enabled;
#else
    return p_adapter_lh->Ipv4Enabled;
#endif
}

inline IP_ADAPTER_ADDRESSES* ListAdapters()
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

inline char* getPreferedAdapter()
{
    IP_ADAPTER_ADDRESSES *p_adapter, *addresses;

    addresses = ListAdapters();
    if (addresses == NULL)
        return NULL;

    /* find one with multicast capabilities */
    p_adapter = addresses;
    while (p_adapter != NULL)
    {
        if (isAdapterSuitable( p_adapter ))
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
#else /* _WIN32 */

#ifdef __APPLE__
#include <TargetConditionals.h>

#if defined(TARGET_OS_OSX) && TARGET_OS_OSX
#include <SystemConfiguration/SystemConfiguration.h>

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

#endif /* _WIN32 */

#endif
