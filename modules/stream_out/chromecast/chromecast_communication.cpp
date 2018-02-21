/*****************************************************************************
 * chromecast_communication.cpp: Handle chromecast protocol messages
 *****************************************************************************
 * Copyright © 2014-2017 VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Steve Lhomme <robux4@videolabs.io>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chromecast.h"
#ifdef HAVE_POLL
# include <poll.h>
#endif

#include <iomanip>

ChromecastCommunication::ChromecastCommunication( vlc_object_t* p_module, const char* targetIP, unsigned int devicePort )
    : m_module( p_module )
    , m_creds( NULL )
    , m_tls( NULL )
    , m_receiver_requestId( 1 )
    , m_requestId( 1 )
{
    if (devicePort == 0)
        devicePort = CHROMECAST_CONTROL_PORT;

    m_creds = vlc_tls_ClientCreate( m_module->obj.parent );
    if (m_creds == NULL)
        throw std::runtime_error( "Failed to create TLS client" );

    /* Ignore ca checks */
    m_creds->obj.flags |= OBJECT_FLAGS_INSECURE;
    m_tls = vlc_tls_SocketOpenTLS( m_creds, targetIP, devicePort, "tcps",
                                   NULL, NULL );
    if (m_tls == NULL)
    {
        vlc_tls_Delete(m_creds);
        throw std::runtime_error( "Failed to create client session" );
    }

    char psz_localIP[NI_MAXNUMERICHOST];
    if (net_GetSockAddress( vlc_tls_GetFD(m_tls), psz_localIP, NULL ))
        throw std::runtime_error( "Cannot get local IP address" );

    m_serverIp = psz_localIP;
}

ChromecastCommunication::~ChromecastCommunication()
{
    disconnect();
}

void ChromecastCommunication::disconnect()
{
    if ( m_tls != NULL )
    {
        vlc_tls_Close(m_tls);
        vlc_tls_Delete(m_creds);
        m_tls = NULL;
    }
}

/**
 * @brief Build a CastMessage to send to the Chromecast
 * @param namespace_ the message namespace
 * @param payloadType the payload type (CastMessage_PayloadType_STRING or
 * CastMessage_PayloadType_BINARY
 * @param payload the payload
 * @param destinationId the destination idenifier
 * @return the generated CastMessage
 */
int ChromecastCommunication::buildMessage(const std::string & namespace_,
                              const std::string & payload,
                              const std::string & destinationId,
                              castchannel::CastMessage_PayloadType payloadType)
{
    castchannel::CastMessage msg;

    msg.set_protocol_version(castchannel::CastMessage_ProtocolVersion_CASTV2_1_0);
    msg.set_namespace_(namespace_);
    msg.set_payload_type(payloadType);
    msg.set_source_id("sender-vlc");
    msg.set_destination_id(destinationId);
    if (payloadType == castchannel::CastMessage_PayloadType_STRING)
        msg.set_payload_utf8(payload);
    else // CastMessage_PayloadType_BINARY
        msg.set_payload_binary(payload);

    return sendMessage(msg);
}

/**
 * @brief Receive a data packet from the Chromecast
 * @param p_data the buffer in which to store the data
 * @param i_size the size of the buffer
 * @param i_timeout maximum time to wait for a packet, in millisecond
 * @param pb_timeout Output parameter that will contain true if no packet was received due to a timeout
 * @return the number of bytes received of -1 on error
 */
