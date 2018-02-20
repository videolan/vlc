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
#include <vlc_httpd.h>

#include <atomic>
#include <sstream>
#include <queue>

#ifndef PROTOBUF_INLINE_NOT_IN_HEADERS
# define PROTOBUF_INLINE_NOT_IN_HEADERS 0
#endif
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

#define PACKET_MAX_LEN 10 * 1024

//#define CHROMECAST_VERBOSE

// Media player Chromecast app id
#define APP_ID "CC1AD845" // Default media player aka DEFAULT_MEDIA_RECEIVER_APPLICATION_ID

enum States
{
    // An authentication request has been sent
    Authenticating,
    // We are sending a connection request
    Connecting,
    // We are connected to the chromecast but the receiver app is not running.
    Connected,
    // We are launching the media receiver app
    Launching,
    // The application is ready, but idle
    Ready,
    // The chromecast rejected the media
    LoadFailed,
    // A media session is being initiated
    Loading,
    Buffering,
    Playing,
    Paused,
    Stopping,
    Stopped,
    // Something went wrong and the connection is dead.
    Dead,
    // Another playback started on the same cast device
    TakenOver,
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

    static const unsigned kInvalidId = 0;

    /* All msg*() methods return kInvalidId on error, 1 or the receiver/player
     * request ID on success */

    unsigned msgPing();
    unsigned msgPong();
    unsigned msgConnect( const std::string& destinationId );

    unsigned msgReceiverLaunchApp();
    unsigned msgReceiverGetStatus();
    unsigned msgReceiverClose(const std::string& destinationId);
    unsigned msgAuth();
    unsigned msgPlayerLoad( const std::string& destinationId, unsigned int i_port,
                            const std::string& mime, const vlc_meta_t *p_meta );
    unsigned msgPlayerPlay( const std::string& destinationId, int64_t mediaSessionId );
    unsigned msgPlayerStop( const std::string& destinationId, int64_t mediaSessionId );
    unsigned msgPlayerPause( const std::string& destinationId, int64_t mediaSessionId );
    unsigned msgPlayerGetStatus( const std::string& destinationId );
    unsigned msgPlayerSeek( const std::string& destinationId, int64_t mediaSessionId,
                            const std::string & currentTime );
    unsigned msgPlayerSetVolume( const std::string& destinationId, int64_t mediaSessionId,
                                 float volume, bool mute);
    ssize_t receive( uint8_t *p_data, size_t i_size, int i_timeout, bool *pb_timeout );

    const std::string getServerIp()
    {
        return m_serverIp;
    }
private:
    int sendMessage(const castchannel::CastMessage &msg);

    int buildMessage(const std::string & namespace_,
                     const std::string & payload,
                     const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER,
                     castchannel::CastMessage_PayloadType payloadType = castchannel::CastMessage_PayloadType_STRING);
    int pushMediaPlayerMessage( const std::string& destinationId, const std::stringstream & payload );
    std::string GetMedia( unsigned int i_port, const std::string& mime,
                          const vlc_meta_t *p_meta );
    unsigned getNextReceiverRequestId();
    unsigned getNextRequestId();

private:
    vlc_object_t* m_module;
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
    enum QueueableMessages
    {
        Stop,
    };
    intf_sys_t(vlc_object_t * const p_this, int local_port, std::string device_addr,
               int device_port, httpd_host_t *);
    ~intf_sys_t();

    void setRetryOnFail(bool);
    void setHasInput(const std::string mime_type = "");

    void setOnInputEventCb(on_input_event_itf on_input_event, void *on_input_event_data);
    void setOnPausedChangedCb(on_paused_changed_itf on_paused_changed,
                              void *on_paused_changed_data);
    void requestPlayerStop();
    States state() const;

    void setPacing(bool do_pace);
    int pace();
    void sendInputEvent(enum cc_input_event event, union cc_input_arg arg);
    mtime_t getPauseDelay();

    int httpd_file_fill( uint8_t *psz_request, uint8_t **pp_data, int *pi_data );
    void interrupt_wake_up();
private:
    bool handleMessages();

    void processMessage(const castchannel::CastMessage &msg);
    void queueMessage( QueueableMessages msg );

    void setPauseState(bool paused, mtime_t delay);
    bool isFinishedPlaying();
    bool isStateError() const;
    bool isStatePlaying() const;
    bool isStateReady() const;
    void tryLoad();
    void doStop();

    void setMeta( vlc_meta_t *p_meta );

    mtime_t getPlaybackTimestamp();

    double getPlaybackPosition() const;

    void setInitialTime( mtime_t time );
    // Sets the current state and signal the associated wait cond.
    // This must be called with the lock held
    void setState( States state );

    void mainLoop();
    void processAuthMessage( const castchannel::CastMessage& msg );
    void processHeartBeatMessage( const castchannel::CastMessage& msg );
    void processReceiverMessage( const castchannel::CastMessage& msg );
    void processMediaMessage( const castchannel::CastMessage& msg );
    void processConnectionMessage( const castchannel::CastMessage& msg );

private:
    static void* ChromecastThread(void* p_data);

    static mtime_t get_time(void*);

    static int pace(void*);
    static void send_input_event(void *, enum cc_input_event event, union cc_input_arg arg);
    static void set_on_paused_changed_cb(void *, on_paused_changed_itf, void *);

    static void set_pause_state(void*, bool paused, mtime_t delay);

    static void set_meta(void*, vlc_meta_t *p_meta);

    void prepareHttpArtwork();

    static mtime_t      timeCCToVLC(double);
    static std::string  timeVLCToCC(mtime_t);

private:
    vlc_object_t  * const m_module;
    const int      m_streaming_port;
    std::string    m_mime;

    std::string m_appTransportId;
    unsigned m_last_request_id;
    int64_t m_mediaSessionId;

    mutable vlc_mutex_t  m_lock;
    vlc_cond_t   m_stateChangedCond;
    vlc_cond_t   m_pace_cond;
    vlc_thread_t m_chromecastThread;


    on_input_event_itf    m_on_input_event;
    void                 *m_on_input_event_data;

    on_paused_changed_itf m_on_paused_changed;
    void                 *m_on_paused_changed_data;

    ChromecastCommunication m_communication;
    std::queue<QueueableMessages> m_msgQueue;
    States m_state;
    bool m_retry_on_fail;
    bool m_played_once;
    bool m_request_stop;
    bool m_request_load;
    bool m_paused;
    bool m_input_eof;
    bool m_cc_eof;
    bool m_pace;
    bool m_interrupted;

    vlc_meta_t *m_meta;

    vlc_interrupt_t *m_ctl_thread_interrupt;

    httpd_host_t     *m_httpd_host;
    httpd_file_t     *m_httpd_file;
    std::string       m_art_http_ip;
    char             *m_art_url;
    unsigned          m_art_idx;

    mtime_t           m_cc_time_last_request_date;
    mtime_t           m_cc_time_date;
    mtime_t           m_cc_time;
    mtime_t           m_pause_delay;

    /* shared structure with the demux-filter */
    chromecast_common      m_common;

    /* Heartbeat */
    uint8_t m_pingRetriesLeft;
};

#endif /* VLC_CHROMECAST_H */
