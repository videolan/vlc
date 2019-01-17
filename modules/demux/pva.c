/*****************************************************************************
 * pva.c: PVA demuxer
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

#include "mpeg/pes.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("PVA demuxer" ) )
    set_capability( "demux", 10 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_callbacks( Open, Close )
    add_shortcut( "pva" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    es_out_id_t *p_video;
    es_out_id_t *p_audio;

    /* counter */
    int          i_vc;
    int          i_ac;

    /* audio/video block */
    block_t     *p_pes; /* audio */
    block_t     *p_es;  /* video */

    int64_t     b_pcr_audio;
} demux_sys_t;

static int  Demux   ( demux_t *p_demux );
static int  Control ( demux_t *p_demux, int i_query, va_list args );

static int  ReSynch ( demux_t * );
static void ParsePES( demux_t * );

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    es_format_t  fmt;
    const uint8_t *p_peek;

    if( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) < 8 ) return VLC_EGENERIC;
    if( p_peek[0] != 'A' || p_peek[1] != 'V' || p_peek[4] != 0x55 )
    {
        /* In case we had forced this demuxer we try to resynch */
        if( !p_demux->obj.force || ReSynch( p_demux ) )
            return VLC_EGENERIC;
    }

    p_sys = malloc( sizeof( demux_sys_t ) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys;

    /* Register one audio and one video stream */
    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_MPGA );
    fmt.b_packetized = false;
    p_sys->p_audio = es_out_Add( p_demux->out, &fmt );

    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_MPGV );
    fmt.b_packetized = false;
    p_sys->p_video = es_out_Add( p_demux->out, &fmt );

    p_sys->i_vc    = -1;
    p_sys->i_ac    = -1;
    p_sys->p_pes   = NULL;
    p_sys->p_es    = NULL;

    p_sys->b_pcr_audio = false;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    block_ChainRelease( p_sys->p_es );
    block_ChainRelease( p_sys->p_pes );

    free( p_sys );
}


