/*****************************************************************************
 * a52tofloat32.c: ATSC A/52 aka AC-3 decoder plugin for VLC.
 *   This plugin makes use of liba52 to decode A/52 audio
 *   (http://liba52.sf.net/).
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#ifdef HAVE_STDINT_H
#   include <stdint.h>                                         /* int16_t .. */
#elif HAVE_INTTYPES_H
#   include <inttypes.h>                                       /* int16_t .. */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

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
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );
static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );
static int  Open      ( vlc_object_t *, filter_sys_t *,
                        audio_format_t, audio_format_t );

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

    int pi_chan_table[AOUT_CHAN_MAX]; /* channel reordering */
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

vlc_module_begin();
    set_shortname( "A/52" );
    set_description( N_("ATSC A/52 (AC-3) audio decoder") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    add_bool( "a52-dynrng", 1, NULL, DYNRNG_TEXT, DYNRNG_LONGTEXT, false );
    add_bool( "a52-upmix", 0, NULL, UPMIX_TEXT, UPMIX_LONGTEXT, true );
    set_capability( "audio filter", 100 );
    set_callbacks( Create, Destroy );

    add_submodule();
    set_description( N_("ATSC A/52 (AC-3) audio decoder") );
    set_capability( "audio filter2", 100 );
    set_callbacks( OpenFilter, CloseFilter );
vlc_module_end();

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t *p_filter = (aout_filter_t *)p_this;
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
    int i_ret;

    if ( p_filter->input.i_format != VLC_FOURCC('a','5','2',' ')
#ifdef LIBA52_FIXED
          || p_filter->output.i_format != VLC_FOURCC('f','i','3','2') )
#else
          || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
#endif
    {
        return -1;
    }

    if ( p_filter->input.i_rate != p_filter->output.i_rate )
    {
        return -1;
    }

    /* Allocate the memory needed to store the module's structure */
    p_sys = malloc( sizeof(filter_sys_t) );
    p_filter->p_sys = (struct aout_filter_sys_t *)p_sys;
    if( p_sys == NULL )
        return -1;

    i_ret = Open( VLC_OBJECT(p_filter), p_sys,
                  p_filter->input, p_filter->output );

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 0;

    return i_ret;
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this, filter_sys_t *p_sys,
                 audio_format_t input, audio_format_t output )
{
    p_sys->b_dynrng = config_GetInt( p_this, "a52-dynrng" );
    p_sys->b_dontwarn = 0;

    /* No upmixing: it's not necessary and some other filters may want to do
     * it themselves. */
    if ( aout_FormatNbChannels( &output ) > aout_FormatNbChannels( &input ) )
    {
        if ( ! config_GetInt( p_this, "a52-upmix" ) )
        {
            return VLC_EGENERIC;
        }
    }

    /* We'll do our own downmixing, thanks. */
    p_sys->i_nb_channels = aout_FormatNbChannels( &output );
    switch ( (output.i_physical_channels & AOUT_CHAN_PHYSMASK)
              & ~AOUT_CHAN_LFE )
    {
    case AOUT_CHAN_CENTER:
        if ( (output.i_original_channels & AOUT_CHAN_CENTER)
              || (output.i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
        {
            p_sys->i_flags = A52_MONO;
        }
        else if ( output.i_original_channels & AOUT_CHAN_LEFT )
        {
            p_sys->i_flags = A52_CHANNEL1;
        }
        else
        {
            p_sys->i_flags = A52_CHANNEL2;
        }
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT:
        if ( output.i_original_channels & AOUT_CHAN_DOLBYSTEREO )
        {
            p_sys->i_flags = A52_DOLBY;
        }
        else if ( input.i_original_channels == AOUT_CHAN_CENTER )
        {
            p_sys->i_flags = A52_MONO;
        }
        else if ( input.i_original_channels & AOUT_CHAN_DUALMONO )
        {
            p_sys->i_flags = A52_CHANNEL;
        }
        else if ( !(output.i_original_channels & AOUT_CHAN_RIGHT) )
        {
            p_sys->i_flags = A52_CHANNEL1;
        }
        else if ( !(output.i_original_channels & AOUT_CHAN_LEFT) )
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
    if ( output.i_physical_channels & AOUT_CHAN_LFE )
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
                              output.i_physical_channels & AOUT_CHAN_PHYSMASK,
                              p_sys->i_nb_channels,
                              p_sys->pi_chan_table );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void Interleave( float * p_out, const float * p_in, int i_nb_channels,
                        int *pi_chan_table )
{
    /* We do not only have to interleave, but also reorder the channels */

    int i, j;
    for ( j = 0; j < i_nb_channels; j++ )
    {
        for ( i = 0; i < 256; i++ )
        {
            p_out[i * i_nb_channels + pi_chan_table[j]] = p_in[j * 256 + i];
        }
    }
}

/*****************************************************************************
 * Duplicate: helper function to duplicate a unique channel
 *****************************************************************************/
static void Duplicate( float * p_out, const float * p_in )
{
    int i;

    for ( i = 256; i--; )
    {
        *p_out++ = *p_in;
        *p_out++ = *p_in;
        p_in++;
    }
}

/*****************************************************************************
 * Exchange: helper function to exchange left & right channels
 *****************************************************************************/
static void Exchange( float * p_out, const float * p_in )
{
    int i;
    const float * p_first = p_in + 256;
    const float * p_second = p_in;

    for ( i = 0; i < 256; i++ )
    {
        *p_out++ = *p_first++;
        *p_out++ = *p_second++;
    }
}

/*****************************************************************************
 * DoWork: decode an ATSC A/52 frame.
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    filter_sys_t    *p_sys = (filter_sys_t *)p_filter->p_sys;
#ifdef LIBA52_FIXED
    sample_t        i_sample_level = (1 << 24);
#else
    sample_t        i_sample_level = 1;
#endif
    int             i_flags = p_sys->i_flags;
    int             i_bytes_per_block = 256 * p_sys->i_nb_channels
                      * sizeof(float);
    int             i;

    /* Do the actual decoding now. */
    a52_frame( p_sys->p_liba52, p_in_buf->p_buffer,
               &i_flags, &i_sample_level, 0 );

    if ( (i_flags & A52_CHANNEL_MASK) != (p_sys->i_flags & A52_CHANNEL_MASK)
          && !p_sys->b_dontwarn )
    {
        msg_Warn( p_aout,
                  "liba52 couldn't do the requested downmix 0x%x->0x%x",
                  p_sys->i_flags  & A52_CHANNEL_MASK,
                  i_flags & A52_CHANNEL_MASK );

        p_sys->b_dontwarn = 1;
    }

    if( !p_sys->b_dynrng )
    {
        a52_dynrng( p_sys->p_liba52, NULL, NULL );
    }

    for ( i = 0; i < 6; i++ )
    {
        sample_t * p_samples;

        if( a52_block( p_sys->p_liba52 ) )
        {
            msg_Warn( p_aout, "a52_block failed for block %d", i );
        }

        p_samples = a52_samples( p_sys->p_liba52 );

        if ( ((p_sys->i_flags & A52_CHANNEL_MASK) == A52_CHANNEL1
               || (p_sys->i_flags & A52_CHANNEL_MASK) == A52_CHANNEL2
               || (p_sys->i_flags & A52_CHANNEL_MASK) == A52_MONO)
              && (p_filter->output.i_physical_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
        {
            Duplicate( (float *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                       p_samples );
        }
        else if ( p_filter->output.i_original_channels
                    & AOUT_CHAN_REVERSESTEREO )
        {
            Exchange( (float *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                      p_samples );
        }
        else
        {
            /* Interleave the *$£%ù samples. */
            Interleave( (float *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                        p_samples, p_sys->i_nb_channels, p_sys->pi_chan_table);
        }
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = i_bytes_per_block * 6;
}

/*****************************************************************************
 * Destroy : deallocate data structures
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    aout_filter_t *p_filter = (aout_filter_t *)p_this;
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;

    a52_free( p_sys->p_liba52 );
    free( p_sys );
}

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    int i_ret;

    if( p_filter->fmt_in.i_codec != VLC_FOURCC('a','5','2',' ')  )
    {
        return VLC_EGENERIC;
    }

    p_filter->fmt_out.audio.i_format =
#ifdef LIBA52_FIXED
        p_filter->fmt_out.i_codec = VLC_FOURCC('f','i','3','2');
#else
        p_filter->fmt_out.i_codec = VLC_FOURCC('f','l','3','2');
#endif
    p_filter->fmt_out.audio.i_bitspersample =
        aout_BitsPerSample( p_filter->fmt_out.i_codec );

    /* Allocate the memory needed to store the module's structure */
    p_filter->p_sys = p_sys = malloc( sizeof(filter_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    i_ret = Open( VLC_OBJECT(p_filter), p_sys,
                  p_filter->fmt_in.audio, p_filter->fmt_out.audio );

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

static block_t *Convert( filter_t *p_filter, block_t *p_block )
{
    aout_filter_t aout_filter;
    aout_buffer_t in_buf, out_buf;
    block_t *p_out;
    int i_out_size;

    if( !p_block || !p_block->i_samples )
    {
        if( p_block )
            block_Release( p_block );
        return NULL;
    }

    i_out_size = p_block->i_samples *
      p_filter->fmt_out.audio.i_bitspersample *
        p_filter->fmt_out.audio.i_channels / 8;

    p_out = p_filter->pf_audio_buffer_new( p_filter, i_out_size );
    if( !p_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        block_Release( p_block );
        return NULL;
    }

    p_out->i_samples = p_block->i_samples;
    p_out->i_dts = p_block->i_dts;
    p_out->i_pts = p_block->i_pts;
    p_out->i_length = p_block->i_length;

    aout_filter.p_sys = (struct aout_filter_sys_t *)p_filter->p_sys;
    aout_filter.input = p_filter->fmt_in.audio;
    aout_filter.input.i_format = p_filter->fmt_in.i_codec;
    aout_filter.output = p_filter->fmt_out.audio;
    aout_filter.output.i_format = p_filter->fmt_out.i_codec;

    in_buf.p_buffer = p_block->p_buffer;
    in_buf.i_nb_bytes = p_block->i_buffer;
    in_buf.i_nb_samples = p_block->i_samples;
    out_buf.p_buffer = p_out->p_buffer;
    out_buf.i_nb_bytes = p_out->i_buffer;
    out_buf.i_nb_samples = p_out->i_samples;

    DoWork( (aout_instance_t *)p_filter, &aout_filter, &in_buf, &out_buf );

    p_out->i_buffer = out_buf.i_nb_bytes;
    p_out->i_samples = out_buf.i_nb_samples;

    block_Release( p_block );

    return p_out;
}
