/*****************************************************************************
 * ndi.c: NDI support plugin
 *****************************************************************************
 * Copyright © 2025-2026 VideoLAN, VLC authors, and libnoidea authors
 *
 * Authors: Ahmed Hamed <ahmedhamed3699@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_interrupt.h>
#include <vlc_network.h>
#include <vlc_url.h>

#include <noidea/recv.h>

#define NDI_STREAM_TCP_PORT 5961

typedef struct {
    noidea_recv_ctx_t noidea_ctx;
    es_out_id_t *p_video_es;
    es_out_id_t *p_audio_es;
    vlc_tick_t i_pcr;
    vlc_tick_t i_last_video_pts;
    vlc_tick_t i_last_audio_pts;
    uint64_t i_video_timecode_base;
    uint64_t i_audio_timecode_base;
    uint32_t i_audio_bps;
} demux_sys_t;

static void update_pcr( demux_sys_t *p_sys, demux_t *p_demux )
{
    vlc_tick_t i_min;

    if( p_sys->i_last_video_pts == VLC_TICK_MIN )
        i_min = p_sys->i_last_audio_pts;
    else if( p_sys->i_last_audio_pts == VLC_TICK_MIN )
        i_min = p_sys->i_last_video_pts;
    else
        i_min = __MIN( p_sys->i_last_video_pts, p_sys->i_last_audio_pts );

    if( i_min > p_sys->i_pcr )
    {
        p_sys->i_pcr = i_min;
        es_out_SetPCR( p_demux->out, i_min );
    }
}

static vlc_tick_t timecode_to_pts( uint64_t *p_base, uint64_t i_timecode )
{
    if ( unlikely( *p_base == UINT64_MAX ) )
        *p_base = i_timecode;
    return VLC_TICK_0 + (vlc_tick_t)( i_timecode - *p_base ) / 10;
}

static uint32_t get_physical_channels( unsigned channels )
{
    switch ( channels )
    {
        case 1:  return AOUT_CHAN_CENTER;
        case 2:  return AOUT_CHANS_STEREO;
        case 4:  return AOUT_CHANS_4_0;
        case 5:  return AOUT_CHANS_5_0;
        case 6:  return AOUT_CHANS_5_1;
        case 8:  return AOUT_CHANS_7_1;
        default: return AOUT_CHANS_STEREO;
    }
}

/*****************************************************************************/

