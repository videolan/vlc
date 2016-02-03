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

#include "ts_pid.h"
#include "ts_scte.h"
#include "ts_streams_private.h"
#include "timestamps.h"

#include <assert.h>

void SCTE18_Section_Handler( demux_t *p_demux, ts_pid_t *pid, block_t *p_content )
{
    assert( pid->u.p_pes->p_es->fmt.i_codec == VLC_CODEC_SCTE_18 );
    ts_pmt_t *p_pmt = pid->u.p_pes->p_es->p_program;
    mtime_t i_date = TimeStampWrapAround( p_pmt->pcr.i_first, p_pmt->pcr.i_current );

    int i_priority = scte18_get_EAS_priority( p_content->p_buffer, p_content->i_buffer );
    msg_Dbg( p_demux, "Received EAS Alert with priority %d", i_priority );
    /* We need to extract the truncated pts stored inside the payload */
    ts_pes_es_t *p_es = pid->u.p_pes->p_es;
    if( p_es->id )
    {
        if( i_priority == EAS_PRIORITY_HIGH || i_priority == EAS_PRIORITY_MAX )
            es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, p_es->id, true );
        p_content->i_dts = p_content->i_pts = FROM_SCALE( i_date );
        es_out_Send( p_demux->out, p_es->id, p_content );
    }
    else
        block_Release( p_content );
}

void SCTE27_Section_Handler( demux_t *p_demux, ts_pid_t *pid, block_t *p_content )
{
    assert( pid->u.p_pes->p_es->fmt.i_codec == VLC_CODEC_SCTE_27 );
    ts_pmt_t *p_pmt = pid->u.p_pes->p_es->p_program;
    mtime_t i_date = p_pmt->pcr.i_current;

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
            mtime_t i_display_in = GetDWBE( &p_content->p_buffer[i_offset + 4] );
            if( i_display_in < i_date )
                i_date = i_display_in + (1ll << 32);
            else
                i_date = i_display_in;
        }

    }

    p_content->i_dts = p_content->i_pts = VLC_TS_0 + i_date * 100 / 9;
    //PCRFixHandle( p_demux, p_pmt, p_content );

    if( pid->u.p_pes->p_es->id )
        es_out_Send( p_demux->out, pid->u.p_pes->p_es->id, p_content );
    else
        block_Release( p_content );
}
