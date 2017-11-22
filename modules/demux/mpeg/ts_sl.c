/*****************************************************************************
 * ts_sl.c : MPEG SL/FMC handling for TS demuxer
 *****************************************************************************
 * Copyright (C) 2014-2016 - VideoLAN Authors
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

#include "ts_streams.h"
#include "ts_pid.h"
#include "ts_streams_private.h"
#include "ts.h"

#include "ts_sl.h"

const es_mpeg4_descriptor_t * GetMPEG4DescByEsId( const ts_pmt_t *pmt, uint16_t i_es_id )
{
    for( int i = 0; i < ES_DESCRIPTOR_COUNT; i++ )
    {
        const es_mpeg4_descriptor_t *es_descr = &pmt->iod->es_descr[i];
        if( es_descr->i_es_id == i_es_id && es_descr->b_ok )
            return es_descr;
    }
    for( int i=0; i<pmt->od.objects.i_size; i++ )
    {
        const od_descriptor_t *od = pmt->od.objects.p_elems[i];
        for( int j = 0; j < ES_DESCRIPTOR_COUNT; j++ )
        {
            const es_mpeg4_descriptor_t *es_descr = &od->es_descr[j];
            if( es_descr->i_es_id == i_es_id && es_descr->b_ok )
                return es_descr;
        }
    }
    return NULL;
}

static ts_es_t * GetPMTESBySLEsId( ts_pmt_t *pmt, uint16_t i_sl_es_id )
{
    for( int i=0; i< pmt->e_streams.i_size; i++ )
    {
        ts_es_t *p_es = pmt->e_streams.p_elems[i]->u.p_stream->p_es;
        if( p_es->i_sl_es_id == i_sl_es_id )
            return p_es;
    }
    return NULL;
}

bool SetupISO14496LogicalStream( demux_t *p_demux, const decoder_config_descriptor_t *dcd,
                                  es_format_t *p_fmt )
{
    msg_Dbg( p_demux, "     - IOD objecttype: %"PRIx8" streamtype:%"PRIx8,
             dcd->i_objectTypeIndication, dcd->i_streamType );

    if( dcd->i_streamType == 0x04 )    /* VisualStream */
    {
        switch( dcd->i_objectTypeIndication )
        {
        case 0x0B: /* mpeg4 sub */
            es_format_Change( p_fmt, SPU_ES, VLC_CODEC_SUBT );
            break;

        case 0x20: /* mpeg4 */
            es_format_Change( p_fmt, VIDEO_ES, VLC_CODEC_MP4V );
            break;
        case 0x21: /* h264 */
            es_format_Change( p_fmt, VIDEO_ES, VLC_CODEC_H264 );
            break;
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65: /* mpeg2 */
        case 0x6a: /* mpeg1 */
            es_format_Change( p_fmt, VIDEO_ES, VLC_CODEC_MPGV );
            break;
        case 0x6c: /* mpeg1 */
            es_format_Change( p_fmt, VIDEO_ES, VLC_CODEC_JPEG );
            break;
        default:
            break;
        }
    }
    else if( dcd->i_streamType == 0x05 )    /* AudioStream */
    {
        switch( dcd->i_objectTypeIndication )
        {
        case 0x40: /* mpeg4 */
        case 0x66:
        case 0x67:
        case 0x68: /* mpeg2 aac */
            es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_MP4A );
            break;
        case 0x69: /* mpeg2 */
        case 0x6b: /* mpeg1 */
            es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_MPGA );
            break;
        default:
            break;
        }
    }

    if( p_fmt->i_cat != UNKNOWN_ES )
    {
        p_fmt->i_extra = __MIN(dcd->i_extra, INT32_MAX);
        if( p_fmt->i_extra > 0 )
        {
            p_fmt->p_extra = malloc( p_fmt->i_extra );
            if( p_fmt->p_extra )
                memcpy( p_fmt->p_extra, dcd->p_extra, p_fmt->i_extra );
            else
                p_fmt->i_extra = 0;
        }
    }

    return true;
}