/*****************************************************************************
 * Demux:
 *****************************************************************************
 * See http://multimedia.cx/mirror/av_format_v1.pdf
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    const uint8_t *p_peek;
    int         i_size;
    block_t     *p_frame;
    int64_t     i_pts;
    int         i_skip;

    if( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
    {
        msg_Warn( p_demux, "eof ?" );
        return VLC_DEMUXER_EOF;
    }
    if( p_peek[0] != 'A' || p_peek[1] != 'V' || p_peek[4] != 0x55 )
    {
        msg_Warn( p_demux, "lost synchro" );
        if( ReSynch( p_demux ) )
        {
            return VLC_DEMUXER_EGENERIC;
        }
        if( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
        {
            msg_Warn( p_demux, "eof ?" );
            return VLC_DEMUXER_EOF;
        }
    }

    i_size = GetWBE( &p_peek[6] );
    switch( p_peek[2] )
    {
        case 0x01:  /* VideoStream */
            if( p_sys->i_vc < 0 )
            {
                msg_Dbg( p_demux, "first packet for video" );
            }
            else if( ((p_sys->i_vc + 1)&0xff) != p_peek[3] )
            {
                msg_Dbg( p_demux, "packet lost (video)" );
                if( p_sys->p_es )
                {
                    block_ChainRelease( p_sys->p_es );
                    p_sys->p_es = NULL;
                }
            }
            p_sys->i_vc = p_peek[3];

            /* read the PTS and potential extra bytes TODO: make it a bit more optimised */
            i_pts = -1;
            i_skip = 8;
            if( p_peek[5]&0x10 )
            {
                int i_pre = p_peek[5]&0x3;

                if( ( p_frame = vlc_stream_Block( p_demux->s, 8 + 4 + i_pre ) ) )
                {
                    i_pts = GetDWBE( &p_frame->p_buffer[8] );
                    if( p_frame->i_buffer > 12 )
                    {
                        p_frame->p_buffer += 12;
                        p_frame->i_buffer -= 12;
                        block_ChainAppend( &p_sys->p_es, p_frame );
                    }
                    else
                    {
                        block_Release( p_frame );
                    }
                }
                i_size -= 4 + i_pre;
                i_skip  = 0;
                /* Set PCR */
                if( ( p_frame = p_sys->p_es ) )
                {

                    if( p_frame->i_pts != VLC_TICK_INVALID && !p_sys->b_pcr_audio )
                    {
                        es_out_SetPCR( p_demux->out, p_frame->i_pts);
                    }

                    p_frame = block_ChainGather( p_frame );
                    if( likely(p_frame) )
                        es_out_Send( p_demux->out, p_sys->p_video, p_frame );

                    p_sys->p_es = NULL;
                }
            }

            if( ( p_frame = vlc_stream_Block( p_demux->s, i_size + i_skip ) ) )
            {
                p_frame->p_buffer += i_skip;
                p_frame->i_buffer -= i_skip;
                if( i_pts >= 0 )
                    p_frame->i_pts = FROM_SCALE(i_pts);
                block_ChainAppend( &p_sys->p_es, p_frame );
            }
            break;

        case 0x02:  /* MainAudioStream */
            if( p_sys->i_ac < 0 )
            {
                msg_Dbg( p_demux, "first packet for audio" );
            }
            else if( ((p_sys->i_ac + 1)&0xff) != p_peek[3] )
            {
                msg_Dbg( p_demux, "packet lost (audio)" );
                if( p_sys->p_pes )
                {
                    block_ChainRelease( p_sys->p_pes );
                    p_sys->p_pes = NULL;
                }
            }
            p_sys->i_ac = p_peek[3];

            if( p_peek[5]&0x10 && p_sys->p_pes )
            {
                ParsePES( p_demux );
            }
            if( ( p_frame = vlc_stream_Block( p_demux->s, i_size + 8 ) ) )
            {
                p_frame->p_buffer += 8;
                p_frame->i_buffer -= 8;
                /* XXX this a hack, some streams aren't compliant and
                 * don't set pes_start flag */
                if( p_sys->p_pes && p_frame->i_buffer > 4 &&
                    p_frame->p_buffer[0] == 0x00 &&
                    p_frame->p_buffer[1] == 0x00 &&
                    p_frame->p_buffer[2] == 0x01 )
                {
                    ParsePES( p_demux );
                }
                block_ChainAppend( &p_sys->p_pes, p_frame );
            }
            break;

        default:
            msg_Warn( p_demux, "unknown id=0x%x", p_peek[2] );
            if( vlc_stream_Read( p_demux->s, NULL, i_size + 8 ) < i_size + 8 )
                return VLC_DEMUXER_EOF;
            break;
    }
    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    /* demux_sys_t *p_sys = p_demux->p_sys; */
    double f, *pf;
    int64_t i64;
    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            return vlc_stream_vaControl( p_demux->s, i_query, args );

        case DEMUX_GET_POSITION:
            if( ( i64 = stream_Size( p_demux->s ) ) > 0 )
            {
                pf = va_arg( args, double * );
                double current = vlc_stream_Tell( p_demux->s );
                *pf = current / (double)i64;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            i64 = stream_Size( p_demux->s );

            if( vlc_stream_Seek( p_demux->s, (int64_t)(i64 * f) ) || ReSynch( p_demux ) )
            {
                return VLC_EGENERIC;
            }
            return VLC_SUCCESS;

#if 0
        case DEMUX_GET_TIME:
            pi64 = va_arg( args, vlc_tick_t * );
            if( p_sys->i_time < 0 )
            {
                *pi64 = 0;
                return VLC_EGENERIC;
            }
            *pi64 = p_sys->i_time;
            return VLC_SUCCESS;

#if 0
        case DEMUX_GET_LENGTH:
            pi64 = va_arg( args, vlc_tick_t * );
            if( p_sys->i_mux_rate > 0 )
            {
                *pi64 = vlc_tick_from_samples( stream_Size( p_demux->s ) / 50, p_sys->i_mux_rate);
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;

#endif
        case DEMUX_GET_FPS:
            pf = va_arg( args, double * );
            *pf = (double)1000000.0 / (double)p_sys->i_pcr_inc;
            return VLC_SUCCESS;
#endif
        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

        case DEMUX_SET_TIME:
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * ReSynch:
 *****************************************************************************/
static int ReSynch( demux_t *p_demux )
{
    for( ;; )
    {
        const uint8_t *p_peek;
        int i_peek = vlc_stream_Peek( p_demux->s, &p_peek, 1024 );
        if( i_peek < 8 )
            break;

        int i_skip = 0;

        while( i_skip < i_peek - 5 )
        {
            if( p_peek[0] == 'A' && p_peek[1] == 'V' && p_peek[4] == 0x55 )
            {
                if( i_skip > 0
                 && vlc_stream_Read( p_demux->s, NULL, i_skip ) < i_skip )
                    return VLC_EGENERIC;
                return VLC_SUCCESS;
            }
            p_peek++;
            i_skip++;
        }

        if( vlc_stream_Read( p_demux->s, NULL, i_skip ) < i_skip )
            break;
    }

    return VLC_EGENERIC;
}

static void ParsePES( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_pes = p_sys->p_pes;
    uint8_t     hdr[30];

    unsigned    i_skip;
    stime_t     i_dts = -1;
    stime_t     i_pts = -1;

    p_sys->p_pes = NULL;

    /* FIXME find real max size */
    block_ChainExtract( p_pes, hdr, 30 );

    /* See §2.4.3.6 of ISO 13818-1 */
    if( hdr[0] != 0 || hdr[1] != 0 || hdr[2] != 1 )
    {
        msg_Warn( p_demux, "invalid hdr [0x%2.2x:%2.2x:%2.2x:%2.2x]",
                  hdr[0], hdr[1],hdr[2],hdr[3] );
        block_ChainRelease( p_pes );
        return;
    }
    // hdr[4] i_pes_size, 2 bytes
    // hdr[6] Marker -> original_or_copy

    /* we assume mpeg2 PES */
    i_skip = hdr[8] + 9;
    if( hdr[7]&0x80 )    /* has pts */
    {
        i_pts = GetPESTimestamp( &hdr[9] );

        if( hdr[7]&0x40 )    /* has dts */
        {
             i_dts = GetPESTimestamp( &hdr[14] );
        }
    }

    p_pes = block_ChainGather( p_pes );
    if( unlikely(p_pes == NULL) )
        return;
    if( p_pes->i_buffer <= i_skip )
    {
        block_ChainRelease( p_pes );
        return;
    }

    p_pes->i_buffer -= i_skip;
    p_pes->p_buffer += i_skip;

    if( i_dts >= 0 )
        p_pes->i_dts = FROM_SCALE(i_dts);
    if( i_pts >= 0 )
        p_pes->i_pts = FROM_SCALE(i_pts);

    /* Set PCR */
    if( p_pes->i_pts != VLC_TICK_INVALID )
    {
        es_out_SetPCR( p_demux->out, p_pes->i_pts);
        p_sys->b_pcr_audio = true;
    }
    es_out_Send( p_demux->out, p_sys->p_audio, p_pes );
}

