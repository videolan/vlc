/*****************************************************************************
 * ts.c: MPEG-II TS Muxer
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: ts.c,v 1.35 2003/11/17 14:46:37 massiot Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"
#include "bits.h"
#include "pes.h"

#if defined MODULE_NAME_IS_mux_ts_dvbpsi
#   ifdef HAVE_DVBPSI_DR_H
#       include <dvbpsi/dvbpsi.h>
#       include <dvbpsi/descriptor.h>
#       include <dvbpsi/pat.h>
#       include <dvbpsi/pmt.h>
#       include <dvbpsi/dr.h>
#       include <dvbpsi/psi.h>
#   else
#       include "dvbpsi.h"
#       include "descriptor.h"
#       include "tables/pat.h"
#       include "tables/pmt.h"
#       include "descriptors/dr.h"
#       include "psi.h"
#   endif
#endif

/*
 * TODO:
 *  - check PCR frequency requirement
 *  - check PAT/PMT  "        "
 *  - check PCR/PCR "soft"
 *  - check if "registration" descriptor : "AC-3" should be a program
 *    descriptor or an es one. (xine want an es one)
 *
 *  - remove creation of PAT/PMT without dvbpsi
 *  - ?
 * FIXME:
 *  - subtitle support is far from perfect. I expect some subtitles drop
 *    if they arrive a bit late
 *    (We cannot rely on the fact that the fifo should be full)
 */
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

vlc_module_begin();
#if defined MODULE_NAME_IS_mux_ts
    set_description( _("TS muxer") );
    set_capability( "sout mux", 100 );
    add_shortcut( "ts" );
    add_shortcut( "ts_nodvbpsi" );
#elif defined MODULE_NAME_IS_mux_ts_dvbpsi
    set_description( _("TS muxer (libdvbpsi)") );
    set_capability( "sout mux", 120 );
    add_shortcut( "ts" );
    add_shortcut( "ts_dvbpsi" );