/* Object stream SL in table sections */
void SLPackets_Section_Handler( demux_t *p_demux,
                                const uint8_t *p_sectiondata, size_t i_sectiondata,
                                const uint8_t *p_payloaddata, size_t i_payloaddata,
                                void *p_pes_cbdata )
{
    VLC_UNUSED(p_sectiondata); VLC_UNUSED(i_sectiondata);
    ts_stream_t *p_pes = (ts_stream_t *) p_pes_cbdata;
    ts_pmt_t *p_pmt = p_pes->p_es->p_program;

    const es_mpeg4_descriptor_t *p_mpeg4desc = GetMPEG4DescByEsId( p_pmt, p_pes->p_es->i_sl_es_id );
    if( p_mpeg4desc && p_mpeg4desc->dec_descr.i_objectTypeIndication == 0x01 &&
        p_mpeg4desc->dec_descr.i_streamType == 0x01 /* Object */ )
    {
        const uint8_t *p_data = p_payloaddata;
        size_t i_data = i_payloaddata;

        od_descriptors_t *p_ods = &p_pmt->od;
        sl_header_data header = DecodeSLHeader( i_data, p_data, &p_mpeg4desc->sl_descr );

        DecodeODCommand( VLC_OBJECT(p_demux), p_ods, i_data - header.i_size, &p_data[header.i_size] );
        bool b_changed = false;

        for( int i=0; i<p_ods->objects.i_size; i++ )
        {
            od_descriptor_t *p_od = p_ods->objects.p_elems[i];
            for( int j = 0; j < ES_DESCRIPTOR_COUNT && p_od->es_descr[j].b_ok; j++ )
            {
                p_mpeg4desc = &p_od->es_descr[j];
                ts_es_t *p_es = GetPMTESBySLEsId( p_pmt, p_mpeg4desc->i_es_id );
                es_format_t fmt;
                es_format_Init( &fmt, UNKNOWN_ES, 0 );

                if ( p_mpeg4desc && p_mpeg4desc->b_ok && p_es &&
                     SetupISO14496LogicalStream( p_demux, &p_mpeg4desc->dec_descr, &fmt ) &&
                     !es_format_IsSimilar( &fmt, &p_es->fmt ) )
                {
                    fmt.i_id = p_es->fmt.i_id;
                    fmt.i_group = p_es->fmt.i_group;
                    es_format_Clean( &p_es->fmt );
                    p_es->fmt = fmt;

                    if( p_es->id )
                        es_out_Del( p_demux->out, p_es->id );
                    p_es->fmt.b_packetized = true; /* Split by access unit, no sync code */
                    p_es->id = es_out_Add( p_demux->out, &p_es->fmt );
                    b_changed = true;
                }
                else
                    es_format_Clean( &fmt );
            }
        }

        if( b_changed )
            UpdatePESFilters( p_demux, p_demux->p_sys->seltype == PROGRAM_ALL );
    }
}

typedef struct
{
    block_t     *p_au;
    block_t     **pp_au_last;
    ts_stream_t *p_stream;

} SL_stream_processor_context_t;

static void SL_stream_processor_Delete( ts_stream_processor_t *h )
{
    SL_stream_processor_context_t *ctx = (SL_stream_processor_context_t *) h->priv;
    block_ChainRelease( ctx->p_au );
    free( ctx );
    free( h );
}

static void SL_stream_processor_Reset( ts_stream_processor_t *h )
{
    SL_stream_processor_context_t *ctx = (SL_stream_processor_context_t *) h->priv;
    block_ChainRelease( ctx->p_au );
    ctx->p_au = NULL;
    ctx->pp_au_last = &ctx->p_au;
}

static block_t * SL_stream_processor_Push( ts_stream_processor_t *h, uint8_t i_stream_id, block_t *p_block )
{
    SL_stream_processor_context_t *ctx = (SL_stream_processor_context_t *) h->priv;
    ts_es_t *p_es = ctx->p_stream->p_es;
    ts_pmt_t *p_pmt = p_es->p_program;

    if( ((i_stream_id & 0xFE) != 0xFA) /* 0xFA || 0xFB */ )
    {
        block_Release( p_block );
        return NULL;
    }

    const es_mpeg4_descriptor_t *p_desc = GetMPEG4DescByEsId( p_pmt, p_es->i_sl_es_id );
    if(!p_desc)
    {
        block_Release( p_block );
        p_block = NULL;
    }
    else
    {
        sl_header_data header = DecodeSLHeader( p_block->i_buffer, p_block->p_buffer,
                                                &p_desc->sl_descr );
        p_block->i_buffer -= header.i_size;
        p_block->p_buffer += header.i_size;
        p_block->i_dts = header.i_dts ? header.i_dts : p_block->i_dts;
        p_block->i_pts = header.i_pts ? header.i_pts : p_block->i_pts;

        /* Assemble access units */
        if( header.b_au_start && ctx->p_au )
        {
            block_ChainRelease( ctx->p_au );
            ctx->p_au = NULL;
            ctx->pp_au_last = &ctx->p_au;
        }
        block_ChainLastAppend( &ctx->pp_au_last, p_block );
        p_block = NULL;
        if( header.b_au_end && ctx->p_au )
        {
            p_block = block_ChainGather( ctx->p_au );
            ctx->p_au = NULL;
            ctx->pp_au_last = &ctx->p_au;
        }
    }

    return p_block;
}

ts_stream_processor_t *SL_stream_processor_New( ts_stream_t *p_stream )
{
    ts_stream_processor_t *h = malloc(sizeof(*h));
    if(!h)
        return NULL;

    SL_stream_processor_context_t *ctx = malloc( sizeof(SL_stream_processor_context_t) );
    if(!ctx)
    {
        free(h);
        return NULL;
    }
    ctx->p_au = NULL;
    ctx->pp_au_last = &ctx->p_au;
    ctx->p_stream = p_stream;

    h->priv = ctx;
    h->pf_delete = SL_stream_processor_Delete;
    h->pf_push = SL_stream_processor_Push;
    h->pf_reset = SL_stream_processor_Reset;

    return h;
}
