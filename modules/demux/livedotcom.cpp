/*****************************************************************************
 * live.cpp : live.com support.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <codecs.h>                        /* BITMAPINFOHEADER, WAVEFORMATEX */
#include <iostream>

#if defined( WIN32 )
#   include <winsock2.h>
#endif

#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "liveMedia.hh"

using namespace std;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  DemuxOpen ( vlc_object_t * );
static void DemuxClose( vlc_object_t * );

static int  AccessOpen ( vlc_object_t * );
static void AccessClose( vlc_object_t * );

#define CACHING_TEXT N_("Caching value (ms)")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for RTSP streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    set_description( _("live.com (RTSP/RTP/SDP) demuxer" ) );
    set_capability( "demux", 50 );
    set_callbacks( DemuxOpen, DemuxClose );
    add_shortcut( "live" );

    add_submodule();
        set_description( _("RTSP/RTP describe") );
        add_shortcut( "rtsp" );
        add_shortcut( "sdp" );
        set_capability( "access", 0 );
        set_callbacks( AccessOpen, AccessClose );
        add_bool( "rtsp-tcp", 0, NULL,
                  N_("Use RTP over RTSP (TCP)"),
                  N_("Use RTP over RTSP (TCP)"), VLC_TRUE );
        add_integer( "rtsp-caching", 4 * DEFAULT_PTS_DELAY / 1000, NULL,
            CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
vlc_module_end();

/* TODO:
 *  - Support PS/TS (need to rework the TS/PS demuxer a lot).
 *  - Support X-QT/X-QUICKTIME generic codec for audio.
 *
 *  - Check memory leak, delete/free -> still one when using rtsp-tcp but I'm
 *  not sure if it comes from me.
 *
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct access_sys_t
{
    int     i_sdp;
    char    *p_sdp;

    int     i_pos;
};

typedef struct
{
    input_thread_t *p_input;

    vlc_bool_t   b_quicktime;
    vlc_bool_t   b_muxed;

    es_format_t  fmt;
    es_out_id_t  *p_es;

    stream_t     *p_out_muxed;    /* for muxed stream */

    RTPSource    *rtpSource;
    FramedSource *readSource;
    vlc_bool_t   b_rtcp_sync;

    uint8_t      buffer[65536];

    char         waiting;

    mtime_t      i_pts;
} live_track_t;

struct demux_sys_t
{
    char         *p_sdp;    /* XXX mallocated */

    MediaSession     *ms;
    TaskScheduler    *scheduler;
    UsageEnvironment *env ;
    RTSPClient       *rtsp;

    int              i_track;
    live_track_t     **track;   /* XXX mallocated */
    mtime_t          i_pcr;
    mtime_t          i_pcr_start;

    mtime_t          i_length;
    mtime_t          i_start;

    char             event;
};

static ssize_t Read   ( input_thread_t *, byte_t *, size_t );
static ssize_t MRLRead( input_thread_t *, byte_t *, size_t );

static int     Demux  ( input_thread_t * );
static int     Control( input_thread_t *, int, va_list );



/*****************************************************************************
 * AccessOpen:
 *****************************************************************************/