static int data_callback( noidea_packet_t *noidea_packet, void *user_data )
{
    demux_t *p_demux = user_data;
    demux_sys_t *p_sys = p_demux->p_sys;

    switch ( noidea_packet->type )
    {
        case NDI_DATA_VIDEO:
        {
            noidea_packet_video_t *video = (noidea_packet_video_t *) noidea_packet->packet;
            block_t *p_video_frame = NULL;
            if ( p_sys->p_video_es == NULL )
            {
                es_format_t video_format;
                vlc_fourcc_t fourcc;

                switch ( video->fourcc )
                {
                    case NDI_FOURCC_SPEEDHQ:
                        fourcc = VLC_CODEC_SPEEDHQ;
                        break;

                    default:
                        msg_Err( p_demux, "Unsupported video FourCC: %4.4s", (char *) &video->fourcc );
                        return -1;
                }

                es_format_Init( &video_format, VIDEO_ES, fourcc );
                video_format.video.i_width = video->width;
                video_format.video.i_height = video->height;
                video_format.video.i_frame_rate = video->fps_num;
                video_format.video.i_frame_rate_base = video->fps_den;

                p_sys->p_video_es = es_out_Add( p_demux->out, &video_format );
                if ( p_sys->p_video_es == NULL )
                {
                    msg_Err( p_demux, "Failed to add video ES" );
                    return -1;
                }
            }

            p_video_frame = block_heap_Alloc(video->data, video->size);
            if ( p_video_frame == NULL )
                return -1;

            video->data = NULL;
            p_video_frame->i_buffer = video->size;
            p_video_frame->i_pts = timecode_to_pts( &p_sys->i_video_timecode_base, video->timecode );
            p_video_frame->i_dts = p_video_frame->i_pts;
            if ( video->fps_num != 0 )
                p_video_frame->i_length = vlc_tick_from_frac( video->fps_den, video->fps_num );

            p_sys->i_last_video_pts = p_video_frame->i_pts;
            update_pcr( p_sys, p_demux );

            es_out_Send( p_demux->out, p_sys->p_video_es, p_video_frame );
            break;
        }

        case NDI_DATA_AUDIO:
        {
            noidea_packet_audio_t *audio = (noidea_packet_audio_t *) noidea_packet->packet;
            if ( audio->sample_rate == 0 )
            {
                msg_Warn( p_demux, "Audio packet with zero sample rate, skipping" );
                return 0;
            }

            block_t *p_audio_frame = NULL;
            if ( p_sys->p_audio_es == NULL )
            {
                es_format_t audio_format;
                vlc_fourcc_t fourcc;

                switch ( audio->fourcc )
                {
                    case NDI_FOURCC_FOWT:
                        p_sys->i_audio_bps = sizeof( float );
                        fourcc = VLC_CODEC_F32L;
                        break;

                    case NDI_FOURCC_SOWT:
                        p_sys->i_audio_bps = sizeof( int16_t );
                        fourcc = VLC_CODEC_S16L;
                        break;

                    default:
                        msg_Err( p_demux, "Unsupported audio FourCC: %4.4s", (char *) &audio->fourcc );
                        return -1;
                }

                es_format_Init( &audio_format, AUDIO_ES, fourcc );
                audio_format.audio.i_rate = audio->sample_rate;
                audio_format.audio.i_channels = audio->num_channels;
                audio_format.audio.i_bitspersample = p_sys->i_audio_bps * 8;
                audio_format.audio.i_physical_channels = get_physical_channels( audio->num_channels );

                p_sys->p_audio_es = es_out_Add( p_demux->out, &audio_format );
                if ( p_sys->p_audio_es == NULL )
                {
                    msg_Err( p_demux, "Failed to add audio ES" );
                    return -1;
                }
            }

            int buffer_size = audio->num_channels * audio->num_samples * p_sys->i_audio_bps;
            p_audio_frame = block_Alloc( buffer_size );
            if ( p_audio_frame == NULL )
                return -1;

            uint8_t *p_frame_buffer = p_audio_frame->p_buffer;
            for ( uint32_t s = 0; s < audio->num_samples; s++ )
            {
                for ( uint32_t c = 0; c < audio->num_channels; c++ )
                {
                    memcpy( p_frame_buffer, &audio->data[c][p_sys->i_audio_bps * s], p_sys->i_audio_bps );
                    p_frame_buffer += p_sys->i_audio_bps;
                }
            }
            p_audio_frame->i_buffer = buffer_size;
            p_audio_frame->i_nb_samples = audio->num_samples;
            p_audio_frame->i_length = vlc_tick_from_samples( audio->num_samples, audio->sample_rate );

            if ( p_sys->i_audio_bps == sizeof( float ) )
            {
                float *p = (float *) p_audio_frame->p_buffer;
                for ( uint32_t i = 0; i < audio->num_samples * audio->num_channels; i++ )
                    p[i] *= 10.0f;
            }

            p_audio_frame->i_pts = timecode_to_pts( &p_sys->i_audio_timecode_base, audio->timecode );
            p_audio_frame->i_dts = p_audio_frame->i_pts;

            p_sys->i_last_audio_pts = p_audio_frame->i_pts;
            update_pcr( p_sys, p_demux );

            es_out_Send( p_demux->out, p_sys->p_audio_es, p_audio_frame );
            break;
        }

        case NDI_DATA_METADATA:
        {
            noidea_packet_metadata_t *meta = (noidea_packet_metadata_t *)noidea_packet->packet;
            msg_Dbg( p_demux, "Received NDI metadata: %s", meta->data );
            break;
        }

        default:
            msg_Warn( p_demux, "Received unsupported NDI data type: %d", noidea_packet->type );
            return -1;
    }

    return 0;
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = (demux_sys_t *) p_demux->p_sys;

    struct pollfd fd;
    fd.fd = noidea_recv_get_socket_fd( p_sys->noidea_ctx );
    fd.events = noidea_recv_which_events( p_sys->noidea_ctx );

    if ( !noidea_recv_is_connected( p_sys->noidea_ctx ) )
        return VLC_DEMUXER_EOF;

    if ( vlc_poll_i11e( &fd, 1, -1 ) < 0 )
    {
        if ( errno == EINTR )
        {
            msg_Warn( p_demux, "NDI demux interrupted" );
            return VLC_DEMUXER_EOF;
        }
        else if ( errno == EBADF )
        {
            msg_Err( p_demux, "NDI socket closed unexpectedly" );
            return VLC_DEMUXER_EOF;
        }
        return VLC_DEMUXER_EGENERIC;
    }

    while ( noidea_recv_capture( p_sys->noidea_ctx ) < 0 && !vlc_killed() )
    {
        if ( errno != EAGAIN )
        {
            msg_Err( p_demux, "Failed to capture NDI data: %s", vlc_strerror_c(errno) );
            break;
        }

        fd.fd = noidea_recv_get_socket_fd( p_sys->noidea_ctx );
        fd.events = noidea_recv_which_events( p_sys->noidea_ctx );
        if ( vlc_poll_i11e( &fd, 1, -1 ) < 0 )
        {
            if ( errno != EINTR )
                return VLC_DEMUXER_EGENERIC;
            msg_Warn( p_demux, "NDI demux interrupted" );
        }
    }

    return VLC_DEMUXER_SUCCESS;
}

