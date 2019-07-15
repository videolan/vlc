/*****************************************************************************
 * ts_psip.c : TS demux ATSC A65 PSIP handling
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN Authors
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
#include <vlc_meta.h>
#include <vlc_epg.h>

#ifndef _DVBPSI_DVBPSI_H_
 #include <dvbpsi/dvbpsi.h>
#endif
#ifndef _DVBPSI_DEMUX_H_
 #include <dvbpsi/demux.h>
#endif
#include <dvbpsi/descriptor.h>
#include <dvbpsi/atsc_mgt.h>
#include <dvbpsi/atsc_vct.h>
#include <dvbpsi/atsc_eit.h>
#include <dvbpsi/atsc_ett.h>
#include <dvbpsi/atsc_stt.h>
#include <dvbpsi/dr_a0.h>
/* Custom decoders */
#include <dvbpsi/psi.h>
#include "ts_decoders.h"
#include "ts_psip_dvbpsi_fixes.h"

#include "timestamps.h"
#include "ts_pid.h"
#include "ts.h"
#include "ts_streams_private.h"
#include "ts_scte.h"

#include "ts_psip.h"

#include "../../codec/atsc_a65.h"
#include "../../codec/scte18.h"

#include <assert.h>

static inline char *grab_notempty( char **ppsz )
{
    char *psz_ret = NULL;
    if( *ppsz && **ppsz )
    {
        psz_ret = *ppsz;
        *ppsz = NULL;
    }
    return psz_ret;
}

/*
 * Decoders activation order due to dependencies,
 * and because callbacks will be fired once per MGT/VCT version
 * STT(ref by EIT,EAS) -> MGT
 * MGT -> VCT,EAS,EIT/ETT
 */

struct ts_psip_context_t
{
    dvbpsi_atsc_mgt_t *p_mgt; /* Used to match (EITx,ETTx)<->PIDn */
    dvbpsi_atsc_stt_t *p_stt; /* Time reference for EIT/EAS */
    dvbpsi_atsc_vct_t *p_vct; /* Required for EIT vchannel -> program remapping */
    atsc_a65_handle_t *p_a65; /* Shared Handle to avoid iconv reopens */
    uint16_t i_tabletype; /* Only used by EIT/ETT pid */
    DECL_ARRAY(dvbpsi_atsc_ett_t *) etts; /* For ETT pid, used on new EIT update */
    DECL_ARRAY(dvbpsi_atsc_eit_t *) eits; /* For EIT pid, used on new ETT update */
};

void ts_psip_Packet_Push( ts_pid_t *p_pid, const uint8_t *p_pktbuffer )
{
    if( p_pid->u.p_psip->handle->p_decoder && likely(p_pid->type == TYPE_PSIP) )
        dvbpsi_packet_push( p_pid->u.p_psip->handle, (uint8_t *) p_pktbuffer );
}

ts_psip_context_t * ts_psip_context_New()
{
    ts_psip_context_t *p_ctx = malloc(sizeof(*p_ctx));
    if(likely(p_ctx))
    {
        p_ctx->p_mgt = NULL;
        p_ctx->p_stt = NULL;
        p_ctx->p_vct = NULL;
        p_ctx->p_a65 = NULL;
        p_ctx->i_tabletype = 0;
        ARRAY_INIT(p_ctx->etts);
        ARRAY_INIT(p_ctx->eits);
    }
    return p_ctx;
}

void ts_psip_context_Delete( ts_psip_context_t *p_ctx )
{
    assert( !p_ctx->p_mgt || !p_ctx->etts.i_size );
    assert( !p_ctx->p_vct || !p_ctx->eits.i_size );

    if( p_ctx->p_mgt )
        dvbpsi_atsc_DeleteMGT( p_ctx->p_mgt );
    if( p_ctx->p_stt )
        dvbpsi_atsc_DeleteSTT( p_ctx->p_stt );
    if ( p_ctx->p_vct )
        dvbpsi_atsc_DeleteVCT( p_ctx->p_vct );
    if( p_ctx->p_a65 )
        atsc_a65_handle_Release( p_ctx->p_a65 );
    /* Things only used for ETT/EIT */
    for( int i=0; i<p_ctx->etts.i_size; i++ )
        dvbpsi_atsc_DeleteETT( p_ctx->etts.p_elems[i] );
    for( int i=0; i<p_ctx->eits.i_size; i++ )
        dvbpsi_atsc_DeleteEIT( p_ctx->eits.p_elems[i] );
    ARRAY_RESET( p_ctx->etts );
    ARRAY_RESET( p_ctx->eits );
    free( p_ctx );
}

