/*****************************************************************************
 * a52tospdif.c : encapsulates A/52 frames into S/PDIF packets
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: a52tospdif.c,v 1.6 2002/08/13 16:11:15 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>
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
    set_description( _("aout filter for A/52->S/PDIF encapsulation") );
    set_capability( "audio filter", 10 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate trivial mixer
 *****************************************************************************
 * This function allocates and initializes a Crop vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != AOUT_FMT_A52
          || p_filter->output.i_format != AOUT_FMT_SPDIF )
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
#ifdef WORDS_BIGENDIAN
    static const u8 p_sync[6] = { 0xF8, 0x72, 0x4E, 0x1F, 0x00, 0x01 };
#else
    static const u8 p_sync[6] = { 0x72, 0xF8, 0x1F, 0x4E, 0x01, 0x00 };
#endif
    u16 i_length = p_in_buf->i_nb_samples;
    u16 * pi_length;
    byte_t * p_in = p_in_buf->p_buffer;
    byte_t * p_out = p_out_buf->p_buffer;

    /* Copy the S/PDIF headers. */
    memcpy( p_out, p_sync, 6 );
    pi_length = (u16 *)(p_out + 6);
    *pi_length = i_length;

    /* FIXME : if i_length is odd, the following code sucks. What should
     * we do ? --Meuuh */

#ifndef WORDS_BIGENDIAN
#   ifdef HAVE_SWAB
    swab( p_out + 8, p_in, i_length );
#   else
    p_out += 8;
    for ( i = i_length / 2 ; i-- ; )
    {
        p_out[0] = p_in[1];
        p_out[1] = p_in[0];
        p_out += 2; p_in += 2;
    }
#   endif

#else
    p_filter->p_vlc->pf_memcpy( p_out + 8, p_in, i_length );
#endif

    p_filter->p_vlc->pf_memset( p_out + 8 + i_length, 0,
                               AOUT_SPDIF_SIZE - i_length - 8 );

    p_out_buf->i_nb_samples = 1;
}

