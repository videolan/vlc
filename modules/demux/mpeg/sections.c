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
#include <vlc_block.h>

#include "ts_pid.h"
#include "sections.h"

typedef struct ts_sections_assembler_t
{
    int8_t i_version;
    int8_t i_prev_version;
    int8_t i_prev_section;
    block_t *p_sections;
    block_t **pp_sections_tail;
    bool b_raw; /* Pass unstripped section data (SCTE-27 mess) */
} ts_sections_assembler_t;

struct ts_sections_processor_t
{
    uint8_t i_stream_type;
    uint8_t i_table_id;
    ts_sections_assembler_t assembler;
    ts_section_callback_t pf_callback;
    ts_sections_processor_t *p_next;
};

static void ts_sections_assembler_Reset( ts_sections_assembler_t *p_as, bool b_full )
{
    if( p_as->p_sections )
        block_ChainRelease( p_as->p_sections );
    p_as->i_version = -1;
    if( b_full )
        p_as->i_prev_version = -1;
    p_as->i_prev_section = -1;
    p_as->p_sections = NULL;
    p_as->pp_sections_tail = &p_as->p_sections;
}

static void ts_sections_assembler_Init( ts_sections_assembler_t *p_as )
{
    p_as->p_sections = NULL;
    p_as->b_raw = false;
    ts_sections_assembler_Reset( p_as, true );
}

static block_t * ts_sections_assembler_Append( ts_sections_assembler_t *p_as, block_t *p_content )
{
    const bool b_short = !( p_content->p_buffer[1] & 0x80 );
    const uint16_t i_private_length = ((p_content->p_buffer[1] & 0x0f) << 8) | p_content->p_buffer[2];
    if( b_short )
    {
        /* Short, unsegmented section */
        if(unlikely(( i_private_length > 0xFFD || i_private_length > p_content->i_buffer - 3 )))
        {
            block_Release( p_content );
            return NULL;
        }

        if( !p_as->b_raw )
        {
            p_content->p_buffer += 3;
            p_content->i_buffer = i_private_length;
        }
        return p_content;
    }
    else
    {
        /* Payload can span on multiple sections */
        if (unlikely( p_content->i_buffer < (size_t)12 + i_private_length))
        {
            block_Release( p_content );
            return NULL;
        }
        /* TODO: CRC32 */
        const uint8_t i_version = ( p_content->p_buffer[5] & 0x3F ) >> 1;
        const uint8_t i_current = p_content->p_buffer[5] & 0x01;
        const uint8_t i_section = p_content->p_buffer[6];
        const uint8_t i_section_last = p_content->p_buffer[7];

        if( !p_as->b_raw )
        {
            p_content->p_buffer += 3 + 5;
            p_content->i_buffer = i_private_length - 4;
        }

        if( !i_current ||
           ( p_as->i_version != -1 && i_version != p_as->i_version ) || /* Only merge same version */
             p_as->i_prev_version == i_version || /* No duplicates */
             i_section > i_section_last )
        {
            block_Release( p_content );
            return NULL;
        }

        if( i_section != p_as->i_prev_section + 1 ) /* first or unfinished sections gathering */
        {
            ts_sections_assembler_Reset( p_as, false );
            if( i_section > 0 || i_version == p_as->i_prev_version )
            {
                block_Release( p_content );
                return NULL;
            }
        }

        p_as->i_version = i_version;
        p_as->i_prev_section = i_section;

        /* Add one more section */
        block_ChainLastAppend( &p_as->pp_sections_tail, p_content );

        /* We finished gathering our sections */
        if( i_section == i_section_last )
        {
            block_t *p_all_sections = block_ChainGather( p_as->p_sections );
            p_as->p_sections = NULL;
            p_as->pp_sections_tail = &p_as->p_sections;
            p_as->i_prev_version = i_version;
            ts_sections_assembler_Reset( p_as, false );
            return p_all_sections;
        }
    }

    return NULL;
}

void ts_sections_processor_Add( ts_sections_processor_t **pp_chain,
                                uint8_t i_table_id, uint8_t i_stream_type, bool b_raw,
                                ts_section_callback_t pf_callback )
{
    ts_sections_processor_t *p_proc = *pp_chain;
    for( ; p_proc; p_proc = p_proc->p_next )
    {
        /* Avoid duplicates */
        if ( p_proc->i_stream_type == i_stream_type &&
             p_proc->i_table_id == i_table_id &&
             p_proc->pf_callback == pf_callback )
            return;
    }

    p_proc = malloc( sizeof(ts_sections_processor_t) );
    if( p_proc )
    {
        ts_sections_assembler_Init( &p_proc->assembler );
        p_proc->assembler.b_raw = b_raw;
        p_proc->pf_callback = pf_callback;
        p_proc->i_stream_type = i_stream_type;
        p_proc->i_table_id = i_table_id;

        /* Insert as head */
        p_proc->p_next = *pp_chain;
        *pp_chain = p_proc;
    }
}

void ts_sections_processor_ChainDelete( ts_sections_processor_t *p_chain )
{
    while( p_chain )
    {
        ts_sections_assembler_Reset( &p_chain->assembler, false );
        ts_sections_processor_t *p_next = p_chain->p_next;
        free( p_chain );
        p_chain = p_next;
    }
}

void ts_sections_processor_Reset( ts_sections_processor_t *p_chain )
{
    while( p_chain )
    {
        ts_sections_assembler_Reset( &p_chain->assembler, true );
        p_chain = p_chain->p_next;
    }
}

void ts_sections_processor_Push( ts_sections_processor_t *p_chain,
                                 uint8_t i_table_id, uint8_t i_stream_type,
                                 demux_t *p_demux, ts_pid_t *p_pid,
                                 block_t *p_blockschain )
{
    while( p_chain )
    {
        if( ( !p_chain->i_stream_type || p_chain->i_stream_type == i_stream_type ) &&
            ( !p_chain->i_table_id || p_chain->i_table_id == i_table_id ) )
        {
            block_t *p_block = block_ChainGather( p_blockschain );
            if( p_block )
            {
                p_block = ts_sections_assembler_Append( &p_chain->assembler, p_block );
                if( p_block )
                    p_chain->pf_callback( p_demux, p_pid, p_block );
            }
            return;
        }
        p_chain = p_chain->p_next;
    }
    block_ChainRelease( p_blockschain );
}