static ts_pid_t *ATSC_GetSiblingxTTPID( ts_pid_list_t *p_list, const dvbpsi_atsc_mgt_t *p_mgt, ts_psip_t *p_psip )
{
    uint16_t i_lookup;
    assert( p_psip->p_ctx->i_tabletype );
    if( p_psip->p_ctx->i_tabletype >= ATSC_TABLE_TYPE_ETT_0 )
        i_lookup = p_psip->p_ctx->i_tabletype - ATSC_TABLE_TYPE_ETT_0 + ATSC_TABLE_TYPE_EIT_0;
    else
        i_lookup = p_psip->p_ctx->i_tabletype - ATSC_TABLE_TYPE_EIT_0 + ATSC_TABLE_TYPE_ETT_0;

    for( const dvbpsi_atsc_mgt_table_t *p_tab = p_mgt->p_first_table;
                                        p_tab; p_tab = p_tab->p_next )
    {
        if( p_tab->i_table_type == i_lookup )
            return ts_pid_Get( p_list, p_tab->i_table_type_pid );
    }
    return NULL;
}

static inline uint32_t toETMId( uint16_t i_vchannel, uint16_t i_event_id )
{
    return (i_vchannel << 16) | (i_event_id << 2) | 0x02;
}

static inline void fromETMId( uint32_t i_etm_id, uint16_t *pi_vchannel, uint16_t *pi_event_id )
{
    *pi_vchannel = i_etm_id >> 16;
    *pi_event_id = (i_etm_id & 0xFFFF) >> 2;
}

static const dvbpsi_atsc_ett_t * ATSC_ETTFindByETMId( ts_psip_context_t *p_ettctx, uint32_t i_etm_id, uint8_t i_version )
{
    int i;
    ARRAY_BSEARCH( p_ettctx->etts, ->i_etm_id, uint32_t, i_etm_id, i );
    if( i != -1 && p_ettctx->etts.p_elems[i]->i_version == i_version )
        return p_ettctx->etts.p_elems[i];
    return NULL;
}

static const dvbpsi_atsc_eit_event_t * ATSC_EventFindByETMId( ts_psip_context_t *p_eitctx,
                                                              uint32_t i_etm_id, uint8_t i_version )
{
    uint16_t i_vchannel_id, i_event_id;
    fromETMId( i_etm_id, &i_vchannel_id, &i_event_id );

    for( int i=0; i<p_eitctx->eits.i_size; i++ )
    {
        dvbpsi_atsc_eit_t *p_eit = p_eitctx->eits.p_elems[i];
        if( p_eit->i_version != i_version || p_eit->i_source_id != i_vchannel_id )
            continue;

        for( const dvbpsi_atsc_eit_event_t *p_evt = p_eit->p_first_event;
                                            p_evt ; p_evt = p_evt->p_next )
        {
            if( p_evt->i_event_id == i_event_id )
                return p_evt;
        }
    }
    return NULL;
}

static void ATSC_EITInsert( ts_psip_context_t *p_ctx, dvbpsi_atsc_eit_t *p_eit )
{
    for( int i=0; i<p_ctx->eits.i_size; i++ )
    {
        dvbpsi_atsc_eit_t *p_cur_eit = p_ctx->eits.p_elems[i];
        if( p_cur_eit->i_source_id == p_eit->i_source_id )
        {
            dvbpsi_atsc_DeleteEIT( p_cur_eit ); /* Updated version */
            p_ctx->eits.p_elems[i] = p_eit;
            return;
        }
    }
    ARRAY_APPEND( p_ctx->eits, p_eit );
}

static void ATSC_CleanETTByChannelVersion( ts_psip_context_t *p_ctx, uint16_t i_channel, uint8_t i_version )
{
    int i=0;
    while( i<p_ctx->etts.i_size )
    {
        dvbpsi_atsc_ett_t *p = p_ctx->etts.p_elems[i];
        uint16_t i_curchan = p->i_etm_id >> 16;
        if( i_channel <  i_curchan )
            break; /* because ordered */
        if( i_curchan == i_channel && p->i_version != i_version )
        {
            dvbpsi_atsc_DeleteETT( p );
            ARRAY_REMOVE( p_ctx->etts, i );
        }
        else i++;
    }
}

static void ATSC_InsertETTOrdered( ts_psip_context_t *p_ctx, dvbpsi_atsc_ett_t *p_ett )
{
    int i=0;
    for( ; i<p_ctx->etts.i_size; i++ )
    {
        dvbpsi_atsc_ett_t *p = p_ctx->etts.p_elems[i];
        if( p->i_etm_id >= p_ett->i_etm_id )
        {
            if( p->i_etm_id == p_ett->i_etm_id )
            {
                dvbpsi_atsc_DeleteETT( p );
                p_ctx->etts.p_elems[i] = p_ett;
                return;
            }
            break;
        }
    }
    ARRAY_INSERT( p_ctx->etts, p_ett, i );
}