static int  AccessOpen( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys;

    TaskScheduler    *scheduler = NULL;
    UsageEnvironment *env       = NULL;
    RTSPClient       *rtsp      = NULL;

    vlc_value_t      val;
    char             *psz_url;

    if( p_input->psz_access == NULL || ( strcasecmp( p_input->psz_access, "rtsp" ) && strcasecmp( p_input->psz_access, "sdp" ) ) )
    {
        msg_Warn( p_input, "RTSP access discarded" );
        return VLC_EGENERIC;
    }
    if( !strcasecmp( p_input->psz_access, "rtsp" ) )
    {
        if( ( scheduler = BasicTaskScheduler::createNew() ) == NULL )
        {
            msg_Err( p_input, "BasicTaskScheduler::createNew failed" );
            return VLC_EGENERIC;
        }
        if( ( env = BasicUsageEnvironment::createNew(*scheduler) ) == NULL )
        {
            delete scheduler;
            msg_Err( p_input, "BasicUsageEnvironment::createNew failed" );
            return VLC_EGENERIC;
        }
        if( ( rtsp = RTSPClient::createNew(*env, 1/*verbose*/, "VLC Media Player" ) ) == NULL )
        {
            delete env;
            delete scheduler;
            msg_Err( p_input, "RTSPClient::createNew failed" );
            return VLC_EGENERIC;
        }

        psz_url = (char*)malloc( strlen( p_input->psz_name ) + 8 );
        sprintf( psz_url, "rtsp://%s", p_input->psz_name );

        p_sys = (access_sys_t*)malloc( sizeof( access_sys_t ) );
        p_sys->p_sdp = rtsp->describeURL( psz_url );

        if( p_sys->p_sdp == NULL )
        {
            msg_Err( p_input, "describeURL failed (%s)", env->getResultMsg() );

            free( psz_url );
            delete env;
            delete scheduler;
            free( p_sys );
            return VLC_EGENERIC;
        }
        free( psz_url );
        p_sys->i_sdp = strlen( p_sys->p_sdp );
        p_sys->i_pos = 0;

        //fprintf( stderr, "sdp=%s\n", p_sys->p_sdp );

        delete env;
        delete scheduler;

        var_Create( p_input, "rtsp-tcp", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );
        var_Get( p_input, "rtsp-tcp", &val );

        p_input->p_access_data = p_sys;
        p_input->i_mtu = 0;

        /* Set exported functions */
        p_input->pf_read = Read;
        p_input->pf_seek = NULL;
        p_input->pf_set_program = input_SetProgram;
        p_input->pf_set_area = NULL;
        p_input->p_private = NULL;

        p_input->psz_demux = "live";

        /* Finished to set some variable */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        /* FIXME that's not true but eg over tcp, server send data too fast */
        p_input->stream.b_pace_control = val.b_bool;
        p_input->stream.p_selected_area->i_tell = 0;
        p_input->stream.b_seekable = 1; /* Hack to display time */
        p_input->stream.p_selected_area->i_size = 0;
        p_input->stream.i_method = INPUT_METHOD_NETWORK;
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        /* Update default_pts to a suitable value for RTSP access */
        var_Create( p_input, "rtsp-caching", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
        var_Get( p_input, "rtsp-caching", &val );
        p_input->i_pts_delay = val.i_int * 1000;

        return VLC_SUCCESS;
    }
    else
    {
        p_input->p_access_data = (access_sys_t*)0;
        p_input->i_mtu = 0;
        p_input->pf_read = MRLRead;
        p_input->pf_seek = NULL;
        p_input->pf_set_program = input_SetProgram;
        p_input->pf_set_area = NULL;
        p_input->p_private = NULL;
        p_input->psz_demux = "live";
        /* Finished to set some variable */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.b_pace_control = VLC_TRUE;
        p_input->stream.p_selected_area->i_tell = 0;
        p_input->stream.b_seekable = VLC_FALSE;
        p_input->stream.p_selected_area->i_size = 0;
        p_input->stream.i_method = INPUT_METHOD_NETWORK;
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        return VLC_SUCCESS;
    }
}

/*****************************************************************************
 * AccessClose:
 *****************************************************************************/
static void AccessClose( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys = p_input->p_access_data;
    if( !strcasecmp( p_input->psz_access, "rtsp" ) )
    {
        delete[] p_sys->p_sdp;
        free( p_sys );
    }
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read ( input_thread_t *p_input, byte_t *p_buffer, size_t i_len )
{
    access_sys_t   *p_sys   = p_input->p_access_data;
    int            i_copy = __MIN( (int)i_len, p_sys->i_sdp - p_sys->i_pos );

    if( i_copy > 0 )
    {
        memcpy( p_buffer, &p_sys->p_sdp[p_sys->i_pos], i_copy );
        p_sys->i_pos += i_copy;
    }
    return i_copy;
}
/*****************************************************************************
 * MRLRead: read data from the mrl
 *****************************************************************************/
static ssize_t MRLRead ( input_thread_t *p_input, byte_t *p_buffer, size_t i_len )
{
    int i_done = (int)p_input->p_access_data;
    int            i_copy = __MIN( (int)i_len, (int)strlen(p_input->psz_name) - i_done );

    if( i_copy > 0 )
    {
        memcpy( p_buffer, &p_input->psz_name[i_done], i_copy );
        i_done += i_copy;
        p_input->p_access_data = (access_sys_t*)i_done;
    }
    return i_copy;
}


/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int  DemuxOpen ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;

    MediaSubsessionIterator *iter;
    MediaSubsession *sub;

    vlc_value_t val;

    uint8_t *p_peek;

    int     i_sdp;
    int     i_sdp_max;
    uint8_t *p_sdp;

    /* See if it looks like a SDP
       v, o, s fields are mandatory and in this order */
    if( stream_Peek( p_input->s, &p_peek, 7 ) < 7 )
    {
        msg_Err( p_input, "cannot peek" );
        return VLC_EGENERIC;
    }
    if( strncmp( (char*)p_peek, "v=0\r\n", 5 ) && strncmp( (char*)p_peek, "v=0\n", 4 ) &&
        ( p_input->psz_access == NULL || strcasecmp( p_input->psz_access, "rtsp" ) ||
          p_peek[0] < 'a' || p_peek[0] > 'z' || p_peek[1] != '=' ) )
    {
        msg_Warn( p_input, "SDP module discarded" );
        return VLC_EGENERIC;
    }

    p_input->pf_demux = Demux;
    p_input->pf_demux_control = Control;
    p_input->p_demux_data = p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    p_sys->p_sdp = NULL;
    p_sys->scheduler = NULL;
    p_sys->env = NULL;
    p_sys->ms = NULL;
    p_sys->rtsp = NULL;
    p_sys->i_track = 0;
    p_sys->track   = NULL;
    p_sys->i_pcr   = 0;
    p_sys->i_pcr_start = 0;
    p_sys->i_length = 0;
    p_sys->i_start = 0;

    /* Gather the complete sdp file */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    i_sdp = 0;
    i_sdp_max = 1000;
    p_sdp = (uint8_t*)malloc( i_sdp_max );
    for( ;; )
    {
        int i_read = stream_Read( p_input->s, &p_sdp[i_sdp], i_sdp_max - i_sdp - 1 );

        if( i_read < 0 )
        {
            msg_Err( p_input, "failed to read SDP" );
            free( p_sys );
            return VLC_EGENERIC;
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

    fprintf( stderr, "sdp=%s\n", p_sys->p_sdp );

    if( ( p_sys->scheduler = BasicTaskScheduler::createNew() ) == NULL )
    {
        msg_Err( p_input, "BasicTaskScheduler::createNew failed" );
        goto error;
    }
    if( ( p_sys->env = BasicUsageEnvironment::createNew(*p_sys->scheduler) ) == NULL )
    {
        msg_Err( p_input, "BasicUsageEnvironment::createNew failed" );
        goto error;
    }
    if( p_input->psz_access != NULL && !strcasecmp( p_input->psz_access, "rtsp" ) )
    {
        char *psz_url;
        char *psz_options;

        if( ( p_sys->rtsp = RTSPClient::createNew(*p_sys->env, 1/*verbose*/, "VLC Media Player" ) ) == NULL )
        {
            msg_Err( p_input, "RTSPClient::createNew failed (%s)", p_sys->env->getResultMsg() );
            goto error;
        }
        psz_url = (char*)malloc( strlen( p_input->psz_name ) + 8 );
        sprintf( psz_url, "rtsp://%s", p_input->psz_name );

        psz_options = p_sys->rtsp->sendOptionsCmd( psz_url );
        /* FIXME psz_options -> delete or free */
        free( psz_url );
    }
    if( ( p_sys->ms = MediaSession::createNew(*p_sys->env, p_sys->p_sdp ) ) == NULL )
    {
        msg_Err( p_input, "MediaSession::createNew failed" );
        goto error;
    }

    var_Create( p_input, "rtsp-tcp", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );
    var_Get( p_input, "rtsp-tcp", &val );

    /* Initialise each media subsession */
    iter = new MediaSubsessionIterator( *p_sys->ms );
    while( ( sub = iter->next() ) != NULL )
    {
        unsigned int i_buffer = 0;

        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
        {
            i_buffer = 100000;
        }
        else if( !strcmp( sub->mediumName(), "video" ) )
        {
            i_buffer = 2000000;
        }
        else
        {
            continue;
        }

        if( !sub->initiate() )
        {
            msg_Warn( p_input, "RTP subsession '%s/%s' failed(%s)", sub->mediumName(), sub->codecName(), p_sys->env->getResultMsg() );
        }
        else
        {
            int fd = sub->rtpSource()->RTPgs()->socketNum();

            msg_Dbg( p_input, "RTP subsession '%s/%s'", sub->mediumName(), sub->codecName() );

            /* Increase the buffer size */
            increaseReceiveBufferTo( *p_sys->env, fd, i_buffer );
            /* Issue the SETUP */
            if( p_sys->rtsp )
            {
                p_sys->rtsp->setupMediaSubsession( *sub, False, val.b_bool ? True : False );
            }
        }
    }

    if( p_sys->rtsp )
    {
        /* The PLAY */
        if( !p_sys->rtsp->playMediaSession( *p_sys->ms ) )
        {
            msg_Err( p_input, "PLAY failed %s", p_sys->env->getResultMsg() );
            goto error;
        }
    }

    /* Create all es struct */
    iter->reset();
    while( ( sub = iter->next() ) != NULL )
    {
        live_track_t *tk;

        if( sub->readSource() == NULL )
        {
            continue;
        }

        tk = (live_track_t*)malloc( sizeof( live_track_t ) );
        tk->p_input = p_input;
        tk->waiting = 0;
        tk->i_pts   = 0;
        tk->b_quicktime = VLC_FALSE;
        tk->b_muxed     = VLC_FALSE;
        tk->b_rtcp_sync = VLC_FALSE;
        tk->p_out_muxed = NULL;
        tk->p_es        = NULL;

        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC( 'u', 'n', 'd', 'f' ) );
            tk->fmt.audio.i_channels = sub->numChannels();
            tk->fmt.audio.i_rate = sub->rtpSource()->timestampFrequency();

            if( !strcmp( sub->codecName(), "MPA" ) ||
                !strcmp( sub->codecName(), "MPA-ROBUST" ) ||
                !strcmp( sub->codecName(), "X-MP3-DRAFT-00" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'a' );
                tk->fmt.audio.i_rate = 0;
            }
            else if( !strcmp( sub->codecName(), "AC3" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
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
                tk->fmt.i_codec = VLC_FOURCC( 'u', 'l', 'a', 'w' );
            }
            else if( !strcmp( sub->codecName(), "PCMA" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'a', 'l', 'a', 'w' );
            }
            else if( !strcmp( sub->codecName(), "MP4A-LATM" ) )
            {
                unsigned int i_extra;
                uint8_t      *p_extra;

                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );

                if( ( p_extra = parseStreamMuxConfigStr( sub->fmtp_config(), i_extra ) ) )
                {
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( i_extra );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );
                    delete[] p_extra;
                }
            }
            else if( !strcmp( sub->codecName(), "MPEG4-GENERIC" ) )
            {
                unsigned int i_extra;
                uint8_t      *p_extra;

                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );

                if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(), i_extra ) ) )
                {
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( i_extra );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );
                    delete[] p_extra;
                }
            }
        }
        else if( !strcmp( sub->mediumName(), "video" ) )
        {
            es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC( 'u', 'n', 'd', 'f' ) );
            if( !strcmp( sub->codecName(), "MPV" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'v' );
            }
            else if( !strcmp( sub->codecName(), "H263" ) ||
                     !strcmp( sub->codecName(), "H263-1998" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'h', '2', '6', '3' );
            }
            else if( !strcmp( sub->codecName(), "H261" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'h', '2', '6', '1' );
            }
            else if( !strcmp( sub->codecName(), "JPEG" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'M', 'J', 'P', 'G' );
            }
            else if( !strcmp( sub->codecName(), "MP4V-ES" ) )
            {
                unsigned int i_extra;
                uint8_t      *p_extra;

                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );

                if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(), i_extra ) ) )
                {
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( i_extra );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );
                    delete[] p_extra;
                }
            }
            else if( !strcmp( sub->codecName(), "X-QT" ) || !strcmp( sub->codecName(), "X-QUICKTIME" ) )
            {
                tk->b_quicktime = VLC_TRUE;
            }
            else if( !strcmp( sub->codecName(), "MP2T" ) )
            {
                tk->b_muxed = VLC_TRUE;
                tk->p_out_muxed = stream_DemuxNew( p_input, "ts2", p_input->p_es_out );
            }
            else if( !strcmp( sub->codecName(), "MP2P" ) || !strcmp( sub->codecName(), "MP1S" ) )   /* FIXME check MP1S */
            {
                tk->b_muxed = VLC_TRUE;
                tk->p_out_muxed = stream_DemuxNew( p_input, "ps2", p_input->p_es_out );
            }
        }

        if( tk->fmt.i_codec != VLC_FOURCC( 'u', 'n', 'd', 'f' ) )
        {
            tk->p_es = es_out_Add( p_input->p_es_out, &tk->fmt );
        }

        if( tk->p_es || tk->b_quicktime || tk->b_muxed )
        {
            tk->readSource = sub->readSource();
            tk->rtpSource  = sub->rtpSource();

            /* Append */
            p_sys->track = (live_track_t**)realloc( p_sys->track, sizeof( live_track_t ) * ( p_sys->i_track + 1 ) );
            p_sys->track[p_sys->i_track++] = tk;
        }
        else
        {
            free( tk );
        }
    }

    delete iter;

    p_sys->i_length = (mtime_t)(p_sys->ms->playEndTime() * 1000000.0);
    if( p_sys->i_length < 0 )
    {
        p_sys->i_length = 0;
    }
    else if( p_sys->i_length > 0 )
    {
        p_input->stream.p_selected_area->i_size = 1000; /* needed for now */
    }

    if( p_sys->i_track <= 0 )
    {
        msg_Err( p_input, "no codec supported, aborting" );
        goto error;
    }

    return VLC_SUCCESS;

