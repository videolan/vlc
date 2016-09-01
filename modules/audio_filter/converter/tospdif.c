/*****************************************************************************
 * tospdif.c : encapsulates A/52 and DTS frames into S/PDIF packets
 *****************************************************************************
 * Copyright (C) 2002, 2006-2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
 *          Rémi Denis-Courmont
 *          Rafaël Carré
 *          Thomas Guillem
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_aout.h>
#include <vlc_filter.h>

#include "../packetizer/a52.h"

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_description( N_("Audio filter for A/52/DTS->S/PDIF encapsulation") )
    set_capability( "audio converter", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

struct filter_sys_t
{
    block_t *p_chain_first;
    block_t **pp_chain_last;

    union
    {
        struct
        {
            unsigned int i_nb_blocks_substream0;
        } eac3;
    } spec;
};

#define IEC61937_AC3 0x01
#define IEC61937_EAC3 0x15
#define IEC61937_DTS1 0x0B
#define IEC61937_DTS2 0x0C
#define IEC61937_DTS3 0x0D

#define SPDIF_MORE_DATA 1
#define SPDIF_SUCCESS VLC_SUCCESS
#define SPDIF_ERROR VLC_EGENERIC
struct hdr_res
{
    uint16_t    i_data_type;
    size_t      i_out_size_padded;
    size_t      i_length_mul;
};

static int parse_header_ac3( filter_t *p_filter, block_t *p_in,
                             struct hdr_res *p_res )
{
    (void) p_filter;

    if( unlikely( p_in->i_buffer < 6 || p_in->i_nb_samples != A52_FRAME_NB ) )
        return SPDIF_ERROR;
    p_res->i_length_mul = 8; /* in bits */
    p_res->i_out_size_padded = A52_FRAME_NB * 4;
    p_res->i_data_type = ( (p_in->p_buffer[5] & 0x7) << 8 ) /* bsmod */
                   | IEC61937_AC3;
    return SPDIF_SUCCESS;
}

static int parse_header_eac3( filter_t *p_filter, block_t *p_in,
                              struct hdr_res *p_res )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_a52_header_t a52 = { };
    if( vlc_a52_header_Parse( &a52, p_in->p_buffer, p_in->i_buffer )
        != VLC_SUCCESS )
        return SPDIF_ERROR;

    p_in->i_buffer = a52.i_size;
    p_in->i_nb_samples = a52.i_samples;

    if( a52.b_eac3 )
    {
        if( ( a52.eac3.strmtyp == EAC3_STRMTYP_INDEPENDENT
           || a52.eac3.strmtyp == EAC3_STRMTYP_AC3_CONVERT )
         && a52.i_blocks_per_sync_frame != 6 )
        {
            /* cf. Annex E 2.3.1.2 of AC3 spec */
            if( a52.eac3.i_substreamid == 0 )
                p_sys->spec.eac3.i_nb_blocks_substream0
                    += a52.i_blocks_per_sync_frame;

            if( p_sys->spec.eac3.i_nb_blocks_substream0 != 6 )
                return SPDIF_MORE_DATA;
            else
                p_sys->spec.eac3.i_nb_blocks_substream0 = 0;
        }
        p_res->i_out_size_padded = AOUT_SPDIF_SIZE * 4;
        p_res->i_data_type = IEC61937_EAC3;
        p_res->i_length_mul = 1; /* in bytes */
        return SPDIF_SUCCESS;
    }
    else
        return SPDIF_MORE_DATA;

}

static int parse_header_dts( filter_t *p_filter, block_t *p_in,
                             struct hdr_res *p_res )
{
    if( unlikely( p_in->i_buffer < 1 ) )
        return SPDIF_ERROR;
    p_res->i_out_size_padded = p_in->i_nb_samples * 4;
    p_res->i_length_mul = 8; /* in bits */
    switch( p_in->i_nb_samples )
    {
    case  512:
        p_res->i_data_type = IEC61937_DTS1;
        return SPDIF_SUCCESS;
    case 1024:
        p_res->i_data_type = IEC61937_DTS2;
        return SPDIF_SUCCESS;
    case 2048:
        p_res->i_data_type = IEC61937_DTS3;
        return SPDIF_SUCCESS;
    default:
        msg_Err( p_filter, "Frame size %d not supported",
                 p_in->i_nb_samples );
        return SPDIF_ERROR;
    }
}

static int parse_header( filter_t *p_filter, block_t *p_in,
                         struct hdr_res *p_res )
{
    switch( p_filter->fmt_in.audio.i_format )
    {
        case VLC_CODEC_A52:
            return parse_header_ac3( p_filter, p_in, p_res );
        case VLC_CODEC_EAC3:
            return parse_header_eac3( p_filter, p_in, p_res );
        case VLC_CODEC_DTS:
            return parse_header_dts( p_filter, p_in, p_res );
        default:
            vlc_assert_unreachable();
    }
}

