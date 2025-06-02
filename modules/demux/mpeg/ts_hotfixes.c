/*****************************************************************************
 * ts_hotfixes.c : MPEG PMT/PAT less streams fixups
 *****************************************************************************
 * Copyright (C) 2014-2016 - VideoLAN Authors
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_es.h>

#ifndef _DVBPSI_DVBPSI_H_
 #include <dvbpsi/dvbpsi.h>
#endif
#include <dvbpsi/descriptor.h>
#include <dvbpsi/pat.h>
#include <dvbpsi/pmt.h>

#include "../../mux/mpeg/streams.h"
#include "../../mux/mpeg/tsutil.h"
#include "../../mux/mpeg/tables.h"

#include "timestamps.h"
#include "pes.h"

#include "ts_streams.h"
#include "ts_psi.h"
#include "ts_pid.h"
#include "ts_streams_private.h"
#include "ts.h"
#include "ts_hotfixes.h"
#include "ts_packet.h"

#include <assert.h>

void ProbePES( demux_t *p_demux, ts_pid_t *pid, const block_t *p_pkt )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    unsigned i_skip = PKTHeaderAndAFSize( p_pkt );
    ts_90khz_t pktpcr = GetPCR( p_pkt );

    if( pktpcr != TS_90KHZ_INVALID )
        pid->probed.i_pcr_count++;

    size_t i_data = p_pkt->i_buffer - i_skip;
    const uint8_t *p_pes = &p_pkt->p_buffer[i_skip];

    ts_pes_header_t pesh;
    ts_pes_header_init( &pesh );
    if( ParsePESHeader( NULL, p_pes, i_data, &pesh ) != VLC_SUCCESS )
        return;

    if( pesh.i_dts != TS_90KHZ_INVALID )
        pid->probed.i_dts_count++;

    if( pid->probed.i_fourcc != 0 )
        goto codecprobingend;

    if( i_data < pesh.i_size + 4 )
        return;

    const uint8_t *p_data = &p_pes[pesh.i_size];
    const uint8_t i_stream_id = pid->probed.i_stream_id = p_pes[3];
    /* NON MPEG audio & subpictures STREAM */
    if(i_stream_id == 0xBD)
    {
        if( !memcmp( p_data, "\x7F\xFE\x80\x01", 4 ) )
        {
            pid->probed.i_fourcc = VLC_CODEC_DTS;
            pid->probed.i_cat = AUDIO_ES;
        }
        else if( !memcmp( p_data, "\x0B\x77", 2 ) )
        {
            pid->probed.i_fourcc = VLC_CODEC_EAC3;
            pid->probed.i_cat = AUDIO_ES;
        }
    }
    /* MPEG AUDIO STREAM */
    else if(i_stream_id >= 0xC0 && i_stream_id <= 0xDF)
    {
        pid->probed.i_cat = AUDIO_ES;
        if( p_data[0] == 0xFF && (p_data[1] & 0xE0) == 0xE0 &&
           (p_data[1] & 0x18) != 0x08 && (p_data[1] & 0x06) != 0x00 )
        {
            pid->probed.i_fourcc = VLC_CODEC_MPGA;
        }
        else if( p_data[0] == 0xFF && (p_data[1] & 0xF6) == 0xF0 )
        {
            pid->probed.i_fourcc = VLC_CODEC_MP4A; /* ADTS */
            pid->probed.i_original_fourcc = VLC_FOURCC('A','D','T','S');
        }
    }
    /* VIDEO STREAM */
    else if( i_stream_id >= 0xE0 && i_stream_id <= 0xEF )
    {
        pid->probed.i_cat = VIDEO_ES;
        if( !memcmp( p_data, "\x00\x00\x00\x01", 4 ) )
        {
            pid->probed.i_fourcc = VLC_CODEC_H264;
        }
        else if( !memcmp( p_data, "\x00\x00\x01", 4 ) )
        {
            pid->probed.i_fourcc = VLC_CODEC_MPGV;
        }
    }

codecprobingend:
    /* Track timestamps and flag missing PAT */
    if( !p_sys->patfix.i_timesourcepid && pesh.i_dts != TS_90KHZ_INVALID )
    {
        p_sys->patfix.i_first_dts = FROM_SCALE(pesh.i_dts);
        p_sys->patfix.i_timesourcepid = pid->i_pid;
    }
    else if( p_sys->patfix.i_timesourcepid == pid->i_pid && pesh.i_dts != TS_90KHZ_INVALID &&
             p_sys->patfix.status == PAT_WAITING )
    {
        if( FROM_SCALE(pesh.i_dts) - p_sys->patfix.i_first_dts > MIN_PAT_INTERVAL )
            p_sys->patfix.status = PAT_MISSING;
    }

}

static void BuildPATCallback( void *p_opaque, block_t *p_block )
{
    ts_pid_t *pat_pid = (ts_pid_t *) p_opaque;
    ts_psi_Packet_Push( pat_pid, p_block->p_buffer );
    block_Release( p_block );
}

static void BuildPMTCallback( void *p_opaque, block_t *p_block )
{
    ts_pid_t *program_pid = (ts_pid_t *) p_opaque;
    assert(program_pid->type == TYPE_PMT);
    while( p_block )
    {
        ts_psi_Packet_Push( program_pid, p_block->p_buffer );
        block_t *p_next = p_block->p_next;
        block_Release( p_block );
        p_block = p_next;
    }
}

