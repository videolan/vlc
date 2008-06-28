/*****************************************************************************
 * dtstospdif.c : encapsulates DTS frames into S/PDIF packets
 *****************************************************************************
 * Copyright (C) 2003, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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
 * Local structures
 *****************************************************************************/
struct aout_filter_sys_t
{
    /* 3 DTS frames have to be packed into an S/PDIF frame.
     * We accumulate DTS frames from the decoder until we have enough to
     * send. */

    uint8_t *p_buf;

    mtime_t start_date;

    int i_frames;
    unsigned int i_frame_size;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Close     ( vlc_object_t * );
static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_description( N_("Audio filter for DTS->S/PDIF encapsulation") );
    set_capability( "audio filter", 10 );
    set_callbacks( Create, Close );
vlc_module_end();

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if( p_filter->input.i_format != VLC_FOURCC('d','t','s',' ') ||
        ( p_filter->output.i_format != VLC_FOURCC('s','p','d','i') &&
          p_filter->output.i_format != VLC_FOURCC('s','p','d','b') ) )
    {
        return -1;
    }

    /* Allocate the memory needed to store the module's structure */
    p_filter->p_sys = malloc( sizeof(struct aout_filter_sys_t) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;
    memset( p_filter->p_sys, 0, sizeof(struct aout_filter_sys_t) );
    p_filter->p_sys->p_buf = 0;

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 1;

    return 0;
}

/*****************************************************************************
 * Close: free our resources
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    if( p_filter->p_sys->i_frame_size ) free( p_filter->p_sys->p_buf );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    uint32_t i_ac5_spdif_type = 0;
    uint16_t i_fz = p_in_buf->i_nb_samples * 4;
    uint16_t i_frame, i_length = p_in_buf->i_nb_bytes;
    static const uint8_t p_sync_le[6] = { 0x72, 0xF8, 0x1F, 0x4E, 0x00, 0x00 };
    static const uint8_t p_sync_be[6] = { 0xF8, 0x72, 0x4E, 0x1F, 0x00, 0x00 };

    if( p_in_buf->i_nb_bytes != p_filter->p_sys->i_frame_size )
    {
        /* Frame size changed, reset everything */
        msg_Warn( p_aout, "Frame size changed from %u to %u, "
                          "resetting everything.",
                  p_filter->p_sys->i_frame_size,
                  (unsigned)p_in_buf->i_nb_bytes );

        p_filter->p_sys->i_frame_size = p_in_buf->i_nb_bytes;
        p_filter->p_sys->p_buf = realloc( p_filter->p_sys->p_buf,
                                          p_in_buf->i_nb_bytes * 3 );
        p_filter->p_sys->i_frames = 0;
    }

    /* Backup frame */
    vlc_memcpy( p_filter->p_sys->p_buf + p_in_buf->i_nb_bytes *
                  p_filter->p_sys->i_frames,
                p_in_buf->p_buffer, p_in_buf->i_nb_bytes );

    p_filter->p_sys->i_frames++;

    if( p_filter->p_sys->i_frames < 3 )
    {
        if( p_filter->p_sys->i_frames == 1 )
            /* We'll need the starting date */
            p_filter->p_sys->start_date = p_in_buf->start_date;

        /* Not enough data */
        p_out_buf->i_nb_samples = 0;
        p_out_buf->i_nb_bytes = 0;
        return;
    }

    p_filter->p_sys->i_frames = 0;

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
        if( p_filter->output.i_format == VLC_FOURCC('s','p','d','b') )
        {
            vlc_memcpy( p_out, p_sync_be, 6 );
            p_out[5] = i_ac5_spdif_type;
            p_out[6] = (( i_length ) >> 5 ) & 0xFF;
            p_out[7] = ( i_length << 3 ) & 0xFF;
        }
        else
        {
            vlc_memcpy( p_out, p_sync_le, 6 );
            p_out[4] = i_ac5_spdif_type;
            p_out[6] = ( i_length << 3 ) & 0xFF;
            p_out[7] = (( i_length ) >> 5 ) & 0xFF;
        }

        if( ( (p_in[0] == 0x1F || p_in[0] == 0x7F) && p_filter->output.i_format == VLC_FOURCC('s','p','d','i') ) ||
            ( (p_in[0] == 0xFF || p_in[0] == 0xFE) && p_filter->output.i_format == VLC_FOURCC('s','p','d','b') ) )
        {
            /* We are dealing with a big endian bitstream and a little endian output
             * or a little endian bitstream and a big endian output.
             * Byteswap the stream */
#ifdef HAVE_SWAB
            swab( p_in, p_out + 8, i_length );
#else
            uint16_t i;
            uint8_t * p_tmp, tmp;
            p_tmp = p_out + 8;
            for( i = i_length / 2 ; i-- ; )
            {
                tmp = p_in[0]; /* in-place filter */
                p_tmp[0] = p_in[1];
                p_tmp[1] = tmp;
                p_tmp += 2; p_in += 2;
            }
#endif
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
            vlc_memcpy( p_out + 8, p_in, i_length );
        }

        if( i_fz > i_length + 8 )
        {
            vlc_memset( p_out + 8 + i_length_padded, 0,
                        i_fz - i_length_padded - 8 );
        }
    }

    p_out_buf->start_date = p_filter->p_sys->start_date;
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples * 3;
    p_out_buf->i_nb_bytes = p_out_buf->i_nb_samples * 4;
}
