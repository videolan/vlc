/*****************************************************************************
 * dtstospdif.c : encapsulates DTS frames into S/PDIF packets
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dtstospdif.c,v 1.1 2003/03/09 20:07:47 jlj Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "audio_output.h"
#include "aout_internal.h"

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
    set_description( _("audio filter for DTS->S/PDIF encapsulation") );
    set_capability( "audio filter", 10 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('d','t','s',' ')
          || p_filter->output.i_format != VLC_FOURCC('s','p','d','i') )
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
    uint16_t i_fz = (p_in_buf->i_nb_samples / 3) * 4;
    uint16_t i_frame, i_length = p_in_buf->i_nb_bytes / 3;
    static const uint8_t p_sync[6] = { 0x72, 0xF8, 0x1F, 0x4E, 0x00, 0x00 };

    for( i_frame = 0; i_frame < 3; i_frame++ )
    {
#ifndef HAVE_SWAB
        uint16_t i;
        byte_t * p_tmp;
#endif
        byte_t * p_out = p_out_buf->p_buffer + (i_frame * i_fz);
        byte_t * p_in = p_in_buf->p_buffer + (i_frame * i_length);

        /* Copy the S/PDIF headers. */
        memcpy( p_out, p_sync, 6 );

        switch( p_in_buf->i_nb_samples / 3 )
        {
            case  512: *(p_out + 4) = 0x0B; break;
            case 1024: *(p_out + 4) = 0x0C; break;
            case 2048: *(p_out + 4) = 0x0D; break;
        }

        *(p_out + 6) = (i_length * 8) & 0xff;
        *(p_out + 7) = (i_length * 8) >> 8;

#ifdef HAVE_SWAB
        swab( p_in, p_out + 8, i_length );
#else
        p_tmp = p_out + 8;
        for( i = i_length / 2 ; i-- ; )
        {
            p_tmp[0] = p_in[1];
            p_tmp[1] = p_in[0];
            p_tmp += 2; p_in += 2;
        }
#endif

        p_filter->p_vlc->pf_memset( p_out + 8 + i_length, 0,
                                    i_fz - i_length - 8 );
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_out_buf->i_nb_samples * 4;
}
