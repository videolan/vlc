/*****************************************************************************
 * cast.cpp: Chromecast module for vlc
 *****************************************************************************
 * Copyright © 2014 VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL
# include <poll.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_tls.h>
#include <vlc_url.h>
#include <vlc_threads.h>

#include <cerrno>

#include <sstream>
#include <queue>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include "cast_channel.pb.h"

#include "../../misc/webservices/json.h"

// Status
enum
{
    CHROMECAST_DISCONNECTED,
    CHROMECAST_TLS_CONNECTED,
    CHROMECAST_AUTHENTICATED,
    CHROMECAST_APP_STARTED,
    CHROMECAST_MEDIA_LOAD_SENT,
    CHROMECAST_CONNECTION_DEAD,
};

#define PACKET_MAX_LEN 10 * 1024
#define PACKET_HEADER_LEN 4

struct sout_stream_sys_t
{
    sout_stream_sys_t()
        : p_tls(NULL), i_requestId(0),
          i_status(CHROMECAST_DISCONNECTED), p_out(NULL)
    {
    }

    std::string serverIP;

    int i_sock_fd;
    vlc_tls_creds_t *p_creds;
    vlc_tls_t *p_tls;

    vlc_thread_t chromecastThread;

    unsigned i_requestId;
    std::string appTransportId;

    std::queue<castchannel::CastMessage> messagesToSend;

    int i_status;
    vlc_mutex_t lock;
    vlc_cond_t loadCommandCond;

    sout_stream_t *p_out;
};

// Media player Chromecast app id
#define APP_ID "CC1AD845" // Default media player

#define CHROMECAST_CONTROL_PORT 8009
#define HTTP_PORT               8010

#define SOUT_CFG_PREFIX "sout-chromecast-"

/* deadline regarding pings sent from receiver */
#define PING_WAIT_TIME 6000
#define PING_WAIT_RETRIES 0
/* deadline regarding pong we expect after pinging the receiver */
#define PONG_WAIT_TIME 500
#define PONG_WAIT_RETRIES 2

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);
static void Clean(sout_stream_t *p_stream);
static int connectChromecast(sout_stream_t *p_stream, char *psz_ipChromecast);
static void disconnectChromecast(sout_stream_t *p_stream);
static int sendMessages(sout_stream_t *p_stream);

static void msgAuth(sout_stream_t *p_stream);
static void msgPing(sout_stream_t *p_stream);
static void msgPong(sout_stream_t *p_stream);
static void msgConnect(sout_stream_t *p_stream, std::string destinationId);
static void msgClose(sout_stream_t *p_stream, std::string destinationId);
static void msgLaunch(sout_stream_t *p_stream);
static void msgLoad(sout_stream_t *p_stream);
static void msgStatus(sout_stream_t *p_stream);

static void *chromecastThread(void *data);

static const char *const ppsz_sout_options[] = {
    "ip", "http-port", "mux", "mime", NULL
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define IP_TEXT N_("Chromecast IP address")
#define IP_LONGTEXT N_("This sets the IP adress of the Chromecast receiver.")
#define HTTP_PORT_TEXT N_("HTTP port")
#define HTTP_PORT_LONGTEXT N_("This sets the HTTP port of the server " \
                              "used to stream the media to the Chromecast.")
#define MUX_TEXT N_("Muxer")
#define MUX_LONGTEXT N_("This sets the muxer used to stream to the Chromecast.")
#define MIME_TEXT N_("MIME content type")
#define MIME_LONGTEXT N_("This sets the media MIME content type sent to the Chromecast.")

vlc_module_begin ()

    set_shortname(N_("Chromecast"))
    set_description(N_("Chromecast stream output"))
    set_capability("sout stream", 0)
    add_shortcut("chromecast")
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_STREAM)
    set_callbacks(Open, Close)

    add_string(SOUT_CFG_PREFIX "ip", "", IP_TEXT, IP_LONGTEXT, false)
    add_integer(SOUT_CFG_PREFIX "http-port", HTTP_PORT, HTTP_PORT_TEXT, HTTP_PORT_LONGTEXT, false)
    add_string(SOUT_CFG_PREFIX "mux", "mp4stream", MUX_TEXT, MUX_LONGTEXT, false)
    add_string(SOUT_CFG_PREFIX "mime", "video/mp4", MIME_TEXT, MIME_LONGTEXT, false)

