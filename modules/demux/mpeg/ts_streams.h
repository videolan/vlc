/*****************************************************************************
 * ts_streams.h: Transport Stream input module for VLC.
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
#ifndef VLC_TS_STREAMS_H
#define VLC_TS_STREAMS_H

typedef struct ts_es_t ts_es_t;
typedef struct ts_pat_t ts_pat_t;
typedef struct ts_pmt_t ts_pmt_t;
typedef struct ts_stream_t ts_stream_t;
typedef struct ts_si_t  ts_si_t;
typedef struct ts_psip_t ts_psip_t;

typedef struct ts_stream_processor_t ts_stream_processor_t;
struct ts_stream_processor_t
{
    void *priv;
    void (*pf_delete)(ts_stream_processor_t *);
    void (*pf_reset)  (ts_stream_processor_t *);
    block_t * (*pf_push) (ts_stream_processor_t *, uint8_t, block_t * );
};

static inline void ts_stream_processor_Delete( ts_stream_processor_t *sp )
{
    if( sp )
        sp->pf_delete( sp );
}

static inline void ts_stream_processor_Reset( ts_stream_processor_t *sp )
{
    if( sp && sp->pf_reset )
        sp->pf_reset( sp );
}

static inline block_t * ts_stream_processor_Push( ts_stream_processor_t *sp, uint8_t i_stream_id, block_t *b )
{
    return (sp) ? sp->pf_push( sp, i_stream_id, b ) : b;
}

/* Structs */
ts_pat_t *ts_pat_New( demux_t * );
void ts_pat_Del( demux_t *, ts_pat_t * );
ts_pmt_t *ts_pat_Get_pmt( ts_pat_t *, uint16_t );

ts_pmt_t *ts_pmt_New( demux_t * );
void ts_pmt_Del( demux_t *, ts_pmt_t * );

ts_es_t * ts_es_New( ts_pmt_t * );
void ts_stream_Add_es( ts_stream_t *, ts_es_t *, bool );
ts_es_t * ts_stream_Extract_es( ts_stream_t *, const ts_pmt_t * );
ts_es_t * ts_stream_Find_es( ts_stream_t *, const ts_pmt_t * );
size_t ts_Count_es( const ts_es_t *, bool, const ts_pmt_t * );

ts_stream_t *ts_stream_New( demux_t *, ts_pmt_t * );
void ts_stream_Del( demux_t *, ts_stream_t * );

ts_si_t *ts_si_New( demux_t * );
void ts_si_Del( demux_t *, ts_si_t * );

ts_psip_t *ts_psip_New( demux_t * );
void ts_psip_Del( demux_t *, ts_psip_t * );

#endif
