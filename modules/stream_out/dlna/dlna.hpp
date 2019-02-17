/*****************************************************************************
 * dlna.hpp : DLNA/UPNP (renderer) sout module header
 *****************************************************************************
 * Copyright © 2018 VLC authors and VideoLAN
 *
 * Authors: Shaleen Jain <shaleen@jain.sh>
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

#ifndef DLNA_H
#define DLNA_H

#include "../../services_discovery/upnp-wrapper.hpp"
#include "dlna_common.hpp"
#include "profile_names.hpp"

struct protocol_info_t {
    protocol_info_t() = default;
    protocol_info_t(const protocol_info_t&) = default;

    // aggreate initializers do not work in c++11
    // for class types with default member initializers
    protocol_info_t(dlna_transport_protocol_t transport,
            dlna_org_conversion_t ci,
            dlna_profile_t profile)
        : transport(transport)
        , ci(ci)
        , profile(profile)
        {}

    dlna_transport_protocol_t transport = DLNA_TRANSPORT_PROTOCOL_HTTP;
    dlna_org_conversion_t ci = DLNA_ORG_CONVERSION_NONE;
    dlna_profile_t profile;
};

using ProtocolPtr = std::unique_ptr<protocol_info_t>;
static inline ProtocolPtr make_protocol(protocol_info_t a)
{
    return std::unique_ptr<protocol_info_t>(new protocol_info_t(a));
}

const protocol_info_t default_audio_protocol = {
    DLNA_TRANSPORT_PROTOCOL_HTTP,
    DLNA_ORG_CONVERSION_TRANSCODED,
    default_audio_profile,
};

const protocol_info_t default_video_protocol = {
    DLNA_TRANSPORT_PROTOCOL_HTTP,
    DLNA_ORG_CONVERSION_TRANSCODED,
    default_video_profile,
};

namespace DLNA
{

class MediaRenderer
{
public:
    MediaRenderer(sout_stream_t *p_stream, UpnpInstanceWrapper *upnp,
            std::string base_url, std::string device_url)
        : parent(p_stream)
        , base_url(base_url)
        , device_url(device_url)
        , handle(upnp->handle())
        , ConnectionID("0")
        , AVTransportID("0")
        , RcsID("0")
    {
    }

    sout_stream_t       *parent;
    std::string         base_url;
    std::string         device_url;
    UpnpClient_Handle   handle;

    std::string         ConnectionID;
    std::string         AVTransportID;
    std::string         RcsID;

    char *getServiceURL(const char* type, const char* service);
    IXML_Document *SendAction(const char* action_name, const char *service_type,
                    std::list<std::pair<const char*, const char*>> arguments);

    int Play(const char *speed);
    int Stop();
    std::vector<protocol_info_t> GetProtocolInfo();
    int PrepareForConnection(const char* protocol_str);
    int ConnectionComplete();
    int SetAVTransportURI(const char* uri, const protocol_info_t& proto);
};

}
#endif /* DLNA_H */
