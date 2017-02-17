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

#include <cassert>
#include <cerrno>

#include "../../misc/webservices/json.h"

/* deadline regarding pings sent from receiver */
#define PING_WAIT_TIME 6000
#define PING_WAIT_RETRIES 0

static const mtime_t SEEK_FORWARD_OFFSET = 1000000;

/*****************************************************************************
 * intf_sys_t: class definition
 *****************************************************************************/
intf_sys_t::intf_sys_t(vlc_object_t * const p_this, int port, std::string device_addr, int device_port, vlc_interrupt_t *p_interrupt)
 : m_module(p_this)
 , m_streaming_port(port)
 , m_receiverState(RECEIVER_IDLE)
 , m_communication( p_this, device_addr.c_str(), device_port )
 , m_requested_stop(false)
 , m_requested_seek(false)
 , m_conn_status(CHROMECAST_DISCONNECTED)
 , m_cmd_status(NO_CMD_PENDING)
 , m_has_input(false)
 , m_ctl_thread_interrupt(p_interrupt)
 , m_time_playback_started( VLC_TS_INVALID )
 , m_ts_local_start( VLC_TS_INVALID )
 , m_length( VLC_TS_INVALID )
 , m_chromecast_start_time( VLC_TS_INVALID )
 , m_seek_request_time( VLC_TS_INVALID )
{
    vlc_mutex_init(&m_lock);
    vlc_cond_init(&m_loadCommandCond);
    vlc_cond_init(&m_seekCommandCond);

    m_common.p_opaque = this;
    m_common.pf_get_position     = get_position;
    m_common.pf_get_time         = get_time;
    m_common.pf_set_length       = set_length;
    m_common.pf_wait_app_started = wait_app_started;
    m_common.pf_request_seek     = request_seek;
    m_common.pf_wait_seek_done   = wait_seek_done;
    m_common.pf_set_pause_state  = set_pause_state;
    m_common.pf_set_artwork      = set_artwork;
    m_common.pf_set_title        = set_title;

    assert( var_Type( m_module->obj.parent->obj.parent, CC_SHARED_VAR_NAME) == 0 );
    if (var_Create( m_module->obj.parent->obj.parent, CC_SHARED_VAR_NAME, VLC_VAR_ADDRESS ) == VLC_SUCCESS )
        var_SetAddress( m_module->obj.parent->obj.parent, CC_SHARED_VAR_NAME, &m_common );

    // Start the Chromecast event thread.
    if (vlc_clone(&m_chromecastThread, ChromecastThread, this,
                  VLC_THREAD_PRIORITY_LOW))
    {
        msg_Err( m_module, "Could not start the Chromecast talking thread");
    }
}

intf_sys_t::~intf_sys_t()
{
    setHasInput( false );

    var_Destroy( m_module->obj.parent->obj.parent, CC_SHARED_VAR_NAME );

    switch ( m_conn_status )
    {
    case CHROMECAST_APP_STARTED:
        // Generate the close messages.
        m_communication.msgReceiverClose(m_appTransportId);
        // ft
    case CHROMECAST_TLS_CONNECTED:
    case CHROMECAST_AUTHENTICATED:
        m_communication.msgReceiverClose(DEFAULT_CHOMECAST_RECEIVER);
        // ft
    case CHROMECAST_DISCONNECTED:
    case CHROMECAST_CONNECTION_DEAD:
    default:
        break;
    }

    vlc_interrupt_kill( m_ctl_thread_interrupt );

    vlc_join(m_chromecastThread, NULL);

    vlc_interrupt_destroy( m_ctl_thread_interrupt );

    // make sure we unblock the demuxer
    m_seek_request_time = VLC_TS_INVALID;
    vlc_cond_signal(&m_seekCommandCond);

    vlc_cond_destroy(&m_seekCommandCond);
    vlc_cond_destroy(&m_loadCommandCond);
    vlc_mutex_destroy(&m_lock);
}

