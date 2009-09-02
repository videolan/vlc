/*****************************************************************************
 * live555.cpp : LIVE555 Streaming Media support.
 *****************************************************************************
 * Copyright (C) 2003-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan. org>
 *          Derk-Jan Hartman <djhartman at m2x .dot. nl> for M2X
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

/* For inttypes.h
 * Note: config.h may include inttypes.h, so make sure we define this option
 * early enough. */
#define __STDC_CONSTANT_MACROS 1

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

#include <iostream>
#include <limits.h>
#include <assert.h>


#if defined( WIN32 )
#   include <winsock2.h>
#endif

#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>

extern "C" {
#include "../access/mms/asf.h"  /* Who said ugly ? */
}

using namespace std;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value (ms)")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for RTSP streams. This " \
    "value should be set in millisecond units." )

#define KASENNA_TEXT N_( "Kasenna RTSP dialect")
#define KASENNA_LONGTEXT N_( "Kasenna servers use an old and nonstandard " \
    "dialect of RTSP. With this parameter VLC will try this dialect, but "\
    "then it cannot connect to normal RTSP servers." )

#define WMSERVER_TEXT N_("WMServer RTSP dialect")
#define WMSERVER_LONGTEXT N_("WMServer uses an unstandard dialect " \
    "of RTSP. Selecting this parameter will tell VLC to assume some " \
    "options contrary to RFC 2326 guidelines.")

#define USER_TEXT N_("RTSP user name")
#define USER_LONGTEXT N_("Sets the username for the connection, " \
    "if no username or password are set in the url.")
#define PASS_TEXT N_("RTSP password")
#define PASS_LONGTEXT N_("Sets the password for the connection, " \
    "if no username or password are set in the url.")

vlc_module_begin ()
    set_description( N_("RTP/RTSP/SDP demuxer (using Live555)" ) )
    set_capability( "demux", 50 )
    set_shortname( "RTP/RTSP")
    set_callbacks( Open, Close )
    add_shortcut( "live" )
    add_shortcut( "livedotcom" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_submodule ()
        set_description( N_("RTSP/RTP access and demux") )
        add_shortcut( "rtsp" )
        add_shortcut( "sdp" )
        add_shortcut( "live" )
        add_shortcut( "livedotcom" )
        set_capability( "access_demux", 0 )
        set_callbacks( Open, Close )
        add_bool( "rtsp-tcp", false, NULL,
                  N_("Use RTP over RTSP (TCP)"),
                  N_("Use RTP over RTSP (TCP)"), true )
            change_safe()
        add_integer( "rtp-client-port", -1, NULL,
                  N_("Client port"),
                  N_("Port to use for the RTP source of the session"), true )
        add_bool( "rtsp-mcast", false, NULL,
                  N_("Force multicast RTP via RTSP"),
                  N_("Force multicast RTP via RTSP"), true )
            change_safe()
        add_bool( "rtsp-http", false, NULL,
                  N_("Tunnel RTSP and RTP over HTTP"),
                  N_("Tunnel RTSP and RTP over HTTP"), true )
            change_safe()
        add_integer( "rtsp-http-port", 80, NULL,
                  N_("HTTP tunnel port"),
                  N_("Port to use for tunneling the RTSP/RTP over HTTP."),
                  true )
        add_integer("rtsp-caching", 4 * DEFAULT_PTS_DELAY / 1000, NULL,
                    CACHING_TEXT, CACHING_LONGTEXT, true )
            change_safe()
        add_bool(   "rtsp-kasenna", false, NULL, KASENNA_TEXT,
                    KASENNA_LONGTEXT, true )
        add_bool(   "rtsp-wmserver", false, NULL, WMSERVER_TEXT,
                    WMSERVER_LONGTEXT, true)
            change_safe()
        add_string( "rtsp-user", NULL, NULL, USER_TEXT,
                    USER_LONGTEXT, true )
            change_safe()
        add_password( "rtsp-pwd", NULL, NULL, PASS_TEXT,
                      PASS_LONGTEXT, true )
            change_safe()
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
    bool            b_discard_trunc;
    stream_t        *p_out_muxed;    /* for muxed stream */

    uint8_t         *p_buffer;
    unsigned int    i_buffer;

    bool            b_rtcp_sync;
    char            waiting;
    int64_t         i_pts;
    float           i_npt;

} live_track_t;