ssize_t ChromecastCommunication::receive( uint8_t *p_data, size_t i_size, int i_timeout, bool *pb_timeout )
{
    ssize_t i_received = 0;
    struct pollfd ufd[1];
    ufd[0].fd = vlc_tls_GetFD( m_tls );
    ufd[0].events = POLLIN;

    struct iovec iov;
    iov.iov_base = p_data;
    iov.iov_len = i_size;

    /* The Chromecast normally sends a PING command every 5 seconds or so.
     * If we do not receive one after 6 seconds, we send a PING.
     * If after this PING, we do not receive a PONG, then we consider the
     * connection as dead. */
    do
    {
        ssize_t i_ret = m_tls->readv( m_tls, &iov, 1 );
        if ( i_ret < 0 )
        {
#ifdef _WIN32
            if ( WSAGetLastError() != WSAEWOULDBLOCK )
#else
            if ( errno != EAGAIN )
#endif
            {
                return -1;
            }
            ssize_t val = vlc_poll_i11e(ufd, 1, i_timeout);
            if ( val < 0 )
                return -1;
            else if ( val == 0 )
            {
                *pb_timeout = true;
                return i_received;
            }
            assert( ufd[0].revents & POLLIN );
            continue;
        }
        else if ( i_ret == 0 )
            return -1;
        assert( i_size >= (size_t)i_ret );
        i_size -= i_ret;
        i_received += i_ret;
        iov.iov_base = (uint8_t*)iov.iov_base + i_ret;
        iov.iov_len = i_size;
    } while ( i_size > 0 );
    return i_received;
}


/*****************************************************************************
 * Message preparation
 *****************************************************************************/
unsigned ChromecastCommunication::getNextReceiverRequestId()
{
    unsigned id = m_receiver_requestId++;
    return likely(id != 0) ? id : m_receiver_requestId++;
}

unsigned ChromecastCommunication::getNextRequestId()
{
    unsigned id = m_requestId++;
    return likely(id != 0) ? id : m_requestId++;
}

unsigned ChromecastCommunication::msgAuth()
{
    castchannel::DeviceAuthMessage authMessage;
    authMessage.mutable_challenge();

    return buildMessage(NAMESPACE_DEVICEAUTH, authMessage.SerializeAsString(),
                        DEFAULT_CHOMECAST_RECEIVER, castchannel::CastMessage_PayloadType_BINARY)
           == VLC_SUCCESS ? 1 : kInvalidId;
}


unsigned ChromecastCommunication::msgPing()
{
    std::string s("{\"type\":\"PING\"}");
    return buildMessage( NAMESPACE_HEARTBEAT, s, DEFAULT_CHOMECAST_RECEIVER )
           == VLC_SUCCESS ? 1 : kInvalidId;
}


unsigned ChromecastCommunication::msgPong()
{
    std::string s("{\"type\":\"PONG\"}");
    return buildMessage( NAMESPACE_HEARTBEAT, s, DEFAULT_CHOMECAST_RECEIVER )
           == VLC_SUCCESS ? 1 : kInvalidId;
}

unsigned ChromecastCommunication::msgConnect( const std::string& destinationId )
{
    std::string s("{\"type\":\"CONNECT\"}");
    return buildMessage( NAMESPACE_CONNECTION, s, destinationId )
           == VLC_SUCCESS ? 1 : kInvalidId;
}

unsigned ChromecastCommunication::msgReceiverClose( const std::string& destinationId )
{
    std::string s("{\"type\":\"CLOSE\"}");
    return buildMessage( NAMESPACE_CONNECTION, s, destinationId )
           == VLC_SUCCESS ? 1 : kInvalidId;
}

unsigned ChromecastCommunication::msgReceiverGetStatus()
{
    unsigned id = getNextReceiverRequestId();
    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\","
       <<  "\"requestId\":" << id << "}";

    return buildMessage( NAMESPACE_RECEIVER, ss.str(), DEFAULT_CHOMECAST_RECEIVER )
           == VLC_SUCCESS ? id : kInvalidId;
}

unsigned ChromecastCommunication::msgReceiverLaunchApp()
{
    unsigned id = getNextReceiverRequestId();
    std::stringstream ss;
    ss << "{\"type\":\"LAUNCH\","
       <<  "\"appId\":\"" << APP_ID << "\","
       <<  "\"requestId\":" << id << "}";

    return buildMessage( NAMESPACE_RECEIVER, ss.str(), DEFAULT_CHOMECAST_RECEIVER )
           == VLC_SUCCESS ? id : kInvalidId;
}

