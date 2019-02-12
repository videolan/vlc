/*****************************************************************************
 * dlna.cpp : DLNA/UPNP (renderer) sout module
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dlna.hpp"

#include <vector>
#include <string>
#include <sstream>

#include <vlc_cxx_helpers.hpp>
#include <vlc_dialog.h>
#include <vlc_rand.h>
#include <vlc_sout.h>
#include <vlc_block.h>

static const char* AV_TRANSPORT_SERVICE_TYPE = "urn:schemas-upnp-org:service:AVTransport:1";
static const char* CONNECTION_MANAGER_SERVICE_TYPE = "urn:schemas-upnp-org:service:ConnectionManager:1";

static const char *const ppsz_sout_options[] = {
    "ip", "port", "http-port", "video", "base_url", "url", nullptr
};

namespace DLNA
{

struct sout_stream_id_sys_t
{
    es_format_t           fmt;
    sout_stream_id_sys_t  *p_sub_id;
};

struct sout_stream_sys_t
{
    sout_stream_sys_t(int http_port, bool supports_video)
        : p_out( nullptr )
        , es_changed( true )
        , b_supports_video( supports_video )
        , perf_warning_shown( false )
        , venc_opt_idx ( -1 )
        , http_port( http_port )
    {
    }

    std::shared_ptr<MediaRenderer> renderer;
    UpnpInstanceWrapper *p_upnp;

    ProtocolPtr canDecodeAudio( vlc_fourcc_t i_codec ) const;
    ProtocolPtr canDecodeVideo( vlc_fourcc_t audio_codec,
                         vlc_fourcc_t video_codec ) const;
    bool startSoutChain( sout_stream_t* p_stream,
                         const std::vector<sout_stream_id_sys_t*> &new_streams,
                         const std::string &sout );
    void stopSoutChain( sout_stream_t* p_stream );
    sout_stream_id_sys_t *GetSubId( sout_stream_t *p_stream,
                                    sout_stream_id_sys_t *id,
                                    bool update = true );

    sout_stream_t                       *p_out;
    bool                                es_changed;
    bool                                b_supports_video;
    bool                                perf_warning_shown;
    int                                 venc_opt_idx;
    int                                 http_port;
    std::vector<sout_stream_id_sys_t*>  streams;
    std::vector<sout_stream_id_sys_t*>  out_streams;
    std::vector<protocol_info_t>        device_protocols;

private:
    std::string GetAcodecOption( sout_stream_t *, vlc_fourcc_t *, const audio_format_t *, int );
    int UpdateOutput( sout_stream_t *p_stream );

};

char *getServerIPAddress() {
    char *ip = nullptr;
#ifdef UPNP_ENABLE_IPV6
#ifdef _WIN32
    IP_ADAPTER_UNICAST_ADDRESS *p_best_ip = nullptr;
    wchar_t psz_uri[32];
    DWORD strSize;
    IP_ADAPTER_ADDRESSES *p_adapter, *addresses;

    addresses = ListAdapters();
    if (addresses == nullptr)
        return nullptr;

    p_adapter = addresses;
    while (p_adapter != nullptr)
    {
        if (isAdapterSuitable(p_adapter, false))
        {
            IP_ADAPTER_UNICAST_ADDRESS *p_unicast = p_adapter->FirstUnicastAddress;
            while (p_unicast != nullptr)
            {
                strSize = sizeof( psz_uri ) / sizeof( wchar_t );
                if( WSAAddressToString( p_unicast->Address.lpSockaddr,
                                        p_unicast->Address.iSockaddrLength,
                                        nullptr, psz_uri, &strSize ) == 0 )
                {
                    if ( p_best_ip == nullptr ||
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

    if (p_best_ip != nullptr)
    {
        strSize = sizeof( psz_uri ) / sizeof( wchar_t );
        WSAAddressToString( p_best_ip->Address.lpSockaddr,
                            p_best_ip->Address.iSockaddrLength,
                            nullptr, psz_uri, &strSize );
        free(addresses);
        return FromWide( psz_uri );
    }
    free(addresses);
    return nullptr;
#endif /* _WIN32 */
#else /* UPNP_ENABLE_IPV6 */
    ip = getIpv4ForMulticast();
