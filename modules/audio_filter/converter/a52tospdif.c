/*****************************************************************************
 * a52tospdif.c : encapsulates A/52 frames into S/PDIF packets
 *****************************************************************************
 * Copyright (C) 2002, 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
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
    set_description( N_("Audio filter for A/52->S/PDIF encapsulation") )
    set_capability( "audio converter", 10 )
    set_callbacks( Create, NULL )
vlc_module_end ()

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;

    if ( p_filter->fmt_in.audio.i_format != VLC_CODEC_A52 ||
         ( p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFB &&
           p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFL ) )
    {
        return VLC_EGENERIC;
    }

    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static block_t *DoWork( filter_t * p_filter, block_t *p_in_buf )
{
    /* AC3 is natively big endian. Most SPDIF devices have the native
     * endianness of the computer system.
     * On Mac OS X however, little endian devices are also common.
     */
    static const uint8_t p_sync_le[6] = { 0x72, 0xF8, 0x1F, 0x4E, 0x01, 0x00 };
    static const uint8_t p_sync_be[6] = { 0xF8, 0x72, 0x4E, 0x1F, 0x00, 0x01 };
    uint16_t i_frame_size = p_in_buf->i_buffer / 2;
    uint8_t * p_in = p_in_buf->p_buffer;

    block_t *p_out_buf = block_Alloc( AOUT_SPDIF_SIZE );
    if( !p_out_buf )
        goto out;
    uint8_t * p_out = p_out_buf->p_buffer;

    /* Copy the S/PDIF headers. */
    if( p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB )
    {
        memcpy( p_out, p_sync_be, 6 );
        p_out[4] = p_in[5] & 0x7; /* bsmod */
        SetWBE( p_out + 6, i_frame_size << 4 );
        memcpy( &p_out[8], p_in, i_frame_size * 2 );
    }
    else
    {
        memcpy( p_out, p_sync_le, 6 );
        p_out[5] = p_in[5] & 0x7; /* bsmod */
        SetWLE( p_out + 6, i_frame_size << 4 );
        swab( p_in, &p_out[8], i_frame_size * 2 );
    }
    memset( p_out + 8 + i_frame_size * 2, 0,
                AOUT_SPDIF_SIZE - i_frame_size * 2 - 8 );

    p_out_buf->i_dts = p_in_buf->i_dts;
    p_out_buf->i_pts = p_in_buf->i_pts;
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_buffer = AOUT_SPDIF_SIZE;
out:
    block_Release( p_in_buf );
    return p_out_buf;
}

