/*****************************************************************************
 * sdp.c: SDP parser and builtin UDP/RTP/RTSP
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: sdp.c,v 1.8 2003/09/08 13:09:40 fenrir Exp $
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

#include "codecs.h"

#include "/usr/src/libmtools/mc.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  DescribeOpen ( vlc_object_t * );
static void DescribeClose( vlc_object_t * );

static int  SDPOpen ( vlc_object_t * );
static void SDPClose( vlc_object_t * );

vlc_module_begin();
    set_description( _("SDP demuxer/reader") );
    set_capability( "demux", 100 );
    set_callbacks( SDPOpen, SDPClose );
    add_shortcut( "sdp" );

    add_submodule();
        set_description( _("RTSP/RTP describe") );
        add_shortcut( "rtp_sdp" );
        add_shortcut( "rtsp" );
        set_capability( "access", 0 );
        set_callbacks( DescribeOpen, DescribeClose );
vlc_module_end();

static ssize_t  DescribeRead( input_thread_t *, byte_t *, size_t );
static int      SDPDemux( input_thread_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct access_sys_t
{
    int  i_sdp;
    int  i_sdp_pos;
    char *psz_sdp;
};

struct demux_sys_t
{
    stream_t       *s;

    media_client_t *mc;

    /* try to detect end of stream */
    vlc_bool_t     b_received_data;
    int            i_no_data;
};

struct media_client_sys_t
{
    es_descriptor_t *es;

    int64_t         i_timestamp;
    vlc_bool_t      b_complete;
    pes_packet_t    *p_pes;
};

static void CodecFourcc( char *psz_name, uint8_t *pi_cat,vlc_fourcc_t *pi_fcc);

static void EsDecoderCreate( input_thread_t * p_input, media_client_es_t *p_es );
static int  EsDecoderSend  ( input_thread_t * p_input,
                             media_client_es_t *p_es, media_client_frame_t *p_frame );


/*****************************************************************************
 * DescribeOpen : create a SDP from an URL
 *****************************************************************************
 *
 *****************************************************************************/
