/*****************************************************************************
 * fixed32tos16.c : converter from fixed32 to signed 16 bits integer
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
    set_description( _("audio filter for fixed32->s16 conversion") );
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

    if ( p_filter->input.i_format != VLC_FOURCC('f','i','3','2')
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

struct audio_dither {
    vlc_fixed_t error[3];
    vlc_fixed_t random;
};

/********************************************************************
 * NAME:                prng()
 * DESCRIPTION: 32-bit pseudo-random number generator
 ********************************************************************/
static inline unsigned long prng(unsigned long state)
{
    return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

/********************************************************************
 * NAME:        mpg321_s24_to_s16_pcm()
 * DESCRIPTION: generic linear sample quantize and dither routine
 ********************************************************************/
static inline int16_t mpg321_s24_to_s16_pcm(unsigned int bits, vlc_fixed_t sample,
                                            struct audio_dither *dither)
{
    unsigned int scalebits;
    vlc_fixed_t output, mask, random;

    enum {
        MIN = -VLC_F_ONE,
        MAX = VLC_F_ONE - 1
    };

    /* noise shape */
    sample += dither->error[0] - dither->error[1] + dither->error[2];

    dither->error[2] = dither->error[1];
    dither->error[1] = dither->error[0] / 2;

    /* bias */
    output = sample + (1L << (VLC_F_FRACBITS + 1 - bits - 1));

    scalebits = VLC_F_FRACBITS + 1 - bits;
    mask = (1L << scalebits) - 1;

    /* dither */
    random    = prng(dither->random);
    output += (random & mask) - (dither->random & mask);

    dither->random = random;

    /* clip */
    if (output > MAX) {
        output = MAX;

        if (sample > MAX)
            sample = MAX;
    }
    else if (output < MIN) {
        output = MIN;

        if (sample < MIN)
            sample = MIN;
    }

    /* quantize */
    output &= ~mask;

    /* error feedback */
    dither->error[0] = sample - output;

    /* scale */
    return output >> scalebits;
}

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

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i;
    vlc_fixed_t * p_in = (vlc_fixed_t *)p_in_buf->p_buffer;
    int16_t * p_out = (int16_t *)p_out_buf->p_buffer;
#if 0
    static struct audio_dither dither;
#endif

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ) ; i-- ; )
    {
#if 0
        /* Accurate scaling */
        *p_out++ = mpg321_s24_to_s16_pcm(16, *p_in++, &dither);
#endif
        /* Fast Scaling */
        *p_out++ = s24_to_s16_pcm(*p_in++);
    }
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes / 2;
}
