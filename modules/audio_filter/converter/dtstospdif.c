/*****************************************************************************
 * dtstospdif.c : encapsulates DTS frames into S/PDIF packets
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Gildas Bazin
 *          Derk-Jan Hartman
 *          Pierre d'Herbemont
 *          Rémi Denis-Courmont
 *          Rafaël Carré
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

#include <vlc_aout.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct filter_sys_t
{
    mtime_t start_date;

    /* 3 DTS frames (max 2048) have to be packed into an S/PDIF frame (6144).
     * We accumulate DTS frames from the decoder until we have enough to
     * send. */
    size_t i_frame_size;
    uint8_t *p_buf;
    unsigned i_frames;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Close     ( vlc_object_t * );
static block_t *DoWork( filter_t *, block_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_description( N_("Audio filter for DTS->S/PDIF encapsulation") )
    set_capability( "audio converter", 10 )
    set_callbacks( Create, Close )
vlc_module_end ()

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    if( p_filter->fmt_in.audio.i_format != VLC_CODEC_DTS ||
        ( p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFL &&
          p_filter->fmt_out.audio.i_format != VLC_CODEC_SPDIFB ) )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_filter->p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->p_buf = NULL;
    p_sys->i_frame_size = 0;
    p_sys->i_frames = 0;

    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free our resources
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    filter_t * p_filter = (filter_t *)p_this;

    free( p_filter->p_sys->p_buf );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static block_t *DoWork( filter_t * p_filter, block_t * p_in_buf )
{
    uint32_t i_ac5_spdif_type = 0;
    uint16_t i_fz = p_in_buf->i_nb_samples * 4;
    uint16_t i_frame, i_length = p_in_buf->i_buffer;
    static const uint8_t p_sync_le[6] = { 0x72, 0xF8, 0x1F, 0x4E, 0x00, 0x00 };
    static const uint8_t p_sync_be[6] = { 0xF8, 0x72, 0x4E, 0x1F, 0x00, 0x00 };

    if( p_in_buf->i_buffer != p_filter->p_sys->i_frame_size )
    {
        /* Frame size changed, reset everything */
        msg_Warn( p_filter, "Frame size changed from %zu to %zu, "
                          "resetting everything.",
                  p_filter->p_sys->i_frame_size, p_in_buf->i_buffer );

        p_filter->p_sys->i_frame_size = p_in_buf->i_buffer;
        p_filter->p_sys->p_buf = xrealloc( p_filter->p_sys->p_buf,
                                                  p_in_buf->i_buffer * 3 );
        p_filter->p_sys->i_frames = 0;
    }

    /* Backup frame */
    /* TODO: keeping the blocks in a list would save one memcpy */
    memcpy( p_filter->p_sys->p_buf + p_in_buf->i_buffer *
                  p_filter->p_sys->i_frames,
                p_in_buf->p_buffer, p_in_buf->i_buffer );

    p_filter->p_sys->i_frames++;

    if( p_filter->p_sys->i_frames < 3 )
    {
        if( p_filter->p_sys->i_frames == 1 )
            /* We'll need the starting date */
            p_filter->p_sys->start_date = p_in_buf->i_pts;

        /* Not enough data */
        block_Release( p_in_buf );
        return NULL;
    }

    p_filter->p_sys->i_frames = 0;
    block_t *p_out_buf = block_Alloc( 12 * p_in_buf->i_nb_samples );
    if( !p_out_buf )
        goto out;

    for( i_frame = 0; i_frame < 3; i_frame++ )
    {
        uint16_t i_length_padded = i_length;
        uint8_t * p_out = p_out_buf->p_buffer + (i_frame * i_fz);
        uint8_t * p_in = p_filter->p_sys->p_buf + (i_frame * i_length);

        switch( p_in_buf->i_nb_samples )
        {
            case  512: i_ac5_spdif_type = 0x0B; break;
            case 1024: i_ac5_spdif_type = 0x0C; break;
            case 2048: i_ac5_spdif_type = 0x0D; break;
        }

        /* Copy the S/PDIF headers. */
        if( p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB )
        {
            memcpy( p_out, p_sync_be, 6 );
            p_out[5] = i_ac5_spdif_type;
            SetWBE( p_out + 6, i_length << 3 );
        }
        else
        {
            memcpy( p_out, p_sync_le, 6 );
            p_out[4] = i_ac5_spdif_type;
            SetWLE( p_out + 6, i_length << 3 );
        }

        if( ( (p_in[0] == 0x1F || p_in[0] == 0x7F) && p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFL ) ||
            ( (p_in[0] == 0xFF || p_in[0] == 0xFE) && p_filter->fmt_out.audio.i_format == VLC_CODEC_SPDIFB ) )
        {
            /* We are dealing with a big endian bitstream and a little endian output
             * or a little endian bitstream and a big endian output.
             * Byteswap the stream */
            swab( p_in, p_out + 8, i_length );

            /* If i_length is odd, we have to adjust swapping a bit.. */
            if( i_length & 1 )
            {
                p_out[8+i_length-1] = 0;
                p_out[8+i_length] = p_in[i_length-1];
                i_length_padded++;
            }
        }
        else
        {
            memcpy( p_out + 8, p_in, i_length );
        }

        if( i_fz > i_length + 8 )
        {
            memset( p_out + 8 + i_length_padded, 0,
                        i_fz - i_length_padded - 8 );
        }
    }

    p_out_buf->i_pts = p_filter->p_sys->start_date;
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples * 3;
    p_out_buf->i_buffer = p_out_buf->i_nb_samples * 4;
out:
    block_Release( p_in_buf );
    return p_out_buf;
}
