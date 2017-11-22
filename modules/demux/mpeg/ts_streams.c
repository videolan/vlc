/*****************************************************************************
 * ts_streams.c: Transport Stream input module for VLC.
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

#include "ts_pid.h"
#include "ts_streams.h"
#include "ts_streams_private.h"

#ifndef _DVBPSI_DVBPSI_H_
 #include <dvbpsi/dvbpsi.h>
#endif
#ifndef _DVBPSI_DEMUX_H_
 #include <dvbpsi/demux.h>
#endif
#include <dvbpsi/descriptor.h>
#include <dvbpsi/pat.h>
#include <dvbpsi/pmt.h>
#include "../../mux/mpeg/dvbpsi_compat.h" /* dvbpsi_messages */

#include <vlc_demux.h>
#include <vlc_es.h>
#include <vlc_es_out.h>
#include <vlc_input.h>

#include "sections.h"
#include "ts_pid.h"
#include "ts.h"

#include "ts_psip.h"

static inline bool handle_Init( demux_t *p_demux, dvbpsi_t **handle )
{
    *handle = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
    if( !*handle )
        return false;
    (*handle)->p_sys = (void *) p_demux;
    return true;
}

ts_pat_t *ts_pat_New( demux_t *p_demux )
{
    ts_pat_t *pat = malloc( sizeof( ts_pat_t ) );
    if( !pat )
        return NULL;

    if( !handle_Init( p_demux, &pat->handle ) )
    {
        free( pat );
        return NULL;
    }

    pat->i_version  = -1;
    pat->i_ts_id    = -1;
    pat->b_generated = false;
    ARRAY_INIT( pat->programs );

    return pat;
}

void ts_pat_Del( demux_t *p_demux, ts_pat_t *pat )
{
    if( dvbpsi_decoder_present( pat->handle ) )
        dvbpsi_pat_detach( pat->handle );
    dvbpsi_delete( pat->handle );
    for( int i=0; i<pat->programs.i_size; i++ )
        PIDRelease( p_demux, pat->programs.p_elems[i] );
    ARRAY_RESET( pat->programs );
    free( pat );
}

ts_pmt_t *ts_pat_Get_pmt( ts_pat_t *pat, uint16_t i_number )
{
    ts_pmt_t *p_pmt = NULL;
    for( int i=0; i<pat->programs.i_size; i++ )
    {
        p_pmt = pat->programs.p_elems[i]->u.p_pmt;
        if( p_pmt->i_number == i_number )
            break;
    }
    return p_pmt;
}

ts_pmt_t *ts_pmt_New( demux_t *p_demux )
{
    ts_pmt_t *pmt = malloc( sizeof( ts_pmt_t ) );
    if( !pmt )
        return NULL;

    if( !handle_Init( p_demux, &pmt->handle ) )
    {
        free( pmt );
        return NULL;
    }

    ARRAY_INIT( pmt->e_streams );

    pmt->i_version  = -1;
    pmt->i_number   = -1;
    pmt->i_pid_pcr  = 0x1FFF;
    pmt->b_selected = false;
    pmt->iod        = NULL;
    pmt->od.i_version = -1;
    ARRAY_INIT( pmt->od.objects );

    pmt->i_last_dts = -1;
    pmt->i_last_dts_byte = 0;

    pmt->p_atsc_si_basepid      = NULL;
    pmt->p_si_sdt_pid = NULL;

    pmt->pcr.i_current = -1;
    pmt->pcr.i_first  = -1;
    pmt->pcr.b_disable = false;
    pmt->pcr.i_first_dts = VLC_TS_INVALID;
    pmt->pcr.i_pcroffset = -1;

    pmt->pcr.b_fix_done = false;

    pmt->eit.i_event_length = 0;
    pmt->eit.i_event_start = 0;

    pmt->arib.i_download_id = -1;
    pmt->arib.i_logo_id = -1;

    return pmt;
}

