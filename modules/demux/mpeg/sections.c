/*****************************************************************************
 * sections.c: Transport Stream sections assembler
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "ts_pid.h"
#include "sections.h"

#ifndef _DVBPSI_DVBPSI_H_
 #include <dvbpsi/dvbpsi.h>
#endif
#ifndef _DVBPSI_DEMUX_H_
 #include <dvbpsi/demux.h>
#endif
#include <dvbpsi/psi.h>
#include "../../mux/mpeg/dvbpsi_compat.h" /* dvbpsi_messages */

#include "ts_decoders.h"

#include <assert.h>

struct ts_sections_processor_t
{
    uint8_t i_stream_type;
    uint8_t i_table_id;
    uint16_t i_extension_id;
    dvbpsi_t *p_dvbpsi;
    ts_section_processor_callback_t pf_callback;
    ts_sections_processor_t *p_next;
    void *p_callback_data;
};

static void ts_subdecoder_rawsection_Callback( dvbpsi_t *p_dvbpsi,
                                               const dvbpsi_psi_section_t* p_section,
                                               void* p_proc_cb_data )
{
    ts_sections_processor_t *p_proc = (ts_sections_processor_t *) p_proc_cb_data;
    if( likely(p_proc->pf_callback) )
    {
        for( const dvbpsi_psi_section_t *p_sec = p_section; p_sec; p_sec = p_sec->p_next )
        {
            size_t i_rawlength = p_sec->p_payload_end - p_sec->p_data;
            if ( p_sec->b_syntax_indicator )
                i_rawlength += 4;

            if( p_proc->i_table_id && p_section->i_table_id != p_proc->i_table_id )
                continue;

            if( p_proc->i_extension_id && p_section->i_extension != p_proc->i_extension_id )
                continue;

            p_proc->pf_callback( (demux_t *) p_dvbpsi->p_sys,
                                 p_sec->p_data, i_rawlength,
                                 p_sec->p_payload_start,
                                 p_sec->p_payload_end - p_sec->p_payload_start,
                                 p_proc->p_callback_data );
        }
    }
}

void ts_sections_processor_Add( demux_t *p_demux,
                                ts_sections_processor_t **pp_chain,
                                uint8_t i_table_id, uint16_t i_extension_id,
                                ts_section_processor_callback_t pf_callback,
                                void *p_callback_data )
{
    ts_sections_processor_t *p_proc = *pp_chain;
    for( ; p_proc; p_proc = p_proc->p_next )
    {
        /* Avoid duplicates */
        if ( p_proc->i_extension_id == i_extension_id &&
             p_proc->i_table_id == i_table_id &&
             p_proc->pf_callback == pf_callback )
            return;
    }

    p_proc = malloc( sizeof(ts_sections_processor_t) );
    if( p_proc )
    {
        p_proc->pf_callback = pf_callback;
        p_proc->i_extension_id = i_extension_id;
        p_proc->i_table_id = i_table_id;
        p_proc->p_dvbpsi = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
        p_proc->p_dvbpsi->p_sys = p_demux;
        p_proc->p_callback_data = p_callback_data;

        if( !ts_dvbpsi_AttachRawDecoder( p_proc->p_dvbpsi,
                                         ts_subdecoder_rawsection_Callback, p_proc ) )
        {
            ts_sections_processor_ChainDelete( p_proc );
            return;
        }
        /* Insert as head */
        p_proc->p_next = *pp_chain;
        *pp_chain = p_proc;
    }
}

void ts_sections_processor_ChainDelete( ts_sections_processor_t *p_chain )
{
    while( p_chain )
    {
        ts_dvbpsi_DetachRawDecoder( p_chain->p_dvbpsi );
        dvbpsi_delete( p_chain->p_dvbpsi );
        ts_sections_processor_t *p_next = p_chain->p_next;
        free( p_chain );
        p_chain = p_next;
    }
}

void ts_sections_processor_Reset( ts_sections_processor_t *p_chain )
{
    while( p_chain )
    {
        dvbpsi_decoder_reset( p_chain->p_dvbpsi->p_decoder, true );
        p_chain = p_chain->p_next;
    }
}

void ts_sections_processor_Push( ts_sections_processor_t *p_chain,
                                 const uint8_t *p_buf )
{
    for( ts_sections_processor_t *p_proc = p_chain;
         p_proc; p_proc = p_proc->p_next )
    {
        dvbpsi_packet_push( p_chain->p_dvbpsi, (uint8_t *) p_buf );
    }
}