static int Control( demux_t *p_demux, int query, va_list args )
{
    demux_sys_t *p_sys = (demux_sys_t *) p_demux->p_sys;

    switch ( query )
    {
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = false;
            break;

        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_GET_POSITION:
        case DEMUX_SET_POSITION:
        case DEMUX_GET_LENGTH:
        case DEMUX_GET_TIME:
        case DEMUX_SET_TIME:
            return VLC_EGENERIC;

        case DEMUX_GET_NORMAL_TIME:
        {
            vlc_tick_t *pi_time = va_arg( args, vlc_tick_t * );
            *pi_time = p_sys->i_pcr;
            break;
        }

        case DEMUX_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS( var_InheritInteger( p_demux, "network-caching" ) );
            break;

        case DEMUX_GET_TYPE:
            *va_arg( args, int* ) = ITEM_TYPE_STREAM;
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int Open( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*) p_this;
    demux_sys_t *p_sys;

    if ( p_demux->b_preparsing || p_demux->out == NULL )
        return VLC_EGENERIC;

    p_sys = vlc_obj_malloc( p_this, sizeof( *p_sys ) );
    if ( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    vlc_url_t url;
    if ( vlc_UrlParse( &url, p_demux->psz_url ) )
        return -errno;

    noidea_opts opts = {
        .host = !EMPTY_STR(url.psz_host) ? url.psz_host : "localhost",
        .port = ( url.i_port > 0 ) ? url.i_port : NDI_STREAM_TCP_PORT,
        .initial_tally_state = NDI_TALLY_LIVE
    };
    noidea_recv_ctx_t noidea_ctx = noidea_recv_create( &opts, data_callback, p_demux, 1 );

    if ( noidea_ctx == NULL )
    {
        msg_Err( p_demux, "Failed to create NDI receive context" );
        goto error;
    }

    struct pollfd fd;
    int status = 0;
    int timeout = VLC_TICK_FROM_MS( var_InheritInteger( p_demux, "ipv4-timeout" ) );
    while ( noidea_recv_connect( noidea_ctx ) < 0 )
    {
        if ( errno != EINPROGRESS )
        {
            msg_Err( p_demux, "Failed to connect to NDI source %s:%d", opts.host, opts.port );
            goto error;
        }

        fd.fd = noidea_recv_get_socket_fd( noidea_ctx );
        fd.events = noidea_recv_which_events( noidea_ctx );
        if ( ( status = vlc_poll_i11e( &fd, 1, timeout ) ) <= 0 )
        {
            if ( status == 0 || errno == EINTR )
                continue;
            goto error;
        }

        socklen_t len = sizeof( status );
        if ( getsockopt( fd.fd, SOL_SOCKET, SO_ERROR, &status, &len ) < 0 )
        {
            msg_Err( p_demux, "Failed to connect to NDI source %s:%d", opts.host, opts.port );
            goto error;
        }
        if ( status == 0 )
            break;
    }

    while ( noidea_recv_send_metadata( noidea_ctx ) < 0 )
    {
        if ( errno != EAGAIN )
        {
            msg_Err( p_demux, "Failed to send metadata to NDI source %s:%d", opts.host, opts.port );
            goto error;
        }
        
        fd.fd = noidea_recv_get_socket_fd( noidea_ctx );
        fd.events = noidea_recv_which_events( noidea_ctx );
        if ( ( status = vlc_poll_i11e( &fd, 1, timeout ) ) <= 0 )
        {
            if ( status == 0 || errno == EINTR )
                continue;
            goto error;
        }
    }

    p_sys->p_video_es = NULL;
    p_sys->p_audio_es = NULL;
    p_sys->i_pcr = VLC_TICK_0;
    p_sys->i_last_video_pts = VLC_TICK_MIN;
    p_sys->i_last_audio_pts = VLC_TICK_MIN;
    p_sys->i_video_timecode_base = UINT64_MAX;
    p_sys->i_audio_timecode_base = UINT64_MAX;
    p_sys->noidea_ctx = noidea_ctx;
    p_demux->p_sys = p_sys;
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    vlc_UrlClean( &url );
    return VLC_SUCCESS;

error:
    if ( noidea_ctx )
        noidea_recv_close( noidea_ctx );
    vlc_UrlClean( &url );
    return VLC_EGENERIC;
}

static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*) p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    noidea_recv_close( p_sys->noidea_ctx );
}

vlc_module_begin ()
    set_description( N_("NDI® (Network Device Interface) input") )
    set_shortname( N_( "NDI" ) )
    set_capability( "access", 0 )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_shortcut( "ndi" )
    set_callbacks( Open, Close )
vlc_module_end ()
