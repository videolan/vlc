/*****************************************************************************
 * a52tofloat32.c: ATSC A/52 aka AC-3 decoder plugin for VLC.
 *   This plugin makes use of liba52 to decode A/52 audio
 *   (http://liba52.sf.net/).
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under the GNU General Public License version 2
 * (or later). Therefore, the result of compiling this module will normally
 * be subject to the terms of that later license.
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_cpu.h>

#include <stdint.h>                                         /* int16_t .. */

#ifdef USE_A52DEC_TREE                                 /* liba52 header file */
#   include "include/a52.h"
#else
#   include "a52dec/a52.h"
#endif

#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );
static block_t *Convert( filter_t *, block_t * );

/* liba52 channel order */
static const uint32_t pi_channels_in[] =
{ AOUT_CHAN_LFE, AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARCENTER, AOUT_CHAN_REARRIGHT, 0 };

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct filter_sys_t
{
    a52_state_t * p_liba52; /* liba52 internal structure */
    bool b_dynrng; /* see below */
    int i_flags; /* liba52 flags, see a52dec/doc/liba52.txt */
    bool b_dontwarn;
    int i_nb_channels; /* number of float32 per sample */

    uint8_t pi_chan_table[AOUT_CHAN_MAX]; /* channel reordering */
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DYNRNG_TEXT N_("A/52 dynamic range compression")
#define DYNRNG_LONGTEXT N_( \
    "Dynamic range compression makes the loud sounds softer, and the soft " \
    "sounds louder, so you can more easily listen to the stream in a noisy " \
    "environment without disturbing anyone. If you disable the dynamic range "\
    "compression the playback will be more adapted to a movie theater or a " \
    "listening room.")
#define UPMIX_TEXT N_("Enable internal upmixing")
#define UPMIX_LONGTEXT N_( \
    "Enable the internal upmixing algorithm (not recommended).")

