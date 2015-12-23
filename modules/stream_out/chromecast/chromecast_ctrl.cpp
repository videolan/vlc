/*****************************************************************************
 * chromecast_ctrl.cpp: Chromecast module for vlc
 *****************************************************************************
 * Copyright © 2014-2015 VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Steve Lhomme <robux4@videolabs.io>
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

#include "chromecast.h"

#include <vlc_sout.h>

#include <sstream>

#include "../../misc/webservices/json.h"

// Media player Chromecast app id
#define APP_ID "CC1AD845" // Default media player

#define SOUT_CFG_PREFIX "sout-chromecast-"

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
                                std::string payload, std::string destinationId = DEFAULT_CHOMECAST_RECEIVER)
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

intf_sys_t::intf_sys_t(sout_stream_t * const p_this)
 : p_stream(p_this)
 , conn_status(CHROMECAST_DISCONNECTED)
 , i_requestId(0)
{
    vlc_mutex_init(&lock);
    vlc_cond_init(&loadCommandCond);
}

intf_sys_t::~intf_sys_t()
{
    vlc_cond_destroy(&loadCommandCond);
    vlc_mutex_destroy(&lock);
}

/**
 * @brief Process a message received from the Chromecast
 * @param msg the CastMessage to process
 * @return 0 if the message has been successfuly processed else -1
 */
void intf_sys_t::processMessage(const castchannel::CastMessage &msg)
{
    std::string namespace_ = msg.namespace_();

    if (namespace_ == NAMESPACE_DEVICEAUTH)
    {
        castchannel::DeviceAuthMessage authMessage;
        authMessage.ParseFromString(msg.payload_binary());

        if (authMessage.has_error())
        {
            msg_Err(p_stream, "Authentification error: %d", authMessage.error().error_type());
        }
        else if (!authMessage.has_response())
        {
            msg_Err(p_stream, "Authentification message has no response field");
        }
        else
        {
            vlc_mutex_locker locker(&lock);
            conn_status = CHROMECAST_AUTHENTICATED;
            msgConnect(DEFAULT_CHOMECAST_RECEIVER);
            msgReceiverLaunchApp();
        }
    }
    else if (namespace_ == NAMESPACE_HEARTBEAT)
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);

        if (type == "PING")
        {
            msg_Dbg(p_stream, "PING received from the Chromecast");
            msgPong();
        }
        else if (type == "PONG")
        {
            msg_Dbg(p_stream, "PONG received from the Chromecast");
        }
        else
        {
            msg_Err(p_stream, "Heartbeat command not supported: %s", type.c_str());
        }

        json_value_free(p_data);
    }
    else if (namespace_ == NAMESPACE_RECEIVER)
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
                    vlc_mutex_lock(&lock);
                    if (appTransportId.empty())
                        appTransportId = std::string(applications[i]["transportId"]);
                    vlc_mutex_unlock(&lock);
                    break;
                }
            }

            vlc_mutex_lock(&lock);
            if ( p_app )
            {
                if (!appTransportId.empty()
                        && conn_status == CHROMECAST_AUTHENTICATED)
                {
                    conn_status = CHROMECAST_APP_STARTED;
                    msgConnect(appTransportId);
                    msgPlayerLoad();
                    conn_status = CHROMECAST_MEDIA_LOAD_SENT;
                    vlc_cond_signal(&loadCommandCond);
                }
            }
            else
            {
                switch(conn_status)
                {
                /* If the app is no longer present */
                case CHROMECAST_APP_STARTED:
                case CHROMECAST_MEDIA_LOAD_SENT:
                    msg_Warn(p_stream, "app is no longer present. closing");
                    msgReceiverClose(appTransportId);
                    conn_status = CHROMECAST_CONNECTION_DEAD;
                default:
                    break;
                }

            }
            vlc_mutex_unlock(&lock);
        }
        else
        {
            msg_Err(p_stream, "Receiver command not supported: %s",
                    msg.payload_utf8().c_str());
        }

        json_value_free(p_data);
    }
    else if (namespace_ == NAMESPACE_MEDIA)
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
            msgReceiverClose(appTransportId);
            vlc_mutex_lock(&lock);
            conn_status = CHROMECAST_CONNECTION_DEAD;
            vlc_mutex_unlock(&lock);
        }
        else
        {
            msg_Err(p_stream, "Media command not supported: %s",
                    msg.payload_utf8().c_str());
        }

        json_value_free(p_data);
    }
    else if (namespace_ == NAMESPACE_CONNECTION)
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);
        json_value_free(p_data);

        if (type == "CLOSE")
        {
            msg_Warn(p_stream, "received close message");
            vlc_mutex_lock(&lock);
            conn_status = CHROMECAST_CONNECTION_DEAD;
            vlc_mutex_unlock(&lock);
        }
        else
        {
            msg_Err(p_stream, "Connection command not supported: %s",
                    type.c_str());
        }
    }
    else
    {
        msg_Err(p_stream, "Unknown namespace: %s", msg.namespace_().c_str());
    }
}

