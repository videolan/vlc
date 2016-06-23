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
#include <vlc_tls.h>
#include <vlc_interrupt.h>

#include <atomic>
#include <sstream>

#include "cast_channel.pb.h"
#include "chromecast_common.h"

#define PACKET_HEADER_LEN 4

// Media player Chromecast app id
static const std::string DEFAULT_CHOMECAST_RECEIVER = "receiver-0";
/* see https://developers.google.com/cast/docs/reference/messages */
static const std::string NAMESPACE_MEDIA            = "urn:x-cast:com.google.cast.media";

#define CHROMECAST_CONTROL_PORT 8009
#define HTTP_PORT               8010

// Status
enum connection_status
{
    CHROMECAST_DISCONNECTED,
    CHROMECAST_TLS_CONNECTED,
    CHROMECAST_AUTHENTICATED,
    CHROMECAST_APP_STARTED,
    CHROMECAST_CONNECTION_DEAD,
};

enum command_status {
    NO_CMD_PENDING,
    CMD_LOAD_SENT,
    CMD_PLAYBACK_SENT,
    CMD_SEEK_SENT,
};

enum receiver_state {
    RECEIVER_IDLE,
    RECEIVER_PLAYING,
    RECEIVER_BUFFERING,
    RECEIVER_PAUSED,
};


/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    intf_sys_t(vlc_object_t * const p_this, int local_port, std::string device_addr, int device_port, vlc_interrupt_t *);
    ~intf_sys_t();

    bool isFinishedPlaying() {
        vlc_mutex_locker locker(&lock);
        return conn_status == CHROMECAST_CONNECTION_DEAD || (receiverState == RECEIVER_BUFFERING && cmd_status != CMD_SEEK_SENT);
    }

    void setHasInput( bool has_input, const std::string mime_type = "");

    void requestPlayerSeek(mtime_t pos);
    void requestPlayerStop();

private:
    vlc_object_t  * const p_module;
    const int      i_port;
    std::string    serverIP;
    const int      i_target_port;
    std::string    targetIP;
    std::string    mime;

    std::string appTransportId;
    std::string mediaSessionId;
    receiver_state receiverState;

    int i_sock_fd;
    vlc_tls_creds_t *p_creds;
    vlc_tls_t *p_tls;

    vlc_mutex_t  lock;
    vlc_cond_t   loadCommandCond;
    vlc_thread_t chromecastThread;

    void msgAuth();
    void msgReceiverClose(std::string destinationId);

    bool handleMessages();

    void setConnectionStatus(connection_status status)
    {
        if (conn_status != status)
        {
#ifndef NDEBUG
            msg_Dbg(p_module, "change Chromecast connection status from %d to %d", conn_status, status);
#endif
            conn_status = status;
            vlc_cond_broadcast(&loadCommandCond);
            vlc_cond_signal(&seekCommandCond);
        }
    }

    void waitAppStarted();
    void waitSeekDone();

    int connectChromecast();
    void disconnectChromecast();

    void msgPing();
    void msgPong();
    void msgConnect(const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER);

    void msgReceiverLaunchApp();
    void msgReceiverGetStatus();

    void msgPlayerLoad();
    void msgPlayerPlay();
    void msgPlayerStop();
    void msgPlayerPause();
    void msgPlayerGetStatus();
    void msgPlayerSeek(const std::string & currentTime);
    void msgPlayerSetVolume(float volume);
    void msgPlayerSetMute(bool mute);

    void processMessage(const castchannel::CastMessage &msg);

    void notifySendRequest();
    std::atomic_bool requested_stop;
    std::atomic_bool requested_seek;

    void setPauseState(bool paused);

    void setTitle( const char *psz_title )
    {
        if ( psz_title )
            title = psz_title;
        else
            title = "";
    }

    void setArtwork( const char *psz_artwork )
    {
        if ( psz_artwork )
            artwork = psz_artwork;
        else
            artwork = "";
    }

    int sendMessage(const castchannel::CastMessage &msg);

    void buildMessage(const std::string & namespace_,
                      const std::string & payload,
                      const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER,
                      castchannel::CastMessage_PayloadType payloadType = castchannel::CastMessage_PayloadType_STRING);

    void pushMediaPlayerMessage(const std::stringstream & payload);

    void setPlayerStatus(enum command_status status) {
        if (cmd_status != status)
        {
            msg_Dbg(p_module, "change Chromecast command status from %d to %d", cmd_status, status);
            cmd_status = status;
        }
    }

    enum connection_status conn_status;
    enum command_status    cmd_status;

    unsigned i_receiver_requestId;
    unsigned i_requestId;

    bool           has_input;

    std::string GetMedia();
    std::string artwork;
    std::string title;

    static void* ChromecastThread(void* p_data);
    vlc_interrupt_t *p_ctl_thread_interrupt;

    mtime_t getPlaybackTimestamp() const
    {
        switch( receiverState )
        {
        case RECEIVER_PLAYING:
            return ( mdate() - m_time_playback_started ) + i_ts_local_start;

        case RECEIVER_IDLE:
            msg_Dbg(p_module, "receiver idle using buffering time %" PRId64, i_ts_local_start);
            break;
        case RECEIVER_BUFFERING:
            msg_Dbg(p_module, "receiver buffering using buffering time %" PRId64, i_ts_local_start);
            break;
        case RECEIVER_PAUSED:
            msg_Dbg(p_module, "receiver paused using buffering time %" PRId64, i_ts_local_start);
            break;
        }
        return i_ts_local_start;
    }

    double getPlaybackPosition( mtime_t i_length ) const
    {
        if( i_length > 0 && m_time_playback_started != VLC_TS_INVALID)
            return (double) getPlaybackTimestamp() / (double)( i_length );
        return 0.0;
    }

    /* local date when playback started/resumed, used by monotone clock */
    mtime_t           m_time_playback_started;
    /* local playback time of the input when playback started/resumed */
    mtime_t           i_ts_local_start;
    mtime_t           i_length;

    /* playback time reported by the receiver, used to wait for seeking point */
    mtime_t           m_chromecast_start_time;
    /* seek time with Chromecast relative timestamp */
    mtime_t           m_seek_request_time;

    vlc_cond_t   seekCommandCond;

    int recvPacket(bool &b_msgReceived, uint32_t &i_payloadSize,
                   unsigned *pi_received, uint8_t *p_data, bool *pb_pingTimeout,
                   int *pi_wait_delay, int *pi_wait_retries);

    /* shared structure with the demux-filter */
    chromecast_common      common;

    static void set_length(void*, mtime_t length);
    static mtime_t get_time(void*);
    static double get_position(void*);

    static void wait_app_started(void*);

    static void request_seek(void*, mtime_t pos);
    static void wait_seek_done(void*);

    static void set_pause_state(void*, bool paused);

    static void set_title(void*, const char *psz_title);
    static void set_artwork(void*, const char *psz_artwork);
};

#endif /* VLC_CHROMECAST_H */
