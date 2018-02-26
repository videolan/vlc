/*****************************************************************************
 * chromecast_ctrl.cpp: Chromecast module for vlc
 *****************************************************************************
 * Copyright © 2014-2015 VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chromecast.h"

#include <cassert>
#include <cerrno>
#include <iomanip>

#include <vlc_stream.h>

#include "../../misc/webservices/json.h"

/* deadline regarding pings sent from receiver */
#define PING_WAIT_TIME 6000
#define PING_WAIT_RETRIES 1

static int httpd_file_fill_cb( httpd_file_sys_t *data, httpd_file_t *http_file,
                          uint8_t *psz_request, uint8_t **pp_data, int *pi_data );

static const char* StateToStr( States s )
{
    switch (s )
    {
    case Authenticating:
        return "Authenticating";
    case Connecting:
        return "Connecting";
    case Connected:
        return "Connected";
    case Launching:
        return "Lauching";
    case Ready:
        return "Ready";
    case LoadFailed:
        return "LoadFailed";
    case Loading:
        return "Loading";
    case Buffering:
        return "Buffering";
    case Playing:
        return "Playing";
    case Paused:
        return "Paused";
    case Stopping:
        return "Stopping";
    case Stopped:
        return "Stopped";
    case Dead:
        return "Dead";
    case TakenOver:
        return "TakenOver";
    }
    vlc_assert_unreachable();
}

/*****************************************************************************
 * intf_sys_t: class definition
 *****************************************************************************/
intf_sys_t::intf_sys_t(vlc_object_t * const p_this, int port, std::string device_addr,
                       int device_port, httpd_host_t *httpd_host)
 : m_module(p_this)
 , m_streaming_port(port)
 , m_last_request_id( 0 )
 , m_mediaSessionId( 0 )
 , m_on_input_event( NULL )
 , m_on_input_event_data( NULL )
 , m_on_paused_changed( NULL )
 , m_on_paused_changed_data( NULL )
 , m_communication( p_this, device_addr.c_str(), device_port )
 , m_state( Authenticating )
 , m_retry_on_fail( false )
 , m_played_once( false )
 , m_request_stop( false )
 , m_request_load( false )
 , m_paused( false )
 , m_input_eof( false )
 , m_cc_eof( false )
 , m_pace( false )
 , m_meta( NULL )
 , m_httpd_host(httpd_host)
 , m_httpd_file(NULL)
 , m_art_url(NULL)
 , m_art_idx(0)
 , m_cc_time_date( VLC_TS_INVALID )
 , m_cc_time( VLC_TS_INVALID )
 , m_pause_delay( VLC_TS_INVALID )
 , m_pingRetriesLeft( PING_WAIT_RETRIES )
{
    m_ctl_thread_interrupt = vlc_interrupt_create();
    if( unlikely(m_ctl_thread_interrupt == NULL) )
        throw std::runtime_error( "error creating interrupt context" );

    vlc_mutex_init(&m_lock);
    vlc_cond_init( &m_stateChangedCond );
    vlc_cond_init( &m_pace_cond );

    std::stringstream ss;
    ss << "http://" << m_communication.getServerIp() << ":" << port;
    m_art_http_ip = ss.str();

    m_common.p_opaque = this;
    m_common.pf_set_on_paused_changed_cb = set_on_paused_changed_cb;
    m_common.pf_get_time         = get_time;
    m_common.pf_pace             = pace;
    m_common.pf_send_input_event = send_input_event;
    m_common.pf_set_pause_state  = set_pause_state;
    m_common.pf_set_meta         = set_meta;

    assert( var_Type( m_module->obj.parent->obj.parent, CC_SHARED_VAR_NAME) == 0 );
    if (var_Create( m_module->obj.parent->obj.parent, CC_SHARED_VAR_NAME, VLC_VAR_ADDRESS ) == VLC_SUCCESS )
        var_SetAddress( m_module->obj.parent->obj.parent, CC_SHARED_VAR_NAME, &m_common );

    // Start the Chromecast event thread.
    if (vlc_clone(&m_chromecastThread, ChromecastThread, this,
                  VLC_THREAD_PRIORITY_LOW))
    {
        vlc_interrupt_destroy( m_ctl_thread_interrupt );
        vlc_cond_destroy( &m_stateChangedCond );
        vlc_cond_destroy( &m_pace_cond );
        var_SetAddress( m_module->obj.parent->obj.parent, CC_SHARED_VAR_NAME, NULL );
        throw std::runtime_error( "error creating cc thread" );
    }
}