#endif /* UPNP_ENABLE_IPV6 */
    if (ip == nullptr)
    {
        ip = strdup(UpnpGetServerIpAddress());
    }
    return ip;
}

std::string dlna_write_protocol_info (const protocol_info_t info)
{
    std::ostringstream protocol;
    char dlna_info[448];

    if (info.transport == DLNA_TRANSPORT_PROTOCOL_HTTP)
        protocol << "http-get:*:";

    protocol << info.profile.mime;
    protocol << ":";

    if (info.profile.name != "*")
        protocol << "DLNA.ORG_PN=" << info.profile.name.c_str() << ";";

    dlna_org_flags_t flags = DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE |
                             DLNA_ORG_FLAG_BACKGROUND_TRANSFERT_MODE |
                             DLNA_ORG_FLAG_CONNECTION_STALL |
                             DLNA_ORG_FLAG_DLNA_V15;
    sprintf (dlna_info, "%s=%.2x;%s=%d;%s=%.8x%.24x",
               "DLNA.ORG_OP", DLNA_ORG_OPERATION_RANGE,
               "DLNA.ORG_CI", info.ci,
               "DLNA.ORG_FLAGS", flags, 0);
    protocol << dlna_info;

    return protocol.str();
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::stringstream ss(s);
    std::string item;
    std::vector<std::string> elems;
    while (std::getline(ss, item, delim)) {
        elems.push_back(std::move(item));
    }
    return elems;
}

ProtocolPtr sout_stream_sys_t::canDecodeAudio(vlc_fourcc_t audio_codec) const
{
    for (protocol_info_t protocol : device_protocols) {
        if (protocol.profile.media == DLNA_CLASS_AUDIO
                && protocol.profile.audio_codec == audio_codec)
        {
            return std::unique_ptr<protocol_info_t>(new protocol_info_t(protocol));
        }
    }
    return nullptr;
}

ProtocolPtr sout_stream_sys_t::canDecodeVideo(vlc_fourcc_t audio_codec,
                vlc_fourcc_t video_codec) const
{
    for (protocol_info_t protocol : device_protocols) {
        if (protocol.profile.media == DLNA_CLASS_AV
                && protocol.profile.audio_codec == audio_codec
                && protocol.profile.video_codec == video_codec)
        {
            return std::unique_ptr<protocol_info_t>(new protocol_info_t(protocol));
        }
    }
    return nullptr;
}

bool sout_stream_sys_t::startSoutChain(sout_stream_t *p_stream,
                                       const std::vector<sout_stream_id_sys_t*> &new_streams,
                                       const std::string &sout)
{
    stopSoutChain(p_stream);
    msg_Dbg( p_stream, "Creating chain %s", sout.c_str() );
    out_streams = new_streams;

    p_out = sout_StreamChainNew( p_stream->p_sout, sout.c_str(), nullptr, nullptr);
    if (p_out == nullptr) {
        msg_Err(p_stream, "could not create sout chain:%s", sout.c_str());
        out_streams.clear();
        return false;
    }

    /* check the streams we can actually add */
    for (std::vector<sout_stream_id_sys_t*>::iterator it = out_streams.begin();
            it != out_streams.end(); )
    {
        sout_stream_id_sys_t *p_sys_id = *it;
        p_sys_id->p_sub_id = static_cast<sout_stream_id_sys_t *>(
                sout_StreamIdAdd( p_out, &p_sys_id->fmt ) );
        if ( p_sys_id->p_sub_id == nullptr )
        {
            msg_Err( p_stream, "can't handle %4.4s stream",
                    (char *)&p_sys_id->fmt.i_codec );
            es_format_Clean( &p_sys_id->fmt );
            it = out_streams.erase( it );
        }
        else
            ++it;
    }

    if (out_streams.empty())
    {
        stopSoutChain( p_stream );
        return false;
    }

    return true;
}

