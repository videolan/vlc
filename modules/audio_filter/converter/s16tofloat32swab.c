/*****************************************************************************
 * s16tofloat32swab.c : converter from signed 16 bits integer to float32
 *                      with endianness change
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: s16tofloat32swab.c,v 1.4 2002/09/26 22:40:19 massiot Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Henri Fallon <henri@videolan.org>
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

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
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
    set_description( 
            _("audio filter for s16->float32 with endianness conversion") );
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
    
    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    if ( (p_filter->input.i_format == AOUT_FMT_S16_LE ||
         p_filter->input.i_format == AOUT_FMT_S16_BE)
         && p_filter->output.i_format == AOUT_FMT_FLOAT32
         && p_filter->input.i_format != AOUT_FMT_S16_NE )
    {
        p_filter->pf_do_work = DoWork;
        p_filter->b_in_place = VLC_TRUE;

        return 0;
    }

    return -1;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i = p_in_buf->i_nb_samples * p_filter->input.i_channels;

    /* We start from the end because b_in_place is true */
    s16 * p_in;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

#ifdef HAVE_SWAB
    s16 * p_swabbed = alloca( i * sizeof(s16) );
    swab( p_in_buf->p_buffer, p_swabbed, i * sizeof(s16) );
    p_in = p_swabbed + i - 1;
#else
    byte_t p_tmp[2];
    p_in = (s16 *)p_in_buf->p_buffer + i - 1;
#endif
   
    while( i-- )
    {
#ifndef HAVE_SWAB
        p_tmp[0] = ((byte_t *)p_in)[1];
        p_tmp[1] = ((byte_t *)p_in)[0];
        *p_out = (float)( *(s16 *)p_tmp ) / 32768.0;
#else
        *p_out = (float)*p_in / 32768.0;
#endif
        p_in--; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 2;
}

