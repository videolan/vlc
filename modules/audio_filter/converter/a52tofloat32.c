/*****************************************************************************
 * a52tofloat32.c: ATSC A/52 aka AC-3 decoder plugin for VLC.
 *   This plugin makes use of liba52 to decode A/52 audio
 *   (http://liba52.sf.net/).
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: a52tofloat32.c,v 1.14 2003/02/24 17:06:21 jlj Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
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

#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );
static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,  
                        aout_buffer_t * );

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct aout_filter_sys_t
{
    a52_state_t * p_liba52; /* liba52 internal structure */
    vlc_bool_t b_dynrng; /* see below */
    int i_flags; /* liba52 flags, see a52dec/doc/liba52.txt */
    vlc_bool_t b_dontwarn;
    int i_nb_channels; /* number of float32 per sample */
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

vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL, VLC_FALSE );
    add_bool( "a52-dynrng", 1, NULL, DYNRNG_TEXT, DYNRNG_LONGTEXT, VLC_FALSE );
    set_description( _("ATSC A/52 aka AC-3 audio decoder module") );
    set_capability( "audio filter", 100 );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * Create: 
 *****************************************************************************/
static int Create( vlc_object_t * _p_filter )
{
    aout_filter_t * p_filter = (aout_filter_t *)_p_filter;
    struct aout_filter_sys_t * p_sys;

    if ( p_filter->input.i_format != VLC_FOURCC('a','5','2',' ')
          || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return -1;
    }

    if ( p_filter->input.i_rate != p_filter->output.i_rate )
    {
        return -1;
    }

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_filter->p_sys = malloc( sizeof(struct aout_filter_sys_t) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return -1;
    }

    p_sys->b_dynrng = config_GetInt( p_filter, "a52-dynrng" );
    p_sys->b_dontwarn = 0;

    /* We'll do our own downmixing, thanks. */
    p_sys->i_nb_channels = aout_FormatNbChannels( &p_filter->output );
    switch ( (p_filter->output.i_physical_channels & AOUT_CHAN_PHYSMASK)
              & ~AOUT_CHAN_LFE )
    {
    case AOUT_CHAN_CENTER:
        if ( (p_filter->output.i_original_channels & AOUT_CHAN_CENTER)
              || (p_filter->output.i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
        {
            p_sys->i_flags = A52_MONO;
        }
        else if ( p_filter->output.i_original_channels & AOUT_CHAN_LEFT )
        {
            p_sys->i_flags = A52_CHANNEL1;
        }
        else
        {
            p_sys->i_flags = A52_CHANNEL2;
        }
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT:
        if ( p_filter->output.i_original_channels & AOUT_CHAN_DOLBYSTEREO )
        {
            p_sys->i_flags = A52_DOLBY;
        }
        else if ( p_filter->input.i_original_channels == AOUT_CHAN_CENTER )
        {
            p_sys->i_flags = A52_MONO;
        }
        else if ( p_filter->input.i_original_channels & AOUT_CHAN_DUALMONO )
        {
            p_sys->i_flags = A52_CHANNEL;
        }
        else if ( !(p_filter->output.i_original_channels & AOUT_CHAN_RIGHT) )
        {
            p_sys->i_flags = A52_CHANNEL1;
        }
        else if ( !(p_filter->output.i_original_channels & AOUT_CHAN_LEFT) )
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
        msg_Warn( p_filter, "unknown sample format !" );
        free( p_sys );
        return -1;
    }
    if ( p_filter->output.i_physical_channels & AOUT_CHAN_LFE )
    {
        p_sys->i_flags |= A52_LFE;
    }
    p_sys->i_flags |= A52_ADJUST_LEVEL;

    /* Initialize liba52 */
    p_sys->p_liba52 = a52_init( 0 );
    if( p_sys->p_liba52 == NULL )
    {
        msg_Err( p_filter, "unable to initialize liba52" );
        return -1;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 0;

    return 0;
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void Interleave( float * p_out, const float * p_in, int i_nb_channels )
{
    /* We do not only have to interleave, but also reorder the channels
     * Channel reordering according to number of output channels of libA52
     * The reordering needs to be different for different channel configurations
     * (3F2R, 1F2R etc), so this is only temporary.
     * The WG-4 order is appropriate for stereo, quadrophonia, and 5.1 surround.
     *
     * 6 channel mode
     * channel  liba52 order    WG-4 order
     * 0        LFE             // L
     * 1        L               // R
     * 2        C               // LS
     * 3        R               // RS
     * 4        LS              // C
     * 5        RS              // LFE
     *
     * The liba52 moves channels to the front if there are unused spaces, so
     * there is no gap between channels. The translation table says which
     * channel of the new stream is taken from which original channel [use
     * the new channel as the array index, use the number you get from the
     * array to address the original channel].
     */

    static const int translation[7][6] =
    {{ 0, 0, 0, 0, 0, 0 },      /* 0 channels (rarely used) */
    { 0, 0, 0, 0, 0, 0 },       /* 1 ch */
    { 0, 1, 0, 0, 0, 0 },       /* 2 */
    { 1, 2, 0, 0, 0, 0 },       /* 3 */
    { 1, 3, 2, 0, 0, 0 },       /* 4 */
    { 1, 3, 4, 2, 0, 0 },       /* 5 */
    { 1, 3, 4, 5, 2, 0 }};      /* 6 */

    int i, j;
    for ( j = 0; j < i_nb_channels; j++ )
    {
        for ( i = 0; i < 256; i++ )
        {
            p_out[i * i_nb_channels + j] = p_in[translation[i_nb_channels][j]
                                                 * 256 + i];
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
    struct aout_filter_sys_t * p_sys = p_filter->p_sys;
    sample_t        i_sample_level = 1;
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
        msg_Warn( p_filter,
                  "liba52 couldn't do the requested downmix 0x%x->0x%x",
                  p_sys->i_flags  & A52_CHANNEL_MASK,
                  i_flags & A52_CHANNEL_MASK );

        p_sys->b_dontwarn = 1;
    }

    if( !p_sys->b_dynrng )
    {
        a52_dynrng( p_filter->p_sys->p_liba52, NULL, NULL );
    }

    for ( i = 0; i < 6; i++ )
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
                        p_samples, p_sys->i_nb_channels );
        }
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = i_bytes_per_block * 6;
}

/*****************************************************************************
 * Destroy : deallocate data structures
 *****************************************************************************/
static void Destroy( vlc_object_t * _p_filter )
{
    aout_filter_t * p_filter = (aout_filter_t *)_p_filter;
    struct aout_filter_sys_t * p_sys = p_filter->p_sys;

    a52_free( p_sys->p_liba52 );
    free( p_sys );
}

