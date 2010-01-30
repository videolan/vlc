/*****************************************************************************
 * fixed.c: Fixed-point audio format conversions
 *****************************************************************************
 * Copyright (C) 2002, 2006-2009 the VideoLAN team
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
#include <vlc_filter.h>

static int  CreateTo( vlc_object_t * );
static int  CreateFrom( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Fixed point audio format conversions") )
    set_callbacks( CreateTo, NULL )
    set_capability( "audio filter", 15 )
    add_submodule ()
        set_callbacks( CreateFrom, NULL )
        set_capability( "audio filter", 10 )
vlc_module_end ()

/*** Conversion from FI32 to audio output ***/
static block_t *Do_F32ToS16( filter_t *, block_t * );

static int CreateFrom( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;

    if( p_filter->fmt_in.audio.i_format != VLC_CODEC_FI32
     || !AOUT_FMTS_SIMILAR( &p_filter->fmt_in.audio,
                            &p_filter->fmt_out.audio ) )
        return VLC_EGENERIC;

    /* In fixed-point builds, audio outputs pretty much all use S16N. */
    /* Feel free to add some other format if every needed. */
    switch( p_filter->fmt_out.audio.i_format )
    {
        case VLC_CODEC_S16N:
            p_filter->pf_audio_filter = Do_F32ToS16;
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;;
}

/*****************************************************************************
 * F32 to S16
 *****************************************************************************/

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

static block_t *Do_F32ToS16( filter_t * p_filter, block_t * p_in_buf )
{
    int i;
    vlc_fixed_t * p_in = (vlc_fixed_t *)p_in_buf->p_buffer;
    int16_t * p_out = (int16_t *)p_in_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->fmt_in.audio ) ; i-- ; )
    {
        /* Fast Scaling */
        *p_out++ = s24_to_s16_pcm(*p_in++);
    }
    p_in_buf->i_buffer /= 2;
    return p_in_buf;
}

/*** Conversions from decoders to FI32 */
static block_t *Do_S16ToF32( filter_t *, block_t * );
static block_t *Do_U8ToF32( filter_t *, block_t * );

static int CreateTo( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;

    if( p_filter->fmt_out.audio.i_format != VLC_CODEC_FI32
     || !AOUT_FMTS_SIMILAR( &p_filter->fmt_in.audio,
                            &p_filter->fmt_out.audio ) )
        return VLC_EGENERIC;

    switch( p_filter->fmt_in.audio.i_format )
    {
        case VLC_CODEC_S16N:
            p_filter->pf_audio_filter = Do_S16ToF32;
            break;

        case VLC_CODEC_U8:
            p_filter->pf_audio_filter = Do_U8ToF32;
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * S16 to F32
 *****************************************************************************/
static block_t *Do_S16ToF32( filter_t * p_filter, block_t * p_in_buf )
{
    block_t *p_out_buf;
    p_out_buf = filter_NewAudioBuffer( p_filter, p_in_buf->i_buffer * 2 );
    if( !p_out_buf )
        goto out;

    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->fmt_in.audio );
    const int16_t * p_in = (int16_t *)p_in_buf->p_buffer;
    vlc_fixed_t * p_out = (vlc_fixed_t *)p_out_buf->p_buffer;

    while( i-- )
    {
        *p_out = (vlc_fixed_t)( (int32_t)(*p_in) * (FIXED32_ONE >> 16) );
        p_in++; p_out++;
    }

    p_out_buf->i_pts = p_in_buf->i_pts;
    p_out_buf->i_length = p_in_buf->i_length;
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
out:
    block_Release( p_in_buf );
    return p_out_buf;
}


/*****************************************************************************
 * U8 to F32
 *****************************************************************************/
static block_t *Do_U8ToF32( filter_t * p_filter, block_t * p_in_buf )
{
    block_t *p_out_buf;
    p_out_buf = filter_NewAudioBuffer( p_filter, 4 * p_in_buf->i_buffer );
    if( !p_out_buf )
        goto out;

    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->fmt_in.audio );

    uint8_t * p_in = (uint8_t *)p_in_buf->p_buffer;
    vlc_fixed_t * p_out = (vlc_fixed_t *)p_out_buf->p_buffer;

    while( i-- )
    {
        *p_out = (vlc_fixed_t)( (int32_t)(*p_in - 128) * (FIXED32_ONE / 128) );
        p_in++; p_out++;
    }
    p_out_buf->i_pts = p_in_buf->i_pts;
    p_out_buf->i_length = p_in_buf->i_length;
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
out:
    block_Release( p_in_buf );
    return p_out_buf;
}