#endif
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Capability(sout_mux_t *, int, void *, void * );
static int     AddStream( sout_mux_t *, sout_input_t * );
static int     DelStream( sout_mux_t *, sout_input_t * );
static int     Mux      ( sout_mux_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define SOUT_BUFFER_FLAGS_PRIVATE_PCR  ( 1 << SOUT_BUFFER_FLAGS_PRIVATE_SHIFT )
typedef struct
{
    int           i_depth;
    sout_buffer_t *p_first;
    sout_buffer_t **pp_last;
} sout_buffer_chain_t;

static inline void BufferChainInit  ( sout_buffer_chain_t *c )
{
    c->i_depth = 0;
    c->p_first = NULL;
    c->pp_last = &c->p_first;
}
static inline void BufferChainAppend( sout_buffer_chain_t *c, sout_buffer_t *b )
{
    *c->pp_last = b;
    c->i_depth++;

    while( b->p_next )
    {
        b = b->p_next;
        c->i_depth++;
    }
    c->pp_last = &b->p_next;
}
static inline sout_buffer_t *BufferChainGet( sout_buffer_chain_t *c )
{
    sout_buffer_t *b = c->p_first;

    if( b )
    {
        c->i_depth--;
        c->p_first = b->p_next;

        if( c->p_first == NULL )
        {
            c->pp_last = &c->p_first;
        }

        b->p_next = NULL;
    }
    return b;
}
static inline void BufferChainClean( sout_instance_t *p_sout, sout_buffer_chain_t *c )
{
    sout_buffer_t *b;

    while( ( b = BufferChainGet( c ) ) )
    {
        sout_BufferDelete( p_sout, b );
    }
    BufferChainInit( c );
}

typedef struct ts_stream_s
{
    int             i_pid;
    int             i_stream_type;
    int             i_stream_id;
    int             i_continuity_counter;

    /* to be used for carriege of DIV3 */
    vlc_fourcc_t    i_bih_codec;
    int             i_bih_width, i_bih_height;

    /* Specific to mpeg4 in mpeg2ts */
    int             i_es_id;

    int             i_decoder_specific_info;
    uint8_t         *p_decoder_specific_info;

    sout_buffer_chain_t chain_pes;
    mtime_t             i_pes_dts;
    mtime_t             i_pes_length;
    int                 i_pes_used;

} ts_stream_t;

struct sout_mux_sys_t
{
    int             i_pcr_pid;
    sout_input_t    *p_pcr_input;

    int             i_stream_id_mpga;
    int             i_stream_id_mpgv;

    int             i_audio_bound;
    int             i_video_bound;

    int             i_pid_video;
    int             i_pid_audio;
    int             i_pid_free; // first usable pid

    int             i_pat_version_number;
    ts_stream_t     pat;

    int             i_pmt_version_number;
    ts_stream_t     pmt;        // Up to now only one program

    int             i_mpeg4_streams;

    int             i_null_continuity_counter;  /* Needed ? */

    /* for TS building */
    int64_t             i_bitrate_min;
    int64_t             i_bitrate_max;

    int64_t             i_caching_delay;
    int64_t             i_pcr_delay;

    int64_t             i_dts_delay;

    mtime_t             i_pcr;  /* last PCR emited */
};


/* Reserve a pid and return it */
static int  AllocatePID( sout_mux_sys_t *p_sys, int i_cat )
{
    int i_pid;
    if ( i_cat == VIDEO_ES && p_sys->i_pid_video )
    {
        i_pid = p_sys->i_pid_video;
        p_sys->i_pid_video = 0;
    }
    else if ( i_cat == AUDIO_ES && p_sys->i_pid_audio )
    {
        i_pid = p_sys->i_pid_audio;
        p_sys->i_pid_audio = 0;
    }
    else
    {
        i_pid = ++p_sys->i_pid_free;
    }
    return i_pid;
}

static void GetPAT( sout_mux_t *p_mux, sout_buffer_chain_t *c );
static void GetPMT( sout_mux_t *p_mux, sout_buffer_chain_t *c );

static sout_buffer_t *TSNew( sout_mux_t *p_mux, ts_stream_t *p_stream, vlc_bool_t b_pcr );
static void TSSetPCR( sout_buffer_t *p_ts, mtime_t i_dts );

static void PEStoTS  ( sout_instance_t *, sout_buffer_chain_t *, sout_buffer_t *, ts_stream_t * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t          *p_mux =(sout_mux_t*)p_this;
    sout_mux_sys_t      *p_sys;
    char                *val;

    msg_Dbg( p_mux, "Open" );

    p_sys = malloc( sizeof( sout_mux_sys_t ) );

    p_mux->pf_capacity  = Capability;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys        = p_sys;
    p_mux->i_preheader  = 30; // really enough for a pes header

    srand( (uint32_t)mdate() );

    p_sys->i_stream_id_mpga = 0xc0;
    p_sys->i_stream_id_mpgv = 0xe0;

    p_sys->i_audio_bound = 0;
    p_sys->i_video_bound = 0;

    p_sys->i_pat_version_number = rand() % 32;
    p_sys->pat.i_pid = 0;
    p_sys->pat.i_continuity_counter = 0;

    p_sys->i_pmt_version_number = rand() % 32;
    p_sys->pmt.i_pid = 0x42;
    p_sys->pmt.i_continuity_counter = 0;

    p_sys->i_pid_free = 0x43;

    p_sys->i_pid_video = 0;
    if( ( val = sout_cfg_find_value( p_mux->p_cfg, "pid-video" ) ) )
    {
        p_sys->i_pid_video = strtol( val, NULL, 0 );
        if ( p_sys->i_pid_video > p_sys->i_pid_free )
        {
            p_sys->i_pid_free = p_sys->i_pid_video + 1;
        }
    }
    p_sys->i_pid_audio = 0;
    if( ( val = sout_cfg_find_value( p_mux->p_cfg, "pid-audio" ) ) )
    {
        p_sys->i_pid_audio = strtol( val, NULL, 0 );
        if ( p_sys->i_pid_audio > p_sys->i_pid_free )
        {
            p_sys->i_pid_free = p_sys->i_pid_audio + 1;
        }
    }

    p_sys->i_pcr_pid = 0x1fff;
    p_sys->p_pcr_input = NULL;

    p_sys->i_mpeg4_streams = 0;

    p_sys->i_null_continuity_counter = 0;

    /* Allow to create constrained stream */
    p_sys->i_bitrate_min = 0;
    p_sys->i_bitrate_max = 0;
    if( ( val = sout_cfg_find_value( p_mux->p_cfg, "bmin" ) ) )
    {
        p_sys->i_bitrate_min = atoll( val );
    }
    if( ( val = sout_cfg_find_value( p_mux->p_cfg, "bmax" ) ) )
    {
        p_sys->i_bitrate_max = atoll( val );
    }
    if( p_sys->i_bitrate_min > 0 && p_sys->i_bitrate_max > 0 &&
        p_sys->i_bitrate_min > p_sys->i_bitrate_max )
    {
        msg_Err( p_mux, "incompatible minimum and maximum bitrate, "
                 "disabling bitrate control" );
        p_sys->i_bitrate_min = 0;
        p_sys->i_bitrate_max = 0;
    }
    if( p_sys->i_bitrate_min > 0 || p_sys->i_bitrate_max > 0 )
    {
        msg_Err( p_mux, "bmin and bmax no more supported (if you need them report it)" );
    }

    p_sys->i_caching_delay = 200000;
    if( ( val = sout_cfg_find_value( p_mux->p_cfg, "caching" ) ) )
    {
        p_sys->i_caching_delay = (int64_t)atoi( val ) * 1000;
        if( p_sys->i_caching_delay <= 0 )
        {
            msg_Err( p_mux,
                     "invalid caching ("I64Fd"ms) reseting to 200ms",
                     p_sys->i_caching_delay / 1000 );
            p_sys->i_caching_delay = 200000;
        }
    }
    p_sys->i_pcr_delay = 30000;
    if( ( val = sout_cfg_find_value( p_mux->p_cfg, "pcr" ) ) )
    {
        p_sys->i_pcr_delay = (int64_t)atoi( val ) * 1000;
        if( p_sys->i_pcr_delay <= 0 ||
            p_sys->i_pcr_delay >= p_sys->i_caching_delay )
        {
            msg_Err( p_mux,
                     "invalid pcr delay ("I64Fd"ms) reseting to 30ms",
                     p_sys->i_pcr_delay / 1000 );
            p_sys->i_pcr_delay = 30000;
        }
    }

    msg_Dbg( p_mux, "caching="I64Fd" pcr="I64Fd,
             p_sys->i_caching_delay, p_sys->i_pcr_delay );

    p_sys->i_dts_delay = 200000;
    if( ( val = sout_cfg_find_value( p_mux->p_cfg, "dts-delay" ) ) )
    {
        p_sys->i_dts_delay = (int64_t)atoi( val ) * 1000;
    }

    /* for TS generation */
    p_sys->i_pcr    = 0;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t      *p_sys = p_mux->p_sys;

    msg_Dbg( p_mux, "Close" );

    free( p_sys );
}

/*****************************************************************************
 * Capability:
 *****************************************************************************/
static int Capability( sout_mux_t *p_mux, int i_query, void *p_args, void *p_answer )
{
   switch( i_query )
   {
        case SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:
            *(vlc_bool_t*)p_answer = VLC_TRUE;
            return( SOUT_MUX_CAP_ERR_OK );
        default:
            return( SOUT_MUX_CAP_ERR_UNIMPLEMENTED );
   }
}

/*****************************************************************************
 * AddStream: called for each stream addition
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    ts_stream_t         *p_stream;

    msg_Dbg( p_mux, "adding input codec=%4.4s", (char*)&p_input->p_fmt->i_fourcc );

    p_input->p_sys = (void*)p_stream = malloc( sizeof( ts_stream_t ) );

    /* Init this new stream */
    p_stream->i_pid = AllocatePID( p_sys, p_input->p_fmt->i_cat );
    p_stream->i_continuity_counter    = 0;
    p_stream->i_decoder_specific_info = 0;
    p_stream->p_decoder_specific_info = NULL;

    /* All others fields depand on codec */
    switch( p_input->p_fmt->i_cat )
    {
        case VIDEO_ES:
            switch( p_input->p_fmt->i_fourcc )
            {
                case VLC_FOURCC( 'm', 'p','g', 'v' ):
                    /* TODO: do we need to check MPEG-I/II ? */
                    p_stream->i_stream_type = 0x02;
                    p_stream->i_stream_id = p_sys->i_stream_id_mpgv;
                    p_sys->i_stream_id_mpgv++;
                    break;
                case VLC_FOURCC( 'm', 'p','4', 'v' ):
                    p_stream->i_stream_type = 0x10;
                    p_stream->i_stream_id = 0xfa;
                    p_sys->i_mpeg4_streams++;
                    p_stream->i_es_id = p_stream->i_pid;
                    break;
                /* XXX dirty dirty but somebody want that : using crapy MS-codec XXX */
                /* I didn't want to do that :P */
                case VLC_FOURCC( 'H', '2', '6', '3' ):
                case VLC_FOURCC( 'I', '2', '6', '3' ):
                case VLC_FOURCC( 'W', 'M', 'V', '2' ):
                case VLC_FOURCC( 'W', 'M', 'V', '1' ):
                case VLC_FOURCC( 'D', 'I', 'V', '3' ):
                case VLC_FOURCC( 'D', 'I', 'V', '2' ):
                case VLC_FOURCC( 'D', 'I', 'V', '1' ):
                case VLC_FOURCC( 'M', 'J', 'P', 'G' ):
                    p_stream->i_stream_type = 0xa0; // private
                    p_stream->i_stream_id = 0xa0;   // beurk
                    p_stream->i_bih_codec  = p_input->p_fmt->i_fourcc;
                    p_stream->i_bih_width  = p_input->p_fmt->i_width;
                    p_stream->i_bih_height = p_input->p_fmt->i_height;
                    break;
                default:
                    free( p_stream );
                    return VLC_EGENERIC;
            }
            p_sys->i_video_bound++;
            break;

        case AUDIO_ES:
            switch( p_input->p_fmt->i_fourcc )
            {
                case VLC_FOURCC( 'm', 'p','g', 'a' ):
                    p_stream->i_stream_type = p_input->p_fmt->i_sample_rate >= 32000 ? 0x03 : 0x04;
                    p_stream->i_stream_id = p_sys->i_stream_id_mpga;
                    p_sys->i_stream_id_mpga++;
                    break;
                case VLC_FOURCC( 'a', '5','2', ' ' ):
                    p_stream->i_stream_type = 0x81;
                    p_stream->i_stream_id = 0xbd;
                    break;
                case VLC_FOURCC( 'm', 'p','4', 'a' ):
                    p_stream->i_stream_type = 0x11;
                    p_stream->i_stream_id = 0xfa;
                    p_sys->i_mpeg4_streams++;
                    p_stream->i_es_id = p_stream->i_pid;
                    break;
                default:
                    free( p_stream );
                    return VLC_EGENERIC;
            }
            p_sys->i_audio_bound++;
            break;

        case SPU_ES:
            switch( p_input->p_fmt->i_fourcc )
            {
                case VLC_FOURCC( 's', 'p','u', ' ' ):
                    p_stream->i_stream_type = 0x82;
                    p_stream->i_stream_id = 0x82;
                    break;
                default:
                    free( p_stream );
                    return VLC_EGENERIC;
            }
            break;

        default:
            free( p_stream );
            return VLC_EGENERIC;
    }

    /* Copy extra data (VOL for MPEG-4 and extra BitMapInfoHeader for VFW */
    p_stream->i_decoder_specific_info = p_input->p_fmt->i_extra_data;
    if( p_stream->i_decoder_specific_info > 0 )
    {
        p_stream->p_decoder_specific_info =
            malloc( p_stream->i_decoder_specific_info );
        memcpy( p_stream->p_decoder_specific_info,
                p_input->p_fmt->p_extra_data,
                p_input->p_fmt->i_extra_data );
    }

    /* Init pes chain */
    BufferChainInit( &p_stream->chain_pes );
    p_stream->i_pes_dts    = 0;
    p_stream->i_pes_length = 0;
    p_stream->i_pes_used   = 0;

    /* We only change PMT version (PAT isn't changed) */
    p_sys->i_pmt_version_number = ( p_sys->i_pmt_version_number + 1 )%32;

    /* Update pcr_pid */
    if( p_input->p_fmt->i_cat != SPU_ES &&
        ( p_sys->i_pcr_pid == 0x1fff || p_input->p_fmt->i_cat == VIDEO_ES ) )
    {
        if( p_sys->p_pcr_input )
        {
            /* There was already a PCR stream, so clean context */
            /* FIXME */
        }
        p_sys->i_pcr_pid   = p_stream->i_pid;
        p_sys->p_pcr_input = p_input;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream: called before a stream deletion
 *****************************************************************************/
static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    ts_stream_t     *p_stream;
    char            *val;

    msg_Dbg( p_mux, "removing input" );
    p_stream = (ts_stream_t*)p_input->p_sys;

    if( p_sys->i_pcr_pid == p_stream->i_pid )
    {
        int i;

        /* Find a new pcr stream (Prefer Video Stream) */
        p_sys->i_pcr_pid = 0x1fff;
        p_sys->p_pcr_input = NULL;
        for( i = 0; i < p_mux->i_nb_inputs; i++ )
        {
            if( p_mux->pp_inputs[i] == p_input )
            {
                continue;
            }

            if( p_mux->pp_inputs[i]->p_fmt->i_cat == VIDEO_ES )
            {
                p_sys->i_pcr_pid  =
                    ((ts_stream_t*)p_mux->pp_inputs[i]->p_sys)->i_pid;
                p_sys->p_pcr_input= p_mux->pp_inputs[i];
                break;
            }
            else if( p_mux->pp_inputs[i]->p_fmt->i_cat != SPU_ES &&
                     p_sys->i_pcr_pid == 0x1fff )
            {
                p_sys->i_pcr_pid  =
                    ((ts_stream_t*)p_mux->pp_inputs[i]->p_sys)->i_pid;
                p_sys->p_pcr_input= p_mux->pp_inputs[i];
            }
        }
        if( p_sys->p_pcr_input )
        {
            /* Empty TS buffer */
            /* FIXME */
        }
    }

    /* Empty all data in chain_pes */
    BufferChainClean( p_mux->p_sout, &p_stream->chain_pes );

    if( p_stream->p_decoder_specific_info )
    {
        free( p_stream->p_decoder_specific_info );
    }
    if( p_stream->i_stream_id == 0xfa || p_stream->i_stream_id == 0xfb )
    {
        p_sys->i_mpeg4_streams--;
    }
    if( ( val = sout_cfg_find_value( p_mux->p_cfg, "pid-video" ) ) )
    {
        int i_pid_video = strtol( val, NULL, 0 );
        if ( i_pid_video == p_stream->i_pid )
        {
            p_sys->i_pid_video = i_pid_video;
        }
    }
    if( ( val = sout_cfg_find_value( p_mux->p_cfg, "pid-audio" ) ) )
    {
        int i_pid_audio = strtol( val, NULL, 0 );
        if ( i_pid_audio == p_stream->i_pid )
        {
            p_sys->i_pid_audio = i_pid_audio;
        }
    }
    free( p_stream );

    /* We only change PMT version (PAT isn't changed) */
    p_sys->i_pmt_version_number++; p_sys->i_pmt_version_number %= 32;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Mux: Call each time there is new data for at least one stream
 *****************************************************************************
 *
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    ts_stream_t     *p_pcr_stream;

    if( p_sys->i_pcr_pid == 0x1fff )
    {
        msg_Dbg( p_mux, "waiting PCR streams" );
        msleep( 1000 );
        return VLC_SUCCESS;
    }
    p_pcr_stream = (ts_stream_t*)p_sys->p_pcr_input->p_sys;

    for( ;; )
    {
        sout_buffer_chain_t chain_ts;
        int                 i_packet_count;
        int                 i_packet_pos;
        mtime_t             i_pcr_dts;
        mtime_t             i_pcr_length;
        int i;

        /* 1: get enough PES packet for all input */
        for( ;; )
        {
            vlc_bool_t b_ok = VLC_TRUE;
            sout_buffer_t *p_data;

            /* Accumulate enough data in the pcr stream (>i_caching_delay) */
            /* Accumulate enough data in all other stream ( >= length of pcr) */
            for( i = 0; i < p_mux->i_nb_inputs; i++ )
            {
                sout_input_t *p_input = p_mux->pp_inputs[i];
                ts_stream_t *p_stream = (ts_stream_t*)p_input->p_sys;

                if( p_stream->i_pes_length <= p_sys->i_caching_delay ||
                    p_stream->i_pes_dts + p_stream->i_pes_length < p_pcr_stream->i_pes_dts + p_pcr_stream->i_pes_length )
                {
                    /* Need more data */
                    if( p_input->p_fifo->i_depth <= 1 )
                    {
                        if( p_input->p_fmt->i_cat == AUDIO_ES ||
                            p_input->p_fmt->i_cat == VIDEO_ES )
                        {
                            /* We need more data */
                            return VLC_SUCCESS;
                        }
                        else if( p_input->p_fifo->i_depth <= 0 )
                        {
                            /* spu, only one packet is needed */
                            continue;
                        }
                    }
                    b_ok = VLC_FALSE;

                    p_data = sout_FifoGet( p_input->p_fifo );

                    if( ( p_pcr_stream->i_pes_dts > 0 && p_data->i_dts - 2000000 > p_pcr_stream->i_pes_dts + p_pcr_stream->i_pes_length ) ||
                        p_data->i_dts < p_stream->i_pes_dts ||
                        ( p_stream->i_pes_dts > 0 && p_data->i_dts - 2000000 > p_stream->i_pes_dts + p_stream->i_pes_length ) )
                    {
                        msg_Warn( p_mux, "packet with too strange dts (dts=%lld,old=%lld,pcr=%lld)",
                                  p_data->i_dts,
                                  p_stream->i_pes_dts,
                                  p_pcr_stream->i_pes_dts );
                        sout_BufferDelete( p_mux->p_sout, p_data );

                        BufferChainClean( p_mux->p_sout, &p_stream->chain_pes );
                        p_stream->i_pes_dts = 0;
                        p_stream->i_pes_used = 0;
                        p_stream->i_pes_length = 0;

                        BufferChainClean( p_mux->p_sout, &p_pcr_stream->chain_pes );
                        p_pcr_stream->i_pes_dts = 0;
                        p_pcr_stream->i_pes_used = 0;
                        p_pcr_stream->i_pes_length = 0;

                    }
                    else
                    {
                        p_stream->i_pes_length += p_data->i_length;
                        if( p_stream->i_pes_dts == 0 )
                        {
                            p_stream->i_pes_dts = p_data->i_dts;
                        }

                        /* Convert to pes */
                        E_( EStoPES )( p_mux->p_sout, &p_data, p_data, p_stream->i_stream_id, 1 );

                        BufferChainAppend( &p_stream->chain_pes, p_data );
                    }
                }
            }

            if( b_ok )
            {
                break;
            }
        }

        /* save */
        i_pcr_dts      = p_pcr_stream->i_pes_dts;
        i_pcr_length   = p_pcr_stream->i_pes_length;

        /* msg_Dbg( p_mux, "starting muxing %lldms", i_pcr_length / 1000 ); */
        /* 2: calculate non accurate total size of muxed ts */
        i_packet_count = 0;
        for( i = 0; i < p_mux->i_nb_inputs; i++ )
        {
            ts_stream_t *p_stream = (ts_stream_t*)p_mux->pp_inputs[i]->p_sys;
            sout_buffer_t *p_pes;

            /* False for pcr stream but it will be eough to do PCR algo */
            for( p_pes = p_stream->chain_pes.p_first; p_pes != NULL; p_pes = p_pes->p_next )
            {
                int i_size = p_pes->i_size;
                if( p_pes->i_dts + p_pes->i_length > p_pcr_stream->i_pes_dts + p_pcr_stream->i_pes_length )
                {
                    mtime_t i_frag = p_pcr_stream->i_pes_dts + p_pcr_stream->i_pes_length - p_pes->i_dts;
                    if( i_frag < 0 )
                    {
                        /* Next stream */
                        break;
                    }
                    i_size = p_pes->i_size * i_frag / p_pes->i_length;
                }
                i_packet_count += ( i_size + 183 ) / 184;
            }
        }
        /* add overhead for PCR (not really exact) */
        i_packet_count += ( 8 * i_pcr_length / p_sys->i_pcr_delay + 175 ) / 176;


        /* 3: mux PES into TS */
        BufferChainInit( &chain_ts );
        /* append PAT/PMT  -> FIXME with big pcr delay it won't have enough pat/pmt */
        GetPAT( p_mux, &chain_ts);
        GetPMT( p_mux, &chain_ts );
        i_packet_pos = 0;
        i_packet_count += chain_ts.i_depth;
        /* msg_Dbg( p_mux, "estimated pck=%d", i_packet_count ); */

        for( ;; )
        {
            int         i_stream;
            mtime_t     i_dts;
            ts_stream_t *p_stream;
            sout_buffer_t *p_ts;
            vlc_bool_t   b_pcr;

            /* Select stream (lowest dts)*/
            for( i = 0, i_stream = -1, i_dts = 0; i < p_mux->i_nb_inputs; i++ )
            {
                p_stream = (ts_stream_t*)p_mux->pp_inputs[i]->p_sys;

                if( p_stream->i_pes_dts == 0 )
                {
                    continue;
                }

                if( i_stream == -1 ||
                    p_stream->i_pes_dts < i_dts )
                {
                    i_stream = i;
                    i_dts = p_stream->i_pes_dts;
                }
            }
            if( i_stream == -1 )
            {
                break;
            }
            p_stream = (ts_stream_t*)p_mux->pp_inputs[i_stream]->p_sys;

            /* do we need to issue pcr */
            b_pcr = VLC_FALSE;
            if( p_stream == p_pcr_stream &&
                i_pcr_dts + i_packet_pos * i_pcr_length / i_packet_count >= p_sys->i_pcr + p_sys->i_pcr_delay )
            {
                b_pcr = VLC_TRUE;
                p_sys->i_pcr = i_pcr_dts + i_packet_pos * i_pcr_length / i_packet_count;
            }

            /* Build the TS packet */
            p_ts = TSNew( p_mux, p_stream, b_pcr );
            i_packet_pos++;

            /* */
            BufferChainAppend( &chain_ts, p_ts );
        }

        /* 4: date and send */
        i_packet_count = chain_ts.i_depth;
        /* msg_Dbg( p_mux, "real pck=%d", i_packet_count ); */
        for( i = 0; i < i_packet_count; i++ )
        {
            sout_buffer_t *p_ts = BufferChainGet( &chain_ts );

            p_ts->i_dts    = i_pcr_dts + i_pcr_length * i / i_packet_count;
            p_ts->i_length = i_pcr_length / i_packet_count;

            if( p_ts->i_flags&SOUT_BUFFER_FLAGS_PRIVATE_PCR )
            {
                /* msg_Dbg( p_mux, "pcr=%lld ms", p_ts->i_dts / 1000 ); */
                TSSetPCR( p_ts, p_ts->i_dts - p_sys->i_dts_delay );
            }

            /* latency */
            p_ts->i_dts += 3*p_sys->i_caching_delay/2;

            sout_AccessOutWrite( p_mux->p_access, p_ts );
        }
    }
}

static sout_buffer_t *TSNew( sout_mux_t *p_mux, ts_stream_t *p_stream, vlc_bool_t b_pcr )
{
    sout_buffer_t *p_pes = p_stream->chain_pes.p_first;
    sout_buffer_t *p_ts;

    vlc_bool_t b_new_pes = VLC_FALSE;
    vlc_bool_t b_adaptation_field = VLC_FALSE;

    int        i_payload_max = 184 - ( b_pcr ? 8 : 0 );
    int        i_payload;

    if( p_stream->i_pes_used <= 0 )
    {
        b_new_pes = VLC_TRUE;
    }
    i_payload = __MIN( (int)p_pes->i_size - p_stream->i_pes_used, i_payload_max );

    if( b_pcr || i_payload < i_payload_max )
    {
        b_adaptation_field = VLC_TRUE;
    }

    p_ts = sout_BufferNew( p_mux->p_sout, 188 );

    p_ts->p_buffer[0] = 0x47;
    p_ts->p_buffer[1] = ( b_new_pes ? 0x40 : 0x00 )|( ( p_stream->i_pid >> 8 )&0x1f );
    p_ts->p_buffer[2] = p_stream->i_pid & 0xff;
    p_ts->p_buffer[3] = ( b_adaptation_field ? 0x30 : 0x10 )|p_stream->i_continuity_counter;

    p_stream->i_continuity_counter = (p_stream->i_continuity_counter+1)%16;

    if( b_adaptation_field )
    {
        int i;

        if( b_pcr )
        {
            int     i_stuffing = i_payload_max - i_payload;

            p_ts->i_flags |= SOUT_BUFFER_FLAGS_PRIVATE_PCR;

            p_ts->p_buffer[4] = 7 + i_stuffing;
            p_ts->p_buffer[5] = 0x10;   /* flags */
            p_ts->p_buffer[6] = ( 0 )&0xff;
            p_ts->p_buffer[7] = ( 0 )&0xff;
            p_ts->p_buffer[8] = ( 0 )&0xff;
            p_ts->p_buffer[9] = ( 0 )&0xff;
            p_ts->p_buffer[10]= ( 0 )&0x80;
            p_ts->p_buffer[11]= 0;

            for( i = 12; i < 12 + i_stuffing; i++ )
            {
                p_ts->p_buffer[i] = 0xff;
            }
        }
        else
        {
            int i_stuffing = i_payload_max - i_payload;

            p_ts->p_buffer[4] = i_stuffing - 1;
            if( i_stuffing > 1 )
            {
                p_ts->p_buffer[5] = 0x00;
                for( i = 6; i < 6 + i_stuffing - 2; i++ )
                {
                    p_ts->p_buffer[i] = 0xff;
                }
            }
        }
    }

    /* copy payload */
    memcpy( &p_ts->p_buffer[188 - i_payload], &p_pes->p_buffer[p_stream->i_pes_used], i_payload );

    p_stream->i_pes_used += i_payload;
    p_stream->i_pes_dts      = p_pes->i_dts + p_pes->i_length * p_stream->i_pes_used / p_pes->i_size;
    p_stream->i_pes_length  -= p_pes->i_length * i_payload / p_pes->i_size;

    if( p_stream->i_pes_used >= (int)p_pes->i_size )
    {
        p_pes = BufferChainGet( &p_stream->chain_pes );
        sout_BufferDelete( p_mux->p_sout, p_pes );

        p_pes = p_stream->chain_pes.p_first;
        if( p_pes )
        {
            p_stream->i_pes_dts    = p_pes->i_dts;
            p_stream->i_pes_length = 0;
            while( p_pes )
            {
                p_stream->i_pes_length += p_pes->i_length;

                p_pes = p_pes->p_next;
            }
        }
        else
        {
            p_stream->i_pes_dts = 0;
            p_stream->i_pes_length = 0;
        }
        p_stream->i_pes_used = 0;
    }

    return p_ts;
}


static void TSSetPCR( sout_buffer_t *p_ts, mtime_t i_dts )
{
    mtime_t i_pcr = 9 * i_dts / 100;

    p_ts->p_buffer[6]  = ( i_pcr >> 25 )&0xff;
    p_ts->p_buffer[7]  = ( i_pcr >> 17 )&0xff;
    p_ts->p_buffer[8]  = ( i_pcr >> 9  )&0xff;
    p_ts->p_buffer[9]  = ( i_pcr >> 1  )&0xff;
    p_ts->p_buffer[10]|= ( i_pcr << 7  )&0x80;
}

#if 0
static void TSSetConstraints( sout_mux_t *p_mux, sout_buffer_chain_t *c, mtime_t i_length, int i_bitrate_min, int i_bitrate_max )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    sout_buffer_chain_t s = *c;

    int i_packets = 0;
    int i_packets_min = 0;
    int i_packets_max = 0;

    if( i_length <= 0 )
    {
        return;
    }

    i_packets     = c->i_depth;
    i_packets_min = ( (int64_t)i_bitrate_min * i_length / 8 / 1000000  + 187 ) / 188;
    i_packets_max = ( (int64_t)i_bitrate_max * i_length / 8 / 1000000  + 187 ) / 188;

    if( i_packets < i_packets_min && i_packets_min > 0 )
    {
        sout_buffer_t *p_pk;
        int i_div = ( i_packets_min - i_packets ) / i_packets;
        int i_mod = ( i_packets_min - i_packets ) % i_packets;
        int i_rest = 0;

        /* We need to pad with null packets (pid=0x1fff)
         * We try to melt null packets with true packets */
        msg_Dbg( p_mux,
                 "packets=%d but min=%d -> adding %d packets of padding",
                 i_packets, i_packets_min, i_packets_min - i_packets );

        BufferChainInit( c );
        while( ( p_pk = BufferChainGet( &s ) ) )
        {
            int i, i_null;

            BufferChainAppend( c, p_pk );

            i_null = i_div + ( i_rest + i_mod ) / i_packets;

            for( i = 0; i < i_null; i++ )
            {
                sout_buffer_t *p_null;

                p_null = sout_BufferNew( p_mux->p_sout, 188 );
                p_null->p_buffer[0] = 0x47;
                p_null->p_buffer[1] = 0x1f;
                p_null->p_buffer[2] = 0xff;
                p_null->p_buffer[3] = 0x10 | p_sys->i_null_continuity_counter;
                memset( &p_null->p_buffer[4], 0, 184 );
                p_sys->i_null_continuity_counter =
                    ( p_sys->i_null_continuity_counter + 1 ) % 16;

                BufferChainAppend( c, p_null );
            }

            i_rest = ( i_rest + i_mod ) % i_packets;
        }
    }
    else if( i_packets > i_packets_max && i_packets_max > 0 )
    {
        sout_buffer_t *p_pk;
        int           i;

        /* Arg, we need to drop packets, I don't do something clever (like
         * dropping complete pid, b frames, ... ), I just get the right amount
         * of packets and discard the others */
        msg_Warn( p_mux,
                  "packets=%d but max=%d -> removing %d packets -> stream broken",
                  i_packets, i_packets_max, i_packets - i_packets_max );

        BufferChainInit( c );
        for( i = 0; i < i_packets_max; i++ )
        {
            BufferChainAppend( c, BufferChainGet( &s ) );
        }

        while( ( p_pk = BufferChainGet( &s ) ) )
        {
            sout_BufferDelete( p_mux->p_sout, p_pk );
        }
    }
}
#endif

