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

#include <assert.h>

void ProbePES( demux_t *p_demux, ts_pid_t *pid, const uint8_t *p_pesstart, size_t i_data, bool b_adaptfield )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint8_t *p_pes = p_pesstart;

    if( b_adaptfield )
    {
        if ( i_data < 2 )
            return;

        uint8_t len = *p_pes;
        p_pes++; i_data--;

        if(len == 0)
        {
            p_pes++; i_data--;/* stuffing */
        }
        else
        {
            if( i_data < len )
                return;
            if( len >= 7 && (p_pes[0] & 0x10) )
                pid->probed.i_pcr_count++;
            p_pes += len;
            i_data -= len;
        }
    }

    if( i_data < 9 )
        return;

    if( p_pes[0] != 0 || p_pes[1] != 0 || p_pes[2] != 1 )
        return;

    size_t i_pesextoffset = 8;
    stime_t i_dts = -1;
    if( p_pes[7] & 0x80 ) // PTS
    {
        i_pesextoffset += 5;
        if ( i_data < i_pesextoffset ||
            !ExtractPESTimestamp( &p_pes[9], p_pes[7] >> 6, &i_dts ) )
            return;
    }
    if( p_pes[7] & 0x40 ) // DTS
    {
        i_pesextoffset += 5;
        if ( i_data < i_pesextoffset ||
            !ExtractPESTimestamp( &p_pes[14], 0x01, &i_dts ) )
            return;
    }
    if( p_pes[7] & 0x20 ) // ESCR
        i_pesextoffset += 6;
    if( p_pes[7] & 0x10 ) // ESrate
        i_pesextoffset += 3;
    if( p_pes[7] & 0x08 ) // DSM
        i_pesextoffset += 1;
    if( p_pes[7] & 0x04 ) // CopyInfo
        i_pesextoffset += 1;
    if( p_pes[7] & 0x02 ) // PESCRC
        i_pesextoffset += 2;

    if ( i_data < i_pesextoffset )
        return;

     /* HeaderdataLength */
    const size_t i_payloadoffset = 8 + 1 + p_pes[8];
    i_pesextoffset += 1;

    if ( i_data < i_pesextoffset || i_data < i_payloadoffset )
        return;

    i_data -= 8 + 1 + p_pes[8];

    if( p_pes[7] & 0x01 ) // PESExt
    {
        size_t i_extension2_offset = 1;
        if ( p_pes[i_pesextoffset] & 0x80 ) // private data
            i_extension2_offset += 16;
        if ( p_pes[i_pesextoffset] & 0x40 ) // pack
            i_extension2_offset += 1;
        if ( p_pes[i_pesextoffset] & 0x20 ) // seq
            i_extension2_offset += 2;
        if ( p_pes[i_pesextoffset] & 0x10 ) // P-STD
            i_extension2_offset += 2;
        if ( p_pes[i_pesextoffset] & 0x01 ) // Extension 2
        {
            uint8_t i_len = p_pes[i_pesextoffset + i_extension2_offset] & 0x7F;
            i_extension2_offset += i_len;
        }
        if( i_data < i_extension2_offset )
            return;

        i_data -= i_extension2_offset;
    }
    /* (i_payloadoffset - i_pesextoffset) 0xFF stuffing */

    if ( i_data < 4 )
        return;

    const uint8_t *p_data = &p_pes[i_payloadoffset];
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
           (p_data[1] & 0x0C) != 0x04 && (p_data[1] & 0x03) == 0x00 )
        {
            pid->probed.i_fourcc = VLC_CODEC_MPGA;
        }
        else if( p_data[0] == 0xFF && (p_data[1] & 0xF2) == 0xF0 )
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

    /* Track timestamps and flag missing PAT */
    if( !p_sys->patfix.i_timesourcepid && i_dts > -1 )
    {
        p_sys->patfix.i_first_dts = i_dts;
        p_sys->patfix.i_timesourcepid = pid->i_pid;
    }
    else if( p_sys->patfix.i_timesourcepid == pid->i_pid && i_dts > -1 &&
             p_sys->patfix.status == PAT_WAITING )
    {
        if( i_dts - p_sys->patfix.i_first_dts > TO_SCALE(MIN_PAT_INTERVAL) )
            p_sys->patfix.status = PAT_MISSING;
    }

}

static void BuildPATCallback( void *p_opaque, block_t *p_block )
{
    ts_pid_t *pat_pid = (ts_pid_t *) p_opaque;
    dvbpsi_packet_push( pat_pid->u.p_pat->handle, p_block->p_buffer );
    block_Release( p_block );
}

static void BuildPMTCallback( void *p_opaque, block_t *p_block )
{
    ts_pid_t *program_pid = (ts_pid_t *) p_opaque;
    assert(program_pid->type == TYPE_PMT);
    while( p_block )
    {
        dvbpsi_packet_push( program_pid->u.p_pmt->handle,
                            p_block->p_buffer );
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

    const ts_pid_t *p_pid = NULL;
    ts_pid_next_context_t pidnextctx = ts_pid_NextContextInitValue;
    while( (p_pid = ts_pid_Next( &p_sys->pids, &pidnextctx )) )
    {
        if( !SEEN(p_pid) || p_pid->probed.i_fourcc == 0 )
            continue;

        if( i_pcr_pid == 0x1FFF && ( p_pid->probed.i_cat == AUDIO_ES ||
                                     p_pid->probed.i_pcr_count ) )
            i_pcr_pid = p_pid->i_pid;

        i_num_pes++;
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

    BuildPAT( GetPID(p_sys, 0)->u.p_pat->handle,
            &p_sys->pids.pat, BuildPATCallback,
            0, 1,
            &patstream,
            1, &pmtprogramstream, &i_program_number );

    /* PAT callback should have been triggered */
    if( p_program_pid->type != TYPE_PMT )
    {
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

        BuildPMT( GetPID(p_sys, 0)->u.p_pat->handle, VLC_OBJECT(p_demux),
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
}