error:
    if( p_sys->ms )
    {
        Medium::close( p_sys->ms );
    }
    if( p_sys->rtsp )
    {
        Medium::close( p_sys->rtsp );
    }
    if( p_sys->env )
    {
        delete p_sys->env;
    }
    if( p_sys->scheduler )
    {
        delete p_sys->scheduler;
    }
    if( p_sys->p_sdp )
    {
        free( p_sys->p_sdp );
    }
    free( p_sys );
    return VLC_EGENERIC;
}



/*****************************************************************************
 * DemuxClose:
 *****************************************************************************/
static void DemuxClose( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;
    int            i;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->b_muxed )
        {
            stream_DemuxDelete( tk->p_out_muxed );
        }

        free( tk );
    }
    if( p_sys->i_track )
    {
        free( p_sys->track );
    }

    if( p_sys->rtsp && p_sys->ms )
    {
        /* TEARDOWN */
        p_sys->rtsp->teardownMediaSession( *p_sys->ms );
    }
    Medium::close( p_sys->ms );
    if( p_sys->rtsp )
    {
        Medium::close( p_sys->rtsp );
    }

    if( p_sys->env )
    {
        delete p_sys->env;
    }
    if( p_sys->scheduler )
    {
        delete p_sys->scheduler;
    }
    if( p_sys->p_sdp )
    {
        free( p_sys->p_sdp );
    }
    free( p_sys );
}