static void PEStoTS( sout_instance_t *p_sout,
                     sout_buffer_chain_t *c, sout_buffer_t *p_pes,
                     ts_stream_t *p_stream )
{
    uint8_t *p_data;
    int     i_size;
    int     b_new_pes;

    /* get PES total size */
    i_size = p_pes->i_size;
    p_data = p_pes->p_buffer;

    b_new_pes = VLC_TRUE;

    for( ;; )
    {
        int           b_adaptation_field;
        int           i_copy;
        sout_buffer_t *p_ts;

        p_ts = sout_BufferNew( p_sout, 188 );
        /* write header
         * 8b   0x47    sync byte
         * 1b           transport_error_indicator
         * 1b           payload_unit_start
         * 1b           transport_priority
         * 13b          pid
         * 2b           transport_scrambling_control
         * 2b           if adaptation_field 0x03 else 0x01
         * 4b           continuity_counter
         */

        i_copy    = __MIN( i_size, 184 );
        b_adaptation_field = i_size < 184 ? VLC_TRUE : VLC_FALSE;

        p_ts->p_buffer[0] = 0x47;
        p_ts->p_buffer[1] = ( b_new_pes ? 0x40 : 0x00 )|
                            ( ( p_stream->i_pid >> 8 )&0x1f );
        p_ts->p_buffer[2] = p_stream->i_pid & 0xff;
        p_ts->p_buffer[3] = ( b_adaptation_field ? 0x30 : 0x10 )|
                            p_stream->i_continuity_counter;

        b_new_pes = VLC_FALSE;
        p_stream->i_continuity_counter = (p_stream->i_continuity_counter+1)%16;

        if( b_adaptation_field )
        {
            int i_stuffing = 184 - i_copy;
            int i;

            p_ts->p_buffer[4] = i_stuffing - 1;
            if( i_stuffing > 1 )
            {
                p_ts->p_buffer[5] = 0x00;
                for( i = 6; i < 6 + i_stuffing - 2; i++ )
                {
                    p_ts->p_buffer[i] = 0xff;
                }
            }
        }
        /* copy payload */
        memcpy( &p_ts->p_buffer[188 - i_copy], p_data, i_copy );
        p_data += i_copy;
        i_size -= i_copy;

        BufferChainAppend( c, p_ts );

        if( i_size <= 0 )
        {
            sout_buffer_t *p_next = p_pes->p_next;

            p_pes->p_next = NULL;
            sout_BufferDelete( p_sout, p_pes );
            if( p_next == NULL )
            {
                break;
            }
            b_new_pes = VLC_TRUE;
            p_pes = p_next;
            i_size = p_pes->i_size;
            p_data = p_pes->p_buffer;
        }
    }

    return;
}

