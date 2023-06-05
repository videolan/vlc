/*****************************************************************************
 * g64rtp.c : G64 raw RTP input module for vlc
 *****************************************************************************
 * Copyright (C) 2023 VideoLabs, VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_vector.h>
#include <vlc_charset.h>
#include <vlc_meta.h>

#include "../access/rtp/rtp.h"
#include "../access/rtp/sdp.h"

/*****************************************************************************
 * Definitions of structures used by this plugin
 *****************************************************************************/

//#define G64_DEBUG

typedef struct
{
    vlc_tick_t time;
    uint64_t offset;
} index_t;

typedef struct VLC_VECTOR(index_t) vec_index_t;

typedef struct
{
    es_out_id_t *es;
    struct vlc_rtp_es dummy_es;
    vlc_fourcc_t fcc;
    uint8_t i_type;

    struct vlc_rtp_pt pt;
    void *ptpriv;

    vlc_tick_t pcr;

    vec_index_t index;
    uint64_t i_start_offset;

    vlc_tick_t i_first_pts;
    vlc_tick_t i_duration;
    char *psz_title;

} demux_sys_t;

#define G64_WRAP_HEADER_BYTES   25
#define G64_FLAG_HIDDEN         0x01
#define G64_FLAG_RANDOM_ACCESS  0x04

typedef struct
{
    enum
    {
        UNIT_RTP,
        UNIT_FRAMEMETA,
    } type;
    uint64_t pts;
    uint8_t flags;
    uint32_t size;
} dump_unit_wrapper_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/


/*
#	Type		ReceptionTime		Intrval	Size	Keyfram	Hidden	First	Last	Count	Error	RTP TimeStamp

0	AxisH264 (96)	24/12/2022 01:11:40.797	0	112978	Yes	Yes	43704	43786	83	-	675574737
BEX as 01D9172C4C2A22D0
DEC as 13316314300 7970000

1	AxisH264 (96)	24/12/2022 01:11:40.843	46	9641		Yes	43787	43793	7	-	675578337
DEC as 13316314300 8430000

5	AxisH264 (96)	24/12/2022 01:11:41.003	36	15424		Yes	43826	43836	11	-	675592735
DEC as 13316314301 0030000
13316314301 in secs is 154124 days, 0 hours, 11 minutes and 41 seconds.

30	AxisH264 (96)	24/12/2022 01:11:42.003	30	14203		Yes	44102	44112	11	-	675682736
DEC as 13316314302 0030000

55	AxisH264 (96)	24/12/2022 01:11:43.003	36	13777		Yes	44365	44374	10	-	675772736
DEC as 13316314303 0030000

1555	AxisH264 (96)	24/12/2022 01:12:43.007	44	14217			61619	61629	11	-	681172723
DEC as 13316314363 0070000

6776	AxisH264 (96)	24/12/2022 01:16:11.840	40	14670			55442	55452	11	-	699968282
DEC as 13316314571 8400000
13316314571 in secs is 154124 days, 0 hours, 16 minutes and 11 seconds.

Epoch + sub 10⁷ with Offset 0x2B61082F0 ?

*/

static vlc_tick_t make_vlc_timestamp( uint64_t ts )
{
    if( ts == 0xFFFFFFFFFFFFFFFFU ) /* This is All 0xFF */
        return VLC_TICK_INVALID;
    ts = (ts % 10000000) + ((ts / 10000000) - 0x2B61082F0) * 10000000;
    return VLC_TICK_0 + ts / 10;
}