static void StreamRead( void *p_private, unsigned int i_size, struct timeval pts );
static void StreamClose( void *p_private );
static void TaskInterrupt( void *p_private );

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int  Demux   ( input_thread_t *p_input )
{
    demux_sys_t    *p_sys = p_input->p_demux_data;
    TaskToken      task;

    mtime_t         i_pcr = 0;
    int             i;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( i_pcr == 0 )
        {
            i_pcr = tk->i_pts;
        }
        else if( tk->i_pts != 0 && i_pcr > tk->i_pts )
        {
            i_pcr = tk->i_pts ;
        }
    }
    if( i_pcr != p_sys->i_pcr && i_pcr > 0 )
    {
        input_ClockManageRef( p_input,
                              p_input->stream.p_selected_program,
                              i_pcr * 9 / 100 );
        p_sys->i_pcr = i_pcr;
        if( p_sys->i_pcr_start <= 0 || p_sys->i_pcr_start > i_pcr )
        {
            p_sys->i_pcr_start = i_pcr;
        }
    }

    /* First warm we want to read data */
    p_sys->event = 0;
    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( tk->waiting == 0 )
        {
            tk->waiting = 1;
            tk->readSource->getNextFrame( tk->buffer, 65536,
                                          StreamRead, tk,
                                          StreamClose, tk );
        }
    }
    /* Create a task that will be called if we wait more than 300ms */
    task = p_sys->scheduler->scheduleDelayedTask( 300000, TaskInterrupt, p_input );

    /* Do the read */
    p_sys->scheduler->doEventLoop( &p_sys->event );

    /* remove the task */
    p_sys->scheduler->unscheduleDelayedTask( task );

    /* Check for gap in pts value */
    for( i = 0; i < p_sys->i_track; i++ )
    {
        live_track_t *tk = p_sys->track[i];

        if( !tk->b_muxed && !tk->b_rtcp_sync && tk->rtpSource->hasBeenSynchronizedUsingRTCP() )
        {
            msg_Dbg( p_input, "tk->rtpSource->hasBeenSynchronizedUsingRTCP()" );
            p_input->stream.p_selected_program->i_synchro_state = SYNCHRO_REINIT;
            tk->b_rtcp_sync = VLC_TRUE;

            /* reset PCR and PCR start, mmh won't work well for multi-stream I fear */
            tk->i_pts = 0;
            p_sys->i_pcr_start = 0;
            p_sys->i_pcr = 0;
            i_pcr = 0;
        }
    }

    return p_input->b_error ? 0 : 1;
}