#if defined MODULE_NAME_IS_mux_ts
static uint32_t CalculateCRC( uint8_t *p_begin, int i_count )
{
    static uint32_t CRC32[256] =
    {
        0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
        0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
        0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
        0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
        0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
        0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
        0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
        0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
        0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
        0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
        0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
        0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
        0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
        0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
        0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
        0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
        0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
        0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
        0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
        0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
        0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
        0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
        0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
        0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
        0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
        0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
        0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
        0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
        0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
        0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
        0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
        0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
        0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
        0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
        0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
        0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
        0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
        0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
        0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
        0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
        0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
        0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
        0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
        0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
        0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
        0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
        0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
        0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
        0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
        0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
        0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
        0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
        0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
        0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
        0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
        0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
        0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
        0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
        0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
        0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
        0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
        0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
        0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
        0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
    };

    uint32_t i_crc = 0xffffffff;

    /* Calculate the CRC */
    while( i_count > 0 )
    {
        i_crc = (i_crc<<8) ^ CRC32[ (i_crc>>24) ^ ((uint32_t)*p_begin) ];
        p_begin++;
        i_count--;
    }

    return( i_crc );
}

static void GetPAT( sout_mux_t *p_mux,
                    sout_buffer_chain_t *c )
{
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    sout_buffer_t       *p_pat;
    bits_buffer_t bits;

    p_pat = sout_BufferNew( p_mux->p_sout, 1024 );

    p_pat->i_pts = 0;
    p_pat->i_dts = 0;
    p_pat->i_length = 0;

    bits_initwrite( &bits, 1024, p_pat->p_buffer );

    bits_write( &bits, 8, 0 );      // pointer
    bits_write( &bits, 8, 0x00 );   // table id
    bits_write( &bits, 1,  1 );     // section_syntax_indicator
    bits_write( &bits, 1,  0 );     // 0
    bits_write( &bits, 2,  0x03 );     // reserved FIXME
    bits_write( &bits, 12, 13 );    // XXX for one program only XXX 
    bits_write( &bits, 16, 0x01 );  // FIXME stream id
    bits_write( &bits, 2,  0x03 );     //  FIXME
    bits_write( &bits, 5,  p_sys->i_pat_version_number );
    bits_write( &bits, 1,  1 );     // current_next_indicator
    bits_write( &bits, 8,  0 );     // section number
    bits_write( &bits, 8,  0 );     // last section number

    bits_write( &bits, 16, 1 );     // program number
    bits_write( &bits,  3, 0x07 );     // reserved
    bits_write( &bits, 13, p_sys->pmt.i_pid );  // program map pid

    bits_write( &bits, 32, CalculateCRC( bits.p_data + 1, bits.i_data - 1) );

    p_pat->i_size = bits.i_data;

    PEStoTS( p_mux->p_sout, c, p_pat, &p_sys->pat );
}