void intf_sys_t::setHasInput( bool b_has_input, const std::string mime_type )
{
    vlc_mutex_locker locker(&m_lock);
    msg_Dbg( m_module, "setHasInput %s session:%s",b_has_input ? "true":"false", m_mediaSessionId.c_str() );

    this->m_has_input = b_has_input;
    this->m_mime = mime_type;

    if( this->m_has_input )
    {
        mutex_cleanup_push(&m_lock);
        while (m_conn_status != CHROMECAST_APP_STARTED && m_conn_status != CHROMECAST_CONNECTION_DEAD)
        {
            msg_Dbg( m_module, "setHasInput waiting for Chromecast connection, current %d", m_conn_status);
            vlc_cond_wait(&m_loadCommandCond, &m_lock);
        }
        vlc_cleanup_pop();

        if (m_conn_status == CHROMECAST_CONNECTION_DEAD)
        {
            msg_Warn( m_module, "no Chromecast hook possible");
            return;
        }

        if ( m_receiverState == RECEIVER_IDLE )
        {
            // we cannot start a new load when the last one is still processing
            m_ts_local_start = VLC_TS_0;
            m_communication.msgPlayerLoad( m_appTransportId, m_streaming_port, m_title, m_artwork, mime_type );
            setPlayerStatus(CMD_LOAD_SENT);
        }
    }
}

/**
 * @brief Process a message received from the Chromecast
 * @param msg the CastMessage to process
 * @return 0 if the message has been successfuly processed else -1
 */
void intf_sys_t::processMessage(const castchannel::CastMessage &msg)
{
    const std::string & namespace_ = msg.namespace_();

#ifndef NDEBUG
    msg_Dbg( m_module, "processMessage: %s->%s %s", namespace_.c_str(), msg.destination_id().c_str(), msg.payload_utf8().c_str());
#endif

    if (namespace_ == NAMESPACE_DEVICEAUTH)
        processAuthMessage( msg );
    else if (namespace_ == NAMESPACE_HEARTBEAT)
        processHeartBeatMessage( msg );
    else if (namespace_ == NAMESPACE_RECEIVER)
        processReceiverMessage( msg );
    else if (namespace_ == NAMESPACE_MEDIA)
        processMediaMessage( msg );
    else if (namespace_ == NAMESPACE_CONNECTION)
        processConnectionMessage( msg );
    else
    {
        msg_Err( m_module, "Unknown namespace: %s", msg.namespace_().c_str());
    }
}



/*****************************************************************************
 * Chromecast thread
 *****************************************************************************/
void* intf_sys_t::ChromecastThread(void* p_data)
{
    intf_sys_t *p_sys = reinterpret_cast<intf_sys_t*>(p_data);
    p_sys->mainLoop();
    return NULL;
}

void intf_sys_t::mainLoop()
{
    vlc_interrupt_set( m_ctl_thread_interrupt );

    vlc_mutex_lock(&m_lock);
    setConnectionStatus(CHROMECAST_TLS_CONNECTED);
    vlc_mutex_unlock(&m_lock);

    m_communication.msgAuth();

    while ( !vlc_killed() && handleMessages() )
        ;
}

void intf_sys_t::processAuthMessage( const castchannel::CastMessage& msg )
{
    castchannel::DeviceAuthMessage authMessage;
    if ( authMessage.ParseFromString(msg.payload_binary()) == false )
    {
        msg_Warn( m_module, "Failed to parse the payload" );
        return;
    }

    if (authMessage.has_error())
    {
        msg_Err( m_module, "Authentification error: %d", authMessage.error().error_type());
    }
    else if (!authMessage.has_response())
    {
        msg_Err( m_module, "Authentification message has no response field");
    }
    else
    {
        vlc_mutex_locker locker(&m_lock);
        setConnectionStatus(CHROMECAST_AUTHENTICATED);
        m_communication.msgConnect(DEFAULT_CHOMECAST_RECEIVER);
        m_communication.msgReceiverGetStatus();
    }
}

void intf_sys_t::processHeartBeatMessage( const castchannel::CastMessage& msg )
{
    json_value *p_data = json_parse(msg.payload_utf8().c_str());
    std::string type((*p_data)["type"]);

    if (type == "PING")
    {
        msg_Dbg( m_module, "PING received from the Chromecast");
        m_communication.msgPong();
    }
    else if (type == "PONG")
    {
        msg_Dbg( m_module, "PONG received from the Chromecast");
    }
    else
    {
        msg_Warn( m_module, "Heartbeat command not supported: %s", type.c_str());
    }

    json_value_free(p_data);
}

