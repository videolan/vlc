/*****************************************************************************
 * live.cpp : live.com support.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: livedotcom.cpp,v 1.2 2003/11/07 00:28:58 fenrir Exp $
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

vlc_module_begin();
    set_description( _("live.com (RTSP/RTP/SDP) demuxer" ) );
    set_capability( "demux", 50 );
    set_callbacks( DemuxOpen, DemuxClose );
    add_shortcut( "live" );
    add_category_hint( N_("live-demuxer"), NULL, VLC_TRUE );
        add_bool( "rtsp-tcp", 0, NULL,
                  "Use rtp over rtsp(tcp)",
                  "Use rtp over rtsp(tcp)", VLC_TRUE );


    add_submodule();
        set_description( _("RTSP/RTP describe") );
        add_shortcut( "rtsp" );
        set_capability( "access", 0 );
        set_callbacks( AccessOpen, AccessClose );

vlc_module_end();

/* TODO:
 *  - Support PS/TS (need to rework the TS/PS demuxer a lot).
 *  - Support X-QT/X-QUICKTIME generic codec.
 *  - Handle PTS (for now I just use mdate())
 *
 *  - Check memory leak, delete/free.
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

    es_format_t  fmt;
    es_out_id_t  *p_es;

    FramedSource *readSource;

    uint8_t      buffer[65536];

    char         waiting;
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

    char             event;
};

static ssize_t Read ( input_thread_t *, byte_t *, size_t );
static int     Demux( input_thread_t * );


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

    if( p_input->psz_access == NULL || strcasecmp( p_input->psz_access, "rtsp" ) )
    {
        msg_Warn( p_input, "RTSP access discarded" );
        return VLC_EGENERIC;
    }
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
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for RTSP access */
    p_input->i_pts_delay = 4 * DEFAULT_PTS_DELAY;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * AccessClose:
 *****************************************************************************/
static void AccessClose( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys = p_input->p_access_data;

    delete p_sys->p_sdp;
    free( p_sys );
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
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->p_demux_data = p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    p_sys->p_sdp = NULL;
    p_sys->scheduler = NULL;
    p_sys->env = NULL;
    p_sys->ms = NULL;
    p_sys->rtsp = NULL;
    p_sys->i_track = 0;
    p_sys->track   = NULL;

    /* Gather the complete sdp file */
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

            msg_Warn( p_input, "RTP subsession '%s/%s'", sub->mediumName(), sub->codecName() );

            /* Increase the buffer size */
            increaseReceiveBufferTo( *p_sys->env, fd, i_buffer );
            /* Issue the SETUP */
            if( p_sys->rtsp )
            {
                p_sys->rtsp->setupMediaSubsession( *sub, False, val.i_int ? True : False );
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

        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC( 'u', 'n', 'd', 'f' ) );
            tk->fmt.audio.i_channels = sub->numChannels();
            tk->fmt.audio.i_samplerate = sub->rtpSource()->timestampFrequency();

            if( !strcmp( sub->codecName(), "MPA" ) ||
                !strcmp( sub->codecName(), "MPA-ROBUST" ) ||
                !strcmp( sub->codecName(), "X-MP3-DRAFT-00" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'a' );
                tk->fmt.audio.i_samplerate = 0;
            }
            else if( !strcmp( sub->codecName(), "AC3" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
                tk->fmt.audio.i_samplerate = 0;
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
                    tk->fmt.i_extra_type = ES_EXTRA_TYPE_WAVEFORMATEX;
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( sizeof( i_extra ) );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );
                    delete p_extra;
                }
            }
            else if( !strcmp( sub->codecName(), "MPEG4-GENERIC" ) )
            {
                unsigned int i_extra;
                uint8_t      *p_extra;

                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );

                if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(), i_extra ) ) )
                {
                    tk->fmt.i_extra_type = ES_EXTRA_TYPE_WAVEFORMATEX;
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( i_extra );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );
                    delete p_extra;
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
                tk->fmt.i_codec = VLC_FOURCC( 'J', 'P', 'E', 'G' );
            }
            else if( !strcmp( sub->codecName(), "MP4V-ES" ) )
            {
                unsigned int i_extra;
                uint8_t      *p_extra;

                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );

                if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(), i_extra ) ) )
                {
                    tk->fmt.i_extra_type = ES_EXTRA_TYPE_BITMAPINFOHEADER;
                    tk->fmt.i_extra = i_extra;
                    tk->fmt.p_extra = malloc( i_extra );
                    memcpy( tk->fmt.p_extra, p_extra, i_extra );
                    delete p_extra;
                }
            }
        }

        if( tk->fmt.i_codec != VLC_FOURCC( 'u', 'n', 'd', 'f' ) )
        {
            tk->p_es = es_out_Add( p_input->p_es_out, &tk->fmt );
        }

        if( tk->p_es )
        {
            TAB_APPEND( p_sys->i_track, (void**)p_sys->track, (void*)tk );
            tk->readSource = sub->readSource();
        }
        else
        {
            free( tk );
        }
    }


    delete iter;

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

        free( tk );
    }
    if( p_sys->i_track )
    {
        free( p_sys->track );
    }

    if( p_sys->rtsp )
    {
        /* TEARDOWN */
        MediaSubsessionIterator iter(*p_sys->ms);
        MediaSubsession *sub;

        while( ( sub = iter.next() ) != NULL )
        {
            p_sys->rtsp->teardownMediaSubsession(*sub);
        }
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

    int             i;

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

    return p_input->b_error ? 0 : 1;
}



/*****************************************************************************
 *
 *****************************************************************************/
static void StreamRead( void *p_private, unsigned int i_size, struct timeval pts )
{
    live_track_t   *tk = (live_track_t*)p_private;
    input_thread_t *p_input = tk->p_input;
    demux_sys_t    *p_sys = p_input->p_demux_data;
    pes_packet_t   *p_pes;
    data_packet_t  *p_data;

#if 0
    fprintf( stderr, "StreamRead size=%d pts=%lld\n",
             i_size,
             pts.tv_sec * 1000000LL + pts.tv_usec );
#endif
    /* Create a PES */
    if( ( p_pes = input_NewPES( p_input->p_method_data ) ) == NULL )
    {
        return;
    }
    /* FIXME could i_size be > buffer size ? */
    p_data = input_NewPacket( p_input->p_method_data, i_size );

    memcpy( p_data->p_payload_start, tk->buffer, i_size );
    p_data->p_payload_end = p_data->p_payload_start + i_size;

    p_pes->p_first = p_pes->p_last = p_data;
    p_pes->i_nb_data = 1;
    p_pes->i_pes_size = i_size;

    /* FIXME */
    p_pes->i_pts = mdate() + p_input->i_pts_delay;
    p_pes->i_dts = mdate() + p_input->i_pts_delay;
    es_out_Send( p_input->p_es_out, tk->p_es, p_pes );

    /* warm that's ok */
    p_sys->event = 0xff;

    /* we have read data */
    tk->waiting = 0;
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

