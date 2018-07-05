/*****************************************************************************
 * ts_scte.c: TS Demux SCTE section decoders/handlers
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
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
#include <dvbpsi/psi.h>

#include "ts_pid.h"
#include "ts_scte.h"
#include "ts_streams_private.h"
#include "timestamps.h"

#include <assert.h>

/* EAS Handling */
void SCTE18_Section_Callback( dvbpsi_t *p_handle, const dvbpsi_psi_section_t* p_section,
                              void *p_base_pid )
{
    demux_t *p_demux = (demux_t *) p_handle->p_sys;
    ts_pid_t *p_pid = (ts_pid_t *) p_base_pid;
    ts_psip_t *p_psip = p_pid->u.p_psip;
    if( p_pid->type != TYPE_PSIP || !p_psip->p_eas_es )
        return;

    for( ; p_section; p_section = p_section->p_next )
    {
        size_t i_payload = p_section->p_payload_end - p_section->p_payload_start;
        const int i_priority = scte18_get_EAS_priority( p_section->p_payload_start, i_payload );
        msg_Dbg( p_demux, "Received EAS Alert with priority %d", i_priority );

        if( i_priority != EAS_PRIORITY_HIGH && i_priority != EAS_PRIORITY_MAX )
            continue;

        for( ts_es_t *p_es = p_psip->p_eas_es; p_es; p_es = p_es->p_next )
        {
            if( !p_es->id && !(p_es->id = es_out_Add( p_demux->out, &p_es->fmt )) )
                continue;

            const ts_pmt_t *p_pmt = p_es->p_program;
            const stime_t i_date = TimeStampWrapAround( p_pmt->pcr.i_first, p_pmt->pcr.i_current );
            block_t *p_block = block_Alloc( p_section->p_payload_end - p_section->p_payload_start );
            memcpy( p_block->p_buffer, p_section->p_payload_start, i_payload );
            p_block->i_dts = p_block->i_pts = FROM_SCALE( i_date );

            es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, p_es->id, true );
            es_out_Send( p_demux->out, p_es->id, p_block );
        }
    }
}

void SCTE27_Section_Callback( demux_t *p_demux,
                              const uint8_t *p_sectiondata, size_t i_sectiondata,
                              const uint8_t *p_payloaddata, size_t i_payloaddata,
                              void *p_pes_cb_data )
{
    VLC_UNUSED(p_payloaddata); VLC_UNUSED(i_payloaddata);
    ts_stream_t *p_pes = (ts_stream_t *) p_pes_cb_data;
    assert( p_pes->p_es->fmt.i_codec == VLC_CODEC_SCTE_27 );
    ts_pmt_t *p_pmt = p_pes->p_es->p_program;
    stime_t i_date = p_pmt->pcr.i_current;

    block_t *p_content = block_Alloc( i_sectiondata );
    if( unlikely(!p_content) || unlikely(!p_pes->p_es->id) )
        return;
    memcpy( p_content->p_buffer, p_sectiondata, i_sectiondata );

    /* We need to extract the truncated pts stored inside the payload */
    int i_index = 0;
    size_t i_offset = 4;
    if( p_content->p_buffer[3] & 0x40 )
    {
        i_index = ((p_content->p_buffer[7] & 0x0f) << 8) | /* segment number */
                p_content->p_buffer[8];
        i_offset += 5;
    }
    if( i_index == 0 && p_content->i_buffer > i_offset + 8 ) /* message body */
    {
        bool is_immediate = p_content->p_buffer[i_offset + 3] & 0x40;
        if( !is_immediate )
        {
            stime_t i_display_in = GetDWBE( &p_content->p_buffer[i_offset + 4] );
            if( i_display_in < i_date )
                i_date = i_display_in + (1ll << 32);
            else
                i_date = i_display_in;
        }

    }

    p_content->i_dts = p_content->i_pts = FROM_SCALE(i_date);
    //PCRFixHandle( p_demux, p_pmt, p_content );

    if( p_pes->p_es->id )
        es_out_Send( p_demux->out, p_pes->p_es->id, p_content );
    else
        block_Release( p_content );
}