void intf_sys_t::processReceiverMessage( const castchannel::CastMessage& msg )
{
    json_value *p_data = json_parse(msg.payload_utf8().c_str());
    std::string type((*p_data)["type"]);

    if (type == "RECEIVER_STATUS")
    {
        json_value applications = (*p_data)["status"]["applications"];
        const json_value *p_app = NULL;

        vlc_mutex_locker locker(&m_lock);
        for (unsigned i = 0; i < applications.u.array.length; ++i)
        {
            std::string appId(applications[i]["appId"]);
            if (appId == APP_ID)
            {
                const char *pz_transportId = applications[i]["transportId"];
                if (pz_transportId != NULL)
                {
                    m_appTransportId = std::string(pz_transportId);
                    p_app = &applications[i];
                }
                break;
            }
        }

        if ( p_app )
        {
            if (!m_appTransportId.empty()
                    && m_conn_status == CHROMECAST_AUTHENTICATED)
            {
                m_communication.msgConnect( m_appTransportId );
                setPlayerStatus(NO_CMD_PENDING);
                setConnectionStatus(CHROMECAST_APP_STARTED);
            }
        }
        else
        {
            switch( m_conn_status )
            {
            /* If the app is no longer present */
            case CHROMECAST_APP_STARTED:
                msg_Warn( m_module, "app is no longer present. closing");
                m_communication.msgReceiverClose(m_appTransportId);
                setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
                break;

            case CHROMECAST_AUTHENTICATED:
                msg_Dbg( m_module, "Chromecast was running no app, launch media_app");
                m_appTransportId = "";
                m_mediaSessionId = ""; // this session is not valid anymore
                m_receiverState = RECEIVER_IDLE;
                m_communication.msgReceiverLaunchApp();
                break;

            default:
                break;
            }

        }
    }
    else if (type == "LAUNCH_ERROR")
    {
        json_value reason = (*p_data)["reason"];
        msg_Err( m_module, "Failed to start the MediaPlayer: %s",
                (const char *)reason);
    }
    else
    {
        msg_Warn( m_module, "Receiver command not supported: %s",
                msg.payload_utf8().c_str());
    }

    json_value_free(p_data);
}