void ts_pmt_Del( demux_t *p_demux, ts_pmt_t *pmt )
{
    if( dvbpsi_decoder_present( pmt->handle ) )
        dvbpsi_pmt_detach( pmt->handle );
    dvbpsi_delete( pmt->handle );
    for( int i=0; i<pmt->e_streams.i_size; i++ )
        PIDRelease( p_demux, pmt->e_streams.p_elems[i] );
    ARRAY_RESET( pmt->e_streams );
    if( pmt->p_atsc_si_basepid )
        PIDRelease( p_demux, pmt->p_atsc_si_basepid );
    if( pmt->p_si_sdt_pid )
        PIDRelease( p_demux, pmt->p_si_sdt_pid );
    if( pmt->iod )
        ODFree( pmt->iod );
    for( int i=0; i<pmt->od.objects.i_size; i++ )
        ODFree( pmt->od.objects.p_elems[i] );
    ARRAY_RESET( pmt->od.objects );
    if( pmt->i_number > -1 )
        es_out_Control( p_demux->out, ES_OUT_DEL_GROUP, pmt->i_number );

    free( pmt );
}

ts_es_t * ts_es_New( ts_pmt_t *p_program )
{
    ts_es_t *p_es = malloc( sizeof(*p_es) );
    if( p_es )
    {
        p_es->p_program = p_program;
        p_es->id = NULL;
        p_es->i_sl_es_id = 0;
        p_es->i_next_block_flags = 0;
        p_es->p_extraes = NULL;
        p_es->p_next = NULL;
        p_es->b_interlaced = false;
        es_format_Init( &p_es->fmt, UNKNOWN_ES, 0 );
        p_es->fmt.i_group = p_program->i_number;
        p_es->metadata.i_format = 0;
        p_es->metadata.i_service_id = 0;
    }
    return p_es;
}

static void ts_pes_es_Clean( demux_t *p_demux, ts_es_t *p_es )
{
    if( p_es->id )
    {
        /* Ensure we don't wait for overlap hacks #14257 */
        es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, p_es->id, false );
        es_out_Del( p_demux->out, p_es->id );
        p_demux->p_sys->i_pmt_es--;
    }
    es_format_Clean( &p_es->fmt );
}

void ts_stream_Add_es( ts_stream_t *p_pes, ts_es_t *p_es, bool b_extra )
{
    ts_es_t **pp_es = (b_extra && p_pes->p_es) ?  /* Ensure extra has main es */
                           &p_pes->p_es->p_extraes :
                           &p_pes->p_es;
    if( likely(!*pp_es) )
    {
        *pp_es = p_es;
    }
    else
    {
        ts_es_t *p_next = (*pp_es)->p_next;
        (*pp_es)->p_next = p_es;
        p_es->p_next = p_next;
    }
}

ts_es_t * ts_stream_Find_es( ts_stream_t *p_pes, const ts_pmt_t *p_pmt )
{
    for( ts_es_t *p_es = p_pes->p_es; p_es; p_es = p_es->p_next )
    {
        if( p_es->p_program == p_pmt )
            return p_es;
    }
    return NULL;
}

ts_es_t * ts_stream_Extract_es( ts_stream_t *p_pes, const ts_pmt_t *p_pmt )
{
    ts_es_t **pp_prev = &p_pes->p_es;
    for( ts_es_t *p_es = p_pes->p_es; p_es; p_es = p_es->p_next )
    {
        if( p_es->p_program == p_pmt )
        {
            *pp_prev = p_es->p_next;
            p_es->p_next = NULL;
            return p_es;
        }
        pp_prev = &p_es->p_next;
    }
    return NULL;
}

size_t ts_Count_es( const ts_es_t *p_es, bool b_active, const ts_pmt_t *p_pmt )
{
    size_t i=0;
    for( ; p_es; p_es = p_es->p_next )
    {
        i += ( b_active ) ? !!p_es->id : ( ( !p_pmt || p_pmt == p_es->p_program ) ? 1 : 0 );
        i += ts_Count_es( p_es->p_extraes, b_active, p_pmt );
    }
    return i;
}