unsigned ChromecastCommunication::msgPlayerGetStatus( const std::string& destinationId )
{
    unsigned id = getNextRequestId();
    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\","
       <<  "\"requestId\":" << id
       << "}";

    return pushMediaPlayerMessage( destinationId, ss ) == VLC_SUCCESS ? id : kInvalidId;
}

static std::string escape_json(const std::string &s)
{
    /* Control characters ('\x00' to '\x1f'), '"' and '\"  must be escaped */
    std::ostringstream o;
    for (std::string::const_iterator c = s.begin(); c != s.end(); c++)
    {
        if (*c == '"' || *c == '\\' || ('\x00' <= *c && *c <= '\x1f'))
            o << "\\u"
              << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
        else
            o << *c;
    }
    return o.str();
}

static std::string meta_get_escaped(const vlc_meta_t *p_meta, vlc_meta_type_t type)
{
    const char *psz = vlc_meta_Get(p_meta, type);
    if (!psz)
        return std::string();
    return escape_json(std::string(psz));
}

std::string ChromecastCommunication::GetMedia( unsigned int i_port,
                                               const std::string& mime,
                                               const vlc_meta_t *p_meta )
{
    std::stringstream ss;

    bool b_music = strncmp(mime.c_str(), "audio", strlen("audio")) == 0;

    std::string title;
    std::string artwork;
    std::string artist;
    std::string album;
    std::string albumartist;
    std::string tracknumber;
    std::string discnumber;

    if( p_meta )
    {
        title = meta_get_escaped( p_meta, vlc_meta_Title );
        artwork = meta_get_escaped( p_meta, vlc_meta_ArtworkURL );

        if( b_music && !title.empty() )
        {
            artist = meta_get_escaped( p_meta, vlc_meta_Artist );
            album = meta_get_escaped( p_meta, vlc_meta_Album );
            albumartist = meta_get_escaped( p_meta, vlc_meta_AlbumArtist );
            tracknumber = meta_get_escaped( p_meta, vlc_meta_TrackNumber );
            discnumber = meta_get_escaped( p_meta, vlc_meta_DiscNumber );
        }
        if( title.empty() )
        {
            title = meta_get_escaped( p_meta, vlc_meta_NowPlaying );
            if( title.empty() )
                title = meta_get_escaped( p_meta, vlc_meta_ESNowPlaying );
        }

        if ( !title.empty() )
        {
            ss << "\"metadata\":{"
               << " \"metadataType\":" << ( b_music ? "3" : "0" )
               << ",\"title\":\"" << title << "\"";
            if( b_music )
            {
                if( !artist.empty() )
                    ss << ",\"artist\":\"" << artist << "\"";
                if( album.empty() )
                    ss << ",\"album\":\"" << album << "\"";
                if( albumartist.empty() )
                    ss << ",\"albumArtist\":\"" << albumartist << "\"";
                if( tracknumber.empty() )
                    ss << ",\"trackNumber\":\"" << tracknumber << "\"";
                if( discnumber.empty() )
                    ss << ",\"discNumber\":\"" << discnumber << "\"";
            }

            if ( !artwork.empty() && !strncmp( artwork.c_str(), "http", 4 ) )
                ss << ",\"images\":[{\"url\":\"" << artwork << "\"}]";

            ss << "},";
        }
    }

    std::stringstream chromecast_url;
    chromecast_url << "http://" << m_serverIp << ":" << i_port << "/stream";

    msg_Dbg( m_module, "s_chromecast_url: %s", chromecast_url.str().c_str());

    ss << "\"contentId\":\"" << chromecast_url.str() << "\""
       << ",\"streamType\":\"LIVE\""
       << ",\"contentType\":\"" << mime << "\"";

    return ss.str();
}