void MissingPATPMTFixup( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_program_number = 1234;
    int i_program_pid = 1337;
    int i_pcr_pid = 0x1FFF;
    int i_num_pes = 0;

    ts_pid_t *p_program_pid = GetPID( p_sys, i_program_pid );
    if( SEEN(p_program_pid) )
    {
        /* Find a free one */
        for( i_program_pid = MIN_ES_PID;
             i_program_pid <= MAX_ES_PID && SEEN(p_program_pid);
             i_program_pid++ )
        {
            p_program_pid = GetPID( p_sys, i_program_pid );
        }
    }

    const ts_pid_t * candidates[4] = { NULL };
    const ts_pid_t *p_pid = NULL;
    ts_pid_next_context_t pidnextctx = ts_pid_NextContextInitValue;
    while( (p_pid = ts_pid_Next( &p_sys->pids, &pidnextctx )) )
    {
        if( !SEEN(p_pid) || p_pid->probed.i_fourcc == 0 )
            continue;

        if( p_pid->probed.i_pcr_count && candidates[0] == NULL && false )
            candidates[0] = p_pid;

        if( p_pid->probed.i_cat == AUDIO_ES &&
            (candidates[1] == NULL ||
             candidates[1]->probed.i_dts_count > p_pid->probed.i_dts_count) )
            candidates[1] = p_pid;

        if( candidates[2] == NULL && p_pid != candidates[1] &&
            p_pid->probed.i_dts_count > 0 )
            candidates[2] = p_pid;

        if( candidates[3] == NULL )
            candidates[3] = p_pid;

        i_num_pes++;
    }

    for(int i=0; i<4; i++)
    {
        if(!candidates[i])
            continue;
        i_pcr_pid = candidates[i]->i_pid;
        p_sys->patfix.b_pcrhasnopcrfield = (candidates[i]->probed.i_pcr_count < 1);
        break;
    }

    if( i_num_pes == 0 )
        return;

    tsmux_stream_t patstream =
    {
        .i_pid = 0,
        .i_continuity_counter = 0x10,
        .b_discontinuity = false
    };

    tsmux_stream_t pmtprogramstream =
    {
        .i_pid = i_program_pid,
        .i_continuity_counter = 0x0,
        .b_discontinuity = false
    };

    dvbpsi_t *handle = dvbpsi_new( NULL, DVBPSI_MSG_DEBUG );
    if( !handle )
        return;

    BuildPAT( handle,
            &p_sys->pids.pat, BuildPATCallback,
            0, 1,
            &patstream,
            1, &pmtprogramstream, &i_program_number );

    /* PAT callback should have been triggered */
    if( p_program_pid->type != TYPE_PMT )
    {
        dvbpsi_delete( handle );
        msg_Err( p_demux, "PAT creation failed" );
        return;
    }

    ts_mux_standard mux_standard = (p_sys->standard == TS_STANDARD_ATSC) ? TS_MUX_STANDARD_ATSC
                                                                         : TS_MUX_STANDARD_DVB;
    struct esstreams_t
    {
        pesmux_stream_t pes;
        tsmux_stream_t ts;
        es_format_t fmt;
    };

    struct esstreams_t *esstreams = calloc( i_num_pes, sizeof(struct esstreams_t) );
    pes_mapped_stream_t *mapped = calloc( i_num_pes, sizeof(pes_mapped_stream_t) );
    if( esstreams && mapped )
    {
        int j=0;
        for( int i=0; i<p_sys->pids.i_all; i++ )
        {
            p_pid = p_sys->pids.pp_all[i];

            if( !SEEN(p_pid) ||
                p_pid->probed.i_fourcc == 0 )
                continue;

            es_format_Init(&esstreams[j].fmt, p_pid->probed.i_cat, p_pid->probed.i_fourcc);
            esstreams[j].fmt.i_original_fourcc = p_pid->probed.i_original_fourcc;

            if( VLC_SUCCESS !=
                FillPMTESParams(mux_standard, &esstreams[j].fmt, &esstreams[j].ts, &esstreams[j].pes ) )
            {
                es_format_Clean( &esstreams[j].fmt );
                continue;
            }

            /* Important for correct remapping: Enforce probed PES stream id */
            esstreams[j].pes.i_stream_id = p_pid->probed.i_stream_id;

            esstreams[j].ts.i_pid = p_pid->i_pid;
            mapped[j].pes = &esstreams[j].pes;
            mapped[j].ts = &esstreams[j].ts;
            mapped[j].fmt = &esstreams[j].fmt;
            j++;
        }

        BuildPMT( handle, VLC_OBJECT(p_demux),
                 mux_standard,
                p_program_pid, BuildPMTCallback,
                0, 1,
                i_pcr_pid,
                NULL,
                1, &pmtprogramstream, &i_program_number,
                j, mapped );

        /* Cleanup */
        for( int i=0; i<j; i++ )
            es_format_Clean( &esstreams[i].fmt );
    }
    free(esstreams);
    free(mapped);

    dvbpsi_delete( handle );
}
