/*****************************************************************************
 * s16tofloat32swab.c : converter from signed 16 bits integer to float32
 *                      with endianness change
 *****************************************************************************
 * Copyright (C) 2002-2005 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
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
static void DoWork24  ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
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

    if ( (p_filter->input.i_format == VLC_FOURCC('s','1','6','l') ||
         p_filter->input.i_format == VLC_FOURCC('s','1','6','b'))
         && p_filter->output.i_format == VLC_FOURCC('f','l','3','2')
         && p_filter->input.i_format != AOUT_FMT_S16_NE )
    {
        p_filter->pf_do_work = DoWork;
        p_filter->b_in_place = VLC_TRUE;

        return 0;
    }

    if ( (p_filter->input.i_format == VLC_FOURCC('s','2','4','l') ||
         p_filter->input.i_format == VLC_FOURCC('s','2','4','b'))
         && p_filter->output.i_format == VLC_FOURCC('f','l','3','2')
         && p_filter->input.i_format != AOUT_FMT_S24_NE )
    {
        p_filter->pf_do_work = DoWork24;
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
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    int16_t * p_in;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

#ifdef HAVE_SWAB
#   ifdef HAVE_ALLOCA
    int16_t * p_swabbed = alloca( i * sizeof(int16_t) );
#   else
    int16_t * p_swabbed = malloc( i * sizeof(int16_t) );
#   endif

    swab( p_in_buf->p_buffer, (void *)p_swabbed, i * sizeof(int16_t) );
    p_in = p_swabbed + i - 1;

#else
    byte_t p_tmp[2];
    p_in = (int16_t *)p_in_buf->p_buffer + i - 1;
#endif

    while( i-- )
    {
#ifndef HAVE_SWAB
        p_tmp[0] = ((byte_t *)p_in)[1];
        p_tmp[1] = ((byte_t *)p_in)[0];
        *p_out = (float)( *(int16_t *)p_tmp ) / 32768.0;
#else
        *p_out = (float)*p_in / 32768.0;
#endif
        p_in--; p_out--;
    }

#ifdef HAVE_SWAB
#   ifndef HAVE_ALLOCA
    free( p_swabbed );
#   endif
#endif

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

    byte_t p_tmp[3];

    while( i-- )
    {
        p_tmp[0] = p_in[2];
        p_tmp[1] = p_in[1];
        p_tmp[2] = p_in[0];

#ifdef WORDS_BIGENDIAN
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_tmp)) << 8) + p_tmp[2]))
#else
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_tmp+1)) << 8) + p_tmp[0]))
#endif
            / 8388608.0;

        p_in -= 3; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 4 / 3;
}