unsigned ChromecastCommunication::msgPlayerLoad( const std::string& destinationId, unsigned int i_port,
                                             const std::string& mime, const vlc_meta_t *p_meta )
{
    unsigned id = getNextRequestId();
    std::stringstream ss;
    ss << "{\"type\":\"LOAD\","
       <<  "\"media\":{" << GetMedia( i_port, mime, p_meta ) << "},"
       <<  "\"autoplay\":\"false\","
       <<  "\"requestId\":" << id
       << "}";

    return pushMediaPlayerMessage( destinationId, ss ) == VLC_SUCCESS ? id : kInvalidId;
}

unsigned ChromecastCommunication::msgPlayerPlay( const std::string& destinationId, int64_t mediaSessionId )
{
    assert(mediaSessionId != 0);
    unsigned id = getNextRequestId();

    std::stringstream ss;
    ss << "{\"type\":\"PLAY\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << id
       << "}";

    return pushMediaPlayerMessage( destinationId, ss ) == VLC_SUCCESS ? id : kInvalidId;
}

unsigned ChromecastCommunication::msgPlayerStop( const std::string& destinationId, int64_t mediaSessionId )
{
    assert(mediaSessionId != 0);
    unsigned id = getNextRequestId();

    std::stringstream ss;
    ss << "{\"type\":\"STOP\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << id
       << "}";

    return pushMediaPlayerMessage( destinationId, ss ) == VLC_SUCCESS ? id : kInvalidId;
}

unsigned ChromecastCommunication::msgPlayerPause( const std::string& destinationId, int64_t mediaSessionId )
{
    assert(mediaSessionId != 0);
    unsigned id = getNextRequestId();

    std::stringstream ss;
    ss << "{\"type\":\"PAUSE\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << id
       << "}";

    return pushMediaPlayerMessage( destinationId, ss ) == VLC_SUCCESS ? id : kInvalidId;
}

unsigned ChromecastCommunication::msgPlayerSetVolume( const std::string& destinationId, int64_t mediaSessionId, float f_volume, bool b_mute )
{
    assert(mediaSessionId != 0);
    unsigned id = getNextRequestId();

    if ( f_volume < 0.0 || f_volume > 1.0)
        return VLC_EGENERIC;

    std::stringstream ss;
    ss << "{\"type\":\"SET_VOLUME\","
       <<  "\"volume\":{\"level\":" << f_volume << ",\"muted\":" << ( b_mute ? "true" : "false" ) << "},"
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << id
       << "}";

    return pushMediaPlayerMessage( destinationId, ss ) == VLC_SUCCESS ? id : kInvalidId;
}

/**
 * @brief Send a message to the Chromecast
 * @param msg the CastMessage to send
 * @return vlc error code
 */
int ChromecastCommunication::sendMessage( const castchannel::CastMessage &msg )
{
    int i_size = msg.ByteSize();
    uint8_t *p_data = new(std::nothrow) uint8_t[PACKET_HEADER_LEN + i_size];
    if (p_data == NULL)
        return VLC_ENOMEM;

#ifndef NDEBUG
    msg_Dbg( m_module, "sendMessage: %s->%s %s", msg.namespace_().c_str(), msg.destination_id().c_str(), msg.payload_utf8().c_str());
#endif

    SetDWBE(p_data, i_size);
    msg.SerializeWithCachedSizesToArray(p_data + PACKET_HEADER_LEN);

    int i_ret = vlc_tls_Write(m_tls, p_data, PACKET_HEADER_LEN + i_size);
    delete[] p_data;
    if (i_ret == PACKET_HEADER_LEN + i_size)
        return VLC_SUCCESS;

    msg_Warn( m_module, "failed to send message %s (%s)", msg.payload_utf8().c_str(), strerror( errno ) );

    return VLC_EGENERIC;
}

int ChromecastCommunication::pushMediaPlayerMessage( const std::string& destinationId, const std::stringstream & payload )
{
    assert(!destinationId.empty());
    return buildMessage( NAMESPACE_MEDIA, payload.str(), destinationId );
}
