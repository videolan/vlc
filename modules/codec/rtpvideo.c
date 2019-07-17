/*****************************************************************************
 * rtpvideo.c: video encoder for raw video for RTP (see RFC 4175)
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 * Authors: Tristan Matthews <tmatth@videolan.org>
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
#include <vlc_codec.h>

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenEncoder( vlc_object_t * );
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Raw video encoder for RTP") )
    set_capability( "encoder", 50 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callback( OpenEncoder )
    add_shortcut( "rtpvideo" )
vlc_module_end ()

static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    if( p_enc->fmt_out.i_codec != VLC_CODEC_R420 && !p_enc->obj.force )
        return VLC_EGENERIC;

    p_enc->pf_encode_video = Encode;
    p_enc->fmt_in.i_codec = VLC_CODEC_I420;
    p_enc->fmt_out.i_codec = VLC_CODEC_R420;

    return VLC_SUCCESS;
}

static block_t *Encode( encoder_t *p_enc, picture_t *p_pict )
{
    VLC_UNUSED( p_enc );
    if( !p_pict ) return NULL;

    const int i_length = p_pict->p[0].i_visible_lines * p_pict->p[0].i_visible_pitch +
        p_pict->p[1].i_visible_lines * p_pict->p[1].i_visible_pitch +
        p_pict->p[2].i_visible_lines * p_pict->p[2].i_visible_pitch;

    block_t *p_block = block_Alloc( i_length );
    if( !p_block ) return NULL;

    p_block->i_dts = p_block->i_pts = p_pict->date;
    uint8_t *p_outdata = p_block->p_buffer;
    const int i_xdec = 2;
    const int i_ydec = 2;

    const uint8_t *p_yd1 = p_pict->p[0].p_pixels;
    const uint8_t *p_u = p_pict->p[1].p_pixels;
    const uint8_t *p_v = p_pict->p[2].p_pixels;

    /* Y00-Y01-Y10-Y11-U00-V00...luma from adjacent lines */
    for( int i_lin = 0; i_lin < p_pict->p[0].i_visible_lines; i_lin += i_ydec )
    {
        const uint8_t *p_yd2 = p_yd1 + p_pict->p[0].i_pitch;
        for ( int i_pix = 0; i_pix < p_pict->p[0].i_visible_pitch; i_pix += i_xdec )
        {
            *p_outdata++ = *p_yd1++;
            *p_outdata++ = *p_yd1++;
            *p_outdata++ = *p_yd2++;
            *p_outdata++ = *p_yd2++;
            *p_outdata++ = *p_u++;
            *p_outdata++ = *p_v++;
        }
        /* Skip a line + padding */
        p_yd1 += p_pict->p[0].i_pitch +
                 (p_pict->p[0].i_pitch - p_pict->p[0].i_visible_pitch);
        p_u += p_pict->p[1].i_pitch - p_pict->p[1].i_visible_pitch;
        p_v += p_pict->p[2].i_pitch - p_pict->p[2].i_visible_pitch;
    }

    return p_block;
}