void sout_stream_sys_t::stopSoutChain(sout_stream_t *p_stream)
{
    msg_Dbg( p_stream, "Destroying dlna sout chain");

    for ( size_t i = 0; i < out_streams.size(); i++ )
    {
        sout_StreamIdDel( p_out, out_streams[i]->p_sub_id );
        out_streams[i]->p_sub_id = nullptr;
    }
    out_streams.clear();
    sout_StreamChainDelete( p_out, nullptr );
    p_out = nullptr;
}

sout_stream_id_sys_t *sout_stream_sys_t::GetSubId( sout_stream_t *p_stream,
                                                   sout_stream_id_sys_t *id,
                                                   bool update)
{
    assert( p_stream->p_sys == this );

    if ( update && UpdateOutput( p_stream ) != VLC_SUCCESS )
        return nullptr;

    for (size_t i = 0; i < out_streams.size(); ++i)
    {
        if ( id == (sout_stream_id_sys_t*) out_streams[i] )
            return out_streams[i]->p_sub_id;
    }

    msg_Err( p_stream, "unknown stream ID" );
    return nullptr;
}

std::string
sout_stream_sys_t::GetAcodecOption( sout_stream_t *p_stream, vlc_fourcc_t *p_codec_audio,
                                    const audio_format_t *p_aud, int i_quality )
{
    VLC_UNUSED(p_aud);
    VLC_UNUSED(i_quality);
    std::stringstream ssout;

    msg_Dbg( p_stream, "Converting audio to %.4s", (const char*)p_codec_audio );

    ssout << "acodec=";
    char fourcc[5];
    vlc_fourcc_to_char( *p_codec_audio, fourcc );
    fourcc[4] = '\0';
    ssout << fourcc << ',';

    ssout << "aenc=avcodec{codec=aac},";
    return ssout.str();
}

