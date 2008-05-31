/*****************************************************************************
 * fixed.c: Fixed-point audio format conversions
 *****************************************************************************
 * Copyright (C) 2002, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *          Marc Ariberti <marcari@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create_F32ToS16    ( vlc_object_t * );
static void Do_F32ToS16( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                         aout_buffer_t * );

static int  Create_S16ToF32    ( vlc_object_t * );
static void Do_S16ToF32( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                         aout_buffer_t * );

static int  Create_U8ToF32    ( vlc_object_t * );
static void Do_U8ToF32( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                         aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Fixed point audio format conversions") );
    add_submodule();
        set_callbacks( Create_F32ToS16, NULL );
        set_capability( "audio filter", 10 );
    add_submodule();
        set_callbacks( Create_S16ToF32, NULL );
        set_capability( "audio filter", 15 );
    add_submodule();
        set_callbacks( Create_U8ToF32, NULL );
        set_capability( "audio filter", 1 );
vlc_module_end();

/*****************************************************************************
 * F32 to S16
 *****************************************************************************/
static int Create_F32ToS16( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('f','i','3','2')
          || p_filter->output.i_format != AOUT_FMT_S16_NE )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    p_filter->pf_do_work = Do_F32ToS16;
    p_filter->b_in_place = 1;

    return VLC_SUCCESS;;
}

/*****************************************************************************
 * support routines borrowed from mpg321 (file: mad.c), which is distributed
 * under GPL license
 *
 * mpg321 was written by Joe Drew <drew@debian.org>, and based upon 'plaympeg'
 * from the smpeg sources, which was written by various people from Loki Software
 * (http://www.lokigames.com).
 *
 * It also incorporates some source from mad, written by Robert Leslie
 *****************************************************************************/

/* The following two routines and data structure are from the ever-brilliant
     Rob Leslie.
*/

#define VLC_F_FRACBITS  28

# if VLC_F_FRACBITS == 28
#  define VLC_F(x) ((vlc_fixed_t) (x##L))
# endif

# define VLC_F_ONE VLC_F(0x10000000)

/*****************************************************************************
 * s24_to_s16_pcm: Scale a 24 bit pcm sample to a 16 bit pcm sample.
 *****************************************************************************/
static inline int16_t s24_to_s16_pcm(vlc_fixed_t sample)
{
  /* round */
  sample += (1L << (VLC_F_FRACBITS - 16));

  /* clip */
  if (sample >= VLC_F_ONE)
    sample = VLC_F_ONE - 1;
  else if (sample < -VLC_F_ONE)
    sample = -VLC_F_ONE;

  /* quantize */
  return (sample >> (VLC_F_FRACBITS + 1 - 16));
}

static void Do_F32ToS16( aout_instance_t * p_aout, aout_filter_t * p_filter,
                         aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i;
    vlc_fixed_t * p_in = (vlc_fixed_t *)p_in_buf->p_buffer;
    int16_t * p_out = (int16_t *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ) ; i-- ; )
    {
        /* Fast Scaling */
        *p_out++ = s24_to_s16_pcm(*p_in++);
    }
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes / 2;
}


/*****************************************************************************
 * S16 to F32
 *****************************************************************************/
static int Create_S16ToF32( vlc_object_t *p_this )
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

    p_filter->pf_do_work = Do_S16ToF32;
    p_filter->b_in_place = 1;

    return 0;
}

static void Do_S16ToF32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                         aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
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


/*****************************************************************************
 * U8 to F32
 *****************************************************************************/
static int Create_U8ToF32( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('u','8',' ',' ')
          || p_filter->output.i_format != VLC_FOURCC('f','i','3','2') )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    p_filter->pf_do_work = Do_U8ToF32;
    p_filter->b_in_place = true;

    return 0;
}

static void Do_U8ToF32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                        aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    uint8_t * p_in = (uint8_t *)p_in_buf->p_buffer + i - 1;
    vlc_fixed_t * p_out = (vlc_fixed_t *)p_out_buf->p_buffer + i - 1;

    while( i-- )
    {
        *p_out = (vlc_fixed_t)( (int32_t)(*p_in - 128) * (FIXED32_ONE / 128) );
        p_in--; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * sizeof(vlc_fixed_t);
}