/*
    {File Header}
    |@000 30    Signature & version
    |@030 08    End Time Timestamp
    |           FIXME Metadata starts from here.
    |           Might be variable number of entries. Go figure.
    |@038 54    Fixedsize? Fixeddata? Unknown
            |+000 07    Unknown
            |+000 53    Fixed? Fixeddata? Unknown
    |@090 20     Some Metadata header
            |+000 16    Entity UUID
            |+000 04    $M1 Metadata characters count in UTF16
    |@110 $M1*2 Metadata bytes
    | ( if version >= 5 )
        |+000 20    Some Metadata header
                |+000 16    Entity UUID
                |+000 04    $M2 Metadata characters count in UTF16
        |+000 $M2*2 Metadata bytes
        |+000 60    Some UUID meta
                |+000 16    Usage UUID
                |+000 16    Origin UUID
                |+000 16    MediaType UUID
    |+000 04    Fixedsize? Fixeddata? Unkown
    | ( if version >= 5 )
        |+000 02    Fixedsize? Fixeddata? Unkown
    |+000 04    Fixedsize? Fixeddata? Unkown
    |+000 04    First Unit Header backward offset 0xFFFFFFFF

    {RTP Dump Wrapper}
        |+000 08    Timestamp
        |+000 01    Flags
        |+000 04    Wrapped packets size $VAR
        |+000 12    ? 0x80 .. 0x30 (seems saved rtp packet header)
        |+000 $VAR  Wrapped packets
            | [1..N]
                |+000 06    RTP Packet Wrapper
                    |+000 02    Fixed? 0x00 (0x1F|0x1D)
                    |+000 02    Payload size $VAR2
                    |+000 02    Unknown (maybe high bytes for $VAR2)
                |+000 $VAR2 RTP Packet
        |+000 04    Total Wrapper size, backward navigation offset

    {Frame Metadata}
        |+000 08    Timestamp == 0xFFFFFFFFFFFFFFFF
        |+000 04    Frame Meta Payload size $VAR
        |+000 13    Unknown
        |+000 $VAR  Payload
            |+000 06    Unknown
            | [1..N]
                |+000 51    Index Entry
                    |+000 18    Unknown
                    |+000 08    Timestamp
                    |+000 25    Unknown
        |+000 04    Total size, backward navigation offset

    {IFrame Index}
        |+000 08    Timestamp == 0xFFFFFFFF
        | [1..N]
            |+000 08    Timestamp
            |+000 08    File offset
            |+000 02    Unknown
        |+000 02    Unknown
        |+000 04    Total size, backward navigation offset

    File Layout
        | {File Header}
        | {RTP Dump Wrapper}
        | {Frame Metadata}
        | {IFrame Index}
*/

#if 0
static int parse_framemetadata_payload( stream_t *s, uint32_t payloadsize )
{
    fprintf(stderr," parse_framemetadata_unitpayload @%"PRIu64"\n", vlc_stream_Tell(s));
    if( vlc_stream_Read( s, NULL, 6 ) != 6 )
        return VLC_EGENERIC;
    payloadsize -= 6;
    while( payloadsize >= 51 )
    {
        uint8_t unitheader[51];
        if( vlc_stream_Read( s, unitheader, 51 ) != 51 )
            return VLC_EGENERIC;
        uint64_t ts = make_vlc_timestamp( GetQWLE( &unitheader[18] ) );
        fprintf(stderr,"dump index %"PRId64" %u %u\n", ts,
                GetDWLE( &unitheader[22] ), GetDWLE( &unitheader[26] ));
        payloadsize -= 51;
    }
    assert(payloadsize == 0);
    return payloadsize == 0 ? VLC_SUCCESS : VLC_EGENERIC;
}
#endif

static int parse_iframeindex_payload( stream_t *s, uint32_t payloadsize, vec_index_t *index )
{
    msg_Dbg( s, "parsing index @%" PRIu64, vlc_stream_Tell(s) );

    if( index && !vlc_vector_reserve( index, payloadsize / 18 ) )
        return VLC_EGENERIC;

    while( payloadsize > 18 )
    {
        uint8_t unitheader[18];
        if( vlc_stream_Read( s, unitheader, 18 ) != 18 )
            return VLC_EGENERIC;

        index_t entry;
        entry.time = make_vlc_timestamp( GetQWLE( &unitheader[0] ) );
        entry.offset = GetQWLE( &unitheader[8] );
        if( index )
            vlc_vector_push( index, entry );

        payloadsize -= 18;
    }

    if( payloadsize == 2 )
        if( vlc_stream_Read( s, NULL, 2 ) != 2 )
            return VLC_EGENERIC;

    return payloadsize == 2 ? VLC_SUCCESS : VLC_EGENERIC;
}