int sout_stream_sys_t::UpdateOutput( sout_stream_t *p_stream )
{
    assert( p_stream->p_sys == this );

    if ( !es_changed )
        return VLC_SUCCESS;

    es_changed = false;

    bool canRemux = true;
    // To keep track of which stream needs transcoding if at all.
    vlc_fourcc_t i_codec_video = 0, i_codec_audio = 0;
    const es_format_t *p_original_audio = nullptr;
    const es_format_t *p_original_video = nullptr;
    std::vector<sout_stream_id_sys_t*> new_streams;

    for (sout_stream_id_sys_t *stream : streams)
    {
        const es_format_t *p_es = &stream->fmt;
        if (p_es->i_cat == AUDIO_ES)
        {
            p_original_audio = p_es;
            new_streams.push_back(stream);
        }
        else if (b_supports_video && p_es->i_cat == VIDEO_ES)
        {
            p_original_video = p_es;
            new_streams.push_back(stream);
        }
    }

    if (new_streams.empty())
        return VLC_SUCCESS;

    ProtocolPtr stream_protocol;
    // check if we have an audio only stream
    if (!p_original_video && p_original_audio)
    {
        if( !(stream_protocol = canDecodeAudio(p_original_audio->i_codec)) )
        {
            msg_Dbg( p_stream, "can't remux audio track %d codec %4.4s",
                p_original_audio->i_id, (const char*)&p_original_audio->i_codec );
            stream_protocol = make_protocol(default_audio_protocol);
            canRemux = false;
        }
        else
            i_codec_audio = p_original_audio->i_codec;
    }
    // video only stream
    else if (p_original_video && !p_original_audio)
    {
        if( !(stream_protocol = canDecodeVideo(VLC_CODEC_NONE,
                        p_original_video->i_codec)) )
        {
            msg_Dbg(p_stream, "can't remux video track %d codec: %4.4s",
                p_original_video->i_id, (const char*)&p_original_video->i_codec);
            stream_protocol = make_protocol(default_video_protocol);
            canRemux = false;
        }
        else
            i_codec_video = p_original_video->i_codec;
    }
    else
    {
        if( !(stream_protocol = canDecodeVideo( p_original_audio->i_codec,
                        p_original_video->i_codec)) )
        {
            msg_Dbg(p_stream, "can't remux video track %d with audio: %4.4s and video: %4.4s",
                p_original_video->i_id, (const char*)&p_original_audio->i_codec,
                    (const char*)&p_original_video->i_codec);
            stream_protocol = make_protocol(default_video_protocol);
            canRemux = false;

            // check which codec needs transcoding
            if (stream_protocol->profile.audio_codec == p_original_audio->i_codec)
                i_codec_audio = p_original_audio->i_codec;
            if (stream_protocol->profile.video_codec == p_original_video->i_codec)
                i_codec_video = p_original_video->i_codec;
        }
        else
        {
            i_codec_audio = p_original_audio->i_codec;
            i_codec_video = p_original_video->i_codec;
        }
    }

    msg_Dbg( p_stream, "using DLNA profile %s:%s",
                stream_protocol->profile.mime.c_str(),
                stream_protocol->profile.name.c_str() );

    std::ostringstream ssout;
    if ( !canRemux )
    {
        if ( !perf_warning_shown && i_codec_video == 0 && p_original_video
          && var_InheritInteger( p_stream, RENDERER_CFG_PREFIX "show-perf-warning" ) )
        {
            int res = vlc_dialog_wait_question( p_stream,
                          VLC_DIALOG_QUESTION_WARNING,
                         _("Cancel"), _("OK"), _("Ok, Don't warn me again"),
                         _("Performance warning"),
                         _("Casting this video requires conversion. "
                           "This conversion can use all the available power and "
                           "could quickly drain your battery." ) );
            if ( res <= 0 )
                 return false;
            perf_warning_shown = true;
            if ( res == 2 )
                config_PutInt(RENDERER_CFG_PREFIX "show-perf-warning", 0 );
        }

        const int i_quality = var_InheritInteger( p_stream, SOUT_CFG_PREFIX "conversion-quality" );

        /* TODO: provide audio samplerate and channels */
        ssout << "transcode{";
        if ( i_codec_audio == 0 && p_original_audio )
        {
            i_codec_audio = stream_protocol->profile.audio_codec;
            ssout << GetAcodecOption( p_stream, &i_codec_audio,
                                      &p_original_audio->audio, i_quality );
        }
        if ( i_codec_video == 0 && p_original_video )
        {
            i_codec_video = stream_protocol->profile.video_codec;
            try {
                ssout << vlc_sout_renderer_GetVcodecOption( p_stream,
                                        { i_codec_video },
                                        &i_codec_video,
                                        &p_original_video->video, i_quality );
            } catch(const std::exception& e) {
                return VLC_EGENERIC ;
            }
        }
        ssout << "}:";
    }

    std::ostringstream ss;
    ss << "/dlna"
       << "/" << vlc_tick_now()
       << "/" << static_cast<uint64_t>( vlc_mrand48() )
       << "/stream";
    std::string root_url = ss.str();

    ssout << "http{dst=:" << http_port << root_url
          << ",mux=" << stream_protocol->profile.mux
          << ",access=http{mime=" << stream_protocol->profile.mime << "}}";

    auto ip = vlc::wrap_cptr<char>(getServerIPAddress());
    if (ip == nullptr)
    {
        msg_Err(p_stream, "could not get the local ip address");
        return VLC_EGENERIC;
    }

    char *uri;
    if (asprintf(&uri, "http://%s:%d%s", ip.get(), http_port, root_url.c_str()) < 0) {
        return VLC_ENOMEM;
    }

    if ( !startSoutChain( p_stream, new_streams, ssout.str() ) ) {
        free(uri);
        return VLC_EGENERIC;
    }

    renderer->PrepareForConnection(dlna_write_protocol_info(*stream_protocol).c_str());

    msg_Dbg(p_stream, "AVTransportURI: %s", uri);
    renderer->Stop();
    renderer->SetAVTransportURI(uri, *stream_protocol);
    renderer->Play("1");

    free(uri);
    return VLC_SUCCESS;
}