static bool ATSC_TranslateVChannelToProgram( const dvbpsi_atsc_vct_t *p_vct,
                                             uint16_t i_channel, uint16_t *pi_program )
{
    for( const dvbpsi_atsc_vct_channel_t *p_channel = p_vct->p_first_channel;
                                          p_channel; p_channel = p_channel->p_next )
    {
        if( p_channel->i_source_id == i_channel )
        {
            *pi_program = p_channel->i_program_number;
            return true;
        }
    }
    return false;
}

static void ATSC_NewTable_Callback( dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
                                    uint16_t i_extension, void *p_pid );

/* Just Hook a base demux, and let NewTableCallback handle decoders creation */
static bool ATSC_Ready_SubDecoders( dvbpsi_t *p_handle, void *p_cb_pid )
{
    if( !dvbpsi_decoder_present( p_handle ) )
        return dvbpsi_AttachDemux( p_handle, ATSC_NewTable_Callback, p_cb_pid );
    return true;
}

void ATSC_Detach_Dvbpsi_Decoders( dvbpsi_t *p_handle )
{
    if( dvbpsi_decoder_present( p_handle ) )
        dvbpsi_DetachDemux( p_handle );
}

#define ATSC_ATTACH( handle, type, table, extension, pid ) \
    ( ATSC_Ready_SubDecoders( handle, pid ) &&\
      ( dvbpsi_demuxGetSubDec( (dvbpsi_demux_t *) handle->p_decoder, table, extension ) ||\
        dvbpsi_atsc_Attach ## type( handle, table, extension, ATSC_ ## type ## _Callback, pid ) ) )

#define ATSC_ATTACH_WITH_FIXED_DECODER( handle, type, table, extension, pid ) \
    ( ATSC_Ready_SubDecoders( handle, pid ) &&\
        ( dvbpsi_demuxGetSubDec( (dvbpsi_demux_t *) handle->p_decoder, table, extension ) ||\
          ts_dvbpsi_AttachRawSubDecoder( handle, table, extension, ATSC_ ## type ## _RawCallback, pid ) ) )

static const char * const rgpsz_ATSC_A53_service_types[] =
{
    "Analog Television",
    "ATSC Digital Television",
    "ATSC Audio",
    "ATSC Data Only Service",
    "ATSC Software Download Service",
};

static const char * ATSC_A53_get_service_type( uint8_t i_type )
{
    if( i_type == 0 || i_type > 5 )
        return NULL;
    return rgpsz_ATSC_A53_service_types[i_type - 1];
}

#ifndef ATSC_DEBUG_EIT
 #define EIT_DEBUG_TIMESHIFT(t)
#else
 /* Define static time var used as anchor to current time to offset all eit entries */
 static time_t i_eit_debug_offset = 0;
 #define EIT_DEBUG_TIMESHIFT(t) \
    do {\
        if( i_eit_debug_offset == 0 )\
            i_eit_debug_offset = time(NULL) - t;\
        t = t + i_eit_debug_offset;\
    } while(0);
#endif

static vlc_epg_event_t * ATSC_CreateVLCEPGEvent( demux_t *p_demux, ts_psip_context_t *p_basectx,
                                                 const dvbpsi_atsc_eit_event_t *p_evt,
                                                 const dvbpsi_atsc_ett_t *p_ett )
{
#ifndef ATSC_DEBUG_EIT
    VLC_UNUSED(p_demux);
#endif
    char *psz_title = atsc_a65_Decode_multiple_string( p_basectx->p_a65,
                                                       p_evt->i_title, p_evt->i_title_length );
    char *psz_shortdesc_text = NULL;
    char *psz_longdesc_text = NULL;
    vlc_epg_event_t *p_epgevt = NULL;

    time_t i_start = atsc_a65_GPSTimeToEpoch( p_evt->i_start_time, p_basectx->p_stt->i_gps_utc_offset );
    EIT_DEBUG_TIMESHIFT( i_start );

    for( const dvbpsi_descriptor_t *p_dr = p_evt->p_first_descriptor;
                                    p_dr; p_dr = p_dr->p_next )
    {
        switch( p_dr->i_tag )
        {
            case ATSC_DESCRIPTOR_CONTENT_ADVISORY:
            {
                const uint8_t *p_data = p_dr->p_data;
                size_t i_data = p_dr->i_length;
                uint8_t i_ratings_count = p_dr->p_data[0] & 0x3F;
                p_data++; i_data--;
                for( ; i_ratings_count && i_data > 3; i_ratings_count-- )
                {
                    uint8_t i_rated_dimensions = p_data[1];
                    if( (size_t) i_rated_dimensions * 2 + 3 > i_data ) /* one more sanity check */
                        break;

                    uint8_t desclen = p_data[(size_t) 2 + 2 * i_rated_dimensions];
                    p_data += (size_t) 3 + 2 * i_rated_dimensions;
                    i_data -= (size_t) 3 + 2 * i_rated_dimensions;
                    if( desclen > i_data )
                        break;

                    if( unlikely(psz_shortdesc_text) )
                        free( psz_shortdesc_text );
                    psz_shortdesc_text = atsc_a65_Decode_multiple_string( p_basectx->p_a65, p_data, desclen );
                    if( psz_shortdesc_text ) /* Only keep first for now */
                        break;
                    p_data += desclen;
                    i_data -= desclen;
                }
            }
            default:
                break;
        }
    }

    /* Try to match ETT */
    if( p_ett )
    {
        psz_longdesc_text = atsc_a65_Decode_multiple_string( p_basectx->p_a65,
                                                             p_ett->p_etm_data, p_ett->i_etm_length );
    }

    if( i_start != VLC_TICK_INVALID && psz_title )
    {
#ifdef ATSC_DEBUG_EIT
        msg_Dbg( p_demux, "EIT Event time %ld +%d %s id 0x%x",
                 i_start, p_evt->i_length_seconds, psz_title, p_evt->i_event_id );
#endif
        p_epgevt = vlc_epg_event_New( p_evt->i_event_id, i_start, p_evt->i_length_seconds );
        if( p_epgevt )
        {
            p_epgevt->psz_name = grab_notempty( &psz_title );
            p_epgevt->psz_short_description = grab_notempty( &psz_shortdesc_text );
            p_epgevt->psz_description = grab_notempty( &psz_longdesc_text );
        }
    }

    free( psz_title );
    free( psz_shortdesc_text );
    free( psz_longdesc_text );
    return p_epgevt;
}

static time_t ATSC_AddVLCEPGEvent( demux_t *p_demux, ts_psip_context_t *p_basectx,
                                   const dvbpsi_atsc_eit_event_t *p_event,
                                   const dvbpsi_atsc_ett_t *p_ett,
                                   vlc_epg_t *p_epg )
{
    vlc_epg_event_t *p_evt = ATSC_CreateVLCEPGEvent( p_demux, p_basectx,
                                                     p_event, p_ett );
    if( p_evt )
    {
        if( vlc_epg_AddEvent( p_epg, p_evt ) )
            return p_evt->i_start;
        vlc_epg_event_Delete( p_evt );
    }
    return VLC_TICK_INVALID;
}


static void ATSC_EIT_Callback( void *p_pid, dvbpsi_atsc_eit_t* p_eit )
{
    ts_pid_t *p_eit_pid = (ts_pid_t *) p_pid;
    if( unlikely(p_eit_pid->type != TYPE_PSIP) )
    {
        assert( p_eit_pid->type == TYPE_PSIP );
        dvbpsi_atsc_DeleteEIT( p_eit );
        return;
    }

    demux_t *p_demux = (demux_t *) p_eit_pid->u.p_psip->handle->p_sys;
    demux_sys_t *p_sys = p_demux->p_sys;
    ts_pid_t *p_base_pid = GetPID(p_sys, ATSC_BASE_PID);
    ts_psip_t *p_basepsip = p_base_pid->u.p_psip;
    ts_psip_context_t *p_basectx = p_basepsip->p_ctx;

    if( !p_eit->b_current_next ||
        unlikely(p_base_pid->type != TYPE_PSIP || !p_basectx->p_stt || !p_basectx->p_vct) )
    {
        dvbpsi_atsc_DeleteEIT( p_eit );
        return;
    }

    uint16_t i_program_number;
    if ( !ATSC_TranslateVChannelToProgram( p_basectx->p_vct, p_eit->i_source_id, &i_program_number ) )
    {
        msg_Warn( p_demux, "Received EIT for unknown channel %d", p_eit->i_source_id );
        dvbpsi_atsc_DeleteEIT( p_eit );
        return;
    }

    const ts_pid_t *pid_sibling_ett = ATSC_GetSiblingxTTPID( &p_sys->pids, p_basectx->p_mgt,
                                                     p_eit_pid->u.p_psip );

    /* Get System Time for finding and setting current event */
    time_t i_current_time = atsc_a65_GPSTimeToEpoch( p_basectx->p_stt->i_system_time,
                                                     p_basectx->p_stt->i_gps_utc_offset );
    EIT_DEBUG_TIMESHIFT( i_current_time );

    const uint16_t i_table_type = p_eit_pid->u.p_psip->p_ctx->i_tabletype;
    assert(i_table_type);

    /* Use PID for segmenting our EPG tables updates. 1 EIT/PID transmits 3 hours,
     * with a max of 16 days over 128 EIT/PID. Unlike DVD, table ID is here fixed.
     * see ATSC A/65 5.0 */
    vlc_epg_t *p_epg = vlc_epg_New( i_table_type - ATSC_TABLE_TYPE_EIT_0,
                                    i_program_number );
    if( !p_epg )
    {
        dvbpsi_atsc_DeleteEIT( p_eit );
        return;
    }

    /* Use first table as present/following (not split like DVB) */
    p_epg->b_present = (i_table_type == ATSC_TABLE_TYPE_EIT_0);

    if( !p_basectx->p_a65 && !(p_basectx->p_a65 = atsc_a65_handle_New( NULL )) )
        goto end;

    time_t i_current_event_start_time = 0;
    for( const dvbpsi_atsc_eit_event_t *p_evt = p_eit->p_first_event;
                                        p_evt ; p_evt = p_evt->p_next )
    {
        /* Try to match ETT */
        const dvbpsi_atsc_ett_t *p_ett = NULL;
        if( pid_sibling_ett )
            p_ett = ATSC_ETTFindByETMId( pid_sibling_ett->u.p_psip->p_ctx,
                                         toETMId( p_eit->i_source_id, p_evt->i_event_id ),
                                         p_eit->i_version );

        /* Add Event to EPG based on EIT / available ETT */
        time_t i_start = ATSC_AddVLCEPGEvent( p_demux, p_basectx, p_evt, p_ett, p_epg );

        /* Try to find current event */
        if( i_start <= i_current_time && i_start + p_evt->i_length_seconds > i_current_time )
            i_current_event_start_time = i_start;
    }

    /* Update epg current time from system time ( required for pruning ) */
    if( p_epg->b_present && i_current_event_start_time )
    {
        vlc_epg_SetCurrent( p_epg, i_current_event_start_time );
        ts_pat_t *p_pat = ts_pid_Get(&p_sys->pids, 0)->u.p_pat;
        ts_pmt_t *p_pmt = ts_pat_Get_pmt(p_pat, i_program_number);
        if(p_pmt)
        {
            p_pmt->eit.i_event_start = p_epg->p_current->i_start;
            p_pmt->eit.i_event_length = p_epg->p_current->i_duration;
        }
    }

    if( p_epg->i_event > 0 )
        es_out_Control( p_demux->out, ES_OUT_SET_GROUP_EPG, (int)i_program_number, p_epg );

end:
    vlc_epg_Delete( p_epg );
    ATSC_EITInsert( p_eit_pid->u.p_psip->p_ctx, p_eit );
}

static void ATSC_ETT_Callback( void *p_pid, dvbpsi_atsc_ett_t *p_ett )
{
    ts_pid_t *p_ett_pid = (ts_pid_t *) p_pid;
    if( unlikely(p_ett_pid->type != TYPE_PSIP) )
    {
        assert( p_ett_pid->type == TYPE_PSIP );
        dvbpsi_atsc_DeleteETT( p_ett );
        return;
    }

    demux_t *p_demux = (demux_t *) p_ett_pid->u.p_psip->handle->p_sys;
    demux_sys_t *p_sys = p_demux->p_sys;
    ts_pid_t *p_base_pid = GetPID(p_sys, ATSC_BASE_PID);
    ts_psip_t *p_basepsip = p_base_pid->u.p_psip;
    ts_psip_context_t *p_basectx = p_basepsip->p_ctx;

    if( p_ett->i_etm_id & 0x02 ) /* Event ETT */
    {
        ts_psip_context_t *p_ctx = p_ett_pid->u.p_psip->p_ctx;
        uint16_t i_vchannel_id, i_event_id;
        fromETMId( p_ett->i_etm_id, &i_vchannel_id, &i_event_id );

        uint16_t i_program_number;
        if ( !ATSC_TranslateVChannelToProgram( p_basectx->p_vct, i_vchannel_id, &i_program_number ) )
        {
            msg_Warn( p_demux, "Received EIT for unknown channel %d", i_vchannel_id );
            dvbpsi_atsc_DeleteETT( p_ett );
            return;
        }

        /* If ETT with that version isn't already in list (inserted when matched eit is present) */
        if( ATSC_ETTFindByETMId( p_ctx, p_ett->i_etm_id, p_ett->i_version ) == NULL )
        {
            const dvbpsi_atsc_mgt_t *p_mgt = ts_pid_Get( &p_sys->pids, ATSC_BASE_PID )->u.p_psip->p_ctx->p_mgt;
            ts_pid_t *p_sibling_eit = ATSC_GetSiblingxTTPID( &p_sys->pids, p_mgt, p_ett_pid->u.p_psip );
            if( p_sibling_eit )
            {
                const dvbpsi_atsc_eit_event_t *p_event =
                        ATSC_EventFindByETMId( p_sibling_eit->u.p_psip->p_ctx, p_ett->i_etm_id, p_ett->i_version );
                if( p_event )
                {
#ifdef ATSC_DEBUG_EIT
                    msg_Dbg( p_demux, "Should update EIT %x (matched EIT)", p_event->i_event_id );
#endif
                    vlc_epg_event_t *p_evt = ATSC_CreateVLCEPGEvent( p_demux, p_basectx, p_event, p_ett );
                    if( likely(p_evt) )
                    {
                        es_out_Control( p_demux->out, ES_OUT_SET_GROUP_EPG_EVENT,
                                        (int)i_program_number, p_evt );
#ifdef ATSC_DEBUG_EIT
                        msg_Dbg( p_demux, "Updated event %x with ETT", p_evt->i_id );
#endif
                        vlc_epg_event_Delete( p_evt );
                    }
                }
                /* Insert to avoid duplicated event, and to be available to EIT if didn't appear yet */
                ATSC_InsertETTOrdered( p_ctx, p_ett );
                ATSC_CleanETTByChannelVersion( p_ctx, i_vchannel_id, p_ett->i_version );
                return;
            }
        }
    }
    dvbpsi_atsc_DeleteETT( p_ett );
}

static void ATSC_ETT_RawCallback( dvbpsi_t *p_handle, const dvbpsi_psi_section_t* p_section,
                                  void *p_base_pid )
{
    VLC_UNUSED( p_handle );
    for( ; p_section; p_section = p_section->p_next )
    {
        dvbpsi_atsc_ett_t *p_ett = DVBPlague_ETT_Decode( p_section );
        if( p_ett ) /* Send to real callback */
            ATSC_ETT_Callback( p_base_pid, p_ett );
    }
}

static void ATSC_VCT_Callback( void *p_cb_basepid, dvbpsi_atsc_vct_t* p_vct )
{
    ts_pid_t *p_base_pid = (ts_pid_t *) p_cb_basepid;
    if( unlikely(p_base_pid->type != TYPE_PSIP || p_base_pid->i_pid != ATSC_BASE_PID) )
    {
        assert( p_base_pid->type == TYPE_PSIP );
        assert( p_base_pid->i_pid == ATSC_BASE_PID );
        dvbpsi_atsc_DeleteVCT( p_vct );
        return;
    }
    demux_t *p_demux = (demux_t *) p_base_pid->u.p_psip->handle->p_sys;
    ts_psip_context_t *p_ctx = p_base_pid->u.p_psip->p_ctx;

    if( !p_ctx->p_a65 && !(p_ctx->p_a65 = atsc_a65_handle_New( NULL )) )
        goto end;

    for( const dvbpsi_atsc_vct_channel_t *p_channel = p_vct->p_first_channel;
                                          p_channel; p_channel = p_channel->p_next )
    {
        vlc_meta_t *p_meta = vlc_meta_New();
        if( p_meta )
        {
            char *psz_name = NULL;

            for( dvbpsi_descriptor_t *p_dr = p_channel->p_first_descriptor;
                 p_dr; p_dr = p_dr->p_next )
            {
                switch( p_dr->i_tag )
                {
                    case ATSC_DESCRIPTOR_EXTENDED_CHANNEL_NAME:
                    {
                        dvbpsi_extended_channel_name_dr_t *p_ecndr =
                                                    dvbpsi_ExtendedChannelNameDr( p_dr );
                        if( p_ecndr )
                        {
                            if( unlikely(psz_name) )
                                free( psz_name );
                            psz_name = atsc_a65_Decode_multiple_string( p_ctx->p_a65,
                                                                        p_ecndr->i_long_channel_name,
                                                                        p_ecndr->i_long_channel_name_length );
                        }

                    } break;

                    default:
                        break;
                }
            }

            if( !psz_name )
                psz_name = atsc_a65_Decode_simple_UTF16_string( p_ctx->p_a65,
                                                                p_channel->i_short_name, 14 );
            if( psz_name )
            {
                vlc_meta_SetTitle( p_meta, psz_name );
                free( psz_name );
            }

            const char *psz_service_type = ATSC_A53_get_service_type( p_channel->i_service_type );
            if( psz_service_type )
                vlc_meta_AddExtra( p_meta, "Type", psz_service_type );

            es_out_Control( p_demux->out, ES_OUT_SET_GROUP_META,
                            p_channel->i_program_number, p_meta );

            vlc_meta_Delete( p_meta );
        }
    }

end:
    if( p_ctx->p_vct )
        dvbpsi_atsc_DeleteVCT( p_ctx->p_vct );
    p_ctx->p_vct = p_vct;
}

static void ATSC_MGT_Callback( void *p_cb_basepid, dvbpsi_atsc_mgt_t* p_mgt )
{
    ts_pid_t *p_base_pid = (ts_pid_t *) p_cb_basepid;
    if( unlikely(p_base_pid->type != TYPE_PSIP || p_base_pid->i_pid != ATSC_BASE_PID) )
    {
        assert( p_base_pid->type == TYPE_PSIP );
        assert( p_base_pid->i_pid == ATSC_BASE_PID );
        dvbpsi_atsc_DeleteMGT( p_mgt );
        return;
    }
    ts_psip_t *p_mgtpsip = p_base_pid->u.p_psip;
    demux_t *p_demux = (demux_t *) p_mgtpsip->handle->p_sys;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( ( p_mgtpsip->i_version != -1 && p_mgtpsip->i_version == p_mgt->i_version ) ||
          p_mgt->b_current_next == 0 )
    {
        dvbpsi_atsc_DeleteMGT( p_mgt );
        return;
    }

    /* Easy way, delete and recreate every child if any new version comes
     * (We don't need to keep PID active as with video/PMT update) */
    if( p_mgtpsip->i_version != -1 )
    {
        if( p_mgtpsip->p_ctx->p_vct )
        {
            dvbpsi_atsc_DeleteVCT( p_mgtpsip->p_ctx->p_vct );
            p_mgtpsip->p_ctx->p_vct = NULL;
        }

        /* Remove EIT/ETT */
        for( int i=0; i < p_mgtpsip->eit.i_size; i++ )
        {
             PIDRelease( p_demux, p_mgtpsip->eit.p_elems[i] );
             assert( p_mgtpsip->eit.p_elems[i]->type == TYPE_FREE );
        }
        ARRAY_RESET(p_mgtpsip->eit);

        /* Remove EAS */
        dvbpsi_demux_t *p_dvbpsi_demux = (dvbpsi_demux_t *) p_mgtpsip->handle->p_decoder;
        dvbpsi_demux_subdec_t *p_subdec = dvbpsi_demuxGetSubDec( p_dvbpsi_demux, SCTE18_TABLE_ID, 0x00 );
        if( p_subdec )
        {
            dvbpsi_DetachDemuxSubDecoder( p_dvbpsi_demux, p_subdec );
            dvbpsi_DeleteDemuxSubDecoder( p_subdec );
        }
    }

    if( p_mgtpsip->p_ctx->p_mgt )
        dvbpsi_atsc_DeleteMGT( p_mgtpsip->p_ctx->p_mgt );
    p_mgtpsip->p_ctx->p_mgt = p_mgt;
    p_mgtpsip->i_version = p_mgt->i_version;

    for( const dvbpsi_atsc_mgt_table_t *p_tab = p_mgt->p_first_table;
                                        p_tab; p_tab = p_tab->p_next )
    {
        if( p_tab->i_table_type == ATSC_TABLE_TYPE_TVCT ||
            p_tab->i_table_type == ATSC_TABLE_TYPE_CVCT )
        {
            const uint8_t i_table_id = (p_tab->i_table_type == ATSC_TABLE_TYPE_CVCT)
                                     ? ATSC_CVCT_TABLE_ID
                                     : ATSC_TVCT_TABLE_ID;
            if( !ATSC_ATTACH( p_mgtpsip->handle, VCT, i_table_id,
                              GetPID(p_sys, 0)->u.p_pat->i_ts_id, p_base_pid ) )
                msg_Dbg( p_demux, "  * pid=%d listening for ATSC VCT", p_base_pid->i_pid );
        }
        else if( p_tab->i_table_type >= ATSC_TABLE_TYPE_EIT_0 &&
                 p_tab->i_table_type <= ATSC_TABLE_TYPE_EIT_0 + ATSC_EIT_MAX_DEPTH_MIN1 &&
                 p_tab->i_table_type <= ATSC_TABLE_TYPE_EIT_127 &&
                 p_tab->i_table_type_pid != p_base_pid->i_pid )
        {
            ts_pid_t *pid = GetPID(p_sys, p_tab->i_table_type_pid);
            if( PIDSetup( p_demux, TYPE_PSIP, pid, NULL ) )
            {
                SetPIDFilter( p_demux->p_sys, pid, true );
                pid->u.p_psip->p_ctx->i_tabletype = p_tab->i_table_type;
                ATSC_Ready_SubDecoders( pid->u.p_psip->handle, pid );
                msg_Dbg( p_demux, "  * pid=%d reserved for ATSC EIT", pid->i_pid );
                ARRAY_APPEND( p_mgtpsip->eit, pid );
            }
        }
        else if( p_tab->i_table_type >= ATSC_TABLE_TYPE_ETT_0 &&
                 p_tab->i_table_type <= ATSC_TABLE_TYPE_ETT_0 + ATSC_EIT_MAX_DEPTH_MIN1 &&
                 p_tab->i_table_type <= ATSC_TABLE_TYPE_ETT_127 &&
                 p_tab->i_table_type_pid != p_base_pid->i_pid )
        {
            ts_pid_t *pid = GetPID(p_sys, p_tab->i_table_type_pid);
            if( PIDSetup( p_demux, TYPE_PSIP, pid, NULL ) )
            {
                SetPIDFilter( p_sys, pid, true );
                pid->u.p_psip->p_ctx->i_tabletype = p_tab->i_table_type;
                ATSC_Ready_SubDecoders( pid->u.p_psip->handle, pid );
                msg_Dbg( p_demux, "  * pid=%d reserved for ATSC ETT", pid->i_pid );
                ARRAY_APPEND( p_mgtpsip->eit, pid );
            }
        }
        msg_Dbg( p_demux, "  * pid=%d transport for ATSC PSIP type %x",
                          p_tab->i_table_type_pid, p_tab->i_table_type );
    }

    if( SCTE18_SI_BASE_PID == ATSC_BASE_PID &&
        ts_dvbpsi_AttachRawSubDecoder( p_mgtpsip->handle, SCTE18_TABLE_ID, 0x00,
                                       SCTE18_Section_Callback, p_base_pid ) )
    {
        msg_Dbg( p_demux, "  * pid=%d listening for EAS", p_base_pid->i_pid );
    }
}

static void ATSC_STT_Callback( void *p_cb_basepid, dvbpsi_atsc_stt_t* p_stt )
{
    ts_pid_t *p_base_pid = (ts_pid_t *) p_cb_basepid;
    if( unlikely(p_base_pid->type != TYPE_PSIP || p_base_pid->i_pid != ATSC_BASE_PID) )
    {
        assert( p_base_pid->type == TYPE_PSIP );
        assert( p_base_pid->i_pid == ATSC_BASE_PID );
        dvbpsi_atsc_DeleteSTT( p_stt );
        return;
    }
    demux_t *p_demux = (demux_t *) p_base_pid->u.p_psip->handle->p_sys;
    demux_sys_t *p_sys = p_demux->p_sys;
    ts_psip_context_t *p_ctx = p_base_pid->u.p_psip->p_ctx;
    dvbpsi_t *p_handle = p_base_pid->u.p_psip->handle;

    if( !p_ctx->p_stt ) /* First call */
    {
        if( !ATSC_ATTACH( p_handle, MGT, ATSC_MGT_TABLE_ID, 0x00, p_base_pid ) )
        {
            msg_Err( p_demux, "Can't attach MGT decoder to pid %d", ATSC_BASE_PID );
            ATSC_Detach_Dvbpsi_Decoders( p_handle );
            dvbpsi_atsc_DeleteSTT( p_ctx->p_stt );
            p_stt = NULL;
        }
    }
    else
    {
        dvbpsi_atsc_DeleteSTT( p_ctx->p_stt );
    }

    if( p_stt )
    {
        time_t i_current_time = atsc_a65_GPSTimeToEpoch( p_stt->i_system_time,
                                                         p_stt->i_gps_utc_offset );
        EIT_DEBUG_TIMESHIFT( i_current_time );
        p_sys->i_network_time =  i_current_time;
        p_sys->i_network_time_update = time(NULL);

        es_out_Control( p_demux->out, ES_OUT_SET_EPG_TIME, p_sys->i_network_time );
    }

    p_ctx->p_stt = p_stt;
}

static void ATSC_STT_RawCallback( dvbpsi_t *p_handle, const dvbpsi_psi_section_t* p_section,
                                  void *p_base_pid )
{
    VLC_UNUSED( p_handle );
    for( ; p_section ; p_section = p_section->p_next )
    {
        dvbpsi_atsc_stt_t *p_stt = DVBPlague_STT_Decode( p_section );
        if( p_stt ) /* Send to real callback */
            ATSC_STT_Callback( p_base_pid, p_stt );
    }
}

bool ATSC_Attach_Dvbpsi_Base_Decoders( dvbpsi_t *p_handle, void *p_base_pid )
{
    if( !ATSC_ATTACH_WITH_FIXED_DECODER( p_handle, STT, ATSC_STT_TABLE_ID, 0x00, p_base_pid ) )
    {
        ATSC_Detach_Dvbpsi_Decoders( p_handle ); /* shouldn't be any, except demux */
        return false;
    }
    return true;
}

static void ATSC_NewTable_Callback( dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
                                    uint16_t i_extension, void *p_cb_pid )
{
    demux_t *p_demux = (demux_t *) p_dvbpsi->p_sys;
    demux_sys_t *p_sys = p_demux->p_sys;
    assert( ((ts_pid_t *) p_cb_pid)->type == TYPE_PSIP );
    const ts_pid_t *p_base_pid = ts_pid_Get( &p_sys->pids, ATSC_BASE_PID );
    if( !p_base_pid->u.p_psip->p_ctx->p_vct )
        return;

    switch( i_table_id )
    {
        case ATSC_ETT_TABLE_ID:
            if( !ATSC_ATTACH_WITH_FIXED_DECODER( p_dvbpsi, ETT, ATSC_ETT_TABLE_ID, i_extension, p_cb_pid ) )
                msg_Warn( p_demux, "Cannot attach ETT decoder source %" PRIu16, i_extension );
            break;

        case ATSC_EIT_TABLE_ID:
            if( !ATSC_ATTACH( p_dvbpsi, EIT, ATSC_EIT_TABLE_ID, i_extension, p_cb_pid ) )
                msg_Warn( p_demux, "Cannot attach EIT decoder source %" PRIu16, i_extension );
            break;

        default:
            break;
    }
}