static int  DescribeOpen( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys;
    char           *psz_uri;

    if( p_input->psz_access == NULL ||
        ( strcmp( p_input->psz_access, "rtsp" ) &&
          strcmp( p_input->psz_access, "rtp_sdp" ) ) )
    {
        msg_Dbg( p_input, "invalid access name" );
        return VLC_EGENERIC;
    }

    /* We cannot directly use p_input->psz_source as we cannot accept
     * something like rtsp/<demuxer>:// */
    psz_uri = malloc( strlen( p_input->psz_access ) +
                      strlen( p_input->psz_name ) + 4 );
    if( !strcmp( p_input->psz_access, "rtp_sdp" ) )
    {
        sprintf( psz_uri, "rtp://%s", p_input->psz_name );
    }
    else
    {
        sprintf( psz_uri, "%s://%s", p_input->psz_access, p_input->psz_name );
    }

    msg_Dbg( p_input, "describing %s", psz_uri );

    p_sys = p_input->p_access_data = malloc( sizeof( access_sys_t ) );
    p_sys->i_sdp    = 0;
    p_sys->i_sdp_pos= 0;
    p_sys->psz_sdp  = media_client_describe_url( psz_uri );

    if( p_sys->psz_sdp == NULL )
    {
        msg_Err( p_input, "cannot describe %s", psz_uri );
        free( psz_uri );
        return VLC_EGENERIC;
    }
    free( psz_uri );
    p_sys->i_sdp = strlen( p_sys->psz_sdp );

    p_input->i_mtu = 0;

    /* Set exported functions */
    p_input->pf_read = DescribeRead;
    p_input->pf_seek = NULL;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->p_private = NULL;

    /* Finished to set some variable */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_FALSE;
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
 * DescribeClose
 *****************************************************************************
 *
 *****************************************************************************/
static void DescribeClose( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys   = p_input->p_access_data;

    free( p_sys->psz_sdp );
    free( p_sys );
}

/*****************************************************************************
 * DescribeRead
 *****************************************************************************
 *
 *****************************************************************************/
static ssize_t DescribeRead( input_thread_t * p_input,
                             byte_t * p_buffer, size_t i_len)
{
    access_sys_t   *p_sys   = p_input->p_access_data;
    int            i_copy = __MIN( (int)i_len,
                                   p_sys->i_sdp - p_sys->i_sdp_pos );
    if( i_copy > 0 )
    {
        memcpy( p_buffer, &p_sys->psz_sdp[p_sys->i_sdp_pos], i_copy );
        p_sys->i_sdp_pos += i_copy;
    }

    return i_copy;
}

/*****************************************************************************
 * SDPOpen:
 *****************************************************************************
 *
 *****************************************************************************/
static int SDPOpen( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;

    int            b_tcp = 0;   /* TODO */

    uint8_t        *p_peek;

    int            i_sdp;
    int            i_sdp_max;
    char           *psz_sdp;

    int               i_es;
    media_client_es_t **pp_es;
    int               i;

    char            *psz_uri;

    /* See if it looks like a SDP
       v, o, s fields are mandatory and in this order */
    if( input_Peek( p_input, &p_peek, 7 ) < 7 )
    {
        msg_Err( p_input, "cannot peek" );
        return VLC_EGENERIC;
    }
    if( strncmp( p_peek, "v=0\r\n", 5 ) &&
        strncmp( p_peek, "v=0\n", 4 ) )
    {
        msg_Err( p_input, "SDP module discarded" );
        return VLC_EGENERIC;
    }

    p_input->pf_demux = SDPDemux;
    p_input->pf_demux_control = demux_vaControlDefault;

    p_sys = p_input->p_demux_data = malloc( sizeof( demux_sys_t ) );
    p_sys->mc = media_client_create( b_tcp );
    if( ( p_sys->s = stream_OpenInput( p_input ) ) == NULL )
    {
        msg_Err( p_input, "cannot create stream" );
        goto error;
    }
    p_sys->i_no_data = 0;
    p_sys->b_received_data = VLC_FALSE;

    /* Read the complete SDP file */
    i_sdp = 0;
    i_sdp_max = 1024;
    psz_sdp = malloc( i_sdp_max );
    for( ;; )
    {
        int i_read;

        i_read = stream_Read( p_sys->s, &psz_sdp[i_sdp], i_sdp_max - i_sdp -1 );
        if( i_read < i_sdp_max - i_sdp -1 )
        {
            if( i_read > 0 )
            {
                i_sdp += i_read;
            }
            break;
        }
        i_sdp += i_read;
        i_sdp_max += 1024;
        psz_sdp = realloc( psz_sdp, i_sdp_max );
    }
    psz_sdp[i_sdp] = '\0';

    if( i_sdp == 0 )
    {
        msg_Err( p_input, "cannot read SDP file" );
        free( psz_sdp );
        goto error;
    }
    msg_Dbg( p_input, "------sdp file-----\n%s", psz_sdp );
    msg_Dbg( p_input, "-------------------" );

    /* We cannot directly use p_input->psz_source as we cannot accept
     * something like rtsp/<demuxer>:// */
    psz_uri = malloc( strlen( p_input->psz_access ) +
                      strlen( p_input->psz_name ) + 4 );
    sprintf( psz_uri, "%s://%s", p_input->psz_access, p_input->psz_name );

    if( media_client_add_sdp( p_sys->mc, psz_sdp, psz_uri ) )
    {
        msg_Err( p_input, "cannot add this description" );
        free( psz_sdp );
        goto error;
    }
    free( psz_sdp );

    media_client_es( p_sys->mc, &pp_es, &i_es );
    for( i = 0; i < i_es; i++ )
    {
        msg_Dbg( p_input, "es[%d] cat=%s codec=%s",
                 i, pp_es[i]->psz_cat, pp_es[i]->psz_codec );
    }


    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        goto error;
    }
    p_input->stream.pp_programs[0]->b_is_ok = 0;
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    p_input->stream.i_mux_rate = 0 / 50;
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );


    return VLC_SUCCESS;

error:
    media_client_release( p_sys->mc );

    if( p_sys->s )
    {
        stream_Release( p_sys->s );
    }
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * SDPClose:
 *****************************************************************************
 *
 *****************************************************************************/
static void SDPClose( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    int               i_es;
    media_client_es_t **pp_es;
    int               i;

    media_client_es( p_sys->mc, &pp_es, &i_es );
    for( i = 0; i < i_es; i++ )
    {
        msg_Dbg( p_input, "es[%d] cat=%s codec=%s",
                 i, pp_es[i]->psz_cat, pp_es[i]->psz_codec );

        if( pp_es[i]->p_sys )
        {
            free( pp_es[i]->p_sys );
        }
    }

    media_client_release( p_sys->mc );
    stream_Release( p_sys->s );
    free( p_sys );
}

