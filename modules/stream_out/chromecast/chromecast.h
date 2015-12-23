/*****************************************************************************
 * chromecast.cpp: Chromecast module for vlc
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

#ifndef VLC_CHROMECAST_H
#define VLC_CHROMECAST_H

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_tls.h>

#include <queue>

#include "cast_channel.pb.h"

#define PACKET_HEADER_LEN 4

// Media player Chromecast app id
static const std::string DEFAULT_CHOMECAST_RECEIVER = "receiver-0";
static const std::string NAMESPACE_DEVICEAUTH       = "urn:x-cast:com.google.cast.tp.deviceauth";
static const std::string NAMESPACE_CONNECTION       = "urn:x-cast:com.google.cast.tp.connection";
static const std::string NAMESPACE_HEARTBEAT        = "urn:x-cast:com.google.cast.tp.heartbeat";
static const std::string NAMESPACE_RECEIVER         = "urn:x-cast:com.google.cast.receiver";
/* see https://developers.google.com/cast/docs/reference/messages */
static const std::string NAMESPACE_MEDIA            = "urn:x-cast:com.google.cast.media";


// Status
enum connection_status
{
    CHROMECAST_DISCONNECTED,
    CHROMECAST_TLS_CONNECTED,
    CHROMECAST_AUTHENTICATED,
    CHROMECAST_APP_STARTED,
    CHROMECAST_MEDIA_LOAD_SENT,
    CHROMECAST_CONNECTION_DEAD,
};

struct intf_sys_t
{
    intf_sys_t(sout_stream_t * const p_stream);
    ~intf_sys_t();

    sout_stream_t  * const p_stream;
    std::string    serverIP;
    std::string appTransportId;

    vlc_tls_t *p_tls;

    vlc_mutex_t  lock;
    vlc_cond_t   loadCommandCond;

    void msgAuth();
    void msgReceiverClose(std::string destinationId);

    void handleMessages();
    int sendMessages();

    connection_status getConnectionStatus() const
    {
        return conn_status;
    }

    void setConnectionStatus(connection_status status)
    {
        if (conn_status != status)
        {
#ifndef NDEBUG
            msg_Dbg(p_stream, "change Chromecast connection status from %d to %d", conn_status, status);
#endif
            conn_status = status;
            vlc_cond_broadcast(&loadCommandCond);
        }
    }

    void msgPing();
    void msgPong();
    void msgConnect(const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER);

    void msgReceiverLaunchApp();
    void msgReceiverGetStatus();

    void msgPlayerLoad();

    std::queue<castchannel::CastMessage> messagesToSend;

    void processMessage(const castchannel::CastMessage &msg);

private:
    int sendMessage(castchannel::CastMessage &msg);

    void buildMessage(const std::string & namespace_,
                      castchannel::CastMessage_PayloadType payloadType,
                      const std::string & payload,
                      const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER);

    enum connection_status conn_status;

    unsigned i_requestId;
};

#endif /* VLC_CHROMECAST_H */