static void GetPMT( sout_mux_t *p_mux,
                    sout_buffer_chain_t *c )
{
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    sout_buffer_t       *p_pmt;
    bits_buffer_t bits;
    int           i_stream;

    p_pmt = sout_BufferNew( p_mux->p_sout, 1024 );

    p_pmt->i_pts = 0;
    p_pmt->i_dts = 0;
    p_pmt->i_length = 0;

    bits_initwrite( &bits, 1024, p_pmt->p_buffer );

    bits_write( &bits, 8, 0 );      // pointer
    bits_write( &bits, 8, 0x02 );   // table id
    bits_write( &bits, 1,  1 );     // section_syntax_indicator
    bits_write( &bits, 1,  0 );     // 0
    bits_write( &bits, 2,  0 );     // reserved FIXME
    bits_write( &bits, 12, 13 + 5 * p_mux->i_nb_inputs );
    bits_write( &bits, 16, 1 );     // FIXME program number
    bits_write( &bits, 2,  0 );     //  FIXME
    bits_write( &bits, 5,  p_sys->i_pmt_version_number );
    bits_write( &bits, 1,  1 );     // current_next_indicator
    bits_write( &bits, 8,  0 );     // section number
    bits_write( &bits, 8,  0 );     // last section number

    bits_write( &bits,  3, 0 );     // reserved

    bits_write( &bits, 13, p_sys->i_pcr_pid );     //  FIXME FXIME PCR_PID FIXME
    bits_write( &bits,  4, 0 );     // reserved FIXME

    bits_write( &bits, 12, 0 );    // program info len FIXME

    for( i_stream = 0; i_stream < p_mux->i_nb_inputs; i_stream++ )
    {
        ts_stream_t *p_stream;

        p_stream = (ts_stream_t*)p_mux->pp_inputs[i_stream]->p_sys;

        bits_write( &bits,  8, p_stream->i_stream_type ); // stream_type
        bits_write( &bits,  3, 0 );                 // reserved
        bits_write( &bits, 13, p_stream->i_pid );   // es pid
        bits_write( &bits,  4, 0 );                 //reserved
        bits_write( &bits, 12, 0 );                 // es info len FIXME
    }

    bits_write( &bits, 32, CalculateCRC( bits.p_data + 1, bits.i_data - 1) );

    p_pmt->i_size = bits.i_data;

    PEStoTS( p_mux->p_sout, c, p_pmt, &p_sys->pmt );
}
#elif defined MODULE_NAME_IS_mux_ts_dvbpsi