/*****************************************************************************
 * SDPDemux:
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int SDPDemux( input_thread_t * p_input )
{
    demux_sys_t  *p_sys = p_input->p_demux_data;

    media_client_es_t    *p_es;
    media_client_frame_t *p_frame;


    while( !p_input->b_die && !p_input->b_error )
    {
        if( media_client_read( p_sys->mc, &p_es, &p_frame, 500 ) )
        {
            msg_Dbg( p_input, "no data" );
            p_sys->i_no_data++;
            if( p_sys->b_received_data && p_sys->i_no_data > 5 )
            {
                return 0;
            }
            return 1;
        }
        p_sys->b_received_data = VLC_TRUE;
        p_sys->i_no_data = 0;

        if( p_es == NULL )
        {
            continue;
        }

        if( p_es->p_sys == NULL )
        {
            EsDecoderCreate( p_input, p_es );
        }

        if( p_es->p_sys->es && p_es->p_sys->es->p_decoder_fifo &&
            p_frame->i_data > 0 )
        {
            EsDecoderSend( p_input, p_es, p_frame );
        }
        return 1;
    }

    return 0;
}



static struct
{
    char *psz_codec;

    int          i_cat;
    vlc_fourcc_t i_fcc;

} rtp_codec_map[] =
{
    { "L16",        AUDIO_ES,   VLC_FOURCC( 't', 'w', 'o', 's' ) },
    { "MPA",        AUDIO_ES,   VLC_FOURCC( 'm', 'p', 'g', 'a' ) },
    { "MP4V-ES",    VIDEO_ES,   VLC_FOURCC( 'm', 'p', '4', 'v' ) },
    { "MP4A-ES",    AUDIO_ES,   VLC_FOURCC( 'm', 'p', '4', 'a' ) },
/*  { "H263-1998",  VIDEO_ES,   VLC_FOURCC( 'h', '2', '6', '3' ) }, */
    { NULL,         UNKNOWN_ES, 0 }
};

static void CodecFourcc( char *psz_name, uint8_t *pi_cat, vlc_fourcc_t *pi_fcc)
{
    int i;
    for( i = 0; rtp_codec_map[i].psz_codec != NULL; i++ )
    {
        if( !strcasecmp( psz_name, rtp_codec_map[i].psz_codec ) )
        {
            break;
        }
    }
    *pi_cat = rtp_codec_map[i].i_cat;
    *pi_fcc = rtp_codec_map[i].i_fcc;
}

