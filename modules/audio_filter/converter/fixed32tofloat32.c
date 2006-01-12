/*****************************************************************************
 * fixed32float32.c : converter from fixed32 to float32 bits integer
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create       ( vlc_object_t * );

static void FixedToFloat ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );
static void FloatToFixed ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_description( _("audio filter for fixed32<->float32 conversion") );
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

    if( ( p_filter->input.i_format != VLC_FOURCC('f','i','3','2')
           || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
      && ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
            || p_filter->output.i_format != VLC_FOURCC('f','i','3','2') ) )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    if( p_filter->input.i_format == VLC_FOURCC('f','i','3','2') )
    {
        p_filter->pf_do_work = FixedToFloat;
    }
    else
    {
        p_filter->pf_do_work = FloatToFixed;
    }

    p_filter->b_in_place = 1;

    return 0;
}

/*****************************************************************************
 * FixedToFloat: convert a buffer
 *****************************************************************************/
static void FixedToFloat( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i;
    vlc_fixed_t * p_in = (vlc_fixed_t *)p_in_buf->p_buffer;
    float * p_out = (float *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ) ; i-- ; )
    {
        *p_out++ = (float)*p_in++ / (float)FIXED32_ONE;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;
}

/*****************************************************************************
 * FloatToFixed: convert a buffer
 *****************************************************************************/
static void FloatToFixed( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i;
    float * p_in = (float *)p_in_buf->p_buffer;
    vlc_fixed_t * p_out = (vlc_fixed_t *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ) ; i-- ; )
    {
        *p_out++ = (vlc_fixed_t)( *p_in++ * (float)FIXED32_ONE );
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;
}

