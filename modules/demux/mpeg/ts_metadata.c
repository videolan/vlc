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

#include "../meta_engine/ID3Tag.h"
#include "../meta_engine/ID3Meta.h"

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

} Metadata_stream_processor_context_t;

static void Metadata_stream_processor_Delete( ts_stream_processor_t *h )
{
    Metadata_stream_processor_context_t *ctx = (Metadata_stream_processor_context_t *) h->priv;
    free( ctx );
    free( h );
}

static block_t * Metadata_stream_processor_Push( ts_stream_processor_t *h, uint8_t i_stream_id, block_t *p_block )
{
    Metadata_stream_processor_context_t *ctx = (Metadata_stream_processor_context_t *) h->priv;
    ts_es_t *p_es = ctx->p_stream->p_es;

    if( i_stream_id != 0xbd )
    {
        block_Release( p_block );
        return NULL;
    }

    if( p_es->metadata.i_format == VLC_FOURCC('I', 'D', '3', ' ') )
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

    h->priv = ctx;
    h->pf_delete = Metadata_stream_processor_Delete;
    h->pf_push = Metadata_stream_processor_Push;
    h->pf_reset = NULL;

    return h;
}