struct timeout_thread_t
{
    demux_sys_t  *p_sys;
    vlc_thread_t handle;
    bool         b_handle_keep_alive;
};

struct demux_sys_t
{
    char            *p_sdp;    /* XXX mallocated */
    char            *psz_path; /* URL-encoded path */
    vlc_url_t       url;

    MediaSession     *ms;
    TaskScheduler    *scheduler;
    UsageEnvironment *env ;
    RTSPClient       *rtsp;

    /* */
    int              i_track;
    live_track_t     **track;   /* XXX mallocated */

    /* Weird formats */
    asf_header_t     asfh;
    stream_t         *p_out_asf;
    bool             b_real;

    /* */
    int64_t          i_pcr; /* The clock */
    float            i_npt;
    float            i_npt_length;
    float            i_npt_start;

    /* timeout thread information */
    int              i_timeout;     /* session timeout value in seconds */
    bool             b_timeout_call;/* mark to send an RTSP call to prevent server timeout */
    timeout_thread_t *p_timeout;    /* the actual thread that makes sure we don't timeout */

    /* */
    bool             b_force_mcast;
    bool             b_multicast;   /* if one of the tracks is multicasted */
    bool             b_no_data;     /* if we never received any data */
    int              i_no_data_ti;  /* consecutive number of TaskInterrupt */

    char             event;

    bool             b_get_param;   /* Does the server support GET_PARAMETER */
    bool             b_paused;      /* Are we paused? */
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
static void TaskInterrupt( void * );

static void* TimeoutPrevention( void * );

static unsigned char* parseH264ConfigStr( char const* configStr,
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
    else
    {
        var_Create( p_demux, "rtsp-caching", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    }

    p_demux->pf_demux  = Demux;
    p_demux->pf_control= Control;
    p_demux->p_sys     = p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;

    p_sys->p_sdp = NULL;
    p_sys->scheduler = NULL;
    p_sys->env = NULL;
    p_sys->ms = NULL;
    p_sys->rtsp = NULL;
    p_sys->i_track = 0;
    p_sys->track   = NULL;
    p_sys->i_pcr = 0;
    p_sys->i_npt = 0.;
    p_sys->i_npt_start = 0.;
    p_sys->i_npt_length = 0.;
    p_sys->p_out_asf = NULL;
    p_sys->b_no_data = true;
    p_sys->i_no_data_ti = 0;
    p_sys->p_timeout = NULL;
    p_sys->i_timeout = 0;
    p_sys->b_timeout_call = false;
    p_sys->b_multicast = false;
    p_sys->b_real = false;
    p_sys->psz_path = strdup( p_demux->psz_path );
    p_sys->b_force_mcast = var_CreateGetBool( p_demux, "rtsp-mcast" );
    p_sys->b_get_param = false;
    p_sys->b_paused = false;

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

            if( !vlc_object_alive (p_demux) || p_demux->b_error )
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
            p_sdp = (uint8_t*)realloc( p_sdp, i_sdp_max );
        }
        p_sys->p_sdp = (char*)p_sdp;
    }
    else if( ( p_demux->s == NULL ) &&
             !strcasecmp( p_demux->psz_access, "sdp" ) )
    {
        /* sdp:// link from SAP */
        p_sys->p_sdp = strdup( p_sys->psz_path );
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
    int i;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->b_muxed ) stream_Delete( tk->p_out_muxed );
        es_format_Clean( &tk->fmt );
        free( tk->p_buffer );
        free( tk );
    }

    if( p_sys->i_track ) free( p_sys->track );
    if( p_sys->p_out_asf ) stream_Delete( p_sys->p_out_asf );
    if( p_sys->rtsp && p_sys->ms ) p_sys->rtsp->teardownMediaSession( *p_sys->ms );
    if( p_sys->p_timeout )
    {
        vlc_cancel( p_sys->p_timeout->handle );
        vlc_join( p_sys->p_timeout->handle, NULL );
        free( p_sys->p_timeout );
    }
    if( p_sys->ms ) Medium::close( p_sys->ms );
    if( p_sys->rtsp ) RTSPClient::close( p_sys->rtsp );
    if( p_sys->env ) p_sys->env->reclaim();
    delete p_sys->scheduler;
    free( p_sys->p_sdp );
    free( p_sys->psz_path );

    vlc_UrlClean( &p_sys->url );

    free( p_sys );
}