char *MediaRenderer::getServiceURL(const char* type, const char *service)
{
    IXML_Document *p_description_doc = nullptr;
    if (UpnpDownloadXmlDoc(device_url.c_str(), &p_description_doc) != UPNP_E_SUCCESS)
        return nullptr;

    IXML_NodeList* p_device_list = ixmlDocument_getElementsByTagName( p_description_doc, "device");
    free(p_description_doc);
    if ( !p_device_list )
        return nullptr;

    for (unsigned int i = 0; i < ixmlNodeList_length(p_device_list); ++i)
    {
        IXML_Element* p_device_element = ( IXML_Element* ) ixmlNodeList_item( p_device_list, i );
        if( !p_device_element )
            continue;

        IXML_NodeList* p_service_list = ixmlElement_getElementsByTagName( p_device_element, "service" );
        if ( !p_service_list )
            continue;
        for ( unsigned int j = 0; j < ixmlNodeList_length( p_service_list ); j++ )
        {
            IXML_Element* p_service_element = (IXML_Element*)ixmlNodeList_item( p_service_list, j );

            const char* psz_service_type = xml_getChildElementValue( p_service_element, "serviceType" );
            if ( !psz_service_type || !strstr(psz_service_type, type))
                continue;
            const char* psz_control_url = xml_getChildElementValue( p_service_element,
                                                                    service );
            if ( !psz_control_url )
                continue;

            char* psz_url = NULL;
            if ( UpnpResolveURL2( base_url.c_str(), psz_control_url, &psz_url ) == UPNP_E_SUCCESS )
                return psz_url;
            return nullptr;
        }
    }
    return nullptr;
}

/**
 * Send an action to the control url of the service specified.
 *
 * \return the response as a IXML document or nullptr for failure
 **/
IXML_Document *MediaRenderer::SendAction(const char* action_name,const char *service_type,
                    std::list<std::pair<const char*, const char*>> arguments)
{
    /* Create action */
    IXML_Document *action = UpnpMakeAction(action_name, service_type, 0, nullptr);

    /* Add argument to action */
    for (std::pair<const char*, const char*> arg : arguments) {
      const char *arg_name, *arg_val;
      arg_name = arg.first;
      arg_val  = arg.second;
      UpnpAddToAction(&action, action_name, service_type, arg_name, arg_val);
    }

    /* Get the controlURL of the service */
    char *control_url = getServiceURL(service_type, "controlURL");

    /* Send action */
    IXML_Document *response = nullptr;
    int ret = UpnpSendAction(handle, control_url, service_type,
                                    nullptr, action, &response);

    /* Free action */
    ixmlDocument_free(action);
    free(control_url);

    if (ret != UPNP_E_SUCCESS) {
        msg_Err(parent, "Unable to send action: %s (%d: %s) response: %s",
                action_name, ret, UpnpGetErrorMessage(ret), ixmlPrintDocument(response));
        if (response) ixmlDocument_free(response);
        return nullptr;
    }

    return response;
}

int MediaRenderer::Play(const char *speed)
{
    std::list<std::pair<const char*, const char*>> arg_list = {
        {"InstanceID", AVTransportID.c_str()},
        {"Speed", speed},
    };

    IXML_Document *p_response = SendAction("Play", AV_TRANSPORT_SERVICE_TYPE, arg_list);
    if(!p_response)
    {
        return VLC_EGENERIC;
    }
    ixmlDocument_free(p_response);
    return VLC_SUCCESS;
}

int MediaRenderer::Stop()
{
    std::list<std::pair<const char*, const char*>> arg_list = {
        {"InstanceID", AVTransportID.c_str()},
    };

    IXML_Document *p_response = SendAction("Stop", AV_TRANSPORT_SERVICE_TYPE, arg_list);
    if(!p_response)
    {
        return VLC_EGENERIC;
    }
    ixmlDocument_free(p_response);
    return VLC_SUCCESS;
}