intf_sys_t::~intf_sys_t()
{
    var_Destroy( m_module->obj.parent->obj.parent, CC_SHARED_VAR_NAME );

    vlc_mutex_lock(&m_lock);
    switch ( m_state )
    {
    case Ready:
    case Loading:
    case Buffering:
    case Playing:
    case Paused:
    case Stopping:
    case Stopped:
        // Generate the close messages.
        m_communication.msgReceiverClose( m_appTransportId );
        /* fallthrough */
    case Connecting:
    case Connected:
    case Launching:
        m_communication.msgReceiverClose(DEFAULT_CHOMECAST_RECEIVER);
        /* fallthrough */
    default:
        break;
    }
    vlc_mutex_unlock(&m_lock);

    vlc_interrupt_kill( m_ctl_thread_interrupt );

    vlc_join(m_chromecastThread, NULL);

    vlc_interrupt_destroy( m_ctl_thread_interrupt );

    if (m_meta != NULL)
        vlc_meta_Delete(m_meta);

    if( m_httpd_file )
        httpd_FileDelete( m_httpd_file );

    free( m_art_url );

    vlc_cond_destroy(&m_stateChangedCond);
    vlc_cond_destroy(&m_pace_cond);
    vlc_mutex_destroy(&m_lock);
}

int intf_sys_t::httpd_file_fill( uint8_t *psz_request, uint8_t **pp_data, int *pi_data )
{
    (void) psz_request;

    vlc_mutex_lock( &m_lock );
    if( !m_art_url )
    {
        vlc_mutex_unlock( &m_lock );
        return VLC_EGENERIC;
    }
    char *psz_art = strdup( m_art_url );
    vlc_mutex_unlock( &m_lock );

    stream_t *s = vlc_stream_NewURL( m_module, psz_art );
    free( psz_art );
    if( !s )
        return VLC_EGENERIC;

    uint64_t size;
    if( vlc_stream_GetSize( s, &size ) != VLC_SUCCESS
     || size > INT64_C( 10000000 ) )
    {
        msg_Warn( m_module, "art stream is too big or invalid" );
        vlc_stream_Delete( s );
        return VLC_EGENERIC;
    }

    *pp_data = (uint8_t *)malloc( size );
    if( !*pp_data )
    {
        vlc_stream_Delete( s );
        return VLC_EGENERIC;
    }

    ssize_t read = vlc_stream_Read( s, *pp_data, size );
    vlc_stream_Delete( s );

    if( read < 0 || (size_t)read != size )
    {
        free( *pp_data );
        *pp_data = NULL;
        return VLC_EGENERIC;
    }
    *pi_data = size;

    return VLC_SUCCESS;
}

static int httpd_file_fill_cb( httpd_file_sys_t *data, httpd_file_t *http_file,
                          uint8_t *psz_request, uint8_t **pp_data, int *pi_data )
{
    (void) http_file;
    intf_sys_t *p_sys = static_cast<intf_sys_t*>((void *)data);
    return p_sys->httpd_file_fill( psz_request, pp_data, pi_data );
}

