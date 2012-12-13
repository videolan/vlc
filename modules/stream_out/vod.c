/*****************************************************************************
 * vod.c: rtsp VoD server module
 *****************************************************************************
 * Copyright (C) 2003-2006, 2010 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Pierre Ynard
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_sout.h>
#include <vlc_block.h>

#include <vlc_vod.h>
#include <vlc_url.h>
#include <vlc_network.h>

#include <assert.h>

#include "rtp.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/

typedef struct media_es_t media_es_t;

struct media_es_t
{
    int es_id;
    rtp_format_t rtp_fmt;
    rtsp_stream_id_t *rtsp_id;
};

struct vod_media_t
{
    /* VoD server */
    vod_t *p_vod;

    /* RTSP server */
    rtsp_stream_t *rtsp;

    /* ES list */
    int        i_es;
    media_es_t **es;
    const char *psz_mux;

    /* Infos */
    mtime_t i_length;
};

struct vod_sys_t
{
    char *psz_rtsp_path;

    /* */
    vlc_thread_t thread;
    block_fifo_t *p_fifo_cmd;
};

/* rtsp delayed command (to avoid deadlock between vlm/httpd) */
typedef enum
{
    RTSP_CMD_TYPE_STOP,
    RTSP_CMD_TYPE_ADD,
    RTSP_CMD_TYPE_DEL,
} rtsp_cmd_type_t;

/* */
typedef struct
{
    int i_type;
    vod_media_t *p_media;
    char *psz_arg;
} rtsp_cmd_t;

static vod_media_t *MediaNew( vod_t *, const char *, input_item_t * );
static void         MediaDel( vod_t *, vod_media_t * );
static void         MediaAskDel ( vod_t *, vod_media_t * );

static void* CommandThread( void *obj );
static void  CommandPush( vod_t *, rtsp_cmd_type_t, vod_media_t *,
                          const char *psz_arg );

/*****************************************************************************
 * Open: Starts the RTSP server module
 *****************************************************************************/