static sout_buffer_t *WritePSISection( sout_instance_t *p_sout,
                                       dvbpsi_psi_section_t* p_section )
{
    sout_buffer_t   *p_psi, *p_first = NULL;


    while( p_section )
    {
        int             i_size;

        i_size =  (uint32_t)( p_section->p_payload_end - p_section->p_data )+
                  ( p_section->b_syntax_indicator ? 4 : 0 );

        p_psi = sout_BufferNew( p_sout, i_size + 1 );
        p_psi->i_pts = 0;
        p_psi->i_dts = 0;
        p_psi->i_length = 0;
        p_psi->i_size = i_size + 1;

        p_psi->p_buffer[0] = 0; // pointer
        memcpy( p_psi->p_buffer + 1,
                p_section->p_data,
                i_size );

        sout_BufferChain( &p_first, p_psi );

        p_section = p_section->p_next;
    }

    return( p_first );
}

static void GetPAT( sout_mux_t *p_mux,
                    sout_buffer_chain_t *c )
{
    sout_mux_sys_t       *p_sys = p_mux->p_sys;
    sout_buffer_t        *p_pat;
    dvbpsi_pat_t         pat;
    dvbpsi_psi_section_t *p_section;

    dvbpsi_InitPAT( &pat,
                    0x01,    // i_ts_id
                    p_sys->i_pat_version_number,
                    1 );      // b_current_next
    /* add all program (only one) */
    dvbpsi_PATAddProgram( &pat,
                          1,                    // i_number
                          p_sys->pmt.i_pid );   // i_pid

    p_section = dvbpsi_GenPATSections( &pat,
                                       0 );     // max program per section

    p_pat = WritePSISection( p_mux->p_sout, p_section );

    PEStoTS( p_mux->p_sout, c, p_pat, &p_sys->pat );

    dvbpsi_DeletePSISections( p_section );
    dvbpsi_EmptyPAT( &pat );
}