vlc_module_end ()


/*****************************************************************************
 * Sout callbacks
 *****************************************************************************/
static sout_stream_id_sys_t *Add(sout_stream_t *p_stream, const es_format_t *p_fmt)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    return p_sys->p_out->pf_add(p_sys->p_out, p_fmt);
}


static void Del(sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    p_sys->p_out->pf_del(p_sys->p_out, id);
}


static int Send(sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                block_t *p_buffer)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    return p_sys->p_out->pf_send(p_sys->p_out, id, p_buffer);
}


/*****************************************************************************
 * Open: connect to the Chromecast and initialize the sout
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    p_sys = new(std::nothrow) sout_stream_sys_t;
    if (p_sys == NULL)
        return VLC_ENOMEM;
    p_stream->p_sys = p_sys;

    config_ChainParse(p_stream, SOUT_CFG_PREFIX, ppsz_sout_options, p_stream->p_cfg);

    char *psz_ipChromecast = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "ip");
    if (psz_ipChromecast == NULL)
    {
        msg_Err(p_stream, "No Chromecast receiver IP provided");
        Clean(p_stream);
        return VLC_EGENERIC;
    }

    p_sys->i_sock_fd = connectChromecast(p_stream, psz_ipChromecast);
    free(psz_ipChromecast);
    if (p_sys->i_sock_fd < 0)
    {
        msg_Err(p_stream, "Could not connect the Chromecast");
        Clean(p_stream);
        return VLC_EGENERIC;
    }
    p_sys->i_status = CHROMECAST_TLS_CONNECTED;

    char psz_localIP[NI_MAXNUMERICHOST];
    if (net_GetSockAddress(p_sys->i_sock_fd, psz_localIP, NULL))
    {
        msg_Err(p_this, "Cannot get local IP address");
        Clean(p_stream);
        return VLC_EGENERIC;
    }
    p_sys->serverIP = psz_localIP;

    char *psz_mux = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "mux");
    if (psz_mux == NULL)
    {
        Clean(p_stream);
        return VLC_EGENERIC;
    }
    char *psz_chain = NULL;
    int i_bytes = asprintf(&psz_chain, "http{dst=:%u/stream,mux=%s}",
                           (unsigned)var_InheritInteger(p_stream, SOUT_CFG_PREFIX"http-port"),
                           psz_mux);
    free(psz_mux);
    if (i_bytes < 0)
    {
        Clean(p_stream);
        return VLC_EGENERIC;
    }

    p_sys->p_out = sout_StreamChainNew(p_stream->p_sout, psz_chain, NULL, NULL);
    free(psz_chain);
    if (p_sys->p_out == NULL)
    {
        Clean(p_stream);
        return VLC_EGENERIC;
    }

    vlc_mutex_init(&p_sys->lock);
    vlc_cond_init(&p_sys->loadCommandCond);

    // Start the Chromecast event thread.
    if (vlc_clone(&p_sys->chromecastThread, chromecastThread, p_stream,
                  VLC_THREAD_PRIORITY_LOW))
    {
        msg_Err(p_stream, "Could not start the Chromecast talking thread");
        Clean(p_stream);
        return VLC_EGENERIC;
    }

    /* Ugly part:
     * We want to be sure that the Chromecast receives the first data packet sent by
     * the HTTP server. */

    // Lock the sout thread until we have sent the media loading command to the Chromecast.
    int i_ret = 0;
    const mtime_t deadline = mdate() + 6 * CLOCK_FREQ;
    vlc_mutex_lock(&p_sys->lock);
    while (p_sys->i_status != CHROMECAST_MEDIA_LOAD_SENT)
    {
        i_ret = vlc_cond_timedwait(&p_sys->loadCommandCond, &p_sys->lock, deadline);
        if (i_ret == ETIMEDOUT)
        {
            msg_Err(p_stream, "Timeout reached before sending the media loading command");
            vlc_mutex_unlock(&p_sys->lock);
            vlc_cancel(p_sys->chromecastThread);
            Clean(p_stream);
            return VLC_EGENERIC;
        }
    }
    vlc_mutex_unlock(&p_sys->lock);

    /* Even uglier: sleep more to let to the Chromecast initiate the connection
     * to the http server. */
    msleep(2 * CLOCK_FREQ);

    // Set the sout callbacks.
    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    vlc_cancel(p_sys->chromecastThread);
    vlc_join(p_sys->chromecastThread, NULL);

    switch (p_sys->i_status)
    {
    case CHROMECAST_MEDIA_LOAD_SENT:
    case CHROMECAST_APP_STARTED:
        // Generate the close messages.
        msgClose(p_stream, p_sys->appTransportId);
        // ft
    case CHROMECAST_AUTHENTICATED:
        msgClose(p_stream, "receiver-0");
        // Send the just added close messages.
        sendMessages(p_stream);
        // ft
    default:
        break;
    }

    Clean(p_stream);
}