void intf_sys_t::prepareHttpArtwork()
{
    const char *psz_art = m_meta ? vlc_meta_Get( m_meta, vlc_meta_ArtworkURL ) : NULL;
    /* Abort if there is no art or if the art is already served */
    if( !psz_art || strncmp( psz_art, "http", 4) == 0 )
        return;

    std::stringstream ss_art_idx;

    if( m_art_url && strcmp( m_art_url, psz_art ) == 0 )
    {
        /* Same art: use the previous cached artwork url */
        assert( m_art_idx != 0 );
        ss_art_idx << "/art" << (m_art_idx - 1);
    }
    else
    {
        /* New art: create a new httpd file instance with a new url. The
         * artwork has to be different since the CC will cache the content. */

        ss_art_idx << "/art" << m_art_idx;
        m_art_idx++;

        vlc_mutex_unlock( &m_lock );

        if( m_httpd_file )
            httpd_FileDelete( m_httpd_file );
        m_httpd_file = httpd_FileNew( m_httpd_host, ss_art_idx.str().c_str(),
                                      "application/octet-stream", NULL, NULL,
                                      httpd_file_fill_cb, (httpd_file_sys_t *) this );

        vlc_mutex_lock( &m_lock );
        if( !m_httpd_file )
            return;

        free( m_art_url );
        m_art_url = strdup( psz_art );
    }

    std::stringstream ss;
    ss << m_art_http_ip << ss_art_idx.str();
    vlc_meta_Set( m_meta, vlc_meta_ArtworkURL, ss.str().c_str() );
}

void intf_sys_t::tryLoad()
{
    if( !m_request_load )
        return;

    if ( !isStateReady() )
    {
        if ( m_state == Dead )
        {
            msg_Warn( m_module, "no Chromecast hook possible");
            m_request_load = false;
        }
        else if( m_state == Connected )
        {
            msg_Dbg( m_module, "Starting the media receiver application" );
            // Don't use setState as we don't want to signal the condition in this case.
            m_state = Launching;
            m_communication.msgReceiverLaunchApp();
        }
        return;
    }

    m_request_load = false;

    // We should now be in the ready state, and therefor have a valid transportId
    assert( m_appTransportId.empty() == false );
    // Reset the mediaSessionID to allow the new session to become the current one.
    // we cannot start a new load when the last one is still processing
    m_last_request_id =
        m_communication.msgPlayerLoad( m_appTransportId, m_streaming_port,
                                       m_mime, m_meta );
    if( m_last_request_id != ChromecastCommunication::kInvalidId )
        m_state = Loading;
}

void intf_sys_t::setRetryOnFail( bool enabled )
{
    vlc_mutex_locker locker(&m_lock);
    m_retry_on_fail = enabled;
}

void intf_sys_t::setHasInput( const std::string mime_type )
{
    vlc_mutex_locker locker(&m_lock);
    msg_Dbg( m_module, "Loading content" );

    this->m_mime = mime_type;

    /* new input: clear message queue */
    std::queue<QueueableMessages> empty;
    std::swap(m_msgQueue, empty);

    prepareHttpArtwork();

    m_request_stop = false;
    m_played_once = false;
    m_paused = false;
    m_cc_eof = false;
    m_request_load = true;
    m_cc_time_last_request_date = VLC_TS_INVALID;
    m_cc_time_date = VLC_TS_INVALID;
    m_cc_time = VLC_TS_INVALID;
    m_pause_delay = VLC_TS_INVALID;
    m_mediaSessionId = 0;

    tryLoad();

    vlc_cond_signal( &m_stateChangedCond );
}

bool intf_sys_t::isStateError() const
{
    switch( m_state )
    {
        case LoadFailed:
        case Dead:
        case TakenOver:
            return true;
        default:
            return false;
    }
}

bool intf_sys_t::isStatePlaying() const
{
    switch( m_state )
    {
        case Loading:
        case Buffering:
        case Playing:
        case Paused:
            return true;
        default:
            return false;
    }
}

