/*****************************************************************************
 * trivial.c : trivial channel mixer plug-in (drops unwanted channels)
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: trivial.c,v 1.5 2002/10/16 23:12:46 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
    set_description( _("audio filter for trivial channel mixing") );
    set_capability( "audio filter", 1 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate trivial channel mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_channels == p_filter->output.i_channels
          || p_filter->input.i_format != p_filter->output.i_format
          || p_filter->input.i_rate != p_filter->output.i_rate
          || (p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
               && p_filter->input.i_format != VLC_FOURCC('f','i','3','2')) )
    {
        return -1;
    }

    p_filter->pf_do_work = DoWork;
    if ( p_filter->input.i_channels > p_filter->output.i_channels )
    {
        /* Downmixing */
    	p_filter->b_in_place = 1;
    }
    else
    {
        /* Upmixing */
    	p_filter->b_in_place = 0;
    }

    return 0;
}

/*****************************************************************************
 * SparseCopy: trivially downmix or upmix a buffer
 *****************************************************************************/
static void SparseCopy( s32 * p_dest, const s32 * p_src, size_t i_len,
                        int i_output_stride, int i_input_stride )
{
    int i;
    for ( i = i_len; i--; )
    {
        int j;
        for ( j = 0; j < i_output_stride; j++ )
        {
            p_dest[j] = p_src[j];
        }
        p_src += i_input_stride;
        p_dest += i_output_stride;
    }
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i_input_nb = aout_FormatNbChannels( &p_filter->input );
    int i_output_nb = aout_FormatNbChannels( &p_filter->output );
    s32 * p_dest = (s32 *)p_out_buf->p_buffer;
    s32 * p_src = (s32 *)p_in_buf->p_buffer;
    if ( p_filter->output.i_channels == AOUT_CHAN_CHANNEL2 )
        p_src++;

    SparseCopy( p_dest, p_src, p_in_buf->i_nb_samples, i_output_nb,
                i_input_nb );

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * i_output_nb / i_input_nb;
}