vlc_module_begin ()
    set_shortname( "A/52" )
    set_description( N_("ATSC A/52 (AC-3) audio decoder") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    add_bool( "a52-dynrng", true, DYNRNG_TEXT, DYNRNG_LONGTEXT, false )
    add_bool( "a52-upmix", false, UPMIX_TEXT, UPMIX_LONGTEXT, true )
    set_capability( "audio converter", 100 )
    set_callbacks( OpenFilter, CloseFilter )
vlc_module_end ()

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this, filter_sys_t *p_sys,
                 const audio_format_t *restrict input,
                 const audio_format_t *restrict output )
{
    p_sys->b_dynrng = var_InheritBool( p_this, "a52-dynrng" );
    p_sys->b_dontwarn = 0;

    /* No upmixing: it's not necessary and some other filters may want to do
     * it themselves. */
    if ( aout_FormatNbChannels( output ) > aout_FormatNbChannels( input ) )
    {
        if ( ! var_InheritBool( p_this, "a52-upmix" ) )
        {
            return VLC_EGENERIC;
        }
    }

    /* We'll do our own downmixing, thanks. */
    p_sys->i_nb_channels = aout_FormatNbChannels( output );
    switch ( output->i_physical_channels & ~AOUT_CHAN_LFE )
    {
    case AOUT_CHAN_CENTER:
        if ( (output->i_original_channels & AOUT_CHAN_CENTER)
              || (output->i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
        {
            p_sys->i_flags = A52_MONO;
        }
        else if ( output->i_original_channels & AOUT_CHAN_LEFT )
        {
            p_sys->i_flags = A52_CHANNEL1;
        }
        else
        {
            p_sys->i_flags = A52_CHANNEL2;
        }
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT:
        if ( output->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
        {
            p_sys->i_flags = A52_DOLBY;
        }
        else if ( input->i_original_channels == AOUT_CHAN_CENTER )
        {
            p_sys->i_flags = A52_MONO;
        }
        else if ( input->i_original_channels & AOUT_CHAN_DUALMONO )
        {
            p_sys->i_flags = A52_CHANNEL;
        }
        else if ( !(output->i_original_channels & AOUT_CHAN_RIGHT) )
        {
            p_sys->i_flags = A52_CHANNEL1;
        }
        else if ( !(output->i_original_channels & AOUT_CHAN_LEFT) )
        {
            p_sys->i_flags = A52_CHANNEL2;
        }
        else
        {
            p_sys->i_flags = A52_STEREO;
        }
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER:
        p_sys->i_flags = A52_3F;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER:
        p_sys->i_flags = A52_2F1R;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER:
        p_sys->i_flags = A52_3F1R;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        p_sys->i_flags = A52_2F2R;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        p_sys->i_flags = A52_3F2R;
        break;

    default:
        msg_Warn( p_this, "unknown sample format!" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    if ( output->i_physical_channels & AOUT_CHAN_LFE )
    {
        p_sys->i_flags |= A52_LFE;
    }
    p_sys->i_flags |= A52_ADJUST_LEVEL;

    /* Initialize liba52 */
    p_sys->p_liba52 = a52_init( 0 );
    if( p_sys->p_liba52 == NULL )
    {
        msg_Err( p_this, "unable to initialize liba52" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    aout_CheckChannelReorder( pi_channels_in, NULL,
                              output->i_physical_channels,
                              p_sys->pi_chan_table );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void Interleave( sample_t *restrict p_out, const sample_t *restrict p_in,
                        unsigned i_nb_channels, uint8_t *restrict pi_chan_table )
{
    /* We do not only have to interleave, but also reorder the channels */
    for( unsigned j = 0; j < i_nb_channels; j++ )
    {
        for( unsigned i = 0; i < 256; i++ )
        {
#ifdef LIBA52_FIXED
            p_out[i * i_nb_channels + pi_chan_table[j]] = p_in[j * 256 + i] << 4;
#else
            p_out[i * i_nb_channels + pi_chan_table[j]] = p_in[j * 256 + i];
#endif
        }
    }
}

/*****************************************************************************
 * Duplicate: helper function to duplicate a unique channel
 *****************************************************************************/
static void Duplicate( sample_t *restrict p_out, const sample_t *restrict p_in )
{
    for( unsigned i = 256; i--; )
    {
#ifdef LIBA52_FIXED
        sample_t s = *(p_in++) << 4;
#else
        sample_t s = *(p_in++);
#endif
        *p_out++ = s;
        *p_out++ = s;
    }
}

/*****************************************************************************
 * Exchange: helper function to exchange left & right channels
 *****************************************************************************/
static void Exchange( sample_t *restrict p_out, const sample_t *restrict p_in )
{
    const sample_t *p_first = p_in + 256;
    const sample_t *p_second = p_in;

    for( unsigned i = 0; i < 256; i++ )
    {
#ifdef LIBA52_FIXED
        *p_out++ = *p_first++ << 4;
        *p_out++ = *p_second++ << 4;
#else
        *p_out++ = *p_first++;
        *p_out++ = *p_second++;
#endif
    }
}

/*****************************************************************************
 * Convert: decode an ATSC A/52 frame.
 *****************************************************************************/

static block_t *Convert( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;
#ifdef LIBA52_FIXED
    sample_t i_sample_level = (1 << 24);
#else
    sample_t i_sample_level = 1;
#endif
    int i_flags = p_sys->i_flags;
    size_t i_bytes_per_block = 256 * p_sys->i_nb_channels * sizeof(sample_t);

    block_t *p_out_buf = block_Alloc( 6 * i_bytes_per_block );
    if( unlikely(p_out_buf == NULL) )
        goto out;

    /* Do the actual decoding now. */
    a52_frame( p_sys->p_liba52, p_in_buf->p_buffer,
               &i_flags, &i_sample_level, 0 );

    if ( (i_flags & A52_CHANNEL_MASK) != (p_sys->i_flags & A52_CHANNEL_MASK)
          && !p_sys->b_dontwarn )
    {
        msg_Warn( p_filter,
                  "liba52 couldn't do the requested downmix 0x%x->0x%x",
                  p_sys->i_flags  & A52_CHANNEL_MASK,
                  i_flags & A52_CHANNEL_MASK );

        p_sys->b_dontwarn = 1;
    }

    if( !p_sys->b_dynrng )
    {
        a52_dynrng( p_sys->p_liba52, NULL, NULL );
    }

    for( unsigned i = 0; i < 6; i++ )
    {
        sample_t * p_samples;

        if( a52_block( p_sys->p_liba52 ) )
        {
            msg_Warn( p_filter, "a52_block failed for block %d", i );
        }

        p_samples = a52_samples( p_sys->p_liba52 );

        if ( ((p_sys->i_flags & A52_CHANNEL_MASK) == A52_CHANNEL1
               || (p_sys->i_flags & A52_CHANNEL_MASK) == A52_CHANNEL2
               || (p_sys->i_flags & A52_CHANNEL_MASK) == A52_MONO)
              && (p_filter->fmt_out.audio.i_physical_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
        {
            Duplicate( (sample_t *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                       p_samples );
        }
        else if ( p_filter->fmt_out.audio.i_original_channels
                    & AOUT_CHAN_REVERSESTEREO )
        {
            Exchange( (sample_t *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                      p_samples );
        }
        else
        {
            /* Interleave the *$£%ù samples. */
            Interleave( (sample_t *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                        p_samples, p_sys->i_nb_channels, p_sys->pi_chan_table);
        }
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_dts = p_in_buf->i_dts;
    p_out_buf->i_pts = p_in_buf->i_pts;
    p_out_buf->i_length = p_in_buf->i_length;
out:
    block_Release( p_in_buf );
    return p_out_buf;
}

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    int i_ret;

    if( p_filter->fmt_in.i_codec != VLC_CODEC_A52 )
        return VLC_EGENERIC;
#ifdef LIBA52_FIXED
    if( p_filter->fmt_out.audio.i_format != VLC_CODEC_S32N )
#else
    if( p_filter->fmt_out.audio.i_format != VLC_CODEC_FL32 )
#endif
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the module's structure */
    p_filter->p_sys = p_sys = malloc( sizeof(filter_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    i_ret = Open( VLC_OBJECT(p_filter), p_sys,
                  &p_filter->fmt_in.audio, &p_filter->fmt_out.audio );

    p_filter->pf_audio_filter = Convert;
    p_filter->fmt_out.audio.i_rate = p_filter->fmt_in.audio.i_rate;

    return i_ret;
}

/*****************************************************************************
 * CloseFilter : deallocate data structures
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    a52_free( p_sys->p_liba52 );
    free( p_sys );
}