bool intf_sys_t::isStateReady() const
{
    switch( m_state )
    {
        case Connected:
        case Launching:
        case Authenticating:
        case Connecting:
        case Stopping:
        case Dead:
            return false;
        default:
            return true;
    }
}

void intf_sys_t::setPacing(bool do_pace)
{
    vlc_mutex_lock( &m_lock );
    if( m_pace == do_pace )
    {
        vlc_mutex_unlock( &m_lock );
        return;
    }
    m_pace = do_pace;
    vlc_mutex_unlock( &m_lock );
    vlc_cond_signal( &m_pace_cond );
}

static void interrupt_wake_up_cb( void *data )
{
    intf_sys_t *p_sys = static_cast<intf_sys_t*>((void *)data);
    p_sys->interrupt_wake_up();
}

void intf_sys_t::interrupt_wake_up()
{
    vlc_mutex_locker locker( &m_lock );
    m_interrupted = true;
    vlc_cond_signal( &m_pace_cond );
}

int intf_sys_t::pace()
{
    vlc_mutex_locker locker(&m_lock);

    m_interrupted = false;
    vlc_interrupt_register( interrupt_wake_up_cb, this );
    int ret = 0;
    mtime_t deadline = mdate() + INT64_C(500000);

    /* Wait for the sout to send more data via http (m_pace), or wait for the
     * CC to finish. In case the demux filter is EOF, we always wait for
     * 500msec (unless interrupted from the input thread). */
    while( !isFinishedPlaying() && ( m_pace || m_input_eof ) && !m_interrupted && ret == 0 )
        ret = vlc_cond_timedwait( &m_pace_cond, &m_lock, deadline );

    vlc_interrupt_unregister();

    if( m_cc_eof )
        return CC_PACE_OK_ENDED;
    else if( isStateError() || m_state == Stopped )
    {
        States error_state = m_state;
        m_state = Ready;
        if( error_state == LoadFailed && m_retry_on_fail )
            return CC_PACE_ERR_RETRY;
        return CC_PACE_ERR;
    }

    return ret == 0 ? CC_PACE_OK : CC_PACE_OK_WAIT;
}