/*****************************************************************************
 * Message preparation
 *****************************************************************************/
void intf_sys_t::msgAuth()
{
    castchannel::DeviceAuthMessage authMessage;
    authMessage.mutable_challenge();
    std::string authMessageString;
    authMessage.SerializeToString(&authMessageString);

    castchannel::CastMessage msg = buildMessage(NAMESPACE_DEVICEAUTH,
        castchannel::CastMessage_PayloadType_BINARY, authMessageString);

    messagesToSend.push(msg);
}


void intf_sys_t::msgPing()
{
    std::string s("{\"type\":\"PING\"}");
    castchannel::CastMessage msg = buildMessage(NAMESPACE_HEARTBEAT,
        castchannel::CastMessage_PayloadType_STRING, s);

    messagesToSend.push(msg);
}


void intf_sys_t::msgPong()
{
    std::string s("{\"type\":\"PONG\"}");
    castchannel::CastMessage msg = buildMessage(NAMESPACE_HEARTBEAT,
        castchannel::CastMessage_PayloadType_STRING, s);

    messagesToSend.push(msg);
}

void intf_sys_t::msgConnect(const std::string & destinationId)
{
    std::string s("{\"type\":\"CONNECT\"}");
    castchannel::CastMessage msg = buildMessage(NAMESPACE_CONNECTION,
        castchannel::CastMessage_PayloadType_STRING, s, destinationId);

    messagesToSend.push(msg);
}


void intf_sys_t::msgReceiverClose(std::string destinationId)
{
    std::string s("{\"type\":\"CLOSE\"}");
    castchannel::CastMessage msg = buildMessage(NAMESPACE_CONNECTION,
        castchannel::CastMessage_PayloadType_STRING, s, destinationId);

    messagesToSend.push(msg);
}

void intf_sys_t::msgReceiverGetStatus()
{
    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\"}";

    castchannel::CastMessage msg = buildMessage(NAMESPACE_RECEIVER,
        castchannel::CastMessage_PayloadType_STRING, ss.str());

    messagesToSend.push(msg);
}

void intf_sys_t::msgReceiverLaunchApp()
{
    std::stringstream ss;
    ss << "{\"type\":\"LAUNCH\","
       <<  "\"appId\":\"" << APP_ID << "\","
       <<  "\"requestId\":" << i_requestId++ << "}";

    castchannel::CastMessage msg = buildMessage(NAMESPACE_RECEIVER,
        castchannel::CastMessage_PayloadType_STRING, ss.str());

    messagesToSend.push(msg);
}


void intf_sys_t::msgPlayerLoad()
{
    char *psz_mime = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "mime");
    if (psz_mime == NULL)
        return;

    std::stringstream ss;
    ss << "{\"type\":\"LOAD\","
       <<  "\"media\":{\"contentId\":\"http://" << serverIP << ":"
           << var_InheritInteger(p_stream, SOUT_CFG_PREFIX"http-port")
           << "/stream\","
       <<             "\"streamType\":\"LIVE\","
       <<             "\"contentType\":\"" << std::string(psz_mime) << "\"},"
       <<  "\"requestId\":" << i_requestId++ << "}";

    free(psz_mime);

    castchannel::CastMessage msg = buildMessage("urn:x-cast:com.google.cast.media",
        castchannel::CastMessage_PayloadType_STRING, ss.str(), appTransportId);

    messagesToSend.push(msg);
}

