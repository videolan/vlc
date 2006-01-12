/*****************************************************************************
 * s16tofixed32.c : converter from signed 16 bits integer to fixed 32
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Marc Ariberti <marcari@videolan.ord>
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
static int  Create    ( vlc_object_t * );

static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_description( _("audio filter for s16->fixed32 conversion") );
    set_capability( "audio filter", 15 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate filter
 *****************************************************************************
 * This function allocates and initializes a s16->fixed32 audio filter.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->output.i_format != VLC_FOURCC('f','i','3','2')
          || p_filter->input.i_format != AOUT_FMT_S16_NE )
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
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    int16_t * p_in = (int16_t *)p_in_buf->p_buffer + i - 1;
    vlc_fixed_t * p_out = (vlc_fixed_t *)p_out_buf->p_buffer + i - 1;

    while( i-- )
    {
        *p_out = (vlc_fixed_t)( (int32_t)(*p_in) * (FIXED32_ONE >> 16) );
        p_in--; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes
            * sizeof(vlc_fixed_t) / sizeof(int16_t);
}
