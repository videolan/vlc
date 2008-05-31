/*****************************************************************************
 * a52tospdif.c : encapsulates A/52 frames into S/PDIF packets
 *****************************************************************************
 * Copyright (C) 2002, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_description( N_("Audio filter for A/52->S/PDIF encapsulation") );
    set_capability( "audio filter", 10 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('a','5','2',' ') ||
         ( p_filter->output.i_format != VLC_FOURCC('s','p','d','b') &&
           p_filter->output.i_format != VLC_FOURCC('s','p','d','i') ) )
    {
        return -1;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 0;

    return 0;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    /* AC3 is natively big endian. Most SPDIF devices have the native
     * endianness of the computer system.
     * On Mac OS X however, little endian devices are also common.
     */
    static const uint8_t p_sync_le[6] = { 0x72, 0xF8, 0x1F, 0x4E, 0x01, 0x00 };
    static const uint8_t p_sync_be[6] = { 0xF8, 0x72, 0x4E, 0x1F, 0x00, 0x01 };
#ifndef HAVE_SWAB
    uint8_t * p_tmp;
    uint16_t i;
#endif
    uint16_t i_frame_size = p_in_buf->i_nb_bytes / 2;
    uint8_t * p_in = p_in_buf->p_buffer;
    uint8_t * p_out = p_out_buf->p_buffer;

    /* Copy the S/PDIF headers. */
    if( p_filter->output.i_format == VLC_FOURCC('s','p','d','b') )
    {
        vlc_memcpy( p_out, p_sync_be, 6 );
        p_out[4] = p_in[5] & 0x7; /* bsmod */
        p_out[6] = (i_frame_size >> 4) & 0xff;
        p_out[7] = (i_frame_size << 4) & 0xff;
        vlc_memcpy( &p_out[8], p_in, i_frame_size * 2 );
    }
    else
    {
        vlc_memcpy( p_out, p_sync_le, 6 );
        p_out[5] = p_in[5] & 0x7; /* bsmod */
        p_out[6] = (i_frame_size << 4) & 0xff;
        p_out[7] = (i_frame_size >> 4) & 0xff;
#ifdef HAVE_SWAB
        swab( p_in, &p_out[8], i_frame_size * 2 );
#else
        p_tmp = &p_out[8];
        for( i = i_frame_size; i-- ; )
        {
            p_tmp[0] = p_in[1];
            p_tmp[1] = p_in[0];
            p_tmp += 2; p_in += 2;
        }
#endif
    }
    vlc_memset( p_out + 8 + i_frame_size * 2, 0,
                AOUT_SPDIF_SIZE - i_frame_size * 2 - 8 );

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = AOUT_SPDIF_SIZE;
}