/**
 * @brief Clean and release the variables in a sout_stream_sys_t structure
 */
static void Clean(sout_stream_t *p_stream)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if (p_sys->p_out)
    {
        vlc_mutex_destroy(&p_sys->lock);
        vlc_cond_destroy(&p_sys->loadCommandCond);
        sout_StreamChainDelete(p_sys->p_out, p_sys->p_out);
    }

    disconnectChromecast(p_stream);

    delete p_sys;
}


/**
 * @brief Connect to the Chromecast
 * @param p_stream the sout_stream_t structure
 * @return the opened socket file descriptor or -1 on error
 */
static int connectChromecast(sout_stream_t *p_stream, char *psz_ipChromecast)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int fd = net_ConnectTCP(p_stream, psz_ipChromecast, CHROMECAST_CONTROL_PORT);
    if (fd < 0)
        return -1;

    p_sys->p_creds = vlc_tls_ClientCreate(VLC_OBJECT(p_stream));
    if (p_sys->p_creds == NULL)
    {
        net_Close(fd);
        return -1;
    }

    p_sys->p_tls = vlc_tls_ClientSessionCreate(p_sys->p_creds, fd, psz_ipChromecast,
                                               "tcps", NULL, NULL);

    if (p_sys->p_tls == NULL)
    {
        vlc_tls_Delete(p_sys->p_creds);
        return -1;
    }

    return fd;
}


/**
 * @brief Disconnect from the Chromecast
 */
static void disconnectChromecast(sout_stream_t *p_stream)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if (p_sys->p_tls)
    {
        vlc_tls_SessionDelete(p_sys->p_tls);
        vlc_tls_Delete(p_sys->p_creds);
        p_sys->p_tls = NULL;
        p_sys->i_status = CHROMECAST_DISCONNECTED;
    }
}


/**
 * @brief Send a message to the Chromecast
 * @param p_stream the sout_stream_t structure
 * @param msg the CastMessage to send
 * @return the number of bytes sent or -1 on error
 */
static int sendMessage(sout_stream_t *p_stream, castchannel::CastMessage &msg)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    uint32_t i_size = msg.ByteSize();
    uint32_t i_sizeNetwork = hton32(i_size);

    char *p_data = new(std::nothrow) char[PACKET_HEADER_LEN + i_size];
    if (p_data == NULL)
        return -1;

    memcpy(p_data, &i_sizeNetwork, PACKET_HEADER_LEN);
    msg.SerializeWithCachedSizesToArray((uint8_t *)(p_data + PACKET_HEADER_LEN));

    int i_ret = tls_Send(p_sys->p_tls, p_data, PACKET_HEADER_LEN + i_size);
    delete[] p_data;

    return i_ret;
}


/**
 * @brief Send all the messages in the pending queue to the Chromecast
 * @param p_stream the sout_stream_t structure
 * @param msg the CastMessage to send
 * @return the number of bytes sent or -1 on error
 */
