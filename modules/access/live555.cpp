/*****************************************************************************
 * live555.cpp : LIVE555 Streaming Media support.
 *****************************************************************************
 * Copyright (C) 2003-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan. org>
 *          Derk-Jan Hartman <djhartman at m2x .dot. nl> for M2X
 *          Sébastien Escudier <sebastien-devel celeos eu>
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

/* For inttypes.h
 * Note: config.h may include inttypes.h, so make sure we define this option
 * early enough. */
#define __STDC_CONSTANT_MACROS 1
#define __STDC_LIMIT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_dialog.h>
#include <vlc_url.h>
#include <vlc_strings.h>

#include <limits.h>
#include <assert.h>


#if defined( _WIN32 )
#   include <winsock2.h>
#endif

#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>
#include <liveMedia_version.hh>
#include <Base64.hh>

extern "C" {
#include "../access/mms/asf.h"  /* Who said ugly ? */
}

using namespace std;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define KASENNA_TEXT N_( "Kasenna RTSP dialect")
#define KASENNA_LONGTEXT N_( "Kasenna servers use an old and nonstandard " \
    "dialect of RTSP. With this parameter VLC will try this dialect, but "\
    "then it cannot connect to normal RTSP servers." )

#define WMSERVER_TEXT N_("WMServer RTSP dialect")
#define WMSERVER_LONGTEXT N_("WMServer uses a nonstandard dialect " \
    "of RTSP. Selecting this parameter will tell VLC to assume some " \
    "options contrary to RFC 2326 guidelines.")

#define USER_TEXT N_("RTSP user name")
#define USER_LONGTEXT N_("Sets the username for the connection, " \
    "if no username or password are set in the url.")
#define PASS_TEXT N_("RTSP password")
#define PASS_LONGTEXT N_("Sets the password for the connection, " \
    "if no username or password are set in the url.")
#define FRAME_BUFFER_SIZE_TEXT N_("RTSP frame buffer size")
#define FRAME_BUFFER_SIZE_LONGTEXT N_("RTSP start frame buffer size of the video " \
    "track, can be increased in case of broken pictures due " \
    "to too small buffer.")
#define DEFAULT_FRAME_BUFFER_SIZE 100000

vlc_module_begin ()
    set_description( N_("RTP/RTSP/SDP demuxer (using Live555)" ) )
    set_capability( "demux", 50 )
    set_shortname( "RTP/RTSP")
    set_callbacks( Open, Close )
    add_shortcut( "live", "livedotcom" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_submodule ()
        set_description( N_("RTSP/RTP access and demux") )
        add_shortcut( "rtsp", "pnm", "live", "livedotcom" )
        set_capability( "access_demux", 0 )
        set_callbacks( Open, Close )
        add_bool( "rtsp-tcp", false,
                  N_("Use RTP over RTSP (TCP)"),
                  N_("Use RTP over RTSP (TCP)"), true )
            change_safe()
        add_integer( "rtp-client-port", -1,
                  N_("Client port"),
                  N_("Port to use for the RTP source of the session"), true )
        add_bool( "rtsp-mcast", false,
                  N_("Force multicast RTP via RTSP"),
                  N_("Force multicast RTP via RTSP"), true )
            change_safe()
        add_bool( "rtsp-http", false,
                  N_("Tunnel RTSP and RTP over HTTP"),
                  N_("Tunnel RTSP and RTP over HTTP"), true )
            change_safe()
        add_integer( "rtsp-http-port", 80,
                  N_("HTTP tunnel port"),
                  N_("Port to use for tunneling the RTSP/RTP over HTTP."),
                  true )
        add_bool(   "rtsp-kasenna", false, KASENNA_TEXT,
                    KASENNA_LONGTEXT, true )
            change_safe()
        add_bool(   "rtsp-wmserver", false, WMSERVER_TEXT,
                    WMSERVER_LONGTEXT, true)
            change_safe()
        add_string( "rtsp-user", NULL, USER_TEXT,
                    USER_LONGTEXT, true )
            change_safe()
        add_password( "rtsp-pwd", NULL, PASS_TEXT,
                      PASS_LONGTEXT, true )
        add_integer( "rtsp-frame-buffer-size", DEFAULT_FRAME_BUFFER_SIZE,
                     FRAME_BUFFER_SIZE_TEXT, FRAME_BUFFER_SIZE_LONGTEXT,
                     true )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    demux_t         *p_demux;
    MediaSubsession *sub;

    es_format_t     fmt;
    es_out_id_t     *p_es;

    bool            b_muxed;
    bool            b_quicktime;
    bool            b_asf;
    block_t         *p_asf_block;
    bool            b_discard_trunc;
    stream_t        *p_out_muxed;    /* for muxed stream */

    uint8_t         *p_buffer;
    unsigned int    i_buffer;

    bool            b_rtcp_sync;
    char            waiting;
    int64_t         i_pts;
    double          f_npt;

    bool            b_selected;

} live_track_t;

struct timeout_thread_t
{
    demux_sys_t  *p_sys;
    vlc_thread_t handle;
    bool         b_handle_keep_alive;
};

class RTSPClientVlc;

struct demux_sys_t
{
    char            *p_sdp;    /* XXX mallocated */
    char            *psz_path; /* URL-encoded path */
    vlc_url_t       url;

    MediaSession     *ms;
    TaskScheduler    *scheduler;
    UsageEnvironment *env ;
    RTSPClientVlc    *rtsp;

    /* */
    int              i_track;
    live_track_t     **track;

    /* Weird formats */
    asf_header_t     asfh;
    stream_t         *p_out_asf;
    bool             b_real;

    /* */
    int64_t          i_pcr; /* The clock */
    double           f_npt;
    double           f_npt_length;
    double           f_npt_start;

    /* timeout thread information */
    int              i_timeout;     /* session timeout value in seconds */
    bool             b_timeout_call;/* mark to send an RTSP call to prevent server timeout */
    timeout_thread_t *p_timeout;    /* the actual thread that makes sure we don't timeout */

    /* */
    bool             b_force_mcast;
    bool             b_multicast;   /* if one of the tracks is multicasted */
    bool             b_no_data;     /* if we never received any data */
    int              i_no_data_ti;  /* consecutive number of TaskInterrupt */

    char             event_rtsp;
    char             event_data;

    bool             b_get_param;   /* Does the server support GET_PARAMETER */
    bool             b_paused;      /* Are we paused? */
    bool             b_error;
    int              i_live555_ret; /* live555 callback return code */

    float            f_seek_request;/* In case we receive a seek request while paused*/
};


class RTSPClientVlc : public RTSPClient
{
public:
    RTSPClientVlc( UsageEnvironment& env, char const* rtspURL, int verbosityLevel,
                   char const* applicationName, portNumBits tunnelOverHTTPPortNum,
                   demux_sys_t *p_sys) :
                   RTSPClient( env, rtspURL, verbosityLevel, applicationName,
                   tunnelOverHTTPPortNum
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1373932800
                   , -1
#endif
                   )
    {
        this->p_sys = p_sys;
    }
    demux_sys_t *p_sys;
};

static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

static int Connect      ( demux_t * );
static int SessionsSetup( demux_t * );
static int Play         ( demux_t *);
static int ParseASF     ( demux_t * );
static int RollOverTcp  ( demux_t * );

static void StreamRead  ( void *, unsigned int, unsigned int,
                          struct timeval, unsigned int );
static void StreamClose ( void * );
static void TaskInterruptData( void * );
static void TaskInterruptRTSP( void * );

static void* TimeoutPrevention( void * );

static unsigned char* parseH264ConfigStr( char const* configStr,
                                          unsigned int& configSize );