static int    Control( input_thread_t *p_input, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    int64_t *pi64;
    double  *pf, f;

    switch( i_query )
    {
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_pcr - p_sys->i_pcr_start + p_sys->i_start;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_length;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double* );
            if( p_sys->i_length > 0 )
            {
                *pf = (double)( p_sys->i_pcr - p_sys->i_pcr_start + p_sys->i_start)/
                      (double)(p_sys->i_length);
            }
            else
            {
                *pf = 0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
        {
            float time;

            f = (double)va_arg( args, double );
            time = f * (double)p_sys->i_length / 1000000.0;   /* in second */

            if( p_sys->rtsp && p_sys->i_length > 0 )
            {
                MediaSubsessionIterator *iter = new MediaSubsessionIterator( *p_sys->ms );
                MediaSubsession         *sub;
                int i;

                while( ( sub = iter->next() ) != NULL )
                {
                    p_sys->rtsp->playMediaSubsession( *sub, time );
                }
                delete iter;
                p_sys->i_start = (mtime_t)(f * (double)p_sys->i_length);
                p_sys->i_pcr_start = 0;
                p_sys->i_pcr       = 0;
                for( i = 0; i < p_sys->i_track; i++ )
                {
                    p_sys->track[i]->i_pts = 0;
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        default:
            return demux_vaControlDefault( p_input, i_query, args );
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static void StreamRead( void *p_private, unsigned int i_size, struct timeval pts )
{
    live_track_t   *tk = (live_track_t*)p_private;
    input_thread_t *p_input = tk->p_input;
    demux_sys_t    *p_sys = p_input->p_demux_data;
    block_t        *p_block;

    mtime_t i_pts = (uint64_t)pts.tv_sec * UI64C(1000000) + (uint64_t)pts.tv_usec;

    /* XXX Beurk beurk beurk Avoid having negative value XXX */
    i_pts &= UI64C(0x00ffffffffffffff);

    if( tk->b_quicktime && tk->p_es == NULL )
    {
        QuickTimeGenericRTPSource *qtRTPSource = (QuickTimeGenericRTPSource*)tk->rtpSource;
        QuickTimeGenericRTPSource::QTState &qtState = qtRTPSource->qtState;
        uint8_t *sdAtom = (uint8_t*)&qtState.sdAtom[4];

        if( qtState.sdAtomSize < 16 + 32 )
        {
            /* invalid */
            p_sys->event = 0xff;
            tk->waiting = 0;
            return;
        }
        tk->fmt.i_codec = VLC_FOURCC( sdAtom[0], sdAtom[1], sdAtom[2], sdAtom[3] );
        tk->fmt.video.i_width  = (sdAtom[28] << 8) | sdAtom[29];
        tk->fmt.video.i_height = (sdAtom[30] << 8) | sdAtom[31];

        tk->fmt.i_extra        = qtState.sdAtomSize - 16;
        tk->fmt.p_extra        = malloc( tk->fmt.i_extra );
        memcpy( tk->fmt.p_extra, &sdAtom[12], tk->fmt.i_extra );

        tk->p_es = es_out_Add( p_input->p_es_out, &tk->fmt );
    }

#if 0
    fprintf( stderr, "StreamRead size=%d pts=%lld\n",
             i_size,
             pts.tv_sec * 1000000LL + pts.tv_usec );
#endif
    if( i_size > 65536 )
    {
        msg_Warn( p_input, "buffer overflow" );
    }
    /* FIXME could i_size be > buffer size ? */
    p_block = block_New( p_input, i_size );

    memcpy( p_block->p_buffer, tk->buffer, i_size );
    //p_block->i_rate = p_input->stream.control.i_rate;

    if( i_pts != tk->i_pts && !tk->b_muxed )
    {
        p_block->i_dts =
        p_block->i_pts = input_ClockGetTS( p_input,
                                         p_input->stream.p_selected_program,
                                         i_pts * 9 / 100 );
    }
    //fprintf( stderr, "tk -> dpts=%lld\n", i_pts - tk->i_pts );

    if( tk->b_muxed )
    {
        stream_DemuxSend( tk->p_out_muxed, p_block );
    }
    else
    {
        es_out_Send( p_input->p_es_out, tk->p_es, p_block );
    }

    /* warm that's ok */
    p_sys->event = 0xff;

    /* we have read data */
    tk->waiting = 0;

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
    input_thread_t *p_input = tk->p_input;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    fprintf( stderr, "StreamClose\n" );

    p_sys->event = 0xff;
    p_input->b_error = VLC_TRUE;
}


/*****************************************************************************
 *
 *****************************************************************************/
static void TaskInterrupt( void *p_private )
{
    input_thread_t *p_input = (input_thread_t*)p_private;

    fprintf( stderr, "TaskInterrupt\n" );

    /* Avoid lock */
    p_input->p_demux_data->event = 0xff;
}