static int sendMessages(sout_stream_t *p_stream)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    int i_ret = 0;
    while (!p_sys->messagesToSend.empty())
    {
        unsigned i_retSend = sendMessage(p_stream, p_sys->messagesToSend.front());
        if (i_retSend <= 0)
            return i_retSend;

        p_sys->messagesToSend.pop();
        i_ret += i_retSend;
    }

    return i_ret;
}




/**
 * @brief Receive a data packet from the Chromecast
 * @param p_stream the sout_stream_t structure
 * @param b_msgReceived returns true if a message has been entirely received else false
 * @param i_payloadSize returns the payload size of the message received
 * @return the number of bytes received of -1 on error
 */
// Use here only C linkage and POD types as this function is a cancelation point.
extern "C" int recvPacket(sout_stream_t *p_stream, bool &b_msgReceived,
                          uint32_t &i_payloadSize, int i_sock_fd, vlc_tls_t *p_tls,
                          unsigned *pi_received, char *p_data, bool *pb_pingTimeout,
                          int *pi_wait_delay, int *pi_wait_retries)
{
    struct pollfd ufd[1];
    ufd[0].fd = i_sock_fd;
    ufd[0].events = POLLIN;

    /* The Chromecast normally sends a PING command every 5 seconds or so.
     * If we do not receive one after 6 seconds, we send a PING.
     * If after this PING, we do not receive a PONG, then we consider the
     * connection as dead. */
    if (poll(ufd, 1, *pi_wait_delay) == 0)
    {
        if (*pb_pingTimeout)
        {
            if (!*pi_wait_retries)
            {
                msg_Err(p_stream, "No PONG answer received from the Chromecast");
                return 0; // Connection died
            }
            (*pi_wait_retries)--;
        }
        else
        {
            /* now expect a pong */
            *pi_wait_delay = PONG_WAIT_TIME;
            *pi_wait_retries = PONG_WAIT_RETRIES;
            msg_Warn(p_stream, "No PING received from the Chromecast, sending a PING");
        }
        *pb_pingTimeout = true;
    }
    else
    {
        *pb_pingTimeout = false;
        /* reset to default ping waiting */
        *pi_wait_delay = PING_WAIT_TIME;
        *pi_wait_retries = PING_WAIT_RETRIES;
    }

    int i_ret;

    /* Packet structure:
     * +------------------------------------+------------------------------+
     * | Payload size (uint32_t big endian) |         Payload data         |
     * +------------------------------------+------------------------------+ */
    if (*pi_received < PACKET_HEADER_LEN)
    {
        // We receive the header.
        i_ret = tls_Recv(p_tls, p_data, PACKET_HEADER_LEN - *pi_received);
        if (i_ret <= 0)
            return i_ret;
        *pi_received += i_ret;
    }
    else
    {
        // We receive the payload.

        // Get the size of the payload
        memcpy(&i_payloadSize, p_data, PACKET_HEADER_LEN);
        i_payloadSize = hton32(i_payloadSize);
        const uint32_t i_maxPayloadSize = PACKET_MAX_LEN - PACKET_HEADER_LEN;

        if (i_payloadSize > i_maxPayloadSize)
        {
            // Error case: the packet sent by the Chromecast is too long: we drop it.
            msg_Err(p_stream, "Packet too long: droping its data");

            uint32_t i_size = i_payloadSize - (*pi_received - PACKET_HEADER_LEN);
            if (i_size > i_maxPayloadSize)
                i_size = i_maxPayloadSize;

            i_ret = tls_Recv(p_tls, p_data + PACKET_HEADER_LEN, i_size);
            if (i_ret <= 0)
                return i_ret;
            *pi_received += i_ret;

            if (*pi_received < i_payloadSize + PACKET_HEADER_LEN)
                return i_ret;

            *pi_received = 0;
            return -1;
        }

        // Normal case
        i_ret = tls_Recv(p_tls, p_data + *pi_received,
                         i_payloadSize - (*pi_received - PACKET_HEADER_LEN));
        if (i_ret <= 0)
            return i_ret;
        *pi_received += i_ret;

        if (*pi_received < i_payloadSize + PACKET_HEADER_LEN)
            return i_ret;

        assert(*pi_received == i_payloadSize + PACKET_HEADER_LEN);
        *pi_received = 0;
        b_msgReceived = true;
        return i_ret;
    }

    return i_ret;
}