std::vector<protocol_info_t> MediaRenderer::GetProtocolInfo()
{
    std::string protocol_csv;
    std::vector<protocol_info_t> supported_protocols;
    std::list<std::pair<const char*, const char*>> arg_list;

    IXML_Document *response = SendAction("GetProtocolInfo",
                                CONNECTION_MANAGER_SERVICE_TYPE, arg_list);
    if(!response)
    {
        return supported_protocols;
    }

    // Get the CSV list of protocols/profiles supported by the device
    IXML_NodeList *protocol_list = ixmlDocument_getElementsByTagName( response , "Sink" );
    if( protocol_list )
    {
        IXML_Node* protocol_node = ixmlNodeList_item( protocol_list, 0 );
        if ( protocol_node )
        {
            IXML_Node* p_text_node = ixmlNode_getFirstChild( protocol_node );
            if ( p_text_node )
            {
                protocol_csv.assign(ixmlNode_getNodeValue( p_text_node ));
            }
        }
        ixmlNodeList_free( protocol_list);
    }
    ixmlDocument_free(response);

    msg_Dbg(parent, "Device supports protocols: %s", protocol_csv.c_str());
    // parse the CSV list
    // format: <transportProtocol>:<network>:<mime>:<additionalInfo>
    std::vector<std::string> protocols = split(protocol_csv, ',');
    for (std::string protocol : protocols ) {
        std::vector<std::string> protocol_info = split(protocol, ':');

        // We only support http transport for now.
        if (protocol_info.size() == 4 && protocol_info.at(0) == "http-get")
        {
            protocol_info_t proto;

            // Get the DLNA profile name
            std::string profile_name;
            std::string tag = "DLNA.ORG_PN=";
            std::size_t index = protocol_info.at(3).find(tag);

            if (protocol_info.at(3) == "*")
            {
               profile_name = "*";
            }
            else if (index != std::string::npos)
            {
                std::size_t end = protocol_info.at(3).find(';', index + 1);
                int start = index + tag.length() - 1;
                int length = end - start;
                profile_name = protocol_info.at(3).substr(start, length);
            }

            // Match our supported profiles to device profiles
            for (dlna_profile_t profile : dlna_profile_list) {
                if (protocol_info.at(2) == profile.mime
                        && (profile_name == profile.name || profile_name == "*"))
                {
                    proto.profile = std::move(profile);
                    supported_protocols.push_back(proto);
                    // we do not break here to account for wildcards
                    // as protocolInfo's fourth field aka <additionalInfo>
                }
            }
        }
    }

    msg_Dbg( parent , "Got %zu supported profiles", supported_protocols.size() );
    return supported_protocols;
}

int MediaRenderer::PrepareForConnection(const char* protocol_str)
{
    std::list<std::pair<const char*, const char*>> arg_list = {
        { "PeerConnectionID", "-1" },
        { "PeerConnectionManager", "" },
        { "Direction", "Input" },
        { "RemoteProtocolInfo", protocol_str },
    };

    IXML_Document *response = SendAction("PrepareForConnection",
                                    CONNECTION_MANAGER_SERVICE_TYPE, arg_list);
    if(!response)
    {
        return VLC_EGENERIC;
    }

    msg_Dbg(parent, "PrepareForConnection response: %s",
                                    ixmlPrintDocument(response));

    IXML_NodeList *node_list =
            ixmlDocument_getElementsByTagName(response , "ConnectionID");
    if (node_list)
    {
        IXML_Node* node = ixmlNodeList_item(node_list, 0);
        if (node)
        {
            IXML_Node* p_text_node = ixmlNode_getFirstChild(node);
            if (p_text_node)
            {
                ConnectionID.assign(ixmlNode_getNodeValue(p_text_node));
            }
        }
        ixmlNodeList_free(node_list);
    }

    node_list =
            ixmlDocument_getElementsByTagName(response , "AVTransportID");
    if (node_list)
    {
        IXML_Node* node = ixmlNodeList_item(node_list, 0);
        if (node)
        {
            IXML_Node* p_text_node = ixmlNode_getFirstChild(node);
            if (p_text_node)
            {
                AVTransportID.assign(ixmlNode_getNodeValue(p_text_node));
            }
        }
        ixmlNodeList_free(node_list);
    }

    node_list =
            ixmlDocument_getElementsByTagName(response , "RcsID");
    if (node_list)
    {
        IXML_Node* node = ixmlNodeList_item(node_list, 0);
        if (node)
        {
            IXML_Node* p_text_node = ixmlNode_getFirstChild(node);
            if (p_text_node)
            {
                RcsID.assign(ixmlNode_getNodeValue(p_text_node));
            }
        }
        ixmlNodeList_free(node_list);
    }

    ixmlDocument_free(response);
    return VLC_SUCCESS;
}

