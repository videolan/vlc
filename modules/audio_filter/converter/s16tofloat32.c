/*****************************************************************************
 * s16tofloat32.c : converter from signed 16 bits integer to float32
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
static void DoWork24  ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_description( _("audio filter for s16->float32 conversion") );
    set_capability( "audio filter", 1 );
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

    if ( ( p_filter->input.i_format != AOUT_FMT_S16_NE &&
           p_filter->input.i_format != AOUT_FMT_S24_NE )
          || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    if( p_filter->input.i_format == AOUT_FMT_S24_NE )
        p_filter->pf_do_work = DoWork24;
    else
        p_filter->pf_do_work = DoWork;

    p_filter->b_in_place = VLC_TRUE;

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
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

    while( i-- )
    {
#if 0
        /* Slow version */
        *p_out = (float)*p_in / 32768.0;
#else
        /* This is walken's trick based on IEEE float format. On my PIII
         * this takes 16 seconds to perform one billion conversions, instead
         * of 19 seconds for the above division. */
        union { float f; int32_t i; } u;
        u.i = *p_in + 0x43c00000;
        *p_out = u.f - 384.0;
#endif

        p_in--; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 2;
}

static void DoWork24( aout_instance_t * p_aout, aout_filter_t * p_filter,
                      aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    uint8_t * p_in = (uint8_t *)p_in_buf->p_buffer + (i - 1) * 3;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

    while( i-- )
    {
#ifdef WORDS_BIGENDIAN
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_in)) << 8) + p_in[2]))
#else
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_in+1)) << 8) + p_in[0]))
#endif
            / 8388608.0;

        p_in -= 3; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 4 / 3;
}