/**
 * @brief Process a message received from the Chromecast
 * @param p_stream the sout_stream_t structure
 * @param msg the CastMessage to process
 * @return 0 if the message has been successfuly processed else -1
 */
static int processMessage(sout_stream_t *p_stream, const castchannel::CastMessage &msg)
{
    int i_ret = 0;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    std::string namespace_ = msg.namespace_();

    if (namespace_ == "urn:x-cast:com.google.cast.tp.deviceauth")
    {
        castchannel::DeviceAuthMessage authMessage;
        authMessage.ParseFromString(msg.payload_binary());

        if (authMessage.has_error())
        {
            msg_Err(p_stream, "Authentification error: %d", authMessage.error().error_type());
            i_ret = -1;
        }
        else if (!authMessage.has_response())
        {
            msg_Err(p_stream, "Authentification message has no response field");
            i_ret = -1;
        }
        else
        {
            vlc_mutex_locker locker(&p_sys->lock);
            p_sys->i_status = CHROMECAST_AUTHENTICATED;
            msgConnect(p_stream, "receiver-0");
            msgLaunch(p_stream);
        }
    }
    else if (namespace_ == "urn:x-cast:com.google.cast.tp.heartbeat")
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);

        if (type == "PING")
        {
            msg_Dbg(p_stream, "PING received from the Chromecast");
            msgPong(p_stream);
        }
        else if (type == "PONG")
        {
            msg_Dbg(p_stream, "PONG received from the Chromecast");
        }
        else
        {
            msg_Err(p_stream, "Heartbeat command not supported");
            i_ret = -1;
        }

        json_value_free(p_data);
    }
    else if (namespace_ == "urn:x-cast:com.google.cast.receiver")
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);

        if (type == "RECEIVER_STATUS")
        {
            json_value applications = (*p_data)["status"]["applications"];
            const json_value *p_app = NULL;
            for (unsigned i = 0; i < applications.u.array.length; ++i)
            {
                std::string appId(applications[i]["appId"]);
                if (appId == APP_ID)
                {
                    p_app = &applications[i];
                    vlc_mutex_lock(&p_sys->lock);
                    if (p_sys->appTransportId.empty())
                        p_sys->appTransportId = std::string(applications[i]["transportId"]);
                    vlc_mutex_unlock(&p_sys->lock);
                    break;
                }
            }

            vlc_mutex_lock(&p_sys->lock);
            if ( p_app )
            {
                if (!p_sys->appTransportId.empty()
                        && p_sys->i_status == CHROMECAST_AUTHENTICATED)
                {
                    p_sys->i_status = CHROMECAST_APP_STARTED;
                    msgConnect(p_stream, p_sys->appTransportId);
                    msgLoad(p_stream);
                    p_sys->i_status = CHROMECAST_MEDIA_LOAD_SENT;
                    vlc_cond_signal(&p_sys->loadCommandCond);
                }
            }
            else
            {
                switch(p_sys->i_status)
                {
                /* If the app is no longer present */
                case CHROMECAST_APP_STARTED:
                case CHROMECAST_MEDIA_LOAD_SENT:
                    msg_Warn(p_stream, "app is no longer present. closing");
                    msgClose(p_stream, p_sys->appTransportId);
                    p_sys->i_status = CHROMECAST_CONNECTION_DEAD;
                    // ft
                default:
                    break;
                }

            }
            vlc_mutex_unlock(&p_sys->lock);
        }
        else
        {
            msg_Err(p_stream, "Receiver command not supported: %s",
                    msg.payload_utf8().c_str());
            i_ret = -1;
        }

        json_value_free(p_data);
    }
    else if (namespace_ == "urn:x-cast:com.google.cast.media")
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);

        if (type == "MEDIA_STATUS")
        {
            json_value status = (*p_data)["status"];
            msg_Dbg(p_stream, "Player state: %s",
                    status[0]["playerState"].operator const char *());
        }
        else if (type == "LOAD_FAILED")
        {
            msg_Err(p_stream, "Media load failed");
            msgClose(p_stream, p_sys->appTransportId);
            vlc_mutex_lock(&p_sys->lock);
            p_sys->i_status = CHROMECAST_CONNECTION_DEAD;
            vlc_mutex_unlock(&p_sys->lock);
        }
        else
        {
            msg_Err(p_stream, "Media command not supported: %s",
                    msg.payload_utf8().c_str());
            i_ret = -1;
        }

        json_value_free(p_data);
    }
    else if (namespace_ == "urn:x-cast:com.google.cast.tp.connection")
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);
        json_value_free(p_data);

        if (type == "CLOSE")
        {
            msg_Warn(p_stream, "received close message");
            vlc_mutex_lock(&p_sys->lock);
            p_sys->i_status = CHROMECAST_CONNECTION_DEAD;
            vlc_mutex_unlock(&p_sys->lock);
        }
    }
    else
    {
        msg_Err(p_stream, "Unknown namespace: %s", msg.namespace_().c_str());
        i_ret = -1;
    }

    return i_ret;
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
static castchannel::CastMessage buildMessage(std::string namespace_,
                                castchannel::CastMessage_PayloadType payloadType,
                                std::string payload, std::string destinationId = "receiver-0")
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

    return msg;
}