int MediaRenderer::ConnectionComplete()
{
    std::list<std::pair<const char*, const char*>> arg_list = {
        {"ConnectionID", ConnectionID.c_str()},
    };

    IXML_Document *p_response = SendAction("ConnectionComplete",
                                    CONNECTION_MANAGER_SERVICE_TYPE, arg_list);
    if(!p_response)
    {
        return VLC_EGENERIC;
    }

    ixmlDocument_free(p_response);
    return VLC_SUCCESS;
}

int MediaRenderer::SetAVTransportURI(const char* uri, const protocol_info_t& proto)
{
    static const char didl[] =
        "<DIDL-Lite "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
        "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
        "<item id=\"f-0\" parentID=\"0\" restricted=\"0\">"
        "<dc:title>%s</dc:title>"
        "<upnp:class>%s</upnp:class>"
        "<res protocolInfo=\"%s\">%s</res>"
        "</item>"
        "</DIDL-Lite>";

    bool audio = proto.profile.media == DLNA_CLASS_AUDIO;
    std::string dlna_protocol = dlna_write_protocol_info(proto);

    char *meta_data;
    if (asprintf(&meta_data, didl,
                audio ? "Audio" : "Video",
                audio ? "object.item.audioItem" : "object.item.videoItem",
                dlna_protocol.c_str(),
                uri) < 0) {
        return VLC_ENOMEM;
    }

    msg_Dbg(parent, "didl: %s", meta_data);
    std::list<std::pair<const char*, const char*>> arg_list = {
        {"InstanceID", AVTransportID.c_str()},
        {"CurrentURI", uri},
        {"CurrentURIMetaData", meta_data},
    };

    IXML_Document *p_response = SendAction("SetAVTransportURI",
                                    AV_TRANSPORT_SERVICE_TYPE, arg_list);

    free(meta_data);
    if(!p_response)
    {
        return VLC_EGENERIC;
    }
    ixmlDocument_free(p_response);
    return VLC_SUCCESS;
}

static void *Add(sout_stream_t *p_stream, const es_format_t *p_fmt)
{
    sout_stream_sys_t *p_sys = static_cast<sout_stream_sys_t *>( p_stream->p_sys );

    if (!p_sys->b_supports_video)
    {
        if (p_fmt->i_cat != AUDIO_ES)
            return nullptr;
    }

    sout_stream_id_sys_t *p_sys_id = (sout_stream_id_sys_t *)malloc(sizeof(sout_stream_id_sys_t));
    if(p_sys_id != nullptr)
    {
        es_format_Copy(&p_sys_id->fmt, p_fmt);
        p_sys_id->p_sub_id = nullptr;
        p_sys->streams.push_back(p_sys_id);
        p_sys->es_changed = true;
    }
    return p_sys_id;
}

static int Send(sout_stream_t *p_stream, void *id,
                block_t *p_buffer)
{
    sout_stream_sys_t *p_sys = static_cast<sout_stream_sys_t *>( p_stream->p_sys );
    sout_stream_id_sys_t *id_sys = static_cast<sout_stream_id_sys_t*>( id );

    id_sys = p_sys->GetSubId( p_stream, id_sys );
    if ( id_sys == nullptr )
    {
        block_Release( p_buffer );
        return VLC_EGENERIC;
    }

    return sout_StreamIdSend(p_sys->p_out, id_sys, p_buffer);
}