static unsigned char* parseVorbisConfigStr( char const* configStr,
                                            unsigned int& configSize );

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = NULL;

    int i_return;
    int i_error = VLC_EGENERIC;

    if( p_demux->s )
    {
        /* See if it looks like a SDP
           v, o, s fields are mandatory and in this order */
        const uint8_t *p_peek;
        if( stream_Peek( p_demux->s, &p_peek, 7 ) < 7 ) return VLC_EGENERIC;

        if( memcmp( p_peek, "v=0\r\n", 5 ) &&
            memcmp( p_peek, "v=0\n", 4 ) &&
            ( p_peek[0] < 'a' || p_peek[0] > 'z' || p_peek[1] != '=' ) )
        {
            return VLC_EGENERIC;
        }
    }

    p_demux->pf_demux  = Demux;
    p_demux->pf_control= Control;
    p_demux->p_sys     = p_sys = (demux_sys_t*)calloc( 1, sizeof( demux_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;

    msg_Dbg( p_demux, "version "LIVEMEDIA_LIBRARY_VERSION_STRING );

    TAB_INIT( p_sys->i_track, p_sys->track );
    p_sys->f_npt = 0.;
    p_sys->f_npt_start = 0.;
    p_sys->f_npt_length = 0.;
    p_sys->b_no_data = true;
    p_sys->psz_path = strdup( p_demux->psz_location );
    p_sys->b_force_mcast = var_InheritBool( p_demux, "rtsp-mcast" );
    p_sys->f_seek_request = -1;

    /* parse URL for rtsp://[user:[passwd]@]serverip:port/options */
    vlc_UrlParse( &p_sys->url, p_sys->psz_path, 0 );

    if( ( p_sys->scheduler = BasicTaskScheduler::createNew() ) == NULL )
    {
        msg_Err( p_demux, "BasicTaskScheduler::createNew failed" );
        goto error;
    }
    if( !( p_sys->env = BasicUsageEnvironment::createNew(*p_sys->scheduler) ) )
    {
        msg_Err( p_demux, "BasicUsageEnvironment::createNew failed" );
        goto error;
    }

    if( strcasecmp( p_demux->psz_access, "sdp" ) )
    {
        char *p = p_sys->psz_path;
        while( (p = strchr( p, ' ' )) != NULL ) *p = '+';
    }

    if( p_demux->s != NULL )
    {
        /* Gather the complete sdp file */
        int     i_sdp       = 0;
        int     i_sdp_max   = 1000;
        uint8_t *p_sdp      = (uint8_t*) malloc( i_sdp_max );

        if( !p_sdp )
        {
            i_error = VLC_ENOMEM;
            goto error;
        }

        for( ;; )
        {
            int i_read = stream_Read( p_demux->s, &p_sdp[i_sdp],
                                      i_sdp_max - i_sdp - 1 );

            if( !vlc_object_alive (p_demux) )
            {
                free( p_sdp );
                goto error;
            }

            if( i_read < 0 )
            {
                msg_Err( p_demux, "failed to read SDP" );
                free( p_sdp );
                goto error;
            }

            i_sdp += i_read;

            if( i_read < i_sdp_max - i_sdp - 1 )
            {
                p_sdp[i_sdp] = '\0';
                break;
            }

            i_sdp_max += 1000;
            p_sdp = (uint8_t*)xrealloc( p_sdp, i_sdp_max );
        }
        p_sys->p_sdp = (char*)p_sdp;
    }
    else if( ( i_return = Connect( p_demux ) ) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Failed to connect with rtsp://%s", p_sys->psz_path );
        goto error;
    }

    if( p_sys->p_sdp == NULL )
    {
        msg_Err( p_demux, "Failed to retrieve the RTSP Session Description" );
        i_error = VLC_ENOMEM;
        goto error;
    }

    if( ( i_return = SessionsSetup( p_demux ) ) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Nothing to play for rtsp://%s", p_sys->psz_path );
        goto error;
    }

    if( p_sys->b_real ) goto error;

    if( ( i_return = Play( p_demux ) ) != VLC_SUCCESS )
        goto error;

    if( p_sys->p_out_asf && ParseASF( p_demux ) )
    {
        msg_Err( p_demux, "cannot find a usable asf header" );
        /* TODO Clean tracks */
        goto error;
    }

    if( p_sys->i_track <= 0 )
        goto error;

    return VLC_SUCCESS;

error:
    Close( p_this );
    return i_error;
}

/*****************************************************************************
 * DemuxClose:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_timeout )
    {
        vlc_cancel( p_sys->p_timeout->handle );
        vlc_join( p_sys->p_timeout->handle, NULL );
        free( p_sys->p_timeout );
    }

    if( p_sys->rtsp && p_sys->ms ) p_sys->rtsp->sendTeardownCommand( *p_sys->ms, NULL );
    if( p_sys->ms ) Medium::close( p_sys->ms );
    if( p_sys->rtsp ) RTSPClient::close( p_sys->rtsp );
    if( p_sys->env ) p_sys->env->reclaim();

    for( int i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->b_muxed ) stream_Delete( tk->p_out_muxed );
        es_format_Clean( &tk->fmt );
        free( tk->p_buffer );
        free( tk );
    }
    TAB_CLEAN( p_sys->i_track, p_sys->track );
    if( p_sys->p_out_asf ) stream_Delete( p_sys->p_out_asf );
    delete p_sys->scheduler;
    free( p_sys->p_sdp );
    free( p_sys->psz_path );

    vlc_UrlClean( &p_sys->url );

    free( p_sys );
}

static inline const char *strempty( const char *s ) { return s?s:""; }
static inline Boolean toBool( bool b ) { return b?True:False; } // silly, no?

static void default_live555_callback( RTSPClient* client, int result_code, char* result_string )
{
    RTSPClientVlc *client_vlc = static_cast<RTSPClientVlc *> ( client );
    demux_sys_t *p_sys = client_vlc->p_sys;
    delete []result_string;
    p_sys->i_live555_ret = result_code;
    p_sys->b_error = p_sys->i_live555_ret != 0;
    p_sys->event_rtsp = 1;
}

/* return true if the RTSP command succeeded */
static bool wait_Live555_response( demux_t *p_demux, int i_timeout = 0 /* ms */ )
{
    TaskToken task;
    demux_sys_t * p_sys = p_demux->p_sys;
    p_sys->event_rtsp = 0;
    if( i_timeout > 0 )
    {
        /* Create a task that will be called if we wait more than timeout ms */
        task = p_sys->scheduler->scheduleDelayedTask( i_timeout*1000,
                                                      TaskInterruptRTSP,
                                                      p_demux );
    }
    p_sys->event_rtsp = 0;
    p_sys->b_error = true;
    p_sys->i_live555_ret = 0;
    p_sys->scheduler->doEventLoop( &p_sys->event_rtsp );
    //here, if b_error is true and i_live555_ret = 0 we didn't receive a response
    if( i_timeout > 0 )
    {
        /* remove the task */
        p_sys->scheduler->unscheduleDelayedTask( task );
    }
    return !p_sys->b_error;
}

static void continueAfterDESCRIBE( RTSPClient* client, int result_code,
                                   char* result_string )
{
    RTSPClientVlc *client_vlc = static_cast<RTSPClientVlc *> ( client );
    demux_sys_t *p_sys = client_vlc->p_sys;
    p_sys->i_live555_ret = result_code;
    if ( result_code == 0 )
    {
        char* sdpDescription = result_string;
        free( p_sys->p_sdp );
        p_sys->p_sdp = NULL;
        if( sdpDescription )
        {
            p_sys->p_sdp = strdup( sdpDescription );
            p_sys->b_error = false;
        }
    }
    else
        p_sys->b_error = true;
    delete[] result_string;
    p_sys->event_rtsp = 1;
}

static void continueAfterOPTIONS( RTSPClient* client, int result_code,
                                  char* result_string )
{
    RTSPClientVlc *client_vlc = static_cast<RTSPClientVlc *> (client);
    demux_sys_t *p_sys = client_vlc->p_sys;
    p_sys->b_get_param =
      // If OPTIONS fails, assume GET_PARAMETER is not supported but
      // still continue on with the stream.  Some servers (foscam)
      // return 501/not implemented for OPTIONS.
      result_code == 0
      && result_string != NULL
      && strstr( result_string, "GET_PARAMETER" ) != NULL;
    client->sendDescribeCommand( continueAfterDESCRIBE );
    delete[] result_string;
}

/*****************************************************************************
 * Connect: connects to the RTSP server to setup the session DESCRIBE
 *****************************************************************************/
static int Connect( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    Authenticator authenticator;
    char *psz_user    = NULL;
    char *psz_pwd     = NULL;
    char *psz_url     = NULL;
    int  i_http_port  = 0;
    int  i_ret        = VLC_SUCCESS;
    const int i_timeout = var_InheritInteger( p_demux, "ipv4-timeout" );

    /* Get the user name and password */
    if( p_sys->url.psz_username || p_sys->url.psz_password )
    {
        /* Create the URL by stripping away the username/password part */
        if( p_sys->url.i_port == 0 )
            p_sys->url.i_port = 554;
        if( asprintf( &psz_url, "rtsp://%s:%d%s",
                      strempty( p_sys->url.psz_host ),
                      p_sys->url.i_port,
                      strempty( p_sys->url.psz_path ) ) == -1 )
            return VLC_ENOMEM;

        psz_user = strdup( strempty( p_sys->url.psz_username ) );
        psz_pwd  = strdup( strempty( p_sys->url.psz_password ) );
    }
    else
    {
        if( asprintf( &psz_url, "rtsp://%s", p_sys->psz_path ) == -1 )
            return VLC_ENOMEM;

        psz_user = var_InheritString( p_demux, "rtsp-user" );
        psz_pwd  = var_InheritString( p_demux, "rtsp-pwd" );
    }

createnew:
    if( !vlc_object_alive (p_demux) )
    {
        i_ret = VLC_EGENERIC;
        goto bailout;
    }

    if( var_CreateGetBool( p_demux, "rtsp-http" ) )
        i_http_port = var_InheritInteger( p_demux, "rtsp-http-port" );

    p_sys->rtsp = new RTSPClientVlc( *p_sys->env, psz_url,
                                     var_InheritInteger( p_demux, "verbose" ) > 1 ? 1 : 0,
                                     "LibVLC/"VERSION, i_http_port, p_sys );
    if( !p_sys->rtsp )
    {
        msg_Err( p_demux, "RTSPClient::createNew failed (%s)",
                 p_sys->env->getResultMsg() );
        i_ret = VLC_EGENERIC;
        goto bailout;
    }

    /* Kasenna enables KeepAlive by analysing the User-Agent string.
     * Appending _KA to the string should be enough to enable this feature,
     * however, there is a bug where the _KA doesn't get parsed from the
     * default User-Agent as created by VLC/Live555 code. This is probably due
     * to spaces in the string or the string being too long. Here we override
     * the default string with a more compact version.
     */
    if( var_InheritBool( p_demux, "rtsp-kasenna" ))
    {
        p_sys->rtsp->setUserAgentString( "VLC_MEDIA_PLAYER_KA" );
    }

describe:
    authenticator.setUsernameAndPassword( psz_user, psz_pwd );

    p_sys->rtsp->sendOptionsCommand( &continueAfterOPTIONS, &authenticator );

    if( !wait_Live555_response( p_demux, i_timeout ) )
    {
        int i_code = p_sys->i_live555_ret;
        if( i_code == 401 )
        {
            msg_Dbg( p_demux, "authentication failed" );

            free( psz_user );
            free( psz_pwd );
            dialog_Login( p_demux, &psz_user, &psz_pwd,
                          _("RTSP authentication"), "%s",
                        _("Please enter a valid login name and a password.") );
            if( psz_user != NULL && psz_pwd != NULL )
            {
                msg_Dbg( p_demux, "retrying with user=%s", psz_user );
                goto describe;
            }
        }
        else if( i_code > 0 && i_code != 404 && !var_GetBool( p_demux, "rtsp-http" ) )
        {
            /* Perhaps a firewall is being annoying. Try HTTP tunneling mode */
            msg_Dbg( p_demux, "we will now try HTTP tunneling mode" );
            var_SetBool( p_demux, "rtsp-http", true );
            if( p_sys->rtsp ) RTSPClient::close( p_sys->rtsp );
            p_sys->rtsp = NULL;
            goto createnew;
        }
        else
        {
            if( i_code == 0 )
                msg_Dbg( p_demux, "connection timeout" );
            else
            {
                msg_Dbg( p_demux, "connection error %d", i_code );
                if( i_code == 403 )
                    dialog_Fatal( p_demux, _("RTSP connection failed"),
                                  _("Access to the stream is denied by the server configuration.") );
            }
            if( p_sys->rtsp ) RTSPClient::close( p_sys->rtsp );
            p_sys->rtsp = NULL;
        }
        i_ret = VLC_EGENERIC;
    }

bailout:
    /* malloc-ated copy */
    free( psz_url );
    free( psz_user );
    free( psz_pwd );

    return i_ret;
}

/*****************************************************************************
 * SessionsSetup: prepares the subsessions and does the SETUP
 *****************************************************************************/
static int SessionsSetup( demux_t *p_demux )
{
    demux_sys_t             *p_sys  = p_demux->p_sys;
    MediaSubsessionIterator *iter   = NULL;
    MediaSubsession         *sub    = NULL;

    bool           b_rtsp_tcp;
    int            i_client_port;
    int            i_return = VLC_SUCCESS;
    unsigned int   i_receive_buffer = 0;
    int            i_frame_buffer = DEFAULT_FRAME_BUFFER_SIZE;
    unsigned const thresh = 200000; /* RTP reorder threshold .2 second (default .1) */

    b_rtsp_tcp    = var_CreateGetBool( p_demux, "rtsp-tcp" ) ||
                    var_GetBool( p_demux, "rtsp-http" );
    i_client_port = var_InheritInteger( p_demux, "rtp-client-port" );


    /* Create the session from the SDP */
    if( !( p_sys->ms = MediaSession::createNew( *p_sys->env, p_sys->p_sdp ) ) )
    {
        msg_Err( p_demux, "Could not create the RTSP Session: %s",
            p_sys->env->getResultMsg() );
        return VLC_EGENERIC;
    }

    /* Initialise each media subsession */
    iter = new MediaSubsessionIterator( *p_sys->ms );
    while( ( sub = iter->next() ) != NULL )
    {
        Boolean bInit;
        live_track_t *tk;

        if( !vlc_object_alive (p_demux) )
        {
            delete iter;
            return VLC_EGENERIC;
        }

        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
            i_receive_buffer = 100000;
        else if( !strcmp( sub->mediumName(), "video" ) )
        {
            int i_var_buf_size = var_InheritInteger( p_demux, "rtsp-frame-buffer-size" );
            if( i_var_buf_size > 0 )
                i_frame_buffer = i_var_buf_size;
            i_receive_buffer = 2000000;
        }
        else if( !strcmp( sub->mediumName(), "text" ) )
            ;
        else continue;

        if( i_client_port != -1 )
        {
            sub->setClientPortNum( i_client_port );
            i_client_port += 2;
        }

        if( strcasestr( sub->codecName(), "REAL" ) )
        {
            msg_Info( p_demux, "real codec detected, using real-RTSP instead" );
            p_sys->b_real = true; /* This is a problem, we'll handle it later */
            continue;
        }

        if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
            bInit = sub->initiate( 0 );
        else
            bInit = sub->initiate();

        if( !bInit )
        {
            msg_Warn( p_demux, "RTP subsession '%s/%s' failed (%s)",
                      sub->mediumName(), sub->codecName(),
                      p_sys->env->getResultMsg() );
        }
        else
        {
            if( sub->rtpSource() != NULL )
            {
                int fd = sub->rtpSource()->RTPgs()->socketNum();

                /* Increase the buffer size */
                if( i_receive_buffer > 0 )
                    increaseReceiveBufferTo( *p_sys->env, fd, i_receive_buffer );

                /* Increase the RTP reorder timebuffer just a bit */
                sub->rtpSource()->setPacketReorderingThresholdTime(thresh);
            }
            msg_Dbg( p_demux, "RTP subsession '%s/%s'", sub->mediumName(),
                     sub->codecName() );

            /* Issue the SETUP */
            if( p_sys->rtsp )
            {
                p_sys->rtsp->sendSetupCommand( *sub, default_live555_callback, False,
                                               toBool( b_rtsp_tcp ),
                                               toBool( p_sys->b_force_mcast && !b_rtsp_tcp ) );
                if( !wait_Live555_response( p_demux ) )
                {
                    /* if we get an unsupported transport error, toggle TCP
                     * use and try again */
                    if( p_sys->i_live555_ret == 461 )
                        p_sys->rtsp->sendSetupCommand( *sub, default_live555_callback, False,
                                                       !toBool( b_rtsp_tcp ), False );
                    if( p_sys->i_live555_ret != 461 || !wait_Live555_response( p_demux ) )
                    {
                        msg_Err( p_demux, "SETUP of'%s/%s' failed %s",
                                 sub->mediumName(), sub->codecName(),
                                 p_sys->env->getResultMsg() );
                        continue;
                    }
                    else
                    {
                        var_SetBool( p_demux, "rtsp-tcp", true );
                        b_rtsp_tcp = true;
                    }
                }
            }

            /* Check if we will receive data from this subsession for
             * this track */
            if( sub->readSource() == NULL ) continue;
            if( !p_sys->b_multicast )
            {
                /* We need different rollover behaviour for multicast */
                p_sys->b_multicast = IsMulticastAddress( sub->connectionEndpointAddress() );
            }

            tk = (live_track_t*)malloc( sizeof( live_track_t ) );
            if( !tk )
            {
                delete iter;
                return VLC_ENOMEM;
            }
            tk->p_demux     = p_demux;
            tk->sub         = sub;
            tk->p_es        = NULL;
            tk->b_quicktime = false;
            tk->b_asf       = false;
            tk->p_asf_block = NULL;
            tk->b_muxed     = false;
            tk->b_discard_trunc = false;
            tk->p_out_muxed = NULL;
            tk->waiting     = 0;
            tk->b_rtcp_sync = false;
            tk->i_pts       = VLC_TS_INVALID;
            tk->f_npt       = 0.;
            tk->b_selected  = true;
            tk->i_buffer    = i_frame_buffer;
            tk->p_buffer    = (uint8_t *)malloc( i_frame_buffer );

            if( !tk->p_buffer )
            {
                free( tk );
                delete iter;
                return VLC_ENOMEM;
            }

            /* Value taken from mplayer */
            if( !strcmp( sub->mediumName(), "audio" ) )
            {
                es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('u','n','d','f') );
                tk->fmt.audio.i_channels = sub->numChannels();
                tk->fmt.audio.i_rate = sub->rtpTimestampFrequency();

                if( !strcmp( sub->codecName(), "MPA" ) ||
                    !strcmp( sub->codecName(), "MPA-ROBUST" ) ||
                    !strcmp( sub->codecName(), "X-MP3-DRAFT-00" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_MPGA;
                    tk->fmt.audio.i_rate = 0;
                }
                else if( !strcmp( sub->codecName(), "AC3" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_A52;
                    tk->fmt.audio.i_rate = 0;
                }
                else if( !strcmp( sub->codecName(), "L16" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_S16B;
                    tk->fmt.audio.i_bitspersample = 16;
                }
                else if( !strcmp( sub->codecName(), "L20" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_S20B;
                    tk->fmt.audio.i_bitspersample = 20;
                }
                else if( !strcmp( sub->codecName(), "L24" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_S24B;
                    tk->fmt.audio.i_bitspersample = 24;
                }
                else if( !strcmp( sub->codecName(), "L8" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_U8;
                    tk->fmt.audio.i_bitspersample = 8;
                }
                else if( !strcmp( sub->codecName(), "DAT12" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_DAT12;
                    tk->fmt.audio.i_bitspersample = 12;
                }
                else if( !strcmp( sub->codecName(), "PCMU" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_MULAW;
                    tk->fmt.audio.i_bitspersample = 8;
                }
                else if( !strcmp( sub->codecName(), "PCMA" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_ALAW;
                    tk->fmt.audio.i_bitspersample = 8;
                }
                else if( !strncmp( sub->codecName(), "G726", 4 ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_ADPCM_G726;
                    tk->fmt.audio.i_rate = 8000;
                    tk->fmt.audio.i_channels = 1;
                    if( !strcmp( sub->codecName()+5, "40" ) )
                        tk->fmt.i_bitrate = 40000;
                    else if( !strcmp( sub->codecName()+5, "32" ) )
                        tk->fmt.i_bitrate = 32000;
                    else if( !strcmp( sub->codecName()+5, "24" ) )
                        tk->fmt.i_bitrate = 24000;
                    else if( !strcmp( sub->codecName()+5, "16" ) )
                        tk->fmt.i_bitrate = 16000;
                }
                else if( !strcmp( sub->codecName(), "AMR" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_AMR_NB;
                }
                else if( !strcmp( sub->codecName(), "AMR-WB" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_AMR_WB;
                }
                else if( !strcmp( sub->codecName(), "MP4A-LATM" ) )
                {
                    unsigned int i_extra;
                    uint8_t      *p_extra;

                    tk->fmt.i_codec = VLC_CODEC_MP4A;

                    if( ( p_extra = parseStreamMuxConfigStr( sub->fmtp_config(),
                                                             i_extra ) ) )
                    {
                        tk->fmt.i_extra = i_extra;
                        tk->fmt.p_extra = xmalloc( i_extra );
                        memcpy( tk->fmt.p_extra, p_extra, i_extra );
                        delete[] p_extra;
                    }
                    /* Because the "faad" decoder does not handle the LATM
                     * data length field at the start of each returned LATM
                     * frame, tell the RTP source to omit. */
                    ((MPEG4LATMAudioRTPSource*)sub->rtpSource())->omitLATMDataLengthField();
                }
                else if( !strcmp( sub->codecName(), "MPEG4-GENERIC" ) )
                {
                    unsigned int i_extra;
                    uint8_t      *p_extra;

                    tk->fmt.i_codec = VLC_CODEC_MP4A;

                    if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                           i_extra ) ) )
                    {
                        tk->fmt.i_extra = i_extra;
                        tk->fmt.p_extra = xmalloc( i_extra );
                        memcpy( tk->fmt.p_extra, p_extra, i_extra );
                        delete[] p_extra;
                    }
                }
                else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
                {
                    tk->b_asf = true;
                    if( p_sys->p_out_asf == NULL )
                        p_sys->p_out_asf = stream_DemuxNew( p_demux, "asf",
                                                            p_demux->out );
                }
                else if( !strcmp( sub->codecName(), "X-QT" ) ||
                         !strcmp( sub->codecName(), "X-QUICKTIME" ) )
                {
                    tk->b_quicktime = true;
                }
                else if( !strcmp( sub->codecName(), "SPEEX" ) )
                {
                    tk->fmt.i_codec = VLC_FOURCC( 's', 'p', 'x', 'r' );
                    if ( tk->fmt.audio.i_rate == 0 )
                    {
                        msg_Warn( p_demux,"Using 8kHz as default sample rate." );
                        tk->fmt.audio.i_rate = 8000;
                    }
                }
                else if( !strcmp( sub->codecName(), "VORBIS" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_VORBIS;
                    unsigned int i_extra;
                    unsigned char *p_extra;
                    if( ( p_extra=parseVorbisConfigStr( sub->fmtp_config(),
                                                        i_extra ) ) )
                    {
                        tk->fmt.i_extra = i_extra;
                        tk->fmt.p_extra = p_extra;
                    }
                    else
                        msg_Warn( p_demux,"Missing or unsupported vorbis header." );
                }
            }
            else if( !strcmp( sub->mediumName(), "video" ) )
            {
                es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('u','n','d','f') );
                if( !strcmp( sub->codecName(), "MPV" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_MPGV;
                    tk->fmt.b_packetized = false;
                }
                else if( !strcmp( sub->codecName(), "H263" ) ||
                         !strcmp( sub->codecName(), "H263-1998" ) ||
                         !strcmp( sub->codecName(), "H263-2000" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_H263;
                }
                else if( !strcmp( sub->codecName(), "H261" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_H261;
                }
                else if( !strcmp( sub->codecName(), "H264" ) )
                {
                    unsigned int i_extra = 0;
                    uint8_t      *p_extra = NULL;

                    tk->fmt.i_codec = VLC_CODEC_H264;
                    tk->fmt.b_packetized = false;

                    if((p_extra=parseH264ConfigStr( sub->fmtp_spropparametersets(),
                                                    i_extra ) ) )
                    {
                        tk->fmt.i_extra = i_extra;
                        tk->fmt.p_extra = xmalloc( i_extra );
                        memcpy( tk->fmt.p_extra, p_extra, i_extra );

                        delete[] p_extra;
                    }
                }
                else if( !strcmp( sub->codecName(), "JPEG" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_MJPG;
                }
                else if( !strcmp( sub->codecName(), "MP4V-ES" ) )
                {
                    unsigned int i_extra;
                    uint8_t      *p_extra;

                    tk->fmt.i_codec = VLC_CODEC_MP4V;

                    if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                           i_extra ) ) )
                    {
                        tk->fmt.i_extra = i_extra;
                        tk->fmt.p_extra = xmalloc( i_extra );
                        memcpy( tk->fmt.p_extra, p_extra, i_extra );
                        delete[] p_extra;
                    }
                }
                else if( !strcmp( sub->codecName(), "X-QT" ) ||
                         !strcmp( sub->codecName(), "X-QUICKTIME" ) ||
                         !strcmp( sub->codecName(), "X-QDM" ) ||
                         !strcmp( sub->codecName(), "X-SV3V-ES" )  ||
                         !strcmp( sub->codecName(), "X-SORENSONVIDEO" ) )
                {
                    tk->b_quicktime = true;
                }
                else if( !strcmp( sub->codecName(), "MP2T" ) )
                {
                    tk->b_muxed = true;
                    tk->p_out_muxed = stream_DemuxNew( p_demux, "ts", p_demux->out );
                }
                else if( !strcmp( sub->codecName(), "MP2P" ) ||
                         !strcmp( sub->codecName(), "MP1S" ) )
                {
                    tk->b_muxed = true;
                    tk->p_out_muxed = stream_DemuxNew( p_demux, "ps",
                                                       p_demux->out );
                }
                else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
                {
                    tk->b_asf = true;
                    if( p_sys->p_out_asf == NULL )
                        p_sys->p_out_asf = stream_DemuxNew( p_demux, "asf",
                                                            p_demux->out );;
                }
                else if( !strcmp( sub->codecName(), "DV" ) )
                {
                    tk->b_muxed = true;
                    tk->b_discard_trunc = true;
                    tk->p_out_muxed = stream_DemuxNew( p_demux, "rawdv",
                                                       p_demux->out );
                }
                else if( !strcmp( sub->codecName(), "VP8" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_VP8;
                }
            }
            else if( !strcmp( sub->mediumName(), "text" ) )
            {
                es_format_Init( &tk->fmt, SPU_ES, VLC_FOURCC('u','n','d','f') );

                if( !strcmp( sub->codecName(), "T140" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_ITU_T140;
                }
            }

            if( !tk->b_quicktime && !tk->b_muxed && !tk->b_asf )
            {
                tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
            }

            if( sub->rtcpInstance() != NULL )
            {
                sub->rtcpInstance()->setByeHandler( StreamClose, tk );
            }

            if( tk->p_es || tk->b_quicktime || ( tk->b_muxed && tk->p_out_muxed ) ||
                ( tk->b_asf && p_sys->p_out_asf ) )
            {
                TAB_APPEND_CAST( (live_track_t **), p_sys->i_track, p_sys->track, tk );
            }
            else
            {
                /* BUG ??? */
                msg_Err( p_demux, "unusable RTSP track. this should not happen" );
                es_format_Clean( &tk->fmt );
                free( tk );
            }
        }
    }
    delete iter;
    if( p_sys->i_track <= 0 ) i_return = VLC_EGENERIC;

    /* Retrieve the starttime if possible */
    p_sys->f_npt_start = p_sys->ms->playStartTime();

    /* Retrieve the duration if possible */
    p_sys->f_npt_length = p_sys->ms->playEndTime();

    /* */
    msg_Dbg( p_demux, "setup start: %f stop:%f", p_sys->f_npt_start, p_sys->f_npt_length );

    /* */
    p_sys->b_no_data = true;
    p_sys->i_no_data_ti = 0;

    return i_return;
}

/*****************************************************************************
 * Play: starts the actual playback of the stream
 *****************************************************************************/
static int Play( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->rtsp )
    {
        /* The PLAY */
        p_sys->rtsp->sendPlayCommand( *p_sys->ms, default_live555_callback, p_sys->f_npt_start, -1, 1 );

        if( !wait_Live555_response(p_demux) )
        {
            msg_Err( p_demux, "RTSP PLAY failed %s", p_sys->env->getResultMsg() );
            return VLC_EGENERIC;
        }

        /* Retrieve the timeout value and set up a timeout prevention thread */
        p_sys->i_timeout = p_sys->rtsp->sessionTimeoutParameter();
        if( p_sys->i_timeout <= 0 )
            p_sys->i_timeout = 60; /* default value from RFC2326 */

        /* start timeout-thread only if GET_PARAMETER is supported by the server */
        /* or start it if wmserver dialect, since they don't report that GET_PARAMETER is supported correctly */
        if( !p_sys->p_timeout && ( p_sys->b_get_param || var_InheritBool( p_demux, "rtsp-wmserver" ) ) )
        {
            msg_Dbg( p_demux, "We have a timeout of %d seconds",  p_sys->i_timeout );
            p_sys->p_timeout = (timeout_thread_t *)malloc( sizeof(timeout_thread_t) );
            if( p_sys->p_timeout )
            {
                memset( p_sys->p_timeout, 0, sizeof(timeout_thread_t) );
                p_sys->p_timeout->p_sys = p_demux->p_sys; /* lol, object recursion :D */
                if( vlc_clone( &p_sys->p_timeout->handle,  TimeoutPrevention,
                               p_sys->p_timeout, VLC_THREAD_PRIORITY_LOW ) )
                {
                    msg_Err( p_demux, "cannot spawn liveMedia timeout thread" );
                    free( p_sys->p_timeout );
                    p_sys->p_timeout = NULL;
                }
                else
                    msg_Dbg( p_demux, "spawned timeout thread" );
            }
            else
                msg_Err( p_demux, "cannot spawn liveMedia timeout thread" );
        }
    }
    p_sys->i_pcr = 0;

    /* Retrieve the starttime if possible */
    p_sys->f_npt_start = p_sys->ms->playStartTime();
    if( p_sys->ms->playEndTime() > 0 )
        p_sys->f_npt_length = p_sys->ms->playEndTime();

    msg_Dbg( p_demux, "play start: %f stop:%f", p_sys->f_npt_start, p_sys->f_npt_length );
    return VLC_SUCCESS;
}


/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t    *p_sys = p_demux->p_sys;
    TaskToken      task;

    bool            b_send_pcr = true;
    int64_t         i_pcr = 0;
    int             i;

    /* Check if we need to send the server a Keep-A-Live signal */
    if( p_sys->b_timeout_call && p_sys->rtsp && p_sys->ms )
    {
        char *psz_bye = NULL;
        p_sys->rtsp->sendGetParameterCommand( *p_sys->ms, NULL, psz_bye );
        p_sys->b_timeout_call = false;
    }

    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->p_es )
        {
            bool b;
            es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
            if( !b && tk->b_selected )
            {
                tk->b_selected = false;
                p_sys->rtsp->sendTeardownCommand( *tk->sub, NULL );
            }
            else if( b && !tk->b_selected)
            {
                bool b_rtsp_tcp = var_GetBool( p_demux, "rtsp-tcp" ) ||
                                  var_GetBool( p_demux, "rtsp-http" );
                p_sys->rtsp->sendSetupCommand( *tk->sub, default_live555_callback, False,
                                               toBool( b_rtsp_tcp ),
                                               toBool( p_sys->b_force_mcast && !b_rtsp_tcp ) );
                if( !wait_Live555_response( p_demux ) )
                {
                    msg_Err( p_demux, "SETUP of'%s/%s' failed %s",
                             tk->sub->mediumName(), tk->sub->codecName(),
                             p_sys->env->getResultMsg() );
                }
                else
                {
                    p_sys->rtsp->sendPlayCommand( *tk->sub, default_live555_callback, -1, -1, p_sys->ms->scale() );
                    if( !wait_Live555_response(p_demux) )
                    {
                        msg_Err( p_demux, "RTSP PLAY failed %s", p_sys->env->getResultMsg() );
                        p_sys->rtsp->sendTeardownCommand( *tk->sub, NULL );
                    }
                    else
                        tk->b_selected = true;
                }
                if( !tk->b_selected )
                    es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, tk->p_es, false );
            }
        }

        if( tk->b_asf || tk->b_muxed )
            b_send_pcr = false;
#if 0
        if( i_pcr == 0 )
        {
            i_pcr = tk->i_pts;
        }
        else if( tk->i_pts != 0 && i_pcr > tk->i_pts )
        {
            i_pcr = tk->i_pts ;
        }
#endif
    }
    if( p_sys->i_pcr > 0 )
    {
        if( b_send_pcr )
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, 1 + p_sys->i_pcr );
    }

    /* First warn we want to read data */
    p_sys->event_data = 0;
    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->waiting == 0 )
        {
            tk->waiting = 1;
            tk->sub->readSource()->getNextFrame( tk->p_buffer, tk->i_buffer,
                                          StreamRead, tk, StreamClose, tk );
        }
    }
    /* Create a task that will be called if we wait more than 300ms */
    task = p_sys->scheduler->scheduleDelayedTask( 300000, TaskInterruptData, p_demux );

    /* Do the read */
    p_sys->scheduler->doEventLoop( &p_sys->event_data );

    /* remove the task */
    p_sys->scheduler->unscheduleDelayedTask( task );

    /* Check for gap in pts value */
    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( !tk->b_muxed && !tk->b_rtcp_sync &&
            tk->sub->rtpSource() && tk->sub->rtpSource()->hasBeenSynchronizedUsingRTCP() )
        {
            msg_Dbg( p_demux, "tk->rtpSource->hasBeenSynchronizedUsingRTCP()" );

            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
            tk->b_rtcp_sync = true;
            /* reset PCR */
            tk->i_pts = VLC_TS_INVALID;
            tk->f_npt = 0.;
            p_sys->i_pcr = 0;
            p_sys->f_npt = 0.;
            i_pcr = 0;
        }
    }

    if( p_sys->b_multicast && p_sys->b_no_data &&
        ( p_sys->i_no_data_ti > 120 ) )
    {
        /* FIXME Make this configurable
        msg_Err( p_demux, "no multicast data received in 36s, aborting" );
        return 0;
        */
    }
    else if( !p_sys->b_multicast && !p_sys->b_paused &&
              p_sys->b_no_data && ( p_sys->i_no_data_ti > 34 ) )
    {
        bool b_rtsp_tcp = var_GetBool( p_demux, "rtsp-tcp" ) ||
                                var_GetBool( p_demux, "rtsp-http" );

        if( !b_rtsp_tcp && p_sys->rtsp && p_sys->ms )
        {
            msg_Warn( p_demux, "no data received in 10s. Switching to TCP" );
            if( RollOverTcp( p_demux ) )
            {
                msg_Err( p_demux, "TCP rollover failed, aborting" );
                return 0;
            }
            return 1;
        }
        msg_Err( p_demux, "no data received in 10s, aborting" );
        return 0;
    }
    else if( !p_sys->b_multicast && !p_sys->b_paused &&
             ( p_sys->i_no_data_ti > 34 ) )
    {
        /* EOF ? */
        msg_Warn( p_demux, "no data received in 10s, eof ?" );
        return 0;
    }
    return p_sys->b_error ? 0 : 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t *pi64, i64;
    double  *pf, f;
    bool *pb, *pb2;
    int *pi_int;

    switch( i_query )
    {
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->f_npt > 0 )
            {
                *pi64 = (int64_t)(p_sys->f_npt * 1000000.);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->f_npt_length > 0 )
            {
                double d_length = p_sys->f_npt_length * 1000000.0;
                if( d_length >= INT64_MAX )
                    *pi64 = INT64_MAX;
                else
                    *pi64 = (int64_t)d_length;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double* );
            if( (p_sys->f_npt_length > 0) && (p_sys->f_npt > 0) )
            {
                *pf = p_sys->f_npt / p_sys->f_npt_length;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_POSITION:
        case DEMUX_SET_TIME:
            if( p_sys->rtsp && (p_sys->f_npt_length > 0) )
            {
                int i;
                float time;

                if( (i_query == DEMUX_SET_TIME) && (p_sys->f_npt > 0) )
                {
                    i64 = (int64_t)va_arg( args, int64_t );
                    time = (float)(i64 / 1000000.0); /* in second */
                }
                else if( i_query == DEMUX_SET_TIME )
                    return VLC_EGENERIC;
                else
                {
                    f = (double)va_arg( args, double );
                    time = f * p_sys->f_npt_length;   /* in second */
                }

                if( p_sys->b_paused )
                {
                    p_sys->f_seek_request = time;
                    return VLC_SUCCESS;
                }

                p_sys->rtsp->sendPauseCommand( *p_sys->ms, default_live555_callback );

                if( !wait_Live555_response( p_demux ) )
                {
                    msg_Err( p_demux, "PAUSE before seek failed %s",
                        p_sys->env->getResultMsg() );
                    return VLC_EGENERIC;
                }

                p_sys->rtsp->sendPlayCommand( *p_sys->ms, default_live555_callback, time, -1, 1 );

                if( !wait_Live555_response( p_demux ) )
                {
                    msg_Err( p_demux, "seek PLAY failed %s",
                        p_sys->env->getResultMsg() );
                    return VLC_EGENERIC;
                }
                p_sys->i_pcr = 0;

                for( i = 0; i < p_sys->i_track; i++ )
                {
                    p_sys->track[i]->b_rtcp_sync = false;
                    p_sys->track[i]->i_pts = VLC_TS_INVALID;
                }

                /* Retrieve the starttime if possible */
                p_sys->f_npt = p_sys->f_npt_start = p_sys->ms->playStartTime();

                /* Retrieve the duration if possible */
                if( p_sys->ms->playEndTime() > 0 )
                    p_sys->f_npt_length = p_sys->ms->playEndTime();

                msg_Dbg( p_demux, "seek start: %f stop:%f", p_sys->f_npt_start, p_sys->f_npt_length );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
            pb = (bool*)va_arg( args, bool * );
            if( p_sys->rtsp && p_sys->f_npt_length > 0 )
                /* Not always true, but will be handled in SET_PAUSE_STATE */
                *pb = true;
            else
                *pb = false;
            return VLC_SUCCESS;

        case DEMUX_CAN_CONTROL_PACE:
            pb = (bool*)va_arg( args, bool * );

#if 1       /* Disable for now until we have a clock synchro algo
             * which works with something else than MPEG over UDP */
            *pb = false;
#else
            *pb = true;
#endif
            return VLC_SUCCESS;

        case DEMUX_CAN_CONTROL_RATE:
            pb = (bool*)va_arg( args, bool * );
            pb2 = (bool*)va_arg( args, bool * );

            *pb = (p_sys->rtsp != NULL) &&
                    (p_sys->f_npt_length > 0) &&
                    ( !var_GetBool( p_demux, "rtsp-kasenna" ) ||
                      !var_GetBool( p_demux, "rtsp-wmserver" ) );
            *pb2 = false;
            return VLC_SUCCESS;

        case DEMUX_SET_RATE:
        {
            double f_scale, f_old_scale;

            if( !p_sys->rtsp || (p_sys->f_npt_length <= 0) ||
                var_GetBool( p_demux, "rtsp-kasenna" ) ||
                var_GetBool( p_demux, "rtsp-wmserver" ) )
                return VLC_EGENERIC;

            /* According to RFC 2326 p56 chapter 12.35 a RTSP server that
             * supports Scale:
             *
             * "[...] should try to approximate the viewing rate, but
             *  may restrict the range of scale values that it supports.
             *  The response MUST contain the actual scale value chosen
             *  by the server."
             *
             * Scale = 1 indicates normal play
             * Scale > 1 indicates fast forward
             * Scale < 1 && Scale > 0 indicates slow motion
             * Scale < 0 value indicates rewind
             */

            pi_int = (int*)va_arg( args, int * );
            f_scale = (double)INPUT_RATE_DEFAULT / (*pi_int);
            f_old_scale = p_sys->ms->scale();

            /* Passing -1 for the start and end time will mean liveMedia won't
             * create a Range: section for the RTSP message. The server should
             * pick up from the current position */
            p_sys->rtsp->sendPlayCommand( *p_sys->ms, default_live555_callback, -1, -1, f_scale );

            if( !wait_Live555_response( p_demux ) )
            {
                msg_Err( p_demux, "PLAY with Scale %0.2f failed %s", f_scale,
                        p_sys->env->getResultMsg() );
                return VLC_EGENERIC;
            }

            if( p_sys->ms->scale() == f_old_scale )
            {
                msg_Err( p_demux, "no scale change using old Scale %0.2f",
                          p_sys->ms->scale() );
                return VLC_EGENERIC;
            }

            /* ReSync the stream */
            p_sys->f_npt_start = 0;
            p_sys->i_pcr = 0;
            p_sys->f_npt = 0.0;

            *pi_int = (int)( INPUT_RATE_DEFAULT / p_sys->ms->scale() );
            msg_Dbg( p_demux, "PLAY with new Scale %0.2f (%d)", p_sys->ms->scale(), (*pi_int) );
            return VLC_SUCCESS;
        }

        case DEMUX_SET_PAUSE_STATE:
        {
            bool b_pause = (bool)va_arg( args, int );
            if( p_sys->rtsp == NULL )
                return VLC_EGENERIC;

            if( b_pause == p_sys->b_paused )
                return VLC_SUCCESS;
            if( b_pause )
                p_sys->rtsp->sendPauseCommand( *p_sys->ms, default_live555_callback );
            else
                p_sys->rtsp->sendPlayCommand( *p_sys->ms, default_live555_callback, p_sys->f_seek_request,
                                              -1.0f, p_sys->ms->scale() );

            if( !wait_Live555_response( p_demux ) )
            {
                msg_Err( p_demux, "PLAY or PAUSE failed %s", p_sys->env->getResultMsg() );
                return VLC_EGENERIC;
            }
            p_sys->f_seek_request = -1;
            p_sys->b_paused = b_pause;

            /* When we Pause, we'll need the TimeoutPrevention thread to
             * handle sending the "Keep Alive" message to the server.
             * Unfortunately Live555 isn't thread safe and so can't
             * do this normally while the main Demux thread is handling
             * a live stream. We end up with the Timeout thread blocking
             * waiting for a response from the server. So when we PAUSE
             * we set a flag that the TimeoutPrevention function will check
             * and if it's set, it will trigger the GET_PARAMETER message */
            if( p_sys->b_paused && p_sys->p_timeout != NULL )
                p_sys->p_timeout->b_handle_keep_alive = true;
            else if( !p_sys->b_paused && p_sys->p_timeout != NULL )
                p_sys->p_timeout->b_handle_keep_alive = false;

            if( !p_sys->b_paused )
            {
                for( int i = 0; i < p_sys->i_track; i++ )
                {
                    live_track_t *tk = p_sys->track[i];
                    tk->b_rtcp_sync = false;
                    tk->i_pts = VLC_TS_INVALID;
                    p_sys->i_pcr = 0;
                    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
                }
            }

            /* Reset data received counter */
            p_sys->i_no_data_ti = 0;

            /* Retrieve the starttime if possible */
            p_sys->f_npt_start = p_sys->ms->playStartTime();

            /* Retrieve the duration if possible */
            if( p_sys->ms->playEndTime() )
                p_sys->f_npt_length = p_sys->ms->playEndTime();

            msg_Dbg( p_demux, "pause start: %f stop:%f", p_sys->f_npt_start, p_sys->f_npt_length );
            return VLC_SUCCESS;
        }
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_SET_TITLE:
        case DEMUX_SET_SEEKPOINT:
            return VLC_EGENERIC;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = INT64_C(1000)
                  * var_InheritInteger( p_demux, "network-caching" );
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * RollOverTcp: reopen the rtsp into TCP mode
 * XXX: ugly, a lot of code are duplicated from Open()
 * This should REALLY be fixed
 *****************************************************************************/
static int RollOverTcp( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i, i_return;

    var_SetBool( p_demux, "rtsp-tcp", true );

    /* We close the old RTSP session */
    p_sys->rtsp->sendTeardownCommand( *p_sys->ms, NULL );
    Medium::close( p_sys->ms );
    RTSPClient::close( p_sys->rtsp );

    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->b_muxed ) stream_Delete( tk->p_out_muxed );
        if( tk->p_es ) es_out_Del( p_demux->out, tk->p_es );
        if( tk->p_asf_block ) block_Release( tk->p_asf_block );
        es_format_Clean( &tk->fmt );
        free( tk->p_buffer );
        free( tk );
    }
    TAB_CLEAN( p_sys->i_track, p_sys->track );
    if( p_sys->p_out_asf ) stream_Delete( p_sys->p_out_asf );

    p_sys->ms = NULL;
    p_sys->rtsp = NULL;
    p_sys->b_no_data = true;
    p_sys->i_no_data_ti = 0;
    p_sys->p_out_asf = NULL;

    /* Reopen rtsp client */
    if( ( i_return = Connect( p_demux ) ) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Failed to connect with rtsp://%s",
                 p_sys->psz_path );
        goto error;
    }

    if( p_sys->p_sdp == NULL )
    {
        msg_Err( p_demux, "Failed to retrieve the RTSP Session Description" );
        goto error;
    }

    if( ( i_return = SessionsSetup( p_demux ) ) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Nothing to play for rtsp://%s", p_sys->psz_path );
        goto error;
    }

    if( ( i_return = Play( p_demux ) ) != VLC_SUCCESS )
        goto error;

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}


/*****************************************************************************
 *
 *****************************************************************************/
static block_t *StreamParseAsf( demux_t *p_demux, live_track_t *tk,
                                bool b_marker,
                                const uint8_t *p_data, unsigned i_size )
{
    const unsigned i_packet_size = p_demux->p_sys->asfh.i_min_data_packet_size;
    block_t *p_list = NULL;

    while( i_size >= 4 )
    {
        unsigned i_flags = p_data[0];
        unsigned i_length_offset = (p_data[1] << 16) |
                                   (p_data[2] <<  8) |
                                   (p_data[3]      );
        bool b_length = i_flags & 0x40;
        bool b_relative_ts = i_flags & 0x20;
        bool b_duration = i_flags & 0x10;
        bool b_location_id = i_flags & 0x08;

        //msg_Dbg( p_demux, "ASF: marker=%d size=%d : %c=%d id=%d",
        //         b_marker, i_size, b_length ? 'L' : 'O', i_length_offset );
        unsigned i_header_size = 4;
        if( b_relative_ts )
            i_header_size += 4;
        if( b_duration )
            i_header_size += 4;
        if( b_location_id )
            i_header_size += 4;

        if( i_header_size > i_size )
        {
            msg_Warn( p_demux, "Invalid header size" );
            break;
        }

        /* XXX
         * When b_length is true, the streams I found do not seems to respect
         * the documentation.
         * From them, I have failed to find which choice between '__MIN()' or
         * 'i_length_offset - i_header_size' is the right one.
         */
        unsigned i_payload;
        if( b_length )
            i_payload = __MIN( i_length_offset, i_size - i_header_size);
        else
            i_payload = i_size - i_header_size;

        if( !tk->p_asf_block )
        {
            tk->p_asf_block = block_Alloc( i_packet_size );
            if( !tk->p_asf_block )
                break;
            tk->p_asf_block->i_buffer = 0;
        }
        unsigned i_offset  = b_length ? 0 : i_length_offset;
        if( i_offset == tk->p_asf_block->i_buffer && i_offset + i_payload <= i_packet_size )
        {
            memcpy( &tk->p_asf_block->p_buffer[i_offset], &p_data[i_header_size], i_payload );
            tk->p_asf_block->i_buffer += i_payload;
            if( b_marker )
            {
                /* We have a complete packet */
                tk->p_asf_block->i_buffer = i_packet_size;
                block_ChainAppend( &p_list, tk->p_asf_block );
                tk->p_asf_block = NULL;
            }
        }
        else
        {
            /* Reset on broken stream */
            msg_Err( p_demux, "Broken packet detected (%d vs %zu or %d + %d vs %d)",
                     i_offset, tk->p_asf_block->i_buffer, i_offset, i_payload, i_packet_size);
            tk->p_asf_block->i_buffer = 0;
        }

        /* */
        p_data += i_header_size + i_payload;
        i_size -= i_header_size + i_payload;
    }
    return p_list;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void StreamRead( void *p_private, unsigned int i_size,
                        unsigned int i_truncated_bytes, struct timeval pts,
                        unsigned int duration )
{
    VLC_UNUSED( duration );

    live_track_t   *tk = (live_track_t*)p_private;
    demux_t        *p_demux = tk->p_demux;
    demux_sys_t    *p_sys = p_demux->p_sys;
    block_t        *p_block;

    //msg_Dbg( p_demux, "pts: %d", pts.tv_sec );

    int64_t i_pts = (int64_t)pts.tv_sec * INT64_C(1000000) +
        (int64_t)pts.tv_usec;

    /* XXX Beurk beurk beurk Avoid having negative value XXX */
    i_pts &= INT64_C(0x00ffffffffffffff);

    /* Retrieve NPT for this pts */
    tk->f_npt = tk->sub->getNormalPlayTime(pts);

    if( tk->b_quicktime && tk->p_es == NULL )
    {
        QuickTimeGenericRTPSource *qtRTPSource =
            (QuickTimeGenericRTPSource*)tk->sub->rtpSource();
        QuickTimeGenericRTPSource::QTState &qtState = qtRTPSource->qtState;
        uint8_t *sdAtom = (uint8_t*)&qtState.sdAtom[4];

        /* Get codec informations from the quicktime atoms :
         * http://developer.apple.com/quicktime/icefloe/dispatch026.html */
        if( tk->fmt.i_cat == VIDEO_ES ) {
            if( qtState.sdAtomSize < 16 + 32 )
            {
                /* invalid */
                p_sys->event_data = 0xff;
                tk->waiting = 0;
                return;
            }
            tk->fmt.i_codec = VLC_FOURCC(sdAtom[0],sdAtom[1],sdAtom[2],sdAtom[3]);
            tk->fmt.video.i_width  = (sdAtom[28] << 8) | sdAtom[29];
            tk->fmt.video.i_height = (sdAtom[30] << 8) | sdAtom[31];

            if( tk->fmt.i_codec == VLC_FOURCC('a', 'v', 'c', '1') )
            {
                uint8_t *pos = (uint8_t*)qtRTPSource->qtState.sdAtom + 86;
                uint8_t *endpos = (uint8_t*)qtRTPSource->qtState.sdAtom
                                  + qtRTPSource->qtState.sdAtomSize;
                while (pos+8 < endpos) {
                    unsigned int atomLength = pos[0]<<24 | pos[1]<<16 | pos[2]<<8 | pos[3];
                    if( atomLength == 0 || atomLength > (unsigned int)(endpos-pos)) break;
                    if( memcmp(pos+4, "avcC", 4) == 0 &&
                        atomLength > 8 &&
                        atomLength <= INT_MAX )
                    {
                        tk->fmt.i_extra = atomLength-8;
                        tk->fmt.p_extra = xmalloc( tk->fmt.i_extra );
                        memcpy(tk->fmt.p_extra, pos+8, atomLength-8);
                        break;
                    }
                    pos += atomLength;
                }
            }
            else
            {
                tk->fmt.i_extra        = qtState.sdAtomSize - 16;
                tk->fmt.p_extra        = xmalloc( tk->fmt.i_extra );
                memcpy( tk->fmt.p_extra, &sdAtom[12], tk->fmt.i_extra );
            }
        }
        else {
            if( qtState.sdAtomSize < 24 )
            {
                /* invalid */
                p_sys->event_data = 0xff;
                tk->waiting = 0;
                return;
            }
            tk->fmt.i_codec = VLC_FOURCC(sdAtom[0],sdAtom[1],sdAtom[2],sdAtom[3]);
            tk->fmt.audio.i_bitspersample = (sdAtom[22] << 8) | sdAtom[23];
        }
        tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
    }

#if 0
    fprintf( stderr, "StreamRead size=%d pts=%lld\n",
             i_size,
             pts.tv_sec * 1000000LL + pts.tv_usec );
#endif

    /* grow buffer if it looks like buffer is too small, but don't eat
     * up all the memory on strange streams */
    if( i_truncated_bytes > 0 )
    {
        if( tk->i_buffer < 2000000 )
        {
            void *p_tmp;
            msg_Dbg( p_demux, "lost %d bytes", i_truncated_bytes );
            msg_Dbg( p_demux, "increasing buffer size to %d", tk->i_buffer * 2 );
            p_tmp = realloc( tk->p_buffer, tk->i_buffer * 2 );
            if( p_tmp == NULL )
            {
                msg_Warn( p_demux, "realloc failed" );
            }
            else
            {
                tk->p_buffer = (uint8_t*)p_tmp;
                tk->i_buffer *= 2;
            }
        }

        if( tk->b_discard_trunc )
        {
            p_sys->event_data = 0xff;
            tk->waiting = 0;
            return;
        }
    }

    assert( i_size <= tk->i_buffer );

    if( tk->fmt.i_codec == VLC_CODEC_AMR_NB ||
        tk->fmt.i_codec == VLC_CODEC_AMR_WB )
    {
        AMRAudioSource *amrSource = (AMRAudioSource*)tk->sub->readSource();

        p_block = block_Alloc( i_size + 1 );
        p_block->p_buffer[0] = amrSource->lastFrameHeader();
        memcpy( p_block->p_buffer + 1, tk->p_buffer, i_size );
    }
    else if( tk->fmt.i_codec == VLC_CODEC_H261 )
    {
        H261VideoRTPSource *h261Source = (H261VideoRTPSource*)tk->sub->rtpSource();
        uint32_t header = h261Source->lastSpecialHeader();
        p_block = block_Alloc( i_size + 4 );
        memcpy( p_block->p_buffer, &header, 4 );
        memcpy( p_block->p_buffer + 4, tk->p_buffer, i_size );

        if( tk->sub->rtpSource()->curPacketMarkerBit() )
            p_block->i_flags |= BLOCK_FLAG_END_OF_FRAME;
    }
    else if( tk->fmt.i_codec == VLC_CODEC_H264 )
    {
        if( (tk->p_buffer[0] & 0x1f) >= 24 )
            msg_Warn( p_demux, "unsupported NAL type for H264" );

        /* Normal NAL type */
        p_block = block_Alloc( i_size + 4 );
        p_block->p_buffer[0] = 0x00;
        p_block->p_buffer[1] = 0x00;
        p_block->p_buffer[2] = 0x00;
        p_block->p_buffer[3] = 0x01;
        memcpy( &p_block->p_buffer[4], tk->p_buffer, i_size );
    }
    else if( tk->b_asf )
    {
        p_block = StreamParseAsf( p_demux, tk,
                                  tk->sub->rtpSource()->curPacketMarkerBit(),
                                  tk->p_buffer, i_size );
    }
    else
    {
        p_block = block_Alloc( i_size );
        memcpy( p_block->p_buffer, tk->p_buffer, i_size );
    }

    if( p_sys->i_pcr < i_pts )
    {
        p_sys->i_pcr = i_pts;
    }

    /* Update our global npt value */
    if( tk->f_npt > 0 &&
        ( tk->f_npt < p_sys->f_npt_length || p_sys->f_npt_length <= 0 ) )
        p_sys->f_npt = tk->f_npt;

    if( p_block )
    {
        if( !tk->b_muxed && !tk->b_asf )
        {
            if( i_pts != tk->i_pts )
                p_block->i_pts = VLC_TS_0 + i_pts;
            /*FIXME: for h264 you should check that packetization-mode=1 in sdp-file */
            p_block->i_dts = ( tk->fmt.i_codec == VLC_CODEC_MPGV ) ? VLC_TS_INVALID : (VLC_TS_0 + i_pts);
        }

        if( tk->b_muxed )
            stream_DemuxSend( tk->p_out_muxed, p_block );
        else if( tk->b_asf )
            stream_DemuxSend( p_sys->p_out_asf, p_block );
        else
            es_out_Send( p_demux->out, tk->p_es, p_block );
    }

    /* warn that's ok */
    p_sys->event_data = 0xff;

    /* we have read data */
    tk->waiting = 0;
    p_demux->p_sys->b_no_data = false;
    p_demux->p_sys->i_no_data_ti = 0;

    if( i_pts > 0 && !tk->b_muxed )
    {
        tk->i_pts = i_pts;
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static void StreamClose( void *p_private )
{
    live_track_t   *tk = (live_track_t*)p_private;
    demux_t        *p_demux = tk->p_demux;
    demux_sys_t    *p_sys = p_demux->p_sys;
    tk->b_selected = false;
    p_sys->event_rtsp = 0xff;
    p_sys->event_data = 0xff;

    if( tk->p_es )
        es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, tk->p_es, false );

    int nb_tracks = 0;
    for( int i = 0; i < p_sys->i_track; i++ )
    {
        if( p_sys->track[i]->b_selected )
            nb_tracks++;
    }
    msg_Dbg( p_demux, "RTSP track Close, %d track remaining", nb_tracks );
    if( !nb_tracks )
        p_sys->b_error = true;
}


/*****************************************************************************
 *
 *****************************************************************************/
static void TaskInterruptRTSP( void *p_private )
{
    demux_t *p_demux = (demux_t*)p_private;

    /* Avoid lock */
    p_demux->p_sys->event_rtsp = 0xff;
}

static void TaskInterruptData( void *p_private )
{
    demux_t *p_demux = (demux_t*)p_private;

    p_demux->p_sys->i_no_data_ti++;

    /* Avoid lock */
    p_demux->p_sys->event_data = 0xff;
}

/*****************************************************************************
 *
 *****************************************************************************/
VLC_NORETURN
static void* TimeoutPrevention( void *p_data )
{
    timeout_thread_t *p_timeout = (timeout_thread_t *)p_data;

    for( ;; )
    {
        /* Voodoo (= no) thread safety here! *Ahem* */
        if( p_timeout->b_handle_keep_alive )
        {
            char *psz_bye = NULL;
            int canc = vlc_savecancel ();

            p_timeout->p_sys->rtsp->sendGetParameterCommand( *p_timeout->p_sys->ms, NULL, psz_bye );
            vlc_restorecancel (canc);
        }
        p_timeout->p_sys->b_timeout_call = !p_timeout->b_handle_keep_alive;

        msleep (((int64_t)p_timeout->p_sys->i_timeout - 2) * CLOCK_FREQ);
    }
    assert(0); /* dead code */
}

/*****************************************************************************
 *
 *****************************************************************************/
static int ParseASF( demux_t *p_demux )
{
    demux_sys_t    *p_sys = p_demux->p_sys;

    const char *psz_marker = "a=pgmpu:data:application/vnd.ms.wms-hdr.asfv1;base64,";
    char *psz_asf = strcasestr( p_sys->p_sdp, psz_marker );
    char *psz_end;
    block_t *p_header;

    /* Parse the asf header */
    if( psz_asf == NULL )
        return VLC_EGENERIC;

    psz_asf += strlen( psz_marker );
    psz_asf = strdup( psz_asf );    /* Duplicate it */
    psz_end = strchr( psz_asf, '\n' );

    while( psz_end > psz_asf && ( *psz_end == '\n' || *psz_end == '\r' ) )
        *psz_end-- = '\0';

    if( psz_asf >= psz_end )
    {
        free( psz_asf );
        return VLC_EGENERIC;
    }

    /* Always smaller */
    p_header = block_Alloc( psz_end - psz_asf );
    p_header->i_buffer = vlc_b64_decode_binary_to_buffer( p_header->p_buffer,
                                               p_header->i_buffer, psz_asf );
    //msg_Dbg( p_demux, "Size=%d Hdrb64=%s", p_header->i_buffer, psz_asf );
    if( p_header->i_buffer <= 0 )
    {
        free( psz_asf );
        return VLC_EGENERIC;
    }

    /* Parse it to get packet size */
    asf_HeaderParse( &p_sys->asfh, p_header->p_buffer, p_header->i_buffer );

    /* Send it to demuxer */
    stream_DemuxSend( p_sys->p_out_asf, p_header );

    free( psz_asf );
    return VLC_SUCCESS;
}


static unsigned char* parseH264ConfigStr( char const* configStr,
                                          unsigned int& configSize )
{
    char *dup, *psz;
    size_t i_records = 1;

    configSize = 0;

    if( configStr == NULL || *configStr == '\0' )
        return NULL;

    psz = dup = strdup( configStr );

    /* Count the number of commas */
    for( psz = dup; *psz != '\0'; ++psz )
    {
        if( *psz == ',')
        {
            ++i_records;
            *psz = '\0';
        }
    }

    size_t configMax = 5*strlen(dup);
    unsigned char *cfg = new unsigned char[configMax];
    psz = dup;
    for( size_t i = 0; i < i_records; ++i )
    {
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x01;

        configSize += vlc_b64_decode_binary_to_buffer( cfg+configSize,
                                          configMax-configSize, psz );
        psz += strlen(psz)+1;
    }

    free( dup );
    return cfg;
}

static uint8_t *parseVorbisConfigStr( char const* configStr,
                                      unsigned int& configSize )
{
    configSize = 0;
    if( configStr == NULL || *configStr == '\0' )
        return NULL;
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1332115200 // 2012.03.20
    unsigned char *p_cfg = base64Decode( configStr, configSize );
#else
    char* configStr_dup = strdup( configStr );
    unsigned char *p_cfg = base64Decode( configStr_dup, configSize );
    free( configStr_dup );
#endif
    uint8_t *p_extra = NULL;
    /* skip header count, ident number and length (cf. RFC 5215) */
    const unsigned int headerSkip = 9;
    if( configSize > headerSkip && ((uint8_t*)p_cfg)[3] == 1 )
    {
        configSize -= headerSkip;
        p_extra = (uint8_t*)xmalloc( configSize );
        memcpy( p_extra, p_cfg+headerSkip, configSize );
    }
    delete[] p_cfg;
    return p_extra;
}