static uint32_t GetDescriptorLength24b( int i_length )
{
    uint32_t    i_l1, i_l2, i_l3;

    i_l1 = i_length&0x7f;
    i_l2 = ( i_length >> 7 )&0x7f;
    i_l3 = ( i_length >> 14 )&0x7f;

    return( 0x808000 | ( i_l3 << 16 ) | ( i_l2 << 8 ) | i_l1 );
}

static void GetPMT( sout_mux_t *p_mux,
                    sout_buffer_chain_t *c )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    sout_buffer_t   *p_pmt;

    dvbpsi_pmt_t        pmt;
    dvbpsi_pmt_es_t     *p_es;
    dvbpsi_psi_section_t *p_section;

    int                 i_stream;

    dvbpsi_InitPMT( &pmt,
                    0x01,   // program number
                    p_sys->i_pmt_version_number,
                    1,      // b_current_next
                    p_sys->i_pcr_pid );

    if( p_sys->i_mpeg4_streams > 0 )
    {
        uint8_t iod[4096];
        bits_buffer_t bits;
        bits_buffer_t bits_fix_IOD;

        /* Make valgrind happy : it works at byte level not bit one so
         * bit_write confuse it (but DON'T CHANGE the way that bit_write is
         * working (needed when fixing some bits) */
        memset( iod, 0, 4096 );

        bits_initwrite( &bits, 4096, iod );
        // IOD_label
        bits_write( &bits, 8,   0x01 );
        // InitialObjectDescriptor
        bits_align( &bits );
        bits_write( &bits, 8,   0x02 );     // tag
        bits_fix_IOD = bits;    // save states to fix length later
        bits_write( &bits, 24,  GetDescriptorLength24b( 0 ) ); // variable length (fixed later)
        bits_write( &bits, 10,  0x01 );     // ObjectDescriptorID
        bits_write( &bits, 1,   0x00 );     // URL Flag
        bits_write( &bits, 1,   0x00 );     // includeInlineProfileLevelFlag
        bits_write( &bits, 4,   0x0f );     // reserved
        bits_write( &bits, 8,   0xff );     // ODProfile (no ODcapability )
        bits_write( &bits, 8,   0xff );     // sceneProfile
        bits_write( &bits, 8,   0xfe );     // audioProfile (unspecified)
        bits_write( &bits, 8,   0xfe );     // visualProfile( // )
        bits_write( &bits, 8,   0xff );     // graphicProfile (no )
        for( i_stream = 0; i_stream < p_mux->i_nb_inputs; i_stream++ )
        {
            ts_stream_t *p_stream;
            p_stream = (ts_stream_t*)p_mux->pp_inputs[i_stream]->p_sys;

            if( p_stream->i_stream_id == 0xfa || p_stream->i_stream_id == 0xfb )
            {
                bits_buffer_t bits_fix_ESDescr, bits_fix_Decoder;
                /* ES descriptor */
                bits_align( &bits );
                bits_write( &bits, 8,   0x03 );     // ES_DescrTag
                bits_fix_ESDescr = bits;
                bits_write( &bits, 24,  GetDescriptorLength24b( 0 ) ); // variable size
                bits_write( &bits, 16,  p_stream->i_es_id );
                bits_write( &bits, 1,   0x00 );     // streamDependency
                bits_write( &bits, 1,   0x00 );     // URL Flag
                bits_write( &bits, 1,   0x00 );     // OCRStreamFlag
                bits_write( &bits, 5,   0x1f );     // streamPriority

                    // DecoderConfigDesciptor
                bits_align( &bits );
                bits_write( &bits, 8,   0x04 ); // DecoderConfigDescrTag
                bits_fix_Decoder = bits;
                bits_write( &bits, 24,  GetDescriptorLength24b( 0 ) );
                if( p_stream->i_stream_type == 0x10 )
                {
                    bits_write( &bits, 8, 0x20 );   // Visual 14496-2
                    bits_write( &bits, 6, 0x04 );   // VisualStream
                }
                else if( p_stream->i_stream_type == 0x11 )
                {
                    bits_write( &bits, 8, 0x40 );   // Audio 14496-3
                    bits_write( &bits, 6, 0x05 );   // AudioStream
                }
                else
                {
                    bits_write( &bits, 8, 0x00 );
                    bits_write( &bits, 6, 0x00 );

                    msg_Err( p_mux->p_sout,"Unsupported stream_type => broken IOD");
                }
                bits_write( &bits, 1,   0x00 );     // UpStream
                bits_write( &bits, 1,   0x01 );     // reserved
                bits_write( &bits, 24,  1024 * 1024 );  // bufferSizeDB
                bits_write( &bits, 32,  0x7fffffff );   // maxBitrate
                bits_write( &bits, 32,  0 );            // avgBitrate

                if( p_stream->i_decoder_specific_info > 0 )
                {
                    int i;
                    // DecoderSpecificInfo
                    bits_align( &bits );
                    bits_write( &bits, 8,   0x05 ); // tag
                    bits_write( &bits, 24,
                                GetDescriptorLength24b( p_stream->i_decoder_specific_info ) );
                    for( i = 0; i < p_stream->i_decoder_specific_info; i++ )
                    {
                        bits_write( &bits, 8,   ((uint8_t*)p_stream->p_decoder_specific_info)[i] );
                    }
                }
                /* fix Decoder length */
                bits_write( &bits_fix_Decoder, 24,
                            GetDescriptorLength24b( bits.i_data - bits_fix_Decoder.i_data - 3 ) );

                /* SLConfigDescriptor : predifined (0x01) */
                bits_align( &bits );
                bits_write( &bits, 8,   0x06 ); // tag
                bits_write( &bits, 24,  GetDescriptorLength24b( 8 ) );
                bits_write( &bits, 8,   0x01 ); // predefined
                bits_write( &bits, 1,   0 );   // durationFlag
                bits_write( &bits, 32,  0 );   // OCRResolution
                bits_write( &bits, 8,   0 );   // OCRLength
                bits_write( &bits, 8,   0 );   // InstantBitrateLength
                bits_align( &bits );

                /* fix ESDescr length */
                bits_write( &bits_fix_ESDescr, 24,
                            GetDescriptorLength24b( bits.i_data - bits_fix_ESDescr.i_data - 3 ) );
            }
        }
        bits_align( &bits );
        /* fix IOD length */
        bits_write( &bits_fix_IOD, 24,
                    GetDescriptorLength24b( bits.i_data - bits_fix_IOD.i_data - 3 ) );
        dvbpsi_PMTAddDescriptor( &pmt,
                                 0x1d,
                                 bits.i_data,
                                 bits.p_data );
    }

    for( i_stream = 0; i_stream < p_mux->i_nb_inputs; i_stream++ )
    {
        ts_stream_t *p_stream;

        p_stream = (ts_stream_t*)p_mux->pp_inputs[i_stream]->p_sys;

        p_es = dvbpsi_PMTAddES( &pmt,
                                p_stream->i_stream_type,
                                p_stream->i_pid );
        if( p_stream->i_stream_id == 0xfa || p_stream->i_stream_id == 0xfb )
        {
            uint8_t     es_id[2];

            /* SL descriptor */
            es_id[0] = (p_stream->i_es_id >> 8)&0xff;
            es_id[1] = (p_stream->i_es_id)&0xff;
            dvbpsi_PMTESAddDescriptor( p_es, 0x1f, 2, es_id );
        }
        else if( p_stream->i_stream_type == 0xa0 )
        {
            uint8_t     data[512];
            int         i_extra = __MIN( p_stream->i_decoder_specific_info,
                                        502 );

            /* private DIV3 descripor */
            memcpy( &data[0], &p_stream->i_bih_codec, 4 );
            data[4] = ( p_stream->i_bih_width >> 8 )&&0xff;
            data[5] = ( p_stream->i_bih_width      )&&0xff;
            data[6] = ( p_stream->i_bih_height>> 8 )&&0xff;
            data[7] = ( p_stream->i_bih_height     )&&0xff;
            data[8] = ( i_extra >> 8 )&&0xff;
            data[9] = ( i_extra      )&&0xff;
            if( i_extra > 0 )
            {
                memcpy( &data[10], p_stream->p_decoder_specific_info, i_extra );
            }

            /* 0xa0 is private */
            dvbpsi_PMTESAddDescriptor( p_es, 0xa0, i_extra  + 10, data );
        }
        else if( p_stream->i_stream_type == 0x81 )
        {
            uint8_t format[4] = { 0x41, 0x43, 0x2d, 0x33 };

            /* "registration" descriptor : "AC-3" */
            dvbpsi_PMTESAddDescriptor( p_es, 0x05, 4, format );
        }
    }

    p_section = dvbpsi_GenPMTSections( &pmt );

    p_pmt = WritePSISection( p_mux->p_sout, p_section );

    PEStoTS( p_mux->p_sout, c, p_pmt, &p_sys->pmt );

    dvbpsi_DeletePSISections( p_section );
    dvbpsi_EmptyPMT( &pmt );
}

#endif