static void Flush( sout_stream_t *p_stream, void *id )
{
    sout_stream_sys_t *p_sys = static_cast<sout_stream_sys_t *>( p_stream->p_sys );
    sout_stream_id_sys_t *id_sys = static_cast<sout_stream_id_sys_t*>( id );

    id_sys = p_sys->GetSubId( p_stream, id_sys, false );
    if ( id_sys == nullptr )
        return;

    sout_StreamFlush( p_sys->p_out, id_sys );
    p_sys->stopSoutChain( p_stream );
    p_sys->es_changed = true;
}

static void Del(sout_stream_t *p_stream, void *_id)
{
    sout_stream_sys_t *p_sys = static_cast<sout_stream_sys_t *>( p_stream->p_sys );
    sout_stream_id_sys_t *id = static_cast<sout_stream_id_sys_t *>( _id );

    for (auto it = p_sys->streams.begin(); it != p_sys->streams.end(); ++it)
    {
        sout_stream_id_sys_t *p_sys_id = *it;
        if ( p_sys_id == id )
        {
            if ( p_sys_id->p_sub_id != nullptr )
            {
                sout_StreamIdDel( p_sys->p_out, p_sys_id->p_sub_id );
                for (auto out_it = p_sys->out_streams.begin();
                     out_it != p_sys->out_streams.end(); ++out_it)
                {
                    if (*out_it == id)
                    {
                        p_sys->out_streams.erase(out_it);
                        break;
                    }
                }
            }

            es_format_Clean( &p_sys_id->fmt );
            free( p_sys_id );
            p_sys->streams.erase( it );
            break;
        }
    }

    if (p_sys->out_streams.empty())
    {
        p_sys->stopSoutChain(p_stream);
        p_sys->renderer->Stop();
    }
}

int OpenSout( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);
    sout_stream_sys_t *p_sys = nullptr;

    config_ChainParse(p_stream, SOUT_CFG_PREFIX, ppsz_sout_options, p_stream->p_cfg);

    int http_port = var_InheritInteger(p_stream, SOUT_CFG_PREFIX "http-port");
    bool b_supports_video = var_GetBool(p_stream, SOUT_CFG_PREFIX "video");
    char *base_url = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "base_url");
    char *device_url = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "url");
    if ( device_url == nullptr)
    {
        msg_Err( p_stream, "missing Url" );
        goto error;
    }

    try {
        p_sys = new sout_stream_sys_t(http_port, b_supports_video);
    }
    catch ( const std::exception& ex ) {
        msg_Err( p_stream, "Failed to instantiate sout_stream_sys_t: %s", ex.what() );
        return VLC_EGENERIC;
    }

    p_sys->p_upnp = UpnpInstanceWrapper::get( p_this );
    if ( !p_sys->p_upnp )
        goto error;
    try {
        p_sys->renderer = std::make_shared<MediaRenderer>(p_stream,
                            p_sys->p_upnp, base_url, device_url);
    }
    catch ( const std::bad_alloc& ) {
        msg_Err( p_stream, "Failed to create a MediaRenderer");
        p_sys->p_upnp->release();
        goto error;
    }

    p_sys->device_protocols = p_sys->renderer->GetProtocolInfo();

    p_stream->pf_add     = Add;
    p_stream->pf_del     = Del;
    p_stream->pf_send    = Send;
    p_stream->pf_flush   = Flush;

    p_stream->p_sys = p_sys;

    free(base_url);
    free(device_url);

    return VLC_SUCCESS;

error:
    free(base_url);
    free(device_url);
    delete p_sys;
    return VLC_EGENERIC;
}

void CloseSout( vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>( p_this );
    sout_stream_sys_t *p_sys = static_cast<sout_stream_sys_t *>( p_stream->p_sys );

    p_sys->renderer->ConnectionComplete();
    p_sys->p_upnp->release();
    delete p_sys;
}

}