static void ts_pes_ChainDelete_es( demux_t *p_demux, ts_es_t *p_es )
{
    while( p_es )
    {
        ts_es_t *p_next = p_es->p_next;
        ts_pes_ChainDelete_es( p_demux, p_es->p_extraes );
        ts_pes_es_Clean( p_demux, p_es );
        free( p_es );
        p_es = p_next;
    }
}

ts_stream_t *ts_stream_New( demux_t *p_demux, ts_pmt_t *p_program )
{
    VLC_UNUSED(p_demux);
    ts_stream_t *pes = malloc( sizeof( ts_stream_t ) );
    if( !pes )
        return NULL;

    pes->p_es = ts_es_New( p_program );
    if( !pes->p_es )
    {
        free( pes );
        return NULL;
    }
    pes->i_stream_type = 0;
    pes->transport = TS_TRANSPORT_PES;
    pes->gather.i_data_size = 0;
    pes->gather.i_gathered = 0;
    pes->gather.p_data = NULL;
    pes->gather.pp_last = &pes->gather.p_data;
    pes->gather.i_saved = 0;
    pes->b_broken_PUSI_conformance = false;
    pes->b_always_receive = false;
    pes->p_sections_proc = NULL;
    pes->p_proc = NULL;
    pes->prepcr.p_head = NULL;
    pes->prepcr.pp_last = &pes->prepcr.p_head;

    return pes;
}

void ts_stream_Del( demux_t *p_demux, ts_stream_t *pes )
{
    ts_pes_ChainDelete_es( p_demux, pes->p_es );

    if( pes->gather.p_data )
        block_ChainRelease( pes->gather.p_data );

    if( pes->p_sections_proc )
        ts_sections_processor_ChainDelete( pes->p_sections_proc );

    if( pes->p_proc )
        ts_stream_processor_Delete( pes->p_proc );

    if( pes->prepcr.p_head )
        block_ChainRelease( pes->prepcr.p_head );

    free( pes );
}

ts_si_t *ts_si_New( demux_t *p_demux )
{
    ts_si_t *si = malloc( sizeof( ts_si_t ) );
    if( !si )
        return NULL;

    if( !handle_Init( p_demux, &si->handle ) )
    {
        free( si );
        return NULL;
    }

    si->i_version  = -1;
    si->eitpid = NULL;
    si->tdtpid = NULL;
    si->cdtpid = NULL;

    return si;
}

void ts_si_Del( demux_t *p_demux, ts_si_t *si )
{
    if( dvbpsi_decoder_present( si->handle ) )
        dvbpsi_DetachDemux( si->handle );
    dvbpsi_delete( si->handle );
    if( si->eitpid )
        PIDRelease( p_demux, si->eitpid );
    if( si->tdtpid )
        PIDRelease( p_demux, si->tdtpid );
    if( si->cdtpid )
        PIDRelease( p_demux, si->cdtpid );
    free( si );
}

void ts_psip_Del( demux_t *p_demux, ts_psip_t *psip )
{
    if( psip->p_ctx )
        ts_psip_context_Delete( psip->p_ctx );

    ts_pes_ChainDelete_es( p_demux, psip->p_eas_es );

    if( psip->handle )
    {
        ATSC_Detach_Dvbpsi_Decoders( psip->handle );
        dvbpsi_delete( psip->handle );
    }

    for( int i=0; i<psip->eit.i_size; i++ )
        PIDRelease( p_demux, psip->eit.p_elems[i] );
    ARRAY_RESET( psip->eit );

    free( psip );
}

ts_psip_t *ts_psip_New( demux_t *p_demux )
{
    ts_psip_t *psip = malloc( sizeof( ts_psip_t ) );
    if( !psip )
        return NULL;

    if( !handle_Init( p_demux, &psip->handle ) )
    {
        free( psip );
        return NULL;
    }

    ARRAY_INIT( psip->eit );
    psip->i_version  = -1;
    psip->p_eas_es = NULL;
    psip->p_ctx = ts_psip_context_New();
    if( !psip->p_ctx )
    {
        ts_psip_Del( p_demux, psip );
        psip = NULL;
    }

    return psip;
}
