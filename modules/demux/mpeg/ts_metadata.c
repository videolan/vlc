/*****************************************************************************
 * ts_metadata.c : TS demuxer metadata handling
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
#include <vlc_meta.h>
#include <vlc_es_out.h>
#include <vlc_block.h>

#include "ts_pid.h"
#include "ts_streams.h"
#include "ts_streams_private.h"
#include "ts_metadata.h"

#include "../../meta_engine/ID3Tag.h"
#include "../../meta_engine/ID3Meta.h"

static int ID3TAG_Parse_Handler( uint32_t i_tag, const uint8_t *p_payload,
                                 size_t i_payload, void *p_priv )
{
    vlc_meta_t *p_meta = (vlc_meta_t *) p_priv;

    (void) ID3HandleTag( p_payload, i_payload, i_tag, p_meta, NULL );

    return VLC_SUCCESS;
}

typedef struct
{
    es_out_t *out;
    ts_stream_t *p_stream;
    block_t *p_head;
    block_t **pp_tail;
    uint8_t i_sequence_number;
} Metadata_stream_processor_context_t;

static void Metadata_stream_processor_Reset( ts_stream_processor_t *h )
{
    Metadata_stream_processor_context_t *ctx = (Metadata_stream_processor_context_t *) h->priv;
    block_ChainRelease(ctx->p_head);
    ctx->p_head = NULL;
    ctx->pp_tail = &ctx->p_head;
}

static void Metadata_stream_processor_Delete( ts_stream_processor_t *h )
{
    Metadata_stream_processor_context_t *ctx = (Metadata_stream_processor_context_t *) h->priv;
    block_ChainRelease(ctx->p_head);
    free( ctx );
    free( h );
}

static block_t * Metadata_stream_processor_AggregateMAU( ts_stream_processor_t *h,
                                                         uint8_t i_sequence, block_t *p_block,
                                                         size_t i_cellsize )
{
    Metadata_stream_processor_context_t *ctx = (Metadata_stream_processor_context_t *) h->priv;

    bool b_corrupt = ctx->p_head && i_sequence != ((ctx->i_sequence_number + 1) & 0xFF);

    if( unlikely(p_block->i_buffer > i_cellsize) )
    {
        block_t *cell = block_Duplicate( p_block );
        if( cell )
        {
            cell->i_buffer = i_cellsize;
            block_ChainLastAppend( &ctx->pp_tail, cell );
        }
        p_block->i_buffer -= i_cellsize;
        p_block->i_buffer += i_cellsize;
    }
    else
    {
        assert( p_block->i_buffer == i_cellsize );
        block_ChainLastAppend( &ctx->pp_tail, p_block );
        p_block = NULL;
    }

    ctx->i_sequence_number = i_sequence;
    if( b_corrupt )
        Metadata_stream_processor_Reset( h );

    return p_block;
}

static block_t * Metadata_stream_processor_OutputMAU( ts_stream_processor_t *h )
{
    Metadata_stream_processor_context_t *ctx = (Metadata_stream_processor_context_t *) h->priv;

    block_t *p_chain = ctx->p_head;
    if( !p_chain )
        return NULL;
    ctx->p_head = NULL;
    ctx->pp_tail = &ctx->p_head;
    return block_ChainGather( p_chain );
}

static block_t * Metadata_stream_processor_PushMAU( ts_stream_processor_t *h,
                                                    const ts_es_t *p_es, block_t *p_block )
{
    block_t *p_return_chain = NULL;
    block_t **pp_return = &p_return_chain;

    while( p_block->i_buffer >= 5 )
    {
        const uint8_t i_service_id = p_block->p_buffer[0];
        const uint8_t i_sequence = p_block->p_buffer[1];
        const uint8_t i_fragment_indication = p_block->p_buffer[2] >> 6;
        const uint16_t i_length = GetWBE(&p_block->p_buffer[3]);

        p_block->i_buffer -= 5;
        p_block->p_buffer += 5;

        if( p_block->i_buffer < i_length )
            break;

        if( i_service_id == p_es->metadata.i_service_id )
        {
            if( i_fragment_indication == 0x03 ) /* FULL AU */
            {
                Metadata_stream_processor_Reset( h ); /* flush anything that not went to last frag */
                p_block = Metadata_stream_processor_AggregateMAU( h, i_sequence, p_block, i_length );
            }
            else
            {
                if( i_fragment_indication == 0x02 ) /* First */
                    Metadata_stream_processor_Reset( h ); /* flush anything that not went to last frag */
                p_block = Metadata_stream_processor_AggregateMAU( h, i_sequence, p_block, i_length );
                if( i_fragment_indication == 0x01 ) /* Last */
                {
                    block_t *out = Metadata_stream_processor_OutputMAU( h );
                    if( out )
                        block_ChainLastAppend( &pp_return, out );
                }
            }
        }

        if( !p_block )
            break;

        p_block->i_buffer -= i_length;
        p_block->i_buffer += i_length;
    };

    if( p_block )
        block_Release( p_block );

    return p_return_chain;
}

static block_t * Metadata_stream_processor_Push( ts_stream_processor_t *h, uint8_t i_stream_id, block_t *p_block )
{
    Metadata_stream_processor_context_t *ctx = (Metadata_stream_processor_context_t *) h->priv;
    ts_es_t *p_es = ctx->p_stream->p_es;

    if( i_stream_id == 0xbd && /* Transport in PES packets, 2.12.3 */
        p_es->metadata.i_format_identifier == METADATA_IDENTIFIER_ID3 )
    {
        vlc_meta_t *p_meta = vlc_meta_New();
        if( p_meta )
        {
            (void) ID3TAG_Parse( p_block->p_buffer, p_block->i_buffer, ID3TAG_Parse_Handler, p_meta );
            es_out_Control( ctx->out, ES_OUT_SET_GROUP_META, p_es->p_program->i_number, p_meta );
            vlc_meta_Delete( p_meta );
        }
    }

    return p_block;
}

ts_stream_processor_t *Metadata_stream_processor_New( ts_stream_t *p_stream, es_out_t *out )
{
    ts_stream_processor_t *h = malloc(sizeof(*h));
    if(!h)
        return NULL;

    Metadata_stream_processor_context_t *ctx = malloc( sizeof(Metadata_stream_processor_context_t) );
    if(!ctx)
    {
        free(h);
        return NULL;
    }
    ctx->out = out;
    ctx->p_stream = p_stream;
    ctx->i_sequence_number = 0;
    ctx->p_head = NULL;
    ctx->pp_tail = &ctx->p_head;

    h->priv = ctx;
    h->pf_delete = Metadata_stream_processor_Delete;
    h->pf_push = Metadata_stream_processor_Push;
    h->pf_reset = Metadata_stream_processor_Reset;

    return h;
}
