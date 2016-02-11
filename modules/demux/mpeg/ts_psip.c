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

#include "ts_pid.h"
#include "ts.h"
#include "ts_streams_private.h"
#include "ts_scte.h"

#include "ts_psip.h"

#include "../codec/atsc_a65.h"
#include "../codec/scte18.h"

#include <assert.h>

/*
 * Decoders activation order due to dependencies,
 * and because callbacks will be fired once per MGT/VCT version
 * STT(ref by EIT,EAS) -> MGT
 * MGT -> VCT,EAS,EIT/ETT
 */

struct ts_psip_context_t
{
    dvbpsi_atsc_stt_t *p_stt; /* Time reference for EIT/EAS */
    dvbpsi_atsc_vct_t *p_vct; /* Required for EIT vchannel -> program remapping */
    atsc_a65_handle_t *p_a65; /* Shared Handle to avoid iconv reopens */
};

ts_psip_context_t * ts_psip_context_New()
{
    ts_psip_context_t *p_ctx = malloc(sizeof(*p_ctx));
    if(likely(p_ctx))
    {
        p_ctx->p_stt = NULL;
        p_ctx->p_vct = NULL;
        p_ctx->p_a65 = NULL;
    }
    return p_ctx;
}

void ts_psip_context_Delete( ts_psip_context_t *p_ctx )
{
    if( p_ctx->p_stt )
        dvbpsi_atsc_DeleteSTT( p_ctx->p_stt );
    if ( p_ctx->p_vct )
        dvbpsi_atsc_DeleteVCT( p_ctx->p_vct );
    if( p_ctx->p_a65 )
        atsc_a65_handle_Release( p_ctx->p_a65 );
    free( p_ctx );
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
    if( i_type == 0 || i_type > 6 )
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
    ts_pid_t *p_base_pid = GetPID(p_demux->p_sys, ATSC_BASE_PID);
    ts_psip_t *p_basepsip = p_base_pid->u.p_psip;
    ts_psip_context_t *p_ctx = p_basepsip->p_ctx;

    if( !p_eit->b_current_next ||
        unlikely(p_base_pid->type != TYPE_PSIP || !p_ctx->p_stt || !p_ctx->p_vct) )
    {
        dvbpsi_atsc_DeleteEIT( p_eit );
        return;
    }

    uint16_t i_program_number;
    if ( !ATSC_TranslateVChannelToProgram( p_ctx->p_vct, p_eit->i_source_id, &i_program_number ) )
    {
        msg_Warn( p_demux, "Received EIT for unkown channel %d", p_eit->i_source_id );
        dvbpsi_atsc_DeleteEIT( p_eit );
        return;
    }

    /* Get System Time for finding and setting current event */
    time_t i_current_time = atsc_a65_GPSTimeToEpoch( p_ctx->p_stt->i_system_time,
                                                     p_ctx->p_stt->i_gps_utc_offset );
    EIT_DEBUG_TIMESHIFT( i_current_time );


    vlc_epg_t *p_epg = vlc_epg_New( NULL );
    if( !p_epg )
    {
        dvbpsi_atsc_DeleteEIT( p_eit );
        return;
    }

    if( !p_ctx->p_a65 && !(p_ctx->p_a65 = atsc_a65_handle_New( NULL )) )
        goto end;

    time_t i_current_event_start_time = 0;
    for( const dvbpsi_atsc_eit_event_t *p_evt = p_eit->p_first_event;
                                        p_evt ; p_evt = p_evt->p_next )
    {
        char *psz_title = atsc_a65_Decode_multiple_string( p_ctx->p_a65,
                                                           p_evt->i_title, p_evt->i_title_length );
        char *psz_shortdesc_text = NULL;

        time_t i_start = atsc_a65_GPSTimeToEpoch( p_evt->i_start_time, p_ctx->p_stt->i_gps_utc_offset );
        EIT_DEBUG_TIMESHIFT( i_start );

        /* Try to find current event */
        if( i_start <= i_current_time && i_start + p_evt->i_length_seconds > i_current_time )
            i_current_event_start_time = i_start;

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
                        psz_shortdesc_text = atsc_a65_Decode_multiple_string( p_ctx->p_a65, p_data, desclen );
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

        if( i_start > VLC_TS_INVALID && psz_title )
        {
#ifdef ATSC_DEBUG_EIT
            msg_Dbg( p_demux, "EIT Event vchannel/program %d/%d time %ld +%d %s",
                     p_eit->i_source_id, i_program_number, i_start, p_evt->i_length_seconds, psz_title );
#endif
            vlc_epg_AddEvent( p_epg, i_start, p_evt->i_length_seconds,
                              psz_title, psz_shortdesc_text, NULL, 0 );
        }

        free( psz_title );
        free( psz_shortdesc_text );
    }

    /* Update epg current time from system time ( required for pruning ) */
    if( i_current_event_start_time )
        vlc_epg_SetCurrent( p_epg, i_current_event_start_time );

    if( p_epg->i_event > 0 )
        es_out_Control( p_demux->out, ES_OUT_SET_GROUP_EPG, (int)i_program_number, p_epg );

end:
    vlc_epg_Delete( p_epg );
    dvbpsi_atsc_DeleteEIT( p_eit );
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
                              GetPID(p_demux->p_sys, 0)->u.p_pat->i_ts_id, p_base_pid ) )
                msg_Dbg( p_demux, "  * pid=%d listening for ATSC VCT", p_base_pid->i_pid );
        }
        else if( p_tab->i_table_type >= ATSC_TABLE_TYPE_EIT_0 &&
                 p_tab->i_table_type <= ATSC_TABLE_TYPE_EIT_0 + ATSC_EIT_MAX_DEPTH_MIN1 &&
                 p_tab->i_table_type <= ATSC_TABLE_TYPE_EIT_127 &&
                 p_tab->i_table_type_pid != p_base_pid->i_pid )
        {
            ts_pid_t *pid = GetPID(p_demux->p_sys, p_tab->i_table_type_pid);
            if( PIDSetup( p_demux, TYPE_PSIP, pid, NULL ) )
            {
                SetPIDFilter( p_demux->p_sys, pid, true );
                ATSC_Ready_SubDecoders( pid->u.p_psip->handle, pid );
                msg_Dbg( p_demux, "  * pid=%d reserved for ATSC EIT", pid->i_pid );
                ARRAY_APPEND( p_mgtpsip->eit, pid );
            }
        }
        msg_Dbg( p_demux, "  * pid=%d transport for ATSC PSIP type %x",
                          p_tab->i_table_type_pid, p_tab->i_table_type );
    }

    if( SCTE18_SI_BASE_PID == ATSC_BASE_PID &&
        ts_dvbpsi_AttachRawSubDecoder( p_mgtpsip->handle, SCTE18_TABLE_ID, 0x00,
                                       SCTE18_SectionsCallback, p_base_pid ) )
    {
        msg_Dbg( p_demux, "  * pid=%d listening for EAS", p_base_pid->i_pid );
    }

    dvbpsi_atsc_DeleteMGT( p_mgt );
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

    p_ctx->p_stt = p_stt;
}

static void ATSC_STT_RawCallback( dvbpsi_t *p_handle, const dvbpsi_psi_section_t* p_section,
                                  void *p_base_pid )
{
    VLC_UNUSED( p_handle );
    dvbpsi_atsc_stt_t *p_stt = DVBPlague_STT_Decode( p_section );
    if( p_stt ) /* Send to real callback */
        ATSC_STT_Callback( p_base_pid, p_stt );
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
    assert( ((ts_pid_t *) p_cb_pid)->type == TYPE_PSIP );
    const ts_pid_t *p_base_pid = GetPID(p_demux->p_sys, ATSC_BASE_PID);
    if( !p_base_pid->u.p_psip->p_ctx->p_vct )
        return;

    switch( i_table_id )
    {
        case ATSC_EIT_TABLE_ID:
            ATSC_ATTACH( p_dvbpsi, EIT, ATSC_EIT_TABLE_ID, i_extension, p_cb_pid );
            break;

        default:
            break;
    }
}
