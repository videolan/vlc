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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_aout.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static block_t *DoWork( filter_t *, block_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_description( N_("Audio filter for A/52/DTS->S/PDIF encapsulation") )
    set_capability( "audio converter", 10 )
    set_callbacks( Create, NULL )
vlc_module_end ()

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;

    if( ( p_filter->fmt_in.audio.i_format != VLC_CODEC_DTS &&
          p_filter->fmt_in.audio.i_format != VLC_CODEC_A52 ) ||
        ( p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFL &&
          p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFB ) )
        return VLC_EGENERIC;

    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;
}


static uint16_t get_data_type( filter_t *p_filter, block_t *p_in )
{
#define IEC61937_AC3 0x01
#define IEC61937_DTS1 0x0B
#define IEC61937_DTS2 0x0C
#define IEC61937_DTS3 0x0D

    switch( p_filter->fmt_in.audio.i_format )
    {
        case VLC_CODEC_A52:
            if( unlikely( p_in->i_buffer < 6 ) )
                return 0;
            return ( (p_in->p_buffer[5] & 0x7) << 8 ) /* bsmod */ | IEC61937_AC3;
        case VLC_CODEC_DTS:
            if( unlikely( p_in->i_buffer < 1 ) )
                return 0;
            switch( p_in->i_nb_samples )
            {
            case  512: return IEC61937_DTS1;
            case 1024: return IEC61937_DTS2;
            case 2048: return IEC61937_DTS3;
            default:
                msg_Err( p_filter, "Frame size %d not supported",
                         p_in->i_nb_samples );
                return 0;
            }
        default:
            vlc_assert_unreachable();
    }
    return 0;
}

static bool is_big_endian( filter_t *p_filter, block_t *p_in )
{
    switch( p_filter->fmt_in.audio.i_format )
    {
        case VLC_CODEC_A52:
            return true;
        case VLC_CODEC_DTS:
            return p_in->p_buffer[0] == 0x1F || p_in->p_buffer[0] == 0x7F;
        default:
            vlc_assert_unreachable();
    }
    return 0;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static block_t *DoWork( filter_t * p_filter, block_t *p_in_buf )
{
    uint16_t i_length = p_in_buf->i_buffer;
    uint8_t * p_in = p_in_buf->p_buffer;
    block_t *p_out_buf = NULL;

    uint16_t i_data_type = get_data_type( p_filter, p_in_buf );
    if( i_data_type == 0 || ( i_length + 8 ) > AOUT_SPDIF_SIZE )
        goto out;

    p_out_buf = block_Alloc( AOUT_SPDIF_SIZE );
    if( !p_out_buf )
        goto out;
    uint8_t *p_out = p_out_buf->p_buffer;

    /* Copy the S/PDIF headers. */
    void (*write16)(void *, uint16_t) =
        ( p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB )
        ? SetWBE : SetWLE;

    write16( &p_out[0], 0xf872 ); /* syncword 1 */
    write16( &p_out[2], 0x4e1f ); /* syncword 2 */
    write16( &p_out[4], i_data_type ); /* data type */
    write16( &p_out[6], i_length * 8 ); /* length in bits */

    bool b_input_big_endian = is_big_endian( p_filter, p_in_buf );
    bool b_output_big_endian =
        p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB;

    if( b_input_big_endian != b_output_big_endian )
    {
        swab( p_in, p_out + 8, i_length & ~1 );

        /* If i_length is odd, we have to adjust swapping a bit... */
        if( i_length & 1 && ( i_length + 9 ) <= AOUT_SPDIF_SIZE )
        {
            p_out[8 + i_length - 1] = 0;
            p_out[8 + i_length] = p_in[i_length-1];
            i_length++;
        }
    } else
        memcpy( p_out + 8, p_in, i_length );

    if( 8 + i_length < AOUT_SPDIF_SIZE ) /* padding */
        memset( p_out + 8 + i_length, 0, AOUT_SPDIF_SIZE - i_length - 8 );

    p_out_buf->i_dts = p_in_buf->i_dts;
    p_out_buf->i_pts = p_in_buf->i_pts;
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_buffer = AOUT_SPDIF_SIZE;
out:
    block_Release( p_in_buf );
    return p_out_buf;
}

