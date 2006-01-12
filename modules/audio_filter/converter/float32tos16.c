/*****************************************************************************
 * float32tos16.c : converter from float32 to signed 16 bits integer
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
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
    set_description( _("audio filter for float32->s16 conversion") );
    set_capability( "audio filter", 1 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate trivial mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
          || p_filter->output.i_format != AOUT_FMT_S16_NE )
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
    float * p_in = (float *)p_in_buf->p_buffer;
    int16_t * p_out = (int16_t *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ); i-- ; )
    {
#if 0
        /* Slow version. */
        if ( *p_in >= 1.0 ) *p_out = 32767;
        else if ( *p_in < -1.0 ) *p_out = -32768;
        else *p_out = *p_in * 32768.0;
#else
        /* This is walken's trick based on IEEE float format. */
        union { float f; int32_t i; } u;
        u.f = *p_in + 384.0;
        if ( u.i > 0x43c07fff ) *p_out = 32767;
        else if ( u.i < 0x43bf8000 ) *p_out = -32768;
        else *p_out = u.i - 0x43c00000;
#endif
        p_in++; p_out++;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes / 2;
}