static inline const char *strempty( const char *s ) { return s?s:""; }
static inline Boolean toBool( bool b ) { return b?True:False; } // silly, no?

/*****************************************************************************
 * Connect: connects to the RTSP server to setup the session DESCRIBE
 *****************************************************************************/
static int Connect( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    Authenticator authenticator;
    bool b_firstpass  = true;
    char *psz_user    = NULL;
    char *psz_pwd     = NULL;
    char *psz_url     = NULL;
    char *psz_options = NULL;
    char *p_sdp       = NULL;
    int  i_http_port  = 0;
    int  i_ret        = VLC_SUCCESS;

    /* Create the url using the port number if available */
    if( p_sys->url.i_port == 0 )
    {
        p_sys->url.i_port = 554;
        if( asprintf( &psz_url, "rtsp://%s", p_sys->psz_path ) == -1 )
            return VLC_ENOMEM;
    }
    else
    {
        if( asprintf( &psz_url, "rtsp://%s:%d%s", p_sys->url.psz_host,
                      p_sys->url.i_port, p_sys->url.psz_path ) == -1 )
            return VLC_ENOMEM;
    }

    /* Get the user name and password */
    if( p_sys->url.psz_username || p_sys->url.psz_password )
    {
        psz_user = strdup( strempty( p_sys->url.psz_username ) );
        psz_pwd  = strdup( strempty( p_sys->url.psz_password ) );
    }
    else
    {
        psz_user = var_CreateGetString( p_demux, "rtsp-user" );
        psz_pwd  = var_CreateGetString( p_demux, "rtsp-pwd" );
    }

createnew:
    if( !vlc_object_alive (p_demux) || p_demux->b_error )
    {
        i_ret = VLC_EGENERIC;
        goto bailout;
    }

    if( var_CreateGetBool( p_demux, "rtsp-http" ) )
        i_http_port = var_CreateGetInteger( p_demux, "rtsp-http-port" );

    if( ( p_sys->rtsp = RTSPClient::createNew( *p_sys->env,
          var_CreateGetInteger( p_demux, "verbose" ) > 1,
          "VLC media player", i_http_port ) ) == NULL )
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
    if( var_CreateGetBool( p_demux, "rtsp-kasenna" ))
    {
        p_sys->rtsp->setUserAgentString( "VLC_MEDIA_PLAYER_KA" );
    }

describe:
    authenticator.setUsernameAndPassword( psz_user, psz_pwd );

    /* */
    { /* i_timeout hack scope */
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1223337600
    const int i_timeout = var_CreateGetInteger(p_demux, "ipv4-timeout") / 1000;
    psz_options = p_sys->rtsp->sendOptionsCmd( psz_url, psz_user, psz_pwd,
                                               &authenticator, i_timeout );
#else
    psz_options = p_sys->rtsp->sendOptionsCmd( psz_url, psz_user, psz_pwd,
                                               &authenticator );
#endif
    if( psz_options == NULL && authenticator.realm() != NULL )
    {
        // try again, with the realm set this time
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1223337600
        psz_options = p_sys->rtsp->sendOptionsCmd( psz_url, psz_user, psz_pwd,
                                               &authenticator, i_timeout );
#else
        psz_options = p_sys->rtsp->sendOptionsCmd( psz_url, psz_user, psz_pwd,
                                               &authenticator );
#endif
    }
    if( psz_options )
        p_sys->b_get_param = (bool)strstr( psz_options, "GET_PARAMETER" );
    delete [] psz_options;

    if( var_CreateGetBool( p_demux, "rtsp-wmserver" ) )
       p_sys->b_get_param = true;

#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1223337600
    p_sdp = p_sys->rtsp->describeWithPassword( psz_url, psz_user, psz_pwd,
                          var_GetBool( p_demux, "rtsp-kasenna" ), i_timeout );
#else
    p_sdp = p_sys->rtsp->describeWithPassword( psz_url, psz_user, psz_pwd,
                                     var_GetBool( p_demux, "rtsp-kasenna" ) );
#endif
    } /* i_timeout scope end */

    if( p_sdp == NULL )
    {
        /* failure occurred */
        int i_code = 0;
        const char *psz_error = p_sys->env->getResultMsg();

        if( var_GetBool( p_demux, "rtsp-http" ) )
            sscanf( psz_error, "%*s %*s HTTP GET %*s HTTP/%*u.%*u %3u %*s",
                    &i_code );
        else
        {
            const char *psz_tmp = strstr( psz_error, "RTSP" );
            if( psz_tmp )
                sscanf( psz_tmp, "RTSP/%*s%3u", &i_code );
            else
                i_code = 0;
        }
        msg_Dbg( p_demux, "DESCRIBE failed with %d: %s", i_code, psz_error );

        if( b_firstpass )
        {   /* describeURL always returns an "RTSP/1.0 401 Unauthorized" the
             * first time. This is a workaround to avoid asking for a
             * user/passwd the first time the code passess here. */
            i_code = 0;
            b_firstpass = false;
        }

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
        else if( (i_code != 0) && !var_GetBool( p_demux, "rtsp-http" ) )
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
            msg_Dbg( p_demux, "connection timeout" );
            if( p_sys->rtsp ) RTSPClient::close( p_sys->rtsp );
            p_sys->rtsp = NULL;
        }
        i_ret = VLC_EGENERIC;
    }

    free( p_sys->p_sdp );
    p_sys->p_sdp = NULL;
    if( p_sdp ) p_sys->p_sdp = strdup( (char*)p_sdp );
    delete[] p_sdp;

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

    bool           b_rtsp_tcp = false;
    int            i_client_port;
    int            i_return = VLC_SUCCESS;
    unsigned int   i_buffer = 0;
    unsigned const thresh = 200000; /* RTP reorder threshold .2 second (default .1) */

    b_rtsp_tcp    = var_CreateGetBool( p_demux, "rtsp-tcp" ) ||
                    var_GetBool( p_demux, "rtsp-http" );
    i_client_port = var_CreateGetInteger( p_demux, "rtp-client-port" );

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

        if( !vlc_object_alive (p_demux) || p_demux->b_error )
        {
            delete iter;
            return VLC_EGENERIC;
        }

        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
            i_buffer = 100000;
        else if( !strcmp( sub->mediumName(), "video" ) )
            i_buffer = 2000000;
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
            bInit = sub->initiate( 4 ); /* Constant ? */
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
                if( i_buffer > 0 )
                    increaseReceiveBufferTo( *p_sys->env, fd, i_buffer );

                /* Increase the RTP reorder timebuffer just a bit */
                sub->rtpSource()->setPacketReorderingThresholdTime(thresh);
            }
            msg_Dbg( p_demux, "RTP subsession '%s/%s'", sub->mediumName(),
                     sub->codecName() );

            /* Issue the SETUP */
            if( p_sys->rtsp )
            {
                if( !p_sys->rtsp->setupMediaSubsession( *sub, False,
                                                        toBool( b_rtsp_tcp ),
                             toBool( p_sys->b_force_mcast && !b_rtsp_tcp ) ) )
                {
                    /* if we get an unsupported transport error, toggle TCP
                     * use and try again */
                    if( !strstr(p_sys->env->getResultMsg(),
                                "461 Unsupported Transport")
                        || !p_sys->rtsp->setupMediaSubsession( *sub, False,
                                               toBool( b_rtsp_tcp ), False ) )
                    {
                        msg_Err( p_demux, "SETUP of'%s/%s' failed %s",
                                 sub->mediumName(), sub->codecName(),
                                 p_sys->env->getResultMsg() );
                        continue;
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
            tk->b_muxed     = false;
            tk->b_discard_trunc = false;
            tk->p_out_muxed = NULL;
            tk->waiting     = 0;
            tk->b_rtcp_sync = false;
            tk->i_pts       = 0;
            tk->i_npt       = 0.;
            tk->i_buffer    = 65536;
            tk->p_buffer    = (uint8_t *)malloc( 65536 );
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
                    tk->fmt.i_codec = VLC_FOURCC( 't', 'w', 'o', 's' );
                    tk->fmt.audio.i_bitspersample = 16;
                }
                else if( !strcmp( sub->codecName(), "L8" ) )
                {
                    tk->fmt.i_codec = VLC_FOURCC( 'a', 'r', 'a', 'w' );
                    tk->fmt.audio.i_bitspersample = 8;
                }
                else if( !strcmp( sub->codecName(), "PCMU" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_MULAW;
                }
                else if( !strcmp( sub->codecName(), "PCMA" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_ALAW;
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
                        tk->fmt.p_extra = malloc( i_extra );
                        memcpy( tk->fmt.p_extra, p_extra, i_extra );
                        delete[] p_extra;
                    }
                    /* Because the "faad" decoder does not handle the LATM
                     * data length field at the start of each returned LATM
                     * frame, tell the RTP source to omit it. */
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
                        tk->fmt.p_extra = malloc( i_extra );
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
                    if ( sub->rtpTimestampFrequency() )
                        tk->fmt.audio.i_rate = sub->rtpTimestampFrequency();
                    else
                    {
                        msg_Warn( p_demux,"Using 8kHz as default sample rate." );
                        tk->fmt.audio.i_rate = 8000;
                    }
                }
            }
            else if( !strcmp( sub->mediumName(), "video" ) )
            {
                es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('u','n','d','f') );
                if( !strcmp( sub->codecName(), "MPV" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_MPGV;
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
                        tk->fmt.p_extra = malloc( i_extra );
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
                        tk->fmt.p_extra = malloc( i_extra );
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
            }

            if( !tk->b_quicktime && !tk->b_muxed && !tk->b_asf )
            {
                tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
            }

            if( sub->rtcpInstance() != NULL )
            {
                sub->rtcpInstance()->setByeHandler( StreamClose, tk );
            }

            if( tk->p_es || tk->b_quicktime || tk->b_muxed || tk->b_asf )
            {
                /* Append */
                p_sys->track = (live_track_t**)realloc( p_sys->track, sizeof( live_track_t ) * ( p_sys->i_track + 1 ) );
                p_sys->track[p_sys->i_track++] = tk;
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
    p_sys->i_npt_start = p_sys->ms->playStartTime();

    /* Retrieve the duration if possible */
    p_sys->i_npt_length = p_sys->ms->playEndTime();

    /* */
    msg_Dbg( p_demux, "setup start: %f stop:%f", p_sys->i_npt_start, p_sys->i_npt_length );

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
        if( !p_sys->rtsp->playMediaSession( *p_sys->ms, p_sys->i_npt_start, -1, 1 ) )
        {
            msg_Err( p_demux, "RTSP PLAY failed %s", p_sys->env->getResultMsg() );
            return VLC_EGENERIC;
        }

        /* Retrieve the timeout value and set up a timeout prevention thread */
        p_sys->i_timeout = p_sys->rtsp->sessionTimeoutParameter();
        if( p_sys->i_timeout <= 0 )
            p_sys->i_timeout = 60; /* default value from RFC2326 */

        /* start timeout-thread only if GET_PARAMETER is supported by the server */
        if( !p_sys->p_timeout && p_sys->b_get_param )
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
    p_sys->i_npt_start = p_sys->ms->playStartTime();
    if( p_sys->ms->playEndTime() > 0 )
        p_sys->i_npt_length = p_sys->ms->playEndTime();

    msg_Dbg( p_demux, "play start: %f stop:%f", p_sys->i_npt_start, p_sys->i_npt_length );
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
        p_sys->rtsp->getMediaSessionParameter( *p_sys->ms, NULL, psz_bye );
        p_sys->b_timeout_call = false;
    }

    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

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
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_pcr );
    }

    /* First warn we want to read data */
    p_sys->event = 0;
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
    task = p_sys->scheduler->scheduleDelayedTask( 300000, TaskInterrupt, p_demux );

    /* Do the read */
    p_sys->scheduler->doEventLoop( &p_sys->event );

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
            tk->i_pts = 0;
            tk->i_npt = 0.;
            p_sys->i_pcr = 0;
            p_sys->i_npt = 0.;
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
    return p_demux->b_error ? 0 : 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t *pi64, i64;
    double  *pf, f;
    bool *pb, *pb2, b_bool;
    int *pi_int;

    switch( i_query )
    {
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_npt > 0 )
            {
                *pi64 = (int64_t)(p_sys->i_npt * 1000000.);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_npt_length > 0 )
            {
                *pi64 = (int64_t)((double)p_sys->i_npt_length * 1000000.0);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double* );
            if( (p_sys->i_npt_length > 0) && (p_sys->i_npt > 0) )
            {
                *pf = ( (double)p_sys->i_npt / (double)p_sys->i_npt_length );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_POSITION:
        case DEMUX_SET_TIME:
            if( p_sys->rtsp && (p_sys->i_npt_length > 0) )
            {
                int i;
                float time;

                if( (i_query == DEMUX_SET_TIME) && (p_sys->i_npt > 0) )
                {
                    i64 = (int64_t)va_arg( args, int64_t );
                    time = (float)((double)i64 / (double)1000000.0); /* in second */
                }
                else if( i_query == DEMUX_SET_TIME )
                    return VLC_EGENERIC;
                else
                {
                    f = (double)va_arg( args, double );
                    time = f * (double)p_sys->i_npt_length;   /* in second */
                }

                if( !p_sys->b_paused && !p_sys->rtsp->pauseMediaSession( *p_sys->ms ))
                {
                    msg_Err( p_demux, "PAUSE before seek failed failed %s",
                        p_sys->env->getResultMsg() );
                    return VLC_EGENERIC;
                }
                if( !p_sys->rtsp->playMediaSession( *p_sys->ms, time, -1, 1 ) )
                {
                    msg_Err( p_demux, "seek PLAY failed %s",
                        p_sys->env->getResultMsg() );
                    return VLC_EGENERIC;
                }
                p_sys->i_pcr = 0;

                /* Retrieve RTP-Info values */
                for( i = 0; i < p_sys->i_track; i++ )
                {
                    p_sys->track[i]->b_rtcp_sync = false;
                    p_sys->track[i]->i_pts = 0;
                }

                /* Retrieve the starttime if possible */
                p_sys->i_npt = p_sys->i_npt_start = p_sys->ms->playStartTime();

                /* Retrieve the duration if possible */
                if( p_sys->ms->playEndTime() > 0 )
                    p_sys->i_npt_length = p_sys->ms->playEndTime();

                msg_Dbg( p_demux, "seek start: %f stop:%f", p_sys->i_npt_start, p_sys->i_npt_length );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
            pb = (bool*)va_arg( args, bool * );
            if( p_sys->rtsp && p_sys->i_npt_length > 0 )
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
                    (p_sys->i_npt_length > 0) &&
                    ( !var_GetBool( p_demux, "rtsp-kasenna" ) ||
                      !var_GetBool( p_demux, "rtsp-wmserver" ) );
            *pb2 = false;
            return VLC_SUCCESS;

        case DEMUX_SET_RATE:
        {
            double f_scale, f_old_scale;

            if( !p_sys->rtsp || (p_sys->i_npt_length <= 0) ||
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
            if( !p_sys->rtsp->playMediaSession( *p_sys->ms, -1, -1, f_scale ) )
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
            p_sys->i_npt_start = 0;
            p_sys->i_pcr = 0;
            p_sys->i_npt = 0.0;

            *pi_int = (int)( INPUT_RATE_DEFAULT / p_sys->ms->scale() );
            msg_Dbg( p_demux, "PLAY with new Scale %0.2f (%d)", p_sys->ms->scale(), (*pi_int) );
            return VLC_SUCCESS;
        }

        case DEMUX_SET_PAUSE_STATE:
        {
            int i;
            bool b_pause = (bool)va_arg( args, int );
            if( p_sys->rtsp == NULL )
                return VLC_EGENERIC;

            if( b_pause == p_sys->b_paused )
                return VLC_SUCCESS;
            if( ( b_pause && !p_sys->rtsp->pauseMediaSession( *p_sys->ms ) ) ||
                    ( !b_pause && !p_sys->rtsp->playMediaSession( *p_sys->ms,
                       -1 ) ) )
            {
                    msg_Err( p_demux, "PLAY or PAUSE failed %s", p_sys->env->getResultMsg() );
                    return VLC_EGENERIC;
            }
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

            for( i = 0; !b_bool && i < p_sys->i_track; i++ )
            {
                live_track_t *tk = p_sys->track[i];
                tk->b_rtcp_sync = false;
                tk->i_pts = 0;
                p_sys->i_pcr = 0;
                es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
            }

            /* Reset data received counter */
            p_sys->i_no_data_ti = 0;

            /* Retrieve the starttime if possible */
            p_sys->i_npt_start = p_sys->ms->playStartTime();

            /* Retrieve the duration if possible */
            if( p_sys->ms->playEndTime() )
                p_sys->i_npt_length = p_sys->ms->playEndTime();

            msg_Dbg( p_demux, "pause start: %f stop:%f", p_sys->i_npt_start, p_sys->i_npt_length );
            return VLC_SUCCESS;
        }
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_SET_TITLE:
        case DEMUX_SET_SEEKPOINT:
            return VLC_EGENERIC;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = (int64_t)var_GetInteger( p_demux, "rtsp-caching" ) * 1000;
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
    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->b_muxed ) stream_Delete( tk->p_out_muxed );
        if( tk->p_es ) es_out_Del( p_demux->out, tk->p_es );
        es_format_Clean( &tk->fmt );
        free( tk->p_buffer );
        free( tk );
    }
    if( p_sys->i_track ) free( p_sys->track );
    if( p_sys->p_out_asf ) stream_Delete( p_sys->p_out_asf );

    p_sys->rtsp->teardownMediaSession( *p_sys->ms );
    Medium::close( p_sys->ms );
    RTSPClient::close( p_sys->rtsp );

    p_sys->ms = NULL;
    p_sys->rtsp = NULL;
    p_sys->track = NULL;
    p_sys->i_track = 0;
    p_sys->b_no_data = true;
    p_sys->i_no_data_ti = 0;

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
static void StreamRead( void *p_private, unsigned int i_size,
                        unsigned int i_truncated_bytes, struct timeval pts,
                        unsigned int duration )
{
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
    tk->i_npt = tk->sub->getNormalPlayTime(pts);

    if( tk->b_quicktime && tk->p_es == NULL )
    {
        QuickTimeGenericRTPSource *qtRTPSource =
            (QuickTimeGenericRTPSource*)tk->sub->rtpSource();
        QuickTimeGenericRTPSource::QTState &qtState = qtRTPSource->qtState;
        uint8_t *sdAtom = (uint8_t*)&qtState.sdAtom[4];

        if( tk->fmt.i_cat == VIDEO_ES ) {
            if( qtState.sdAtomSize < 16 + 32 )
            {
                /* invalid */
                p_sys->event = 0xff;
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
                        tk->fmt.p_extra = malloc( tk->fmt.i_extra );
                        memcpy(tk->fmt.p_extra, pos+8, atomLength-8);
                        break;
                    }
                    pos += atomLength;
                }
            }
            else
            {
                tk->fmt.i_extra        = qtState.sdAtomSize - 16;
                tk->fmt.p_extra        = malloc( tk->fmt.i_extra );
                memcpy( tk->fmt.p_extra, &sdAtom[12], tk->fmt.i_extra );
            }
        }
        else {
            if( qtState.sdAtomSize < 4 )
            {
                /* invalid */
                p_sys->event = 0xff;
                tk->waiting = 0;
                return;
            }
            tk->fmt.i_codec = VLC_FOURCC(sdAtom[0],sdAtom[1],sdAtom[2],sdAtom[3]);
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
            p_sys->event = 0xff;
            tk->waiting = 0;
            return;
        }
    }

    assert( i_size <= tk->i_buffer );

    if( tk->fmt.i_codec == VLC_CODEC_AMR_NB ||
        tk->fmt.i_codec == VLC_CODEC_AMR_WB )
    {
        AMRAudioSource *amrSource = (AMRAudioSource*)tk->sub->readSource();

        p_block = block_New( p_demux, i_size + 1 );
        p_block->p_buffer[0] = amrSource->lastFrameHeader();
        memcpy( p_block->p_buffer + 1, tk->p_buffer, i_size );
    }
    else if( tk->fmt.i_codec == VLC_CODEC_H261 )
    {
        H261VideoRTPSource *h261Source = (H261VideoRTPSource*)tk->sub->rtpSource();
        uint32_t header = h261Source->lastSpecialHeader();
        p_block = block_New( p_demux, i_size + 4 );
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
        p_block = block_New( p_demux, i_size + 4 );
        p_block->p_buffer[0] = 0x00;
        p_block->p_buffer[1] = 0x00;
        p_block->p_buffer[2] = 0x00;
        p_block->p_buffer[3] = 0x01;
        memcpy( &p_block->p_buffer[4], tk->p_buffer, i_size );
    }
    else if( tk->b_asf )
    {
        int i_copy = __MIN( p_sys->asfh.i_min_data_packet_size, (int)i_size );
        p_block = block_New( p_demux, p_sys->asfh.i_min_data_packet_size );

        memcpy( p_block->p_buffer, tk->p_buffer, i_copy );
    }
    else
    {
        p_block = block_New( p_demux, i_size );
        memcpy( p_block->p_buffer, tk->p_buffer, i_size );
    }

    if( p_sys->i_pcr < i_pts )
    {
        p_sys->i_pcr = i_pts;
    }

    if( (i_pts != tk->i_pts) && (!tk->b_muxed) )
    {
        p_block->i_pts = i_pts;
    }

    /* Update our global npt value */
    if( tk->i_npt > 0 && tk->i_npt > p_sys->i_npt && tk->i_npt < p_sys->i_npt_length)
        p_sys->i_npt = tk->i_npt;

    if( !tk->b_muxed )
    {
        /*FIXME: for h264 you should check that packetization-mode=1 in sdp-file */
        p_block->i_dts = ( tk->fmt.i_codec == VLC_CODEC_MPGV ) ? 0 : i_pts;
    }

    if( tk->b_muxed )
    {
        stream_DemuxSend( tk->p_out_muxed, p_block );
    }
    else if( tk->b_asf )
    {
        stream_DemuxSend( p_sys->p_out_asf, p_block );
    }
    else
    {
        es_out_Send( p_demux->out, tk->p_es, p_block );
    }

    /* warn that's ok */
    p_sys->event = 0xff;

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

    msg_Dbg( p_demux, "StreamClose" );

    p_sys->event = 0xff;
    p_demux->b_error = true;
}


/*****************************************************************************
 *
 *****************************************************************************/
static void TaskInterrupt( void *p_private )
{
    demux_t *p_demux = (demux_t*)p_private;

    p_demux->p_sys->i_no_data_ti++;

    /* Avoid lock */
    p_demux->p_sys->event = 0xff;
}

/*****************************************************************************
 *
 *****************************************************************************/
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

            p_timeout->p_sys->rtsp->getMediaSessionParameter( *p_timeout->p_sys->ms, NULL, psz_bye );
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
    p_header = block_New( p_demux, psz_end - psz_asf );
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