void intf_sys_t::sendInputEvent(enum cc_input_event event, union cc_input_arg arg)
{
    vlc_mutex_lock(&m_lock);
    on_input_event_itf on_input_event = m_on_input_event;
    void *data = m_on_input_event_data;

    switch (event)
    {
        case CC_INPUT_EVENT_EOF:
            if (m_input_eof != arg.eof)
                m_input_eof = arg.eof;
            else
            {
                /* Don't send twice the same event */
                on_input_event = NULL;
                data = NULL;
            }
            break;
        default:
            break;
    }
    vlc_mutex_unlock(&m_lock);

    if (on_input_event)
        on_input_event(data, event, arg);
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

void intf_sys_t::queueMessage( QueueableMessages msg )
{
    // Assume lock is held by the called
    m_msgQueue.push( msg );
    vlc_interrupt_raise( m_ctl_thread_interrupt );
}

/*****************************************************************************
 * Chromecast thread
 *****************************************************************************/
void* intf_sys_t::ChromecastThread(void* p_data)
{
    intf_sys_t *p_sys = static_cast<intf_sys_t*>(p_data);
    p_sys->mainLoop();
    return NULL;
}

void intf_sys_t::mainLoop()
{
    vlc_savecancel();
    vlc_interrupt_set( m_ctl_thread_interrupt );

    // State was already initialized as Authenticating
    m_communication.msgAuth();

    while ( !vlc_killed() )
    {
        if ( !handleMessages() )
            break;
        // Reset the interrupt state to avoid commands not being sent (since
        // the context is still flagged as interrupted)
        vlc_interrupt_unregister();
        vlc_mutex_locker lock( &m_lock );
        while ( m_msgQueue.empty() == false )
        {
            QueueableMessages msg = m_msgQueue.front();
            switch ( msg )
            {
                case Stop:
                    doStop();
                    break;
            }
            m_msgQueue.pop();
        }
    }
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
        setState( Connecting );
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
        m_pingRetriesLeft = PING_WAIT_RETRIES;
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

        for (unsigned i = 0; i < applications.u.array.length; ++i)
        {
            if ( strcmp( applications[i]["appId"], APP_ID ) == 0 )
            {
                if ( (const char*)applications[i]["transportId"] != NULL)
                {
                    p_app = &applications[i];
                    break;
                }
            }
        }

        vlc_mutex_locker locker(&m_lock);

        switch ( m_state )
        {
        case Connecting:
            // We were connecting & fetching the current status.
            // The media receiver app is running, we are ready to proceed
            if ( p_app != NULL )
            {
                msg_Dbg( m_module, "Media receiver application was already running" );
                m_appTransportId = (const char*)(*p_app)["transportId"];
                m_communication.msgConnect( m_appTransportId );
                setState( Ready );
            }
            else
            {
                setState( Connected );
            }
            break;
        case Launching:
            // We already asked for the media receiver application to start
            if ( p_app != NULL )
            {
                msg_Dbg( m_module, "Media receiver application has been started." );
                m_appTransportId = (const char*)(*p_app)["transportId"];
                m_communication.msgConnect( m_appTransportId );
                setState( Ready );
            }
            break;
        case Loading:
        case Playing:
        case Paused:
        case Ready:
        case TakenOver:
        case Dead:
            if ( p_app == NULL )
            {
                msg_Warn( m_module, "Media receiver application got closed." );
                setState( Stopped );
                m_appTransportId = "";
                m_mediaSessionId = 0;
            }
            break;
        case Connected:
            // We might receive a RECEIVER_STATUS while being connected, when pinging/asking the status
            if ( p_app == NULL )
                break;
            // else: fall through and warn
        default:
            msg_Warn( m_module, "Unexpected RECEIVER_STATUS with state %s. "
                      "Checking media status",
                      StateToStr( m_state ) );
            // This is likely because the chromecast refused the playback, but
            // let's check by explicitely probing the media status
            if (m_last_request_id == 0)
                m_last_request_id = m_communication.msgPlayerGetStatus( m_appTransportId );
            break;
        }
    }
    else if (type == "LAUNCH_ERROR")
    {
        json_value reason = (*p_data)["reason"];
        msg_Err( m_module, "Failed to start the MediaPlayer: %s",
                (const char *)reason);
        vlc_mutex_locker locker(&m_lock);
        m_appTransportId = "";
        m_mediaSessionId = 0;
        setState( Dead );
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
    int64_t requestId = (json_int_t) (*p_data)["requestId"];

    vlc_mutex_locker locker( &m_lock );

    if ((m_last_request_id != 0 && requestId != m_last_request_id))
    {
        json_value_free(p_data);
        return;
    }
    m_last_request_id = 0;

    if (type == "MEDIA_STATUS")
    {
        json_value status = (*p_data)["status"];

        int64_t sessionId = (json_int_t) status[0]["mediaSessionId"];
        std::string newPlayerState = (const char*)status[0]["playerState"];
        std::string idleReason = (const char*)status[0]["idleReason"];

        msg_Dbg( m_module, "Player state: %s sessionId: %" PRId64,
                status[0]["playerState"].operator const char *(),
                sessionId );

        if ((m_mediaSessionId != sessionId && m_mediaSessionId != 0))
        {
            msg_Warn( m_module, "Ignoring message for a different media session");
            json_value_free(p_data);
            return;
        }

        if (newPlayerState == "IDLE" || newPlayerState.empty() == true )
        {
            /* Idle state is expected when the media receiver application is
             * started. In case the state is still Buffering, it denotes an error.
             * In most case, we'd receive a RECEIVER_STATUS message, which causes
             * use to ask for the MEDIA_STATUS before assuming an error occured.
             * If the chromecast silently gave up on playing our stream, we also
             * might have an empty status array.
             * If the media load indeed failed, we need to try another
             * transcode/remux configuration, or give up.
             * In case we are now loading, we might also receive an INTERRUPTED
             * state for the previous session, which we wouldn't ignore earlier
             * since our mediaSessionID was reset to 0.
             * In this case, don't assume we're being taken over, as we are
             * actually doing the take over.
             */
            if ( m_state != Ready && m_state != LoadFailed && m_state != Loading )
            {
                // The playback stopped
                if ( idleReason == "INTERRUPTED" )
                {
                    setState( TakenOver );
                    // Do not reset the mediaSessionId to ensure we refuse all
                    // other MEDIA_STATUS from the new session.
                }
                else if ( m_state == Buffering )
                    setState( LoadFailed );
                else
                {
                    if (idleReason == "FINISHED")
                        m_cc_eof = true;
                    setState( Ready );
                }
            }
        }
        else
        {
            if ( m_mediaSessionId == 0 )
            {
                m_mediaSessionId = sessionId;
                msg_Dbg( m_module, "New mediaSessionId: %" PRId64, m_mediaSessionId );
            }

            if (m_request_stop)
            {
                m_request_stop = false;
                m_last_request_id =
                    m_communication.msgPlayerStop( m_appTransportId, m_mediaSessionId );
                setState( Stopping );
            }
            else if (newPlayerState == "PLAYING")
            {
                mtime_t currentTime = timeCCToVLC((double) status[0]["currentTime"]);
                m_cc_time = currentTime;
                m_cc_time_date = mdate();

                setState( Playing );
            }
            else if (newPlayerState == "BUFFERING")
            {
                if ( m_state != Buffering )
                {
                    /* EOF when state goes from Playing to Buffering. There can
                     * be a lot of false positives (when seeking or when the cc
                     * request more input) but this state is fetched only when
                     * the input has reached EOF. */

                    setState( Buffering );
                }
            }
            else if (newPlayerState == "PAUSED")
            {
                if ( m_state != Paused )
                {
                    setState( Paused );
                }
            }
            else if ( newPlayerState == "LOADING" )
            {
                if ( m_state != Loading )
                {
                    msg_Dbg( m_module, "Chromecast is loading the stream" );
                    setState( Loading );
                }
            }
            else
                msg_Warn( m_module, "Unknown Chromecast MEDIA_STATUS state %s", newPlayerState.c_str());
        }
    }
    else if (type == "LOAD_FAILED")
    {
        msg_Err( m_module, "Media load failed");
        setState( LoadFailed );
    }
    else if (type == "LOAD_CANCELLED")
    {
        msg_Dbg( m_module, "LOAD canceled by another command");
    }
    else if (type == "INVALID_REQUEST")
    {
        msg_Dbg( m_module, "We sent an invalid request reason:%s", (const char*)(*p_data)["reason"] );
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

    if ( type == "CLOSE" )
    {
        // Close message indicates an application is being closed, not the connection.
        // From this point on, we need to relaunch the media receiver app
        vlc_mutex_locker locker(&m_lock);
        m_appTransportId = "";
        m_mediaSessionId = 0;
        setState( Connected );
    }
    else
    {
        msg_Warn( m_module, "Connection command not supported: %s",
                type.c_str());
    }
}

bool intf_sys_t::handleMessages()
{
    uint8_t p_packet[PACKET_MAX_LEN];
    size_t i_payloadSize = 0;
    size_t i_received = 0;
    bool b_timeout = false;
    mtime_t i_begin_time = mdate();

    /* Packet structure:
     * +------------------------------------+------------------------------+
     * | Payload size (uint32_t big endian) |         Payload data         |
     * +------------------------------------+------------------------------+
     */
    while ( true )
    {
        // If we haven't received the payload size yet, let's wait for it. Otherwise, we know
        // how many bytes to read
        ssize_t i_ret = m_communication.receive( p_packet + i_received,
                                        i_payloadSize + PACKET_HEADER_LEN - i_received,
                                        PING_WAIT_TIME - ( mdate() - i_begin_time ) / CLOCK_FREQ,
                                        &b_timeout );
        if ( i_ret < 0 )
        {
            if ( errno == EINTR )
                return true;
            // An error occured, we give up
            msg_Err( m_module, "The connection to the Chromecast died (receiving).");
            vlc_mutex_locker locker(&m_lock);
            setState( Dead );
            return false;
        }
        else if ( b_timeout == true )
        {
            // If no commands were queued to be sent, we timed out. Let's ping the chromecast
            vlc_mutex_locker locker(&m_lock);
            if ( m_pingRetriesLeft == 0 )
            {
                m_state = Dead;
                msg_Warn( m_module, "No PING response from the chromecast" );
                return false;
            }
            --m_pingRetriesLeft;
            m_communication.msgPing();
            m_communication.msgReceiverGetStatus();
            return true;
        }
        assert( i_ret != 0 );
        i_received += i_ret;
        if ( i_payloadSize == 0 )
        {
            i_payloadSize = U32_AT( p_packet );
            if ( i_payloadSize > PACKET_MAX_LEN - PACKET_HEADER_LEN )
            {
                msg_Err( m_module, "Payload size is too long: dropping connection" );
                vlc_mutex_locker locker(&m_lock);
                m_state = Dead;
                return false;
            }
            continue;
        }
        assert( i_received <= i_payloadSize + PACKET_HEADER_LEN );
        if ( i_received == i_payloadSize + PACKET_HEADER_LEN )
            break;
    }
    castchannel::CastMessage msg;
    msg.ParseFromArray(p_packet + PACKET_HEADER_LEN, i_payloadSize);
    processMessage(msg);
    return true;
}

void intf_sys_t::doStop()
{
    if( !isStatePlaying() )
        return;

    if ( m_mediaSessionId == 0 )
        m_request_stop = true;
    else
    {
        m_last_request_id =
            m_communication.msgPlayerStop( m_appTransportId, m_mediaSessionId );
        setState( Stopping );
    }
}

void intf_sys_t::requestPlayerStop()
{
    vlc_mutex_locker locker(&m_lock);

    std::queue<QueueableMessages> empty;
    std::swap(m_msgQueue, empty);

    m_retry_on_fail = false;
    m_request_load = false;

    if( vlc_killed() )
    {
        if( !isStatePlaying() )
            return;
        queueMessage( Stop );
    }
    else
        doStop();
}

States intf_sys_t::state() const
{
    vlc_mutex_locker locker( &m_lock );
    return m_state;
}

mtime_t intf_sys_t::timeCCToVLC(double time)
{
    return mtime_t(time * 1000000.0);
}

std::string intf_sys_t::timeVLCToCC(mtime_t time)
{
    std::stringstream ss;
    ss.setf(std::ios_base::fixed, std::ios_base::floatfield);
    ss << std::setprecision(6) << (double (time) / 1000000.0);
    return ss.str();
}

void intf_sys_t::setOnInputEventCb(on_input_event_itf on_input_event,
                                   void *on_input_event_data)
{
    vlc_mutex_locker locker(&m_lock);
    m_on_input_event = on_input_event;
    m_on_input_event_data = on_input_event_data;
}

void intf_sys_t::setOnPausedChangedCb(on_paused_changed_itf on_paused_changed,
                                      void *on_paused_changed_data)
{
    vlc_mutex_locker locker(&m_lock);
    m_on_paused_changed = on_paused_changed;
    m_on_paused_changed_data = on_paused_changed_data;
}

void intf_sys_t::setPauseState(bool paused, mtime_t delay)
{
    vlc_mutex_locker locker( &m_lock );
    if ( m_mediaSessionId == 0 || paused == m_paused )
        return;

    m_paused = paused;
    msg_Info( m_module, "%s state", paused ? "paused" : "playing" );
    if ( !paused )
    {
        m_last_request_id =
            m_communication.msgPlayerPlay( m_appTransportId, m_mediaSessionId );
        m_pause_delay = delay;
    }
    else if ( m_state != Paused )
        m_last_request_id =
            m_communication.msgPlayerPause( m_appTransportId, m_mediaSessionId );
}

mtime_t intf_sys_t::getPauseDelay()
{
    vlc_mutex_locker locker( &m_lock );
    return m_pause_delay;
}

bool intf_sys_t::isFinishedPlaying()
{
    return m_cc_eof || isStateError() || m_state == Stopped;
}

void intf_sys_t::setMeta(vlc_meta_t *p_meta)
{
    vlc_mutex_locker locker(&m_lock);
    if (m_meta != NULL)
        vlc_meta_Delete(m_meta);
    m_meta = p_meta;
}

mtime_t intf_sys_t::getPlaybackTimestamp()
{
    vlc_mutex_locker locker( &m_lock );
    switch( m_state )
    {
        case Buffering:
        case Paused:
            if( !m_played_once )
                return VLC_TS_INVALID;
            /* fallthrough */
        case Playing:
        {
            mtime_t now = mdate();
            if( m_state == Playing && m_last_request_id == 0
             && now - m_cc_time_last_request_date > INT64_C(4000000) )
            {
                m_cc_time_last_request_date = now;
                m_last_request_id =
                    m_communication.msgPlayerGetStatus( m_appTransportId );
            }
            return m_cc_time + now - m_cc_time_date;
        }
        default:
            return VLC_TS_INVALID;
    }
}

void intf_sys_t::setState( States state )
{
    if ( m_state != state )
    {
#ifndef NDEBUG
        msg_Dbg( m_module, "Switching from state %s to %s", StateToStr( m_state ), StateToStr( state ) );
#endif
        m_state = state;

        switch( m_state )
        {
            case Connected:
            case Ready:
                tryLoad();
                break;
            case Paused:
                if (m_played_once && m_on_paused_changed != NULL)
                    m_on_paused_changed(m_on_paused_changed_data, true);
                break;
            case Playing:
                if (m_played_once && m_on_paused_changed != NULL)
                    m_on_paused_changed(m_on_paused_changed_data, false);
                m_played_once = true;
                break;
            default:
                break;
        }
        vlc_cond_signal( &m_stateChangedCond );
        vlc_cond_signal( &m_pace_cond );
    }
}

mtime_t intf_sys_t::get_time(void *pt)
{
    intf_sys_t *p_this = static_cast<intf_sys_t*>(pt);
    return p_this->getPlaybackTimestamp();
}

void intf_sys_t::set_on_paused_changed_cb(void *pt,
                                          on_paused_changed_itf itf, void *data)
{
    intf_sys_t *p_this = static_cast<intf_sys_t*>(pt);
    p_this->setOnPausedChangedCb(itf, data);
}

int intf_sys_t::pace(void *pt)
{
    intf_sys_t *p_this = static_cast<intf_sys_t*>(pt);
    return p_this->pace();
}

void intf_sys_t::send_input_event(void *pt, enum cc_input_event event, union cc_input_arg arg)
{
    intf_sys_t *p_this = static_cast<intf_sys_t*>(pt);
    return p_this->sendInputEvent(event, arg);
}

void intf_sys_t::set_pause_state(void *pt, bool paused, mtime_t delay)
{
    intf_sys_t *p_this = static_cast<intf_sys_t*>(pt);
    p_this->setPauseState( paused, delay );
}

void intf_sys_t::set_meta(void *pt, vlc_meta_t *p_meta)
{
    intf_sys_t *p_this = static_cast<intf_sys_t*>(pt);
    p_this->setMeta( p_meta );
}