void intf_sys_t::processMediaMessage( const castchannel::CastMessage& msg )
{
    json_value *p_data = json_parse(msg.payload_utf8().c_str());
    std::string type((*p_data)["type"]);

    if (type == "MEDIA_STATUS")
    {
        json_value status = (*p_data)["status"];
        msg_Dbg( m_module, "Player state: %s sessionId:%d",
                status[0]["playerState"].operator const char *(),
                (int)(json_int_t) status[0]["mediaSessionId"]);

        vlc_mutex_locker locker(&m_lock);
        receiver_state oldPlayerState = m_receiverState;
        std::string newPlayerState = status[0]["playerState"].operator const char *();
        std::string idleReason = status[0]["idleReason"].operator const char *();

        if (newPlayerState == "IDLE")
            m_receiverState = RECEIVER_IDLE;
        else if (newPlayerState == "PLAYING")
            m_receiverState = RECEIVER_PLAYING;
        else if (newPlayerState == "BUFFERING")
            m_receiverState = RECEIVER_BUFFERING;
        else if (newPlayerState == "PAUSED")
            m_receiverState = RECEIVER_PAUSED;
        else if (!newPlayerState.empty())
            msg_Warn( m_module, "Unknown Chromecast state %s", newPlayerState.c_str());

        if (m_receiverState == RECEIVER_IDLE)
        {
            m_mediaSessionId = ""; // this session is not valid anymore
        }
        else
        {
            char session_id[32];
            if( snprintf( session_id, sizeof(session_id), "%" PRId64, (json_int_t) status[0]["mediaSessionId"] ) >= (int)sizeof(session_id) )
            {
                msg_Err( m_module, "snprintf() truncated string for mediaSessionId" );
                session_id[sizeof(session_id) - 1] = '\0';
            }
            if (session_id[0] && m_mediaSessionId != session_id) {
                if (!m_mediaSessionId.empty())
                    msg_Warn( m_module, "different mediaSessionId detected %s was %s", session_id, this->m_mediaSessionId.c_str());
                m_mediaSessionId = session_id;
            }
        }

        if (m_receiverState != oldPlayerState)
        {
#ifndef NDEBUG
            msg_Dbg( m_module, "change Chromecast player state from %d to %d", oldPlayerState, m_receiverState );
#endif
            switch( m_receiverState )
            {
            case RECEIVER_BUFFERING:
                if ( double(status[0]["currentTime"]) == 0.0 )
                {
                    m_receiverState = oldPlayerState;
                    msg_Dbg( m_module, "Invalid buffering time, keep previous state %d", oldPlayerState);
                }
                else
                {
                    m_chromecast_start_time = (1 + mtime_t( double( status[0]["currentTime"] ) ) ) * 1000000L;
                    msg_Dbg( m_module, "Playback pending with an offset of %" PRId64, m_chromecast_start_time);
                }
                m_time_playback_started = VLC_TS_INVALID;
                break;

            case RECEIVER_PLAYING:
                /* TODO reset demux PCR ? */
                if (unlikely(m_chromecast_start_time == VLC_TS_INVALID)) {
                    msg_Warn( m_module, "start playing without buffering" );
                    m_chromecast_start_time = (1 + mtime_t( double( status[0]["currentTime"] ) ) ) * 1000000L;
                }
                setPlayerStatus(CMD_PLAYBACK_SENT);
                m_time_playback_started = mdate();
#ifndef NDEBUG
                msg_Dbg( m_module, "Playback started with an offset of %" PRId64 " now:%" PRId64 " i_ts_local_start:%" PRId64, m_chromecast_start_time, m_time_playback_started, m_ts_local_start);
#endif
                break;

            case RECEIVER_PAUSED:
                m_chromecast_start_time = (1 + mtime_t( double( status[0]["currentTime"] ) ) ) * 1000000L;
#ifndef NDEBUG
                msg_Dbg( m_module, "Playback paused with an offset of %" PRId64 " date_play_start:%" PRId64, m_chromecast_start_time, m_time_playback_started);
#endif

                if ( m_time_playback_started != VLC_TS_INVALID && oldPlayerState == RECEIVER_PLAYING )
                {
                    /* this is a pause generated remotely, adjust the playback time */
                    m_ts_local_start += mdate() - m_time_playback_started;
#ifndef NDEBUG
                    msg_Dbg( m_module, "updated i_ts_local_start:%" PRId64, m_ts_local_start);
#endif
                }
                m_time_playback_started = VLC_TS_INVALID;
                break;

            case RECEIVER_IDLE:
                if ( m_has_input )
                    setPlayerStatus(NO_CMD_PENDING);
                m_time_playback_started = VLC_TS_INVALID;
                break;
            }
        }

        if (m_receiverState == RECEIVER_BUFFERING && m_seek_request_time != VLC_TS_INVALID)
        {
            msg_Dbg( m_module, "Chromecast seeking possibly done");
            vlc_cond_signal( &m_seekCommandCond );
        }

        if ( (m_cmd_status != CMD_LOAD_SENT || idleReason == "CANCELLED") && m_receiverState == RECEIVER_IDLE && m_has_input )
        {
            msg_Dbg( m_module, "the device missed the LOAD command");
            m_ts_local_start = VLC_TS_0;
            m_communication.msgPlayerLoad( m_appTransportId, m_streaming_port, m_title, m_artwork, m_mime );
            setPlayerStatus(CMD_LOAD_SENT);
        }
    }
    else if (type == "LOAD_FAILED")
    {
        msg_Err( m_module, "Media load failed");
        vlc_mutex_locker locker(&m_lock);
        /* close the app to restart it */
        if ( m_conn_status == CHROMECAST_APP_STARTED )
            m_communication.msgReceiverClose(m_appTransportId);
        else
            m_communication.msgReceiverGetStatus();
    }
    else if (type == "LOAD_CANCELLED")
    {
        msg_Dbg( m_module, "LOAD canceled by another command");
    }
    else if (type == "INVALID_REQUEST")
    {
        msg_Dbg( m_module, "We sent an invalid request reason:%s", (*p_data)["reason"].operator const char *());
    }
    else
    {
        msg_Warn( m_module, "Media command not supported: %s",
                msg.payload_utf8().c_str());
    }

    json_value_free(p_data);
}

void intf_sys_t::processConnectionMessage( const castchannel::CastMessage& msg )
{
    json_value *p_data = json_parse(msg.payload_utf8().c_str());
    std::string type((*p_data)["type"]);
    json_value_free(p_data);

    if (type == "CLOSE")
    {
        msg_Warn( m_module, "received close message");
        setHasInput( false );
        vlc_mutex_locker locker(&m_lock);
        setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
        // make sure we unblock the demuxer
        m_seek_request_time = VLC_TS_INVALID;
        vlc_cond_signal(&m_seekCommandCond);
    }
    else
    {
        msg_Warn( m_module, "Connection command not supported: %s",
                type.c_str());
    }
}