static int parse_metadata( stream_t *s, char **ppsz )
{
    uint8_t header[20];
    if( vlc_stream_Read( s, header, 20 ) != 20 )
        return VLC_EGENERIC;
    uint32_t sz = GetDWLE( &header[16] ) * 2;
    void *buf;
    if( ppsz && (buf = malloc( sz )) )
    {
        int i_ret;
        if( vlc_stream_Read( s, buf, sz ) == sz )
        {
            *ppsz = FromCharset( "UTF-16LE", buf, sz );
             i_ret = *ppsz ? VLC_SUCCESS : VLC_EGENERIC;
        }
        else
            i_ret = VLC_EGENERIC;
        free( buf );
        return i_ret;
    }
    else
    {
        if( vlc_stream_Read( s, NULL, sz ) != sz )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int parse_file_header( stream_t *s, char **ppsz_title )
{
    const uint8_t *p_peek;
    uint8_t prevoffset[4];
    char *psz_title = NULL, **pp_meta = &psz_title;

    if( vlc_stream_Peek( s, &p_peek, 7 ) != 7 ||
        memcmp( p_peek, "Genetec", 7 ) )
        return VLC_EGENERIC;

    /* Version */
    const uint8_t version = p_peek[26] - '0';
    int uuidtextcount = version < 5 ? 1 : 2;
    int uuidcount = version < 5 ? 0 : 3;

    if( vlc_stream_Seek( s, 30 + 60 ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    /* Parse UUID+text entries */
    for( ; uuidtextcount; uuidtextcount-- )
    {
        if( parse_metadata( s, pp_meta ) != VLC_SUCCESS )
            goto error;
        pp_meta = NULL;
    }

    /* Parse UUID entries */
    for( ; uuidcount; uuidcount-- )
    {
        if( vlc_stream_Read( s, NULL, 16 ) != 16 )
            goto error;
    }

    ssize_t toread = version < 5 ? 10 : 12;
    if (vlc_stream_Read( s, NULL, toread ) != toread ||
        vlc_stream_Read( s, prevoffset, 4 ) != 4 ||
        GetDWLE( prevoffset ) != 0xFFFFFFFFU )
        goto error;

    *ppsz_title = psz_title;

    return VLC_SUCCESS;

error:
    free( psz_title );
    return VLC_EGENERIC;
}

static int parse_wrapper_header( const uint8_t p[G64_WRAP_HEADER_BYTES], dump_unit_wrapper_t *unit )
{
#ifdef G64_DEBUG
    fprintf(stderr, "dump unit: ");
    for(int i=0; i<G64_WRAP_HEADER_BYTES; i++)
        fprintf(stderr, "%2.2x ", p[i]);
#endif
    if( p[G64_WRAP_HEADER_BYTES - 1] == 0x30 && p[13] == 0x80 )
    {
        unit->type = UNIT_RTP;
        unit->pts = make_vlc_timestamp( GetQWLE(&p[0]) );
        unit->flags = p[8];
        unit->size = GetDWLE(&p[9]);
#ifdef G64_DEBUG
        fprintf(stderr, "%"PRId64" %u %2.2x\n", unit->pts, unit->size, unit->flags);
#endif
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static int g64_rtp_pt_instantiate( vlc_object_t *obj, struct vlc_rtp_pt *restrict pt,
                               const struct vlc_sdp_pt *restrict desc, const char *mime )
{
    int ret = VLC_ENOTSUP;

    module_t **mods;
    ssize_t n = vlc_module_match( "rtp parser", mime, true, &mods, NULL );

    for ( ssize_t i = 0; i < n; i++ )
    {
        vlc_rtp_parser_cb cb = vlc_module_map( vlc_object_logger(obj), mods[i] );
        if ( cb == NULL )
            continue;

        ret = cb(obj, pt, desc);
        if ( ret == VLC_SUCCESS )
        {
            msg_Dbg( obj, "- module \"%s\"", module_get_name(mods[i], true) );
            assert( pt->ops != NULL );
            ret = VLC_SUCCESS;
            break;
        }
    }

    free( mods );
    return ret;
}

static void g64_dummy_es_destroy( struct vlc_rtp_es *es )
{
    VLC_UNUSED(es);
}

static void g64_dummy_es_decode( struct vlc_rtp_es *es, block_t *block )
{
    VLC_UNUSED(es);
    block_Release( block );
}

static const struct vlc_rtp_es_operations g64_dummy_es_ops =
{
    g64_dummy_es_destroy, g64_dummy_es_decode,
};

struct g64_es_id {
    struct vlc_rtp_es es;
    es_out_t *out;
    es_out_id_t *id;
    vlc_tick_t *ppts;
};

static void g64_es_id_destroy( struct vlc_rtp_es *es )
{
    struct g64_es_id *ei = container_of(es, struct g64_es_id, es);
    free( ei );
}

static void g64_es_id_send( struct vlc_rtp_es *es, block_t *block )
{
    struct g64_es_id *ei = container_of(es, struct g64_es_id, es);
    block->i_pts = *ei->ppts;

    es_out_Send( ei->out, ei->id, block );
}

static const struct vlc_rtp_es_operations g64_es_id_ops = {
    g64_es_id_destroy, g64_es_id_send,
};

static struct vlc_rtp_es *g64_rtp_es_request( struct vlc_rtp_pt *pt,
                                              const es_format_t *restrict fmt )
{
    demux_t *demux = pt->owner.data;
    demux_sys_t *p_sys  = demux->p_sys;
    VLC_UNUSED(fmt);
    assert(fmt->i_codec == p_sys->fcc);

    struct g64_es_id *ei = malloc(sizeof (*ei));
    if (unlikely(ei == NULL))
        return &p_sys->dummy_es;

    ei->es.ops = &g64_es_id_ops;
    ei->out = demux->out;
    ei->id = p_sys->es;
    ei->ppts = &p_sys->pcr;
    return &ei->es;
}

static struct vlc_rtp_es *g64_rtp_mux_request( struct vlc_rtp_pt *pt,
                                               const char *name )
{
    VLC_UNUSED(pt);
    VLC_UNUSED(name);
    return NULL;
}

static void g64_rtp_pt_clear( struct vlc_rtp_pt *pt, void **pptriv )
{
    vlc_rtp_pt_end( pt, *pptriv );
    pt->ops->release( pt );
    *pptriv = NULL;
}

static const struct vlc_rtp_pt_owner_operations g64_pt_owner_ops = {
    g64_rtp_es_request,
    g64_rtp_mux_request,
};

static int g64_rtp_pt_init( demux_t *p_demux, struct vlc_rtp_pt *pt, vlc_fourcc_t fcc )
{
    struct vlc_rtp_pt_owner owner = {
        &g64_pt_owner_ops, p_demux
    };

    static const struct
    {
        vlc_fourcc_t fcc;
        const char *mime;
        const char *parameters;
    } payloads[] = {
        { VLC_CODEC_H264, "video/H264", "packetization-mode=1;" },
        { VLC_CODEC_HEVC, "video/H265", NULL },
        { VLC_CODEC_MULAW, "audio/PCMU", NULL },
    };

    for(unsigned i=0;i<ARRAY_SIZE(payloads); i++)
    {
        if(payloads[i].fcc != fcc)
            continue;

        /* we mostly don't care about SDP values here,
           we just need correct mime and parameters to open the pt */
        struct vlc_sdp_pt sdp = {0};
        sdp.channel_count = 2;
        sdp.clock_rate = 90000;
        strcat(sdp.name, strchr(payloads[i].mime,'/') + 1);
        sdp.parameters = payloads[i].parameters;
        pt->owner = owner;
        pt->frequency = 90000;
        pt->channel_count = 2;
        pt->number = 96;

        if ( g64_rtp_pt_instantiate( VLC_OBJECT(p_demux), pt, &sdp,
                                     payloads[i].mime ) == VLC_SUCCESS )
            return VLC_SUCCESS;

        break;
    }

    return VLC_EGENERIC;
}

static int LoadIndex( stream_t *s, vec_index_t *index )
{
    uint64_t i_size;
    const uint8_t *p_peek;

    if( vlc_stream_GetSize( s, &i_size ) != VLC_SUCCESS ||
        vlc_stream_Seek( s, i_size - 4 ) != VLC_SUCCESS ||
        vlc_stream_Peek( s, &p_peek, 4 ) != 4 )
        return VLC_EGENERIC;

    uint32_t i_offset = GetDWLE( p_peek );
    if( i_offset < 14 + 18 ||
        i_offset > i_size - 4 - 4 ||
        vlc_stream_Seek( s, i_size - i_offset - 4 - 4 ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    uint8_t header[12];
    if( vlc_stream_Read( s, header, 12 ) != 12 ||
        GetQWLE( &header[4] ) != 0xFFFFFFFFFFFFFFFFU )
        return VLC_EGENERIC;

    return parse_iframeindex_payload( s, i_offset + 4 - 12, index );
}

static int BuildIndex( stream_t *s, vec_index_t *index )
{
    for( ;; )
    {
        index_t entry = { 0, vlc_stream_Tell(s) };

        uint8_t unit_header[G64_WRAP_HEADER_BYTES];
        if( vlc_stream_Read( s, unit_header, G64_WRAP_HEADER_BYTES )
                != G64_WRAP_HEADER_BYTES )
            break;

        dump_unit_wrapper_t unit;
        if( parse_wrapper_header(unit_header, &unit) != VLC_SUCCESS ||
            unit.size < 12 || unit.type != UNIT_RTP )
            break;

        if( unit.flags & G64_FLAG_RANDOM_ACCESS )
        {
            entry.time = unit.pts;
            if( !vlc_vector_push( index, entry ) )
                break;
        }

        if( vlc_stream_Seek( s, vlc_stream_Tell(s) + unit.size - 8 ) != VLC_SUCCESS )
            return VLC_EGENERIC;
    }

    return index->size ? VLC_SUCCESS : VLC_EGENERIC;
}

static void parse_rtp( demux_t *p_demux, block_t *p_block )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    if ( p_block->i_buffer < 12 )
        goto end;

    const uint8_t version = p_block->p_buffer[0] >> 6;
    const uint8_t pad = p_block->p_buffer[0] & 0x20;
    const uint8_t ext = p_block->p_buffer[0] & 0x10;
    const uint8_t cc = p_block->p_buffer[0] & 0x0f;
    const uint8_t mbit = p_block->p_buffer[1] & 0x80;
    //const uint8_t pt = p_block->p_buffer[1] & 0x7f;

    if ( version != 0x02 )
        goto end;

    p_block->p_buffer += 12;
    p_block->i_buffer -= 12;

    if ( pad )
    {
        if( p_block->i_buffer < 2 )
            goto end;

        uint8_t trunc = p_block->p_buffer[p_block->i_buffer - 1];
        if (trunc > 0 && trunc < p_block->i_buffer)
            p_block->i_buffer -= trunc;
        else
            goto end;
    }

    if ( cc )
    {
        if( cc * 4 > p_block->i_buffer )
            goto end;
        p_block->p_buffer += cc * 4;
        p_block->i_buffer -= cc * 4;
    }

    if ( ext )
    {
        if ( p_block->i_buffer < 4 )
            goto end;
        unsigned extlen = GetWBE( &p_block->p_buffer[2] );
        if( extlen * 4 > p_block->i_buffer - 4 )
            goto end;
        p_block->p_buffer += 4 + extlen * 4;
        p_block->i_buffer -= 4 + extlen * 4;
    }

    const struct vlc_rtp_pktinfo pktinfo = { .m = mbit };
    vlc_rtp_pt_decode( &p_sys->pt, p_sys->ptpriv, p_block, &pktinfo );

    return;

end:
    block_Release( p_block );
}

static int parse_rtp_dumps( demux_t *p_demux, uint32_t size )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    while( size >= 6 )
    {
        /* Process one packet dump */
        uint8_t header[6];
        if( vlc_stream_Read( p_demux->s, header, 6 ) != 6 )
            break;

        if( header[0] != 0x00 || header[1] != p_sys->i_type )
            break;

        uint16_t sz = GetWBE( &header[2] );

        if( sz > size + 6 )
            break;

        block_t *p_block = vlc_stream_Block( p_demux->s, sz );
        if( p_block == NULL )
            break;

        parse_rtp( p_demux, p_block );

        size -= 6 + sz;
    }

    return (size != 0) ? VLC_DEMUXER_EOF : VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    uint8_t unit_header[G64_WRAP_HEADER_BYTES];
    if( vlc_stream_Read( p_demux->s, unit_header, G64_WRAP_HEADER_BYTES )
            != G64_WRAP_HEADER_BYTES )
        return VLC_DEMUXER_EOF;

    dump_unit_wrapper_t unit;
    if( parse_wrapper_header( unit_header, &unit ) != VLC_SUCCESS ||
        unit.size < 12 || unit.type != UNIT_RTP )
        return VLC_DEMUXER_EOF;

    if ( p_sys->pcr == VLC_TICK_INVALID && unit.pts != VLC_TICK_INVALID )
        es_out_SetPCR( p_demux->out, unit.pts );
    p_sys->pcr = unit.pts;

    int ret = parse_rtp_dumps( p_demux, unit.size - 12 );
    uint8_t wrappersize[4];
    if(ret != VLC_DEMUXER_SUCCESS)
    {
        msg_Err( p_demux, "Invalid wrapper bytes @%"PRIu64,
                vlc_stream_Tell(p_demux->s) - 6);
        ret = VLC_DEMUXER_EOF;
    }
    else if( vlc_stream_Read( p_demux->s, wrappersize, 4 ) != 4 ||
        GetDWLE(wrappersize) != unit.size + 13 )
    {
        msg_Err( p_demux, "Invalid end of wrapper @%"PRIu64,
                 vlc_stream_Tell(p_demux->s) );
        ret = VLC_DEMUXER_EOF;
    }

    if( p_sys->pcr != VLC_TICK_INVALID )
        es_out_SetPCR( p_demux->out, p_sys->pcr );

    return ret;
}

static int SeekTo( demux_t *p_demux, vlc_tick_t ts, bool b_precise )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    if( p_sys->index.size == 0 )
        return VLC_EGENERIC;

    /* delete and replace session first as we can't reset */
    struct vlc_rtp_pt newpt = {0};
    if( g64_rtp_pt_init( p_demux, &newpt, p_sys->fcc ) != VLC_SUCCESS )
        return VLC_EGENERIC;
    g64_rtp_pt_clear( &p_sys->pt, &p_sys->ptpriv );
    p_sys->pt = newpt;
    p_sys->ptpriv = vlc_rtp_pt_begin( &p_sys->pt );

    uint64_t offset = p_sys->i_start_offset;
    for( size_t i=0; i<p_sys->index.size; i++ )
    {
        if( p_sys->index.data[i].time > ts )
            break;
        offset = p_sys->index.data[i].offset;
    }

    p_sys->pcr = VLC_TICK_INVALID;
    int ret = vlc_stream_Seek( p_demux->s, offset );
    if( ret == VLC_SUCCESS && b_precise )
        b_precise = es_out_SetNextDisplayTime( p_demux->out, ts );
    return ret;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    switch( i_query )
    {
        case DEMUX_GET_LENGTH:
        {
            *va_arg(args, vlc_tick_t *) = p_sys->i_duration;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_NORMAL_TIME:
        {
            if( p_sys->i_first_pts != VLC_TICK_INVALID )
            {
                *va_arg(args, vlc_tick_t *) = p_sys->i_first_pts;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_GET_TIME:
            if( p_sys->pcr == VLC_TICK_INVALID )
                return VLC_EGENERIC;
            *va_arg(args, vlc_tick_t *) = p_sys->pcr;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
        {
            vlc_tick_t ts = va_arg(args, vlc_tick_t);
            bool b_precise = va_arg( args, int );
            return SeekTo( p_demux, p_sys->i_first_pts + ts, b_precise );
        }

        case DEMUX_GET_POSITION:
        {
            if( p_sys->i_duration == 0 || p_sys->pcr == VLC_TICK_INVALID )
                return VLC_EGENERIC;
            *va_arg(args, double *) = (p_sys->pcr - p_sys->i_first_pts) / (double) p_sys->i_duration;
            return VLC_SUCCESS;
        }

        case DEMUX_SET_POSITION:
        {
            if( p_sys->i_duration == 0 )
                return VLC_EGENERIC;
            double f = va_arg(args, double);
            bool b_precise = va_arg( args, int );
            return SeekTo( p_demux, p_sys->i_first_pts + p_sys->i_duration * f, b_precise );
        }

        case DEMUX_GET_META:
        {
            vlc_meta_t *p_meta;
            if ( p_sys->psz_title && (p_meta = va_arg(args, vlc_meta_t *)) )
            {
                vlc_meta_Set( p_meta, vlc_meta_Title, p_sys->psz_title );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        default:
            return demux_vaControlHelper( p_demux->s, 0, -1, -1, -1,
                                          i_query, args );
    }
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*) p_this;
    demux_sys_t *p_sys  = p_demux->p_sys;
    if( p_sys->pt.ops )
        g64_rtp_pt_clear( &p_sys->pt, &p_sys->ptpriv );
    if( p_sys->es )
        es_out_Del( p_demux->out, p_sys->es );
    vlc_vector_clear( &p_sys->index );
    free( p_sys->psz_title );
    free( p_sys );
}

/*****************************************************************************
 * Open:
 *****************************************************************************/

static int Open( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t*) p_this;
    demux_sys_t *p_sys;
    es_format_t fmt;
    char *psz_title = NULL;

    if( parse_file_header( p_demux->s, &psz_title ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    const uint8_t *p_peek;
    if( vlc_stream_Peek( p_demux->s, &p_peek, G64_WRAP_HEADER_BYTES + 6 ) !=
        G64_WRAP_HEADER_BYTES + 6 )
        return VLC_EGENERIC;

    switch( p_peek[G64_WRAP_HEADER_BYTES + 1] )
    {
        case 0x1F:
        case 0x1D:
            es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_H264 );
            break;
        case 0x3D:
            es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_HEVC );
            break;
        case 0xD2:
            es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_MULAW );
            fmt.audio.i_channels = 1;
            fmt.audio.i_rate = 8000;
            break;
        default:
            msg_Err(p_this, "unknown %2.2x", p_peek[G64_WRAP_HEADER_BYTES + 1]);
            free( psz_title );
            return VLC_EGENERIC;
    }

    p_demux->p_sys = p_sys = calloc( 1, sizeof(demux_sys_t) );
    if( !p_sys )
    {
        free( psz_title );
        return VLC_ENOMEM;
    }

    p_sys->i_type = p_peek[G64_WRAP_HEADER_BYTES + 1];
    vlc_vector_init( &p_sys->index );
    p_sys->i_start_offset = vlc_stream_Tell( p_demux->s );
    p_sys->psz_title = psz_title;

    p_sys->fcc = fmt.i_codec;
    fmt.b_packetized = true;

    p_sys->es = es_out_Add( p_demux->out, &fmt );
    if( !p_sys->es )
    {
        Close( p_this );
        return VLC_EGENERIC;
    }
    p_sys->dummy_es.ops = &g64_dummy_es_ops;

    /* Build RTP pt */
    if( g64_rtp_pt_init( p_demux, &p_sys->pt, fmt.i_codec ) != VLC_SUCCESS )
    {
        Close( p_this );
        return VLC_EGENERIC;
    }

    p_sys->ptpriv = vlc_rtp_pt_begin( &p_sys->pt );

    if( !p_demux->b_preparsing && vlc_stream_CanSeek( p_demux->s ) )
    {
        /* Load native index from end of file */
         int ret = LoadIndex( p_demux->s, &p_sys->index );
         if( vlc_stream_Seek( p_demux->s, p_sys->i_start_offset ) )
         {
             Close( p_this );
             return VLC_EGENERIC;
         }

         /* Build Index if file is truncated */
         if( ret != VLC_SUCCESS && vlc_stream_CanFastSeek( p_demux->s ) )
         {
             msg_Warn( p_demux, "Can't load index. Trying to rebuild" );
                if( BuildIndex( p_demux->s, &p_sys->index ) != VLC_SUCCESS )
                    msg_Warn( p_demux, "Can't build index. Won't be able to seek" );
                if( vlc_stream_Seek( p_demux->s, p_sys->i_start_offset ) )
                {
                    Close( p_this );
                    return VLC_EGENERIC;
                }
         }
    }

    if( p_sys->index.size > 0 )
    {
        p_sys->i_first_pts = p_sys->index.data[0].time;
        p_sys->i_duration = p_sys->index.data[p_sys->index.size -1].time - p_sys->i_first_pts;
    }
    else
    {
        p_sys->i_first_pts = VLC_TICK_INVALID;
        p_sys->i_duration = 0;
    }

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    return VLC_SUCCESS;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname( "G64 raw RTP" )
    set_description( N_("G64 Raw RTP") )
    set_capability( "demux", 10 )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_callbacks( Open, Close )
vlc_module_end ()