int OpenVoD( vlc_object_t *p_this )
{
    vod_t *p_vod = (vod_t *)p_this;
    vod_sys_t *p_sys = NULL;
    char *psz_url;

    p_vod->p_sys = p_sys = malloc( sizeof( vod_sys_t ) );
    if( !p_sys ) goto error;

    psz_url = var_InheritString( p_vod, "rtsp-host" );

    if( psz_url == NULL )
        p_sys->psz_rtsp_path = strdup( "/" );
    else
    {
        vlc_url_t url;
        vlc_UrlParse( &url, psz_url, 0 );
        free( psz_url );

        if( url.psz_path == NULL )
            p_sys->psz_rtsp_path = strdup( "/" );
        else
        if( !( strlen( url.psz_path ) > 0
               && url.psz_path[strlen( url.psz_path ) - 1] == '/' ) )
        {
            if( asprintf( &p_sys->psz_rtsp_path, "%s/", url.psz_path ) == -1 )
            {
                p_sys->psz_rtsp_path = NULL;
                vlc_UrlClean( &url );
                goto error;
            }
        }
        else
            p_sys->psz_rtsp_path = strdup( url.psz_path );

        vlc_UrlClean( &url );
    }

    p_vod->pf_media_new = MediaNew;
    p_vod->pf_media_del = MediaAskDel;

    p_sys->p_fifo_cmd = block_FifoNew();
    if( vlc_clone( &p_sys->thread, CommandThread, p_vod, VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_vod, "cannot spawn rtsp vod thread" );
        block_FifoRelease( p_sys->p_fifo_cmd );
        goto error;
    }

    return VLC_SUCCESS;

error:
    if( p_sys )
    {
        free( p_sys->psz_rtsp_path );
        free( p_sys );
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
void CloseVoD( vlc_object_t * p_this )
{
    vod_t *p_vod = (vod_t *)p_this;
    vod_sys_t *p_sys = p_vod->p_sys;

    /* Stop command thread */
    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    while( block_FifoCount( p_sys->p_fifo_cmd ) > 0 )
    {
        rtsp_cmd_t cmd;
        block_t *p_block_cmd = block_FifoGet( p_sys->p_fifo_cmd );
        memcpy( &cmd, p_block_cmd->p_buffer, sizeof(cmd) );
        block_Release( p_block_cmd );
        if ( cmd.i_type == RTSP_CMD_TYPE_DEL )
            MediaDel(p_vod, cmd.p_media);
        free( cmd.psz_arg );
    }
    block_FifoRelease( p_sys->p_fifo_cmd );

    free( p_sys->psz_rtsp_path );
    free( p_sys );
}

/*****************************************************************************
 * Media handling
 *****************************************************************************/
static vod_media_t *MediaNew( vod_t *p_vod, const char *psz_name,
                              input_item_t *p_item )
{
    vod_media_t *p_media = calloc( 1, sizeof(vod_media_t) );
    if( !p_media )
        return NULL;

    p_media->p_vod = p_vod;
    p_media->rtsp = NULL;
    TAB_INIT( p_media->i_es, p_media->es );
    p_media->psz_mux = NULL;
    p_media->i_length = input_item_GetDuration( p_item );

    vlc_mutex_lock( &p_item->lock );
    msg_Dbg( p_vod, "media '%s' has %i declared ES", psz_name, p_item->i_es );
    for( int i = 0; i < p_item->i_es; i++ )
    {
        es_format_t *p_fmt = p_item->es[i];

        switch( p_fmt->i_codec )
        {
            case VLC_FOURCC( 'm', 'p', '2', 't' ):
                p_media->psz_mux = "ts";
                break;
            case VLC_FOURCC( 'm', 'p', '2', 'p' ):
                p_media->psz_mux = "ps";
                break;
        }
        assert(p_media->psz_mux == NULL || p_item->i_es == 1);

        media_es_t *p_es = calloc( 1, sizeof(media_es_t) );
        if( !p_es )
            continue;

        p_es->es_id = p_fmt->i_id;
        p_es->rtsp_id = NULL;

        if (rtp_get_fmt(VLC_OBJECT(p_vod), p_fmt, p_media->psz_mux,
                        &p_es->rtp_fmt) != VLC_SUCCESS)
        {
            free(p_es);
            continue;
        }

        TAB_APPEND( p_media->i_es, p_media->es, p_es );
        msg_Dbg(p_vod, "  - added ES %u %s (%4.4s)",
                p_es->rtp_fmt.payload_type, p_es->rtp_fmt.ptname,
                (char *)&p_fmt->i_codec);
    }
    vlc_mutex_unlock( &p_item->lock );

    if (p_media->i_es == 0)
    {
        msg_Err(p_vod, "no ES was added to the media, aborting");
        goto error;
    }

    msg_Dbg(p_vod, "adding media '%s'", psz_name);

    CommandPush( p_vod, RTSP_CMD_TYPE_ADD, p_media, psz_name );
    return p_media;

error:
    MediaDel(p_vod, p_media);
    return NULL;
}

static void MediaSetup( vod_t *p_vod, vod_media_t *p_media,
                        const char *psz_name )
{
    vod_sys_t *p_sys = p_vod->p_sys;
    char *psz_path;

    if( asprintf( &psz_path, "%s%s", p_sys->psz_rtsp_path, psz_name ) < 0 )
        return;

    p_media->rtsp = RtspSetup(VLC_OBJECT(p_vod), p_media, psz_path);
    free( psz_path );

    if (p_media->rtsp == NULL)
        return;

    for (int i = 0; i < p_media->i_es; i++)
    {
        media_es_t *p_es = p_media->es[i];
        p_es->rtsp_id = RtspAddId(p_media->rtsp, NULL, 0,
                                  p_es->rtp_fmt.clock_rate, -1);
    }
}

static void MediaAskDel ( vod_t *p_vod, vod_media_t *p_media )
{
    msg_Dbg( p_vod, "deleting media" );
    CommandPush( p_vod, RTSP_CMD_TYPE_DEL, p_media, NULL );
}

static void MediaDel( vod_t *p_vod, vod_media_t *p_media )
{
    (void) p_vod;

    if (p_media->rtsp != NULL)
    {
        for (int i = 0; i < p_media->i_es; i++)
        {
            media_es_t *p_es = p_media->es[i];
            if (p_es->rtsp_id != NULL)
                RtspDelId(p_media->rtsp, p_es->rtsp_id);
        }
        RtspUnsetup(p_media->rtsp);
    }

    while( p_media->i_es )
    {
        media_es_t *p_es = p_media->es[0];
        TAB_REMOVE( p_media->i_es, p_media->es, p_es );
        free( p_es->rtp_fmt.fmtp );
        free( p_es );
    }

    TAB_CLEAN( p_media->i_es, p_media->es );
    free( p_media );
}

static void CommandPush( vod_t *p_vod, rtsp_cmd_type_t i_type,
                         vod_media_t *p_media, const char *psz_arg )
{
    rtsp_cmd_t cmd;
    block_t *p_cmd;

    cmd.i_type = i_type;
    cmd.p_media = p_media;
    if( psz_arg )
        cmd.psz_arg = strdup(psz_arg);
    else
        cmd.psz_arg = NULL;

    p_cmd = block_Alloc( sizeof(rtsp_cmd_t) );
    memcpy( p_cmd->p_buffer, &cmd, sizeof(cmd) );

    block_FifoPut( p_vod->p_sys->p_fifo_cmd, p_cmd );
}

static void* CommandThread( void *obj )
{
    vod_t *p_vod = (vod_t*)obj;
    vod_sys_t *p_sys = p_vod->p_sys;

    for( ;; )
    {
        block_t *p_block_cmd = block_FifoGet( p_sys->p_fifo_cmd );
        rtsp_cmd_t cmd;

        if( !p_block_cmd )
            break;

        int canc = vlc_savecancel ();
        memcpy( &cmd, p_block_cmd->p_buffer, sizeof(cmd) );
        block_Release( p_block_cmd );

        /* */
        switch( cmd.i_type )
        {
        case RTSP_CMD_TYPE_ADD:
            MediaSetup(p_vod, cmd.p_media, cmd.psz_arg);
            break;
        case RTSP_CMD_TYPE_DEL:
            MediaDel(p_vod, cmd.p_media);
            break;
        case RTSP_CMD_TYPE_STOP:
            vod_MediaControl( p_vod, cmd.p_media, cmd.psz_arg, VOD_MEDIA_STOP );
            break;

        default:
            break;
        }

        free( cmd.psz_arg );
        vlc_restorecancel (canc);
    }

    return NULL;
}

/*****************************************************************************
 * SDPGenerateVoD
 * FIXME: needs to be merged more?
 *****************************************************************************/
char *SDPGenerateVoD( const vod_media_t *p_media, const char *rtsp_url )
{
    char *psz_sdp;

    assert(rtsp_url != NULL);
    /* Check against URL format rtsp://[<ipv6>]:<port>/<path> */
    bool ipv6 = strlen( rtsp_url ) > 7 && rtsp_url[7] == '[';

    /* Dummy destination address for RTSP */
    struct sockaddr_storage dst;
    socklen_t dstlen = ipv6 ? sizeof( struct sockaddr_in6 )
                            : sizeof( struct sockaddr_in );
    memset (&dst, 0, dstlen);
    dst.ss_family = ipv6 ? AF_INET6 : AF_INET;
#ifdef HAVE_SA_LEN
    dst.ss_len = dstlen;
#endif

    psz_sdp = vlc_sdp_Start( VLC_OBJECT( p_media->p_vod ), "sout-rtp-",
                             NULL, 0, (struct sockaddr *)&dst, dstlen );
    if( psz_sdp == NULL )
        return NULL;

    if( p_media->i_length > 0 )
    {
        lldiv_t d = lldiv( p_media->i_length / 1000, 1000 );
        sdp_AddAttribute( &psz_sdp, "range"," npt=0-%lld.%03u", d.quot,
                          (unsigned)d.rem );
    }

    sdp_AddAttribute ( &psz_sdp, "control", "%s", rtsp_url );

    /* No locking needed, the ES table can't be modified now */
    for( int i = 0; i < p_media->i_es; i++ )
    {
        media_es_t *p_es = p_media->es[i];
        rtp_format_t *rtp_fmt = &p_es->rtp_fmt;
        const char *mime_major; /* major MIME type */

        switch( rtp_fmt->cat )
        {
            case VIDEO_ES:
                mime_major = "video";
                break;
            case AUDIO_ES:
                mime_major = "audio";
                break;
            case SPU_ES:
                mime_major = "text";
                break;
            default:
                continue;
        }

        sdp_AddMedia( &psz_sdp, mime_major, "RTP/AVP", 0,
                      rtp_fmt->payload_type, false, 0,
                      rtp_fmt->ptname, rtp_fmt->clock_rate, rtp_fmt->channels,
                      rtp_fmt->fmtp );

        char *track_url = RtspAppendTrackPath( p_es->rtsp_id, rtsp_url );
        if( track_url != NULL )
        {
            sdp_AddAttribute ( &psz_sdp, "control", "%s", track_url );
            free( track_url );
        }
    }

    return psz_sdp;
}

int vod_check_range(vod_media_t *p_media, const char *psz_session,
                    int64_t start, int64_t end)
{
    (void) psz_session;

    if (p_media->i_length > 0 && (start > p_media->i_length
                                  || end > p_media->i_length))
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/* TODO: add support in the VLM for queueing proper PLAY requests with
 * start and end times, fetch whether the input is seekable... and then
 * clean this up */
void vod_play(vod_media_t *p_media, const char *psz_session,
              int64_t *start, int64_t end)
{
    if (vod_check_range(p_media, psz_session, *start, end) != VLC_SUCCESS)
        return;

    /* We're passing the #vod{} sout chain here */
    vod_MediaControl(p_media->p_vod, p_media, psz_session,
                     VOD_MEDIA_PLAY, "vod", start);
}

void vod_pause(vod_media_t *p_media, const char *psz_session, int64_t *npt)
{
    vod_MediaControl(p_media->p_vod, p_media, psz_session,
                     VOD_MEDIA_PAUSE, npt);
}

void vod_stop(vod_media_t *p_media, const char *psz_session)
{
    CommandPush(p_media->p_vod, RTSP_CMD_TYPE_STOP, p_media, psz_session);
}


const char *vod_get_mux(const vod_media_t *p_media)
{
    return p_media->psz_mux;
}


/* Match an RTP id to a VoD media ES and RTSP track to initialize it
 * with the data that was already set up */
int vod_init_id(vod_media_t *p_media, const char *psz_session, int es_id,
                sout_stream_id_t *sout_id, rtp_format_t *rtp_fmt,
                uint32_t *ssrc, uint16_t *seq_init)
{
    media_es_t *p_es;

    if (p_media->psz_mux != NULL)
    {
        assert(p_media->i_es == 1);
        p_es = p_media->es[0];
    }
    else
    {
        p_es = NULL;
        /* No locking needed, the ES table can't be modified now */
        for (int i = 0; i < p_media->i_es; i++)
        {
            if (p_media->es[i]->es_id == es_id)
            {
                p_es = p_media->es[i];
                break;
            }
        }
        if (p_es == NULL)
            return VLC_EGENERIC;
    }

    memcpy(rtp_fmt, &p_es->rtp_fmt, sizeof(*rtp_fmt));
    if (p_es->rtp_fmt.fmtp != NULL)
        rtp_fmt->fmtp = strdup(p_es->rtp_fmt.fmtp);

    return RtspTrackAttach(p_media->rtsp, psz_session, p_es->rtsp_id,
                           sout_id, ssrc, seq_init);
}

/* Remove references to the RTP id from its RTSP track */
void vod_detach_id(vod_media_t *p_media, const char *psz_session,
                   sout_stream_id_t *sout_id)
{
    RtspTrackDetach(p_media->rtsp, psz_session, sout_id);
}