bool intf_sys_t::handleMessages()
{
    unsigned i_received = 0;
    uint8_t p_packet[PACKET_MAX_LEN];
    bool b_pingTimeout = false;

    int i_waitdelay = PING_WAIT_TIME;
    int i_retries = PING_WAIT_RETRIES;

    bool b_msgReceived = false;
    uint32_t i_payloadSize = 0;

    if ( m_requested_stop.exchange(false) && !m_mediaSessionId.empty() )
    {
        m_communication.msgPlayerStop( m_appTransportId, m_mediaSessionId );
    }

    if ( m_requested_seek.exchange(false) && !m_mediaSessionId.empty() )
    {
        char current_time[32];
        m_seek_request_time = mdate() + SEEK_FORWARD_OFFSET;
        if( snprintf( current_time, sizeof(current_time), "%.3f", double( m_seek_request_time ) / 1000000.0 ) >= (int)sizeof(current_time) )
        {
            msg_Err( m_module, "snprintf() truncated string for mediaSessionId" );
            current_time[sizeof(current_time) - 1] = '\0';
        }
        vlc_mutex_locker locker(&m_lock);
        setPlayerStatus(CMD_SEEK_SENT);
        /* send a fake time to seek to, to make sure the device flushes its buffers */
        m_communication.msgPlayerSeek( m_appTransportId, m_mediaSessionId, current_time );
    }

    int i_ret = m_communication.recvPacket( &b_msgReceived, i_payloadSize,
                            &i_received, p_packet, &b_pingTimeout,
                            &i_waitdelay, &i_retries);


#if defined(_WIN32)
    if ((i_ret < 0 && WSAGetLastError() != WSAEWOULDBLOCK) || (i_ret == 0))
#else
    if ((i_ret < 0 && errno != EAGAIN) || i_ret == 0)
#endif
    {
        msg_Err( m_module, "The connection to the Chromecast died (receiving).");
        vlc_mutex_locker locker(&m_lock);
        setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
    }

    if (b_pingTimeout)
    {
        m_communication.msgPing();
        m_communication.msgReceiverGetStatus();
    }

    if (b_msgReceived)
    {
        castchannel::CastMessage msg;
        msg.ParseFromArray(p_packet + PACKET_HEADER_LEN, i_payloadSize);
        processMessage(msg);
    }

    vlc_mutex_locker locker(&m_lock);
    return m_conn_status != CHROMECAST_CONNECTION_DEAD;
}

void intf_sys_t::notifySendRequest()
{
    vlc_interrupt_raise( m_ctl_thread_interrupt );
}

void intf_sys_t::requestPlayerStop()
{
    m_requested_stop = true;
    setHasInput(false);
    notifySendRequest();
}

void intf_sys_t::requestPlayerSeek(mtime_t pos)
{
    vlc_mutex_locker locker(&m_lock);
    if ( pos != VLC_TS_INVALID )
        m_ts_local_start = pos;
    m_requested_seek = true;
    notifySendRequest();
}

void intf_sys_t::setPauseState(bool paused)
{
    msg_Dbg( m_module, "%s state for %s", paused ? "paused" : "playing", m_title.c_str() );
    if ( !paused )
    {
        if ( !m_mediaSessionId.empty() && m_receiverState != RECEIVER_IDLE )
        {
            m_communication.msgPlayerPlay( m_appTransportId, m_mediaSessionId );
            setPlayerStatus(CMD_PLAYBACK_SENT);
        }
    }
    else
    {
        if ( !m_mediaSessionId.empty() && m_receiverState != RECEIVER_IDLE )
        {
            m_communication.msgPlayerPause( m_appTransportId, m_mediaSessionId );
            setPlayerStatus(CMD_PLAYBACK_SENT);
        }
    }
}

void intf_sys_t::waitAppStarted()
{
    vlc_mutex_locker locker(&m_lock);
    mutex_cleanup_push(&m_lock);
    while ( m_conn_status != CHROMECAST_APP_STARTED &&
            m_conn_status != CHROMECAST_CONNECTION_DEAD )
        vlc_cond_wait(&m_loadCommandCond, &m_lock);
    vlc_cleanup_pop();
}

void intf_sys_t::waitSeekDone()
{
    vlc_mutex_locker locker(&m_lock);
    if ( m_seek_request_time != VLC_TS_INVALID )
    {
        mutex_cleanup_push(&m_lock);
        while ( m_chromecast_start_time < m_seek_request_time &&
                m_conn_status == CHROMECAST_APP_STARTED )
        {
#ifndef NDEBUG
            msg_Dbg( m_module, "waiting for Chromecast seek" );
#endif
            vlc_cond_wait(&m_seekCommandCond, &m_lock);
#ifndef NDEBUG
            msg_Dbg( m_module, "finished waiting for Chromecast seek" );
#endif
        }
        vlc_cleanup_pop();
        m_seek_request_time = VLC_TS_INVALID;
    }
}

