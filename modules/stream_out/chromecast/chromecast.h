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

#include <queue>

#include "cast_channel.pb.h"

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

struct intf_sys_t
{
    intf_sys_t(sout_stream_t * const p_stream);
    ~intf_sys_t();

    sout_stream_t  * const p_stream;
    std::string    serverIP;
    std::string appTransportId;

    void msgAuth();
    void msgClose(std::string destinationId);
    void msgPing();
    void msgPong();
    void msgConnect(std::string destinationId);
    void msgLaunch();
    void msgLoad();
    void msgStatus();

    std::queue<castchannel::CastMessage> messagesToSend;
    unsigned i_requestId;
};

#endif /* VLC_CHROMECAST_H */
