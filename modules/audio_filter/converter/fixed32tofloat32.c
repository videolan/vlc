/*****************************************************************************
 * fixed32float32.c : converter from fixed32 to float32 bits integer
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: fixed32tofloat32.c,v 1.7 2002/08/22 17:14:52 sam Exp $
 *
 * Authors: Jean-Paul Saman <jpsaman@wxs.nl>
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
    set_description( _("audio filter for fixed32->float32 conversion") );
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

    if ( p_filter->input.i_format != AOUT_FMT_FIXED32
          || p_filter->output.i_format != AOUT_FMT_FLOAT32 )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 1;

    return 0;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i;
    vlc_fixed_t * p_in = (vlc_fixed_t *)p_in_buf->p_buffer;
    float * p_out = (float *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples * p_filter->input.i_channels ; i-- ; )
    {
        *p_out++ = (float)*p_in++ / -(float)FIXED32_MIN;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;
}