static bool is_big_endian( filter_t *p_filter, block_t *p_in )
{
    switch( p_filter->fmt_in.audio.i_format )
    {
        case VLC_CODEC_A52:
        case VLC_CODEC_EAC3:
            return true;
        case VLC_CODEC_DTS:
            return p_in->p_buffer[0] == 0x1F || p_in->p_buffer[0] == 0x7F;
        default:
            vlc_assert_unreachable();
    }
}

static void Flush( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->p_chain_first != NULL )
    {
        block_ChainRelease( p_sys->p_chain_first );
        p_sys->p_chain_first = NULL;
        p_sys->pp_chain_last = &p_sys->p_chain_first;
    }
    memset( &p_sys->spec, 0, sizeof( p_sys->spec ) );
}

static block_t *fill_output_buffer( filter_t *p_filter, struct hdr_res *p_res )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    unsigned i_nb_samples = 0;
    size_t i_out_size = 0;
    block_t *p_list = p_sys->p_chain_first;

    assert( p_list != NULL );

    while( p_list )
    {
        i_out_size += p_list->i_buffer;
        i_nb_samples += p_list->i_nb_samples;
        p_list = p_list->p_next;
    }

    if( i_out_size + 8 > p_res->i_out_size_padded )
    {
        msg_Warn( p_filter, "buffer too big for a S/PDIF frame" );
        return NULL;
    }

    block_t *p_out_buf = block_Alloc( p_res->i_out_size_padded );
    if( unlikely(!p_out_buf) )
        return NULL;
    uint8_t *p_out = p_out_buf->p_buffer;

    /* Copy the S/PDIF headers. */
    void (*write16)(void *, uint16_t) =
        ( p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB )
        ? SetWBE : SetWLE;

    write16( &p_out[0], 0xf872 ); /* syncword 1 */
    write16( &p_out[2], 0x4e1f ); /* syncword 2 */
    write16( &p_out[4], p_res->i_data_type ); /* data type */
    write16( &p_out[6], i_out_size * p_res->i_length_mul ); /* length in bits or bytes */

    p_list = p_sys->p_chain_first;
    bool b_input_big_endian = is_big_endian( p_filter, p_list );
    bool b_output_big_endian =
        p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB;

    p_out += 8;
    while( p_list )
    {
        uint8_t *p_in = p_list->p_buffer;
        size_t i_in_size = p_list->i_buffer;

        if( b_input_big_endian != b_output_big_endian )
        {
            swab( p_in, p_out, i_in_size & ~1 );

            if( i_in_size & 1 && ( i_out_size + 9 ) <= p_res->i_out_size_padded )
            {
                p_out[i_in_size - 1] = 0;
                p_out[i_in_size] = p_in[i_in_size - 1];
                i_out_size++;
                p_out++;
            }
            p_out += i_in_size;
        } else
        {
            memcpy( p_out, p_in, i_in_size );
            p_out += i_in_size;
        }
        p_list = p_list->p_next;
    }

    if( 8 + i_out_size < p_res->i_out_size_padded ) /* padding */
        memset( p_out, 0, p_res->i_out_size_padded - i_out_size - 8 );

    p_out_buf->i_dts = p_sys->p_chain_first->i_dts;
    p_out_buf->i_pts = p_sys->p_chain_first->i_pts;
    p_out_buf->i_nb_samples = i_nb_samples;
    p_out_buf->i_buffer = p_res->i_out_size_padded;

    return p_out_buf;
}

static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    block_t *p_out_buf = NULL;

    struct hdr_res res;
    int i_ret = parse_header( p_filter, p_in_buf, &res );
    switch( i_ret )
    {
        case SPDIF_SUCCESS:
            block_ChainLastAppend( &p_sys->pp_chain_last, p_in_buf );
            break;
        case SPDIF_MORE_DATA:
            block_ChainLastAppend( &p_sys->pp_chain_last, p_in_buf );
            return NULL;
        case SPDIF_ERROR:
            Flush( p_filter );
            goto out;
    }
    assert( res.i_data_type > 0 );
    assert( res.i_out_size_padded > 0 );
    assert( res.i_length_mul == 1 || res.i_length_mul == 8 );

    p_out_buf = fill_output_buffer( p_filter, &res );
out:
    block_ChainRelease( p_sys->p_chain_first );
    p_sys->p_chain_first = NULL;
    p_sys->pp_chain_last = &p_sys->p_chain_first;

    return p_out_buf;
}

static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    if( ( p_filter->fmt_in.audio.i_format != VLC_CODEC_DTS &&
          p_filter->fmt_in.audio.i_format != VLC_CODEC_A52 &&
          p_filter->fmt_in.audio.i_format != VLC_CODEC_EAC3 ) ||
        ( p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFL &&
          p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFB ) )
        return VLC_EGENERIC;

    p_sys = p_filter->p_sys = malloc( sizeof(filter_sys_t) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    p_sys->p_chain_first = NULL;
    p_sys->pp_chain_last = &p_sys->p_chain_first;

    memset( &p_sys->spec, 0, sizeof( p_sys->spec ) );

    p_filter->pf_audio_filter = DoWork;
    p_filter->pf_flush = Flush;

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    Flush( p_filter );
    free( p_filter->p_sys );
}
