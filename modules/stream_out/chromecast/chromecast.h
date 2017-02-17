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
static const std::string NAMESPACE_DEVICEAUTH       = "urn:x-cast:com.google.cast.tp.deviceauth";
static const std::string NAMESPACE_CONNECTION       = "urn:x-cast:com.google.cast.tp.connection";
static const std::string NAMESPACE_HEARTBEAT        = "urn:x-cast:com.google.cast.tp.heartbeat";
static const std::string NAMESPACE_RECEIVER         = "urn:x-cast:com.google.cast.receiver";


#define CHROMECAST_CONTROL_PORT 8009
#define HTTP_PORT               8010

/* deadline regarding pings sent from receiver */
#define PING_WAIT_TIME 6000
#define PING_WAIT_RETRIES 0

#define PACKET_MAX_LEN 10 * 1024

// Media player Chromecast app id
#define APP_ID "CC1AD845" // Default media player aka DEFAULT_MEDIA_RECEIVER_APPLICATION_ID

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

class ChromecastCommunication
{
public:
    ChromecastCommunication( vlc_object_t* module, const char* targetIP, unsigned int devicePort );
    ~ChromecastCommunication();
    /**
     * @brief disconnect close the connection with the chromecast
     */
    void disconnect();

    void msgPing();
    void msgPong();
    void msgConnect( const std::string& destinationId );

    void msgReceiverLaunchApp();
    void msgReceiverGetStatus();
    void msgReceiverClose(const std::string& destinationId);
    void msgAuth();
    void msgPlayerLoad( const std::string& destinationId, unsigned int i_port, const std::string& title,
                        const std::string& artwork, const std::string& mime );
    void msgPlayerPlay( const std::string& destinationId, const std::string& mediaSessionId );
    void msgPlayerStop( const std::string& destinationId, const std::string& mediaSessionId );
    void msgPlayerPause( const std::string& destinationId, const std::string& mediaSessionId );
    void msgPlayerGetStatus( const std::string& destinationId );
    void msgPlayerSeek( const std::string& destinationId, const std::string& mediaSessionId,
                        const std::string & currentTime );
    void msgPlayerSetVolume( const std::string& destinationId, const std::string& mediaSessionId,
                             float volume, bool mute);
    int recvPacket( bool *b_msgReceived, uint32_t &i_payloadSize,
                   unsigned *pi_received, uint8_t *p_data, bool *pb_pingTimeout,
                   int *pi_wait_delay, int *pi_wait_retries );
private:
    int sendMessage(const castchannel::CastMessage &msg);

    void buildMessage(const std::string & namespace_,
                      const std::string & payload,
                      const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER,
                      castchannel::CastMessage_PayloadType payloadType = castchannel::CastMessage_PayloadType_STRING);
    void pushMediaPlayerMessage( const std::string& destinationId, const std::stringstream & payload );
    std::string GetMedia( unsigned int i_port, const std::string& title,
                          const std::string& artwork, const std::string& mime );

private:
    vlc_object_t* m_module;
    int m_sock_fd;
    vlc_tls_creds_t *m_creds;
    vlc_tls_t *m_tls;
    unsigned m_receiver_requestId;
    unsigned m_requestId;
    std::string m_serverIp;
};

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    intf_sys_t(vlc_object_t * const p_this, int local_port, std::string device_addr, int device_port, vlc_interrupt_t *);
    ~intf_sys_t();

    bool isFinishedPlaying();

    void setHasInput( bool has_input, const std::string mime_type = "");

    void requestPlayerSeek(mtime_t pos);
    void requestPlayerStop();

private:
    bool handleMessages();

    void setConnectionStatus(connection_status status);

    void waitAppStarted();
    void waitSeekDone();

    void processMessage(const castchannel::CastMessage &msg);

    void notifySendRequest();

    void setPauseState(bool paused);

    void setTitle( const char *psz_title );

    void setArtwork( const char *psz_artwork );

    void setPlayerStatus(enum command_status status);

    mtime_t getPlaybackTimestamp() const;

    double getPlaybackPosition() const;

    void mainLoop();
    void processAuthMessage( const castchannel::CastMessage& msg );
    void processHeartBeatMessage( const castchannel::CastMessage& msg );
    void processReceiverMessage( const castchannel::CastMessage& msg );
    void processMediaMessage( const castchannel::CastMessage& msg );
    void processConnectionMessage( const castchannel::CastMessage& msg );

private:
    static void* ChromecastThread(void* p_data);

    static void set_length(void*, mtime_t length);
    static mtime_t get_time(void*);
    static double get_position(void*);

    static void wait_app_started(void*);

    static void request_seek(void*, mtime_t pos);
    static void wait_seek_done(void*);

    static void set_pause_state(void*, bool paused);

    static void set_title(void*, const char *psz_title);
    static void set_artwork(void*, const char *psz_artwork);


private:
    vlc_object_t  * const m_module;
    const int      m_streaming_port;
    std::string    m_mime;

    std::string m_appTransportId;
    std::string m_mediaSessionId;
    receiver_state m_receiverState;

    vlc_mutex_t  m_lock;
    vlc_cond_t   m_loadCommandCond;
    vlc_thread_t m_chromecastThread;

    ChromecastCommunication m_communication;
    connection_status m_conn_status;
    command_status    m_cmd_status;
    std::atomic_bool m_requested_stop;
    std::atomic_bool m_requested_seek;

    bool           m_has_input;

    std::string m_artwork;
    std::string m_title;

    vlc_interrupt_t *m_ctl_thread_interrupt;

    /* local date when playback started/resumed, used by monotone clock */
    mtime_t           m_time_playback_started;
    /* local playback time of the input when playback started/resumed */
    mtime_t           m_ts_local_start;
    mtime_t           m_length;

    /* playback time reported by the receiver, used to wait for seeking point */
    mtime_t           m_chromecast_start_time;
    /* seek time with Chromecast relative timestamp */
    mtime_t           m_seek_request_time;

    vlc_cond_t   m_seekCommandCond;

    /* shared structure with the demux-filter */
    chromecast_common      m_common;
};

#endif /* VLC_CHROMECAST_H */