/*****************************************************************************
 * Message preparation
 *****************************************************************************/
static void msgAuth(sout_stream_t *p_stream)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    castchannel::DeviceAuthMessage authMessage;
    authMessage.mutable_challenge();
    std::string authMessageString;
    authMessage.SerializeToString(&authMessageString);

    castchannel::CastMessage msg = buildMessage("urn:x-cast:com.google.cast.tp.deviceauth",
        castchannel::CastMessage_PayloadType_BINARY, authMessageString);

    p_sys->messagesToSend.push(msg);
}


static void msgPing(sout_stream_t *p_stream)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    std::string s("{\"type\":\"PING\"}");
    castchannel::CastMessage msg = buildMessage("urn:x-cast:com.google.cast.tp.heartbeat",
        castchannel::CastMessage_PayloadType_STRING, s);

    p_sys->messagesToSend.push(msg);
}


static void msgPong(sout_stream_t *p_stream)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    std::string s("{\"type\":\"PONG\"}");
    castchannel::CastMessage msg = buildMessage("urn:x-cast:com.google.cast.tp.heartbeat",
        castchannel::CastMessage_PayloadType_STRING, s);

    p_sys->messagesToSend.push(msg);
}


static void msgConnect(sout_stream_t *p_stream, std::string destinationId)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    std::string s("{\"type\":\"CONNECT\"}");
    castchannel::CastMessage msg = buildMessage("urn:x-cast:com.google.cast.tp.connection",
        castchannel::CastMessage_PayloadType_STRING, s, destinationId);

    p_sys->messagesToSend.push(msg);
}


static void msgClose(sout_stream_t *p_stream, std::string destinationId)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    std::string s("{\"type\":\"CLOSE\"}");
    castchannel::CastMessage msg = buildMessage("urn:x-cast:com.google.cast.tp.connection",
        castchannel::CastMessage_PayloadType_STRING, s, destinationId);

    p_sys->messagesToSend.push(msg);
}

static void msgStatus(sout_stream_t *p_stream)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\"}";

    castchannel::CastMessage msg = buildMessage("urn:x-cast:com.google.cast.receiver",
        castchannel::CastMessage_PayloadType_STRING, ss.str());

    p_sys->messagesToSend.push(msg);
}

static void msgLaunch(sout_stream_t *p_stream)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    std::stringstream ss;
    ss << "{\"type\":\"LAUNCH\","
       <<  "\"appId\":\"" << APP_ID << "\","
       <<  "\"requestId\":" << p_stream->p_sys->i_requestId++ << "}";

    castchannel::CastMessage msg = buildMessage("urn:x-cast:com.google.cast.receiver",
        castchannel::CastMessage_PayloadType_STRING, ss.str());

    p_sys->messagesToSend.push(msg);
}