static void EsDecoderCreate( input_thread_t * p_input, media_client_es_t *p_es )
{
    msg_Dbg( p_input, "adding es cat=%s codec=%s",
             p_es->psz_cat, p_es->psz_codec );

    p_es->p_sys = malloc( sizeof( media_client_sys_t ) );
    p_es->p_sys->p_pes = NULL;
    p_es->p_sys->b_complete = VLC_FALSE;
    p_es->p_sys->i_timestamp = 0;
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( !strcmp( p_es->psz_cat, "audio" ) )
    {
        p_es->p_sys->es =
            input_AddES( p_input,
                         p_input->stream.p_selected_program,
                         1 , AUDIO_ES, NULL, 0 );
    }
    else if( !strcmp( p_es->psz_cat, "video" ) )
    {
        p_es->p_sys->es =
            input_AddES( p_input,
                         p_input->stream.p_selected_program,
                         1 , VIDEO_ES, NULL, 0 );
    }
    else
    {
        msg_Warn( p_input, "cat unsuported" );
        p_es->p_sys->es = NULL;
    }
    if( p_es->p_sys->es )
    {
        CodecFourcc( p_es->psz_codec,
                     &p_es->p_sys->es->i_cat,
                     &p_es->p_sys->es->i_fourcc );
        msg_Dbg( p_input, "cat=%d fcc=%4.4s",
                 p_es->p_sys->es->i_cat,
                 (char*)&p_es->p_sys->es->i_fourcc );
        if( p_es->p_sys->es->i_cat == AUDIO_ES )
        {
            WAVEFORMATEX *p_wf;
            int          i_size = sizeof( WAVEFORMATEX ) +
                                  p_es->i_extra_data;
            p_wf = malloc( i_size );
            p_wf->wFormatTag     = 0;
            p_wf->nChannels      = p_es->audio.i_channels;
            p_wf->nSamplesPerSec = p_es->audio.i_samplerate;
            p_wf->nBlockAlign    = 0;
            p_wf->wBitsPerSample = 0;
            if( !strcmp( p_es->psz_codec, "L16" ) )
            {
                p_wf->wBitsPerSample = 16;
            }

            p_wf->cbSize         = p_es->i_extra_data;
            if( p_es->i_extra_data > 0 )
            {
                memcpy( &p_wf[1], p_es->p_extra_data,
                        p_es->i_extra_data );
            }
            p_es->p_sys->es->p_waveformatex = (void*)p_wf;
            p_es->p_sys->es->p_bitmapinfoheader = NULL;
        }
        else if( p_es->p_sys->es->i_cat == VIDEO_ES )
        {
            BITMAPINFOHEADER *p_bih;
            int              i_size = sizeof( BITMAPINFOHEADER ) +
                                      p_es->i_extra_data;
            p_bih = malloc( i_size );
            p_bih->biSize       = i_size;
            p_bih->biWidth      = p_es->video.i_width;
            p_bih->biHeight     = p_es->video.i_height;
            p_bih->biPlanes     = 1;
            p_bih->biBitCount   = 24;
            p_bih->biCompression= 0;
            p_bih->biSizeImage  = 0;
            p_bih->biXPelsPerMeter = 0;
            p_bih->biYPelsPerMeter = 0;
            p_bih->biClrUsed       = 0;
            p_bih->biClrImportant  = 0;

            if( p_es->i_extra_data > 0 )
            {
                memcpy( &p_bih[1], p_es->p_extra_data,
                        p_es->i_extra_data );
            }
            p_es->p_sys->es->p_waveformatex = NULL;
            p_es->p_sys->es->p_bitmapinfoheader = (void*)p_bih;
        }
        input_SelectES( p_input, p_es->p_sys->es );
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

static int  EsDecoderSend  ( input_thread_t * p_input,
                             media_client_es_t *p_es, media_client_frame_t *p_frame )
{
    media_client_sys_t *tk = p_es->p_sys;
    data_packet_t *p_data;

    if( tk->p_pes && tk->b_complete )
    {
        tk->p_pes->i_dts = 0;
        tk->p_pes->i_pts = 0;

        if( tk->i_timestamp > 0 )
        {
            input_ClockManageRef( p_input,
                                  p_input->stream.p_selected_program,
                                  tk->i_timestamp * 9 / 100 );
            tk->p_pes->i_pts =
            tk->p_pes->i_dts =
                input_ClockGetTS( p_input,
                                  p_input->stream.p_selected_program,
                                  tk->i_timestamp * 9 / 100 );
        }
#if 0
        msg_Dbg( p_input, "codec=%s frame pts=%lld dts=%lld",
                 p_es->psz_codec,
                 tk->p_pes->i_pts, tk->p_pes->i_dts );
#endif

        input_DecodePES( tk->es->p_decoder_fifo, tk->p_pes );
        tk->p_pes = NULL;
    }

    if( tk->p_pes == NULL )
    {
        tk->b_complete = VLC_FALSE;
        tk->i_timestamp = p_frame->i_pts;
        tk->p_pes = input_NewPES( p_input->p_method_data );
        if( tk->p_pes == NULL )
        {
            msg_Err( p_input, "cannot allocate pes" );
            return -1;
        }
        tk->p_pes->i_rate = p_input->stream.control.i_rate;
        tk->p_pes->i_nb_data  = 0;
        tk->p_pes->i_pes_size = 0;
    }

    if( ( p_data = input_NewPacket( p_input->p_method_data,
                                    p_frame->i_data ) ) == NULL )
    {
        msg_Err( p_input, "cannot allocate data" );
        return -1;
    }
    p_data->p_payload_end = p_data->p_payload_start + p_frame->i_data;
    memcpy( p_data->p_payload_start, p_frame->p_data, p_frame->i_data);

    if( tk->p_pes->p_first == NULL )
    {
        tk->p_pes->p_first = p_data;
    }
    else
    {
        tk->p_pes->p_last->p_next = p_data;
    }
    tk->p_pes->p_last = p_data;
    tk->p_pes->i_pes_size += p_frame->i_data;
    tk->p_pes->i_nb_data++;

    tk->b_complete = p_frame->b_completed;
}