void intf_sys_t::setConnectionStatus(connection_status status)
{
    if (m_conn_status != status)
    {
#ifndef NDEBUG
        msg_Dbg(m_module, "change Chromecast connection status from %d to %d", m_conn_status, status);
#endif
        m_conn_status = status;
        vlc_cond_broadcast(&m_loadCommandCond);
        vlc_cond_signal(&m_seekCommandCond);
    }
}

bool intf_sys_t::isFinishedPlaying()
{
    vlc_mutex_locker locker(&m_lock);
    return m_conn_status == CHROMECAST_CONNECTION_DEAD || (m_receiverState == RECEIVER_BUFFERING && m_cmd_status != CMD_SEEK_SENT);
}

void intf_sys_t::setTitle(const char* psz_title)
{
    if ( psz_title )
        m_title = psz_title;
    else
        m_title = "";
}

void intf_sys_t::setArtwork(const char* psz_artwork)
{
    if ( psz_artwork )
        m_artwork = psz_artwork;
    else
        m_artwork = "";
}

void intf_sys_t::setPlayerStatus(command_status status)
{
    if (m_cmd_status != status)
    {
        msg_Dbg(m_module, "change Chromecast command status from %d to %d", m_cmd_status, status);
        m_cmd_status = status;
    }
}

mtime_t intf_sys_t::getPlaybackTimestamp() const
{
    switch( m_receiverState )
    {
    case RECEIVER_PLAYING:
        return ( mdate() - m_time_playback_started ) + m_ts_local_start;

    case RECEIVER_IDLE:
        msg_Dbg(m_module, "receiver idle using buffering time %" PRId64, m_ts_local_start);
        break;
    case RECEIVER_BUFFERING:
        msg_Dbg(m_module, "receiver buffering using buffering time %" PRId64, m_ts_local_start);
        break;
    case RECEIVER_PAUSED:
        msg_Dbg(m_module, "receiver paused using buffering time %" PRId64, m_ts_local_start);
        break;
    }
    return m_ts_local_start;
}

double intf_sys_t::getPlaybackPosition() const
{
    if( m_length > 0 && m_time_playback_started != VLC_TS_INVALID)
        return (double) getPlaybackTimestamp() / (double)( m_length );
    return 0.0;
}

mtime_t intf_sys_t::get_time(void *pt)
{
    intf_sys_t *p_this = reinterpret_cast<intf_sys_t*>(pt);
    vlc_mutex_locker locker( &p_this->m_lock );
    return p_this->getPlaybackTimestamp();
}

double intf_sys_t::get_position(void *pt)
{
    intf_sys_t *p_this = reinterpret_cast<intf_sys_t*>(pt);
    vlc_mutex_locker locker( &p_this->m_lock );
    return p_this->getPlaybackPosition();
}

void intf_sys_t::set_length(void *pt, mtime_t length)
{
    intf_sys_t *p_this = reinterpret_cast<intf_sys_t*>(pt);
    p_this->m_length = length;
}

void intf_sys_t::wait_app_started(void *pt)
{
    intf_sys_t *p_this = reinterpret_cast<intf_sys_t*>(pt);
    p_this->waitAppStarted();
}

void intf_sys_t::request_seek(void *pt, mtime_t pos)
{
    intf_sys_t *p_this = reinterpret_cast<intf_sys_t*>(pt);
    p_this->requestPlayerSeek(pos);
}

void intf_sys_t::wait_seek_done(void *pt)
{
    intf_sys_t *p_this = reinterpret_cast<intf_sys_t*>(pt);
    p_this->waitSeekDone();
}

void intf_sys_t::set_pause_state(void *pt, bool paused)
{
    intf_sys_t *p_this = reinterpret_cast<intf_sys_t*>(pt);
    p_this->setPauseState( paused );
}

void intf_sys_t::set_title(void *pt, const char *psz_title)
{
    intf_sys_t *p_this = reinterpret_cast<intf_sys_t*>(pt);
    p_this->setTitle( psz_title );
}

void intf_sys_t::set_artwork(void *pt, const char *psz_artwork)
{
    intf_sys_t *p_this = reinterpret_cast<intf_sys_t*>(pt);
    p_this->setArtwork( psz_artwork );
}