static void msgLoad(sout_stream_t *p_stream)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    char *psz_mime = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "mime");
    if (psz_mime == NULL)
        return;

    std::stringstream ss;
    ss << "{\"type\":\"LOAD\","
       <<  "\"media\":{\"contentId\":\"http://" << p_sys->serverIP << ":"
           << var_InheritInteger(p_stream, SOUT_CFG_PREFIX"http-port")
           << "/stream\","
       <<             "\"streamType\":\"LIVE\","
       <<             "\"contentType\":\"" << std::string(psz_mime) << "\"},"
       <<  "\"requestId\":" << p_stream->p_sys->i_requestId++ << "}";

    free(psz_mime);

    castchannel::CastMessage msg = buildMessage("urn:x-cast:com.google.cast.media",
        castchannel::CastMessage_PayloadType_STRING, ss.str(), p_sys->appTransportId);

    p_sys->messagesToSend.push(msg);
}


/*****************************************************************************
 * Chromecast thread
 *****************************************************************************/
static void* chromecastThread(void* p_data)
{
    int canc = vlc_savecancel();
    // Not cancellation-safe part.
    sout_stream_t* p_stream = (sout_stream_t*)p_data;
    sout_stream_sys_t* p_sys = p_stream->p_sys;

    unsigned i_received = 0;
    char p_packet[PACKET_MAX_LEN];
    bool b_pingTimeout = false;

    int i_waitdelay = PING_WAIT_TIME;
    int i_retries = PING_WAIT_RETRIES;

    msgAuth(p_stream);
    sendMessages(p_stream);
    vlc_restorecancel(canc);

    while (1)
    {
        bool b_msgReceived = false;
        uint32_t i_payloadSize = 0;
        int i_ret = recvPacket(p_stream, b_msgReceived, i_payloadSize, p_sys->i_sock_fd,
                               p_sys->p_tls, &i_received, p_packet, &b_pingTimeout,
                               &i_waitdelay, &i_retries);

        canc = vlc_savecancel();
        // Not cancellation-safe part.

#if defined(_WIN32)
        if ((i_ret < 0 && WSAGetLastError() != WSAEWOULDBLOCK) || (i_ret == 0))
#else
        if ((i_ret < 0 && errno != EAGAIN) || i_ret == 0)
#endif
        {
            msg_Err(p_stream, "The connection to the Chromecast died.");
            vlc_mutex_locker locker(&p_sys->lock);
            p_sys->i_status = CHROMECAST_CONNECTION_DEAD;
            break;
        }

        if (b_pingTimeout)
        {
            msgPing(p_stream);
            msgStatus(p_stream);
        }

        if (b_msgReceived)
        {
            castchannel::CastMessage msg;
            msg.ParseFromArray(p_packet + PACKET_HEADER_LEN, i_payloadSize);
            processMessage(p_stream, msg);
        }

        // Send the answer messages if there is any.
        if (!p_sys->messagesToSend.empty())
        {
            i_ret = sendMessages(p_stream);
#if defined(_WIN32)
            if ((i_ret < 0 && WSAGetLastError() != WSAEWOULDBLOCK) || (i_ret == 0))
#else
            if ((i_ret < 0 && errno != EAGAIN) || i_ret == 0)
#endif
            {
                msg_Err(p_stream, "The connection to the Chromecast died.");
                vlc_mutex_locker locker(&p_sys->lock);
                p_sys->i_status = CHROMECAST_CONNECTION_DEAD;
            }
        }

        vlc_mutex_lock(&p_sys->lock);
        if ( p_sys->i_status == CHROMECAST_CONNECTION_DEAD )
        {
            vlc_mutex_unlock(&p_sys->lock);
            break;
        }
        vlc_mutex_unlock(&p_sys->lock);

        vlc_restorecancel(canc);
    }

    return NULL;
}
