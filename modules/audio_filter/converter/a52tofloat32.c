/*****************************************************************************
 * a52tofloat32.c: ATSC A/52 aka AC-3 decoder plugin for VLC.
 *   This plugin makes use of liba52 to decode A/52 audio
 *   (http://liba52.sf.net/).
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: a52tofloat32.c,v 1.2 2002/09/16 20:46:37 massiot Exp $
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
    add_category_hint( N_("Miscellaneous"), NULL );
    add_bool( "a52-dynrng", 1, NULL, DYNRNG_TEXT, DYNRNG_LONGTEXT );
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

    if ( p_filter->input.i_format != AOUT_FMT_A52
          || p_filter->output.i_format != AOUT_FMT_FLOAT32 )
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

    /* We'll do our own downmixing, thanks. */
    p_sys->i_nb_channels = aout_FormatNbChannels( &p_filter->output );
    switch ( p_filter->output.i_channels & AOUT_CHAN_MASK )
    {
    case AOUT_CHAN_CHANNEL: p_sys->i_flags = A52_CHANNEL; break;
    case AOUT_CHAN_CHANNEL1: p_sys->i_flags = A52_CHANNEL1; break;
    case AOUT_CHAN_CHANNEL2: p_sys->i_flags = A52_CHANNEL2; break;
    case AOUT_CHAN_MONO: p_sys->i_flags = A52_MONO; break;
    case AOUT_CHAN_STEREO: p_sys->i_flags = A52_STEREO; break;
    case AOUT_CHAN_DOLBY: p_sys->i_flags = A52_DOLBY; break;
    case AOUT_CHAN_3F: p_sys->i_flags = A52_3F; break;
    case AOUT_CHAN_2F1R: p_sys->i_flags = A52_2F1R; break;
    case AOUT_CHAN_3F1R: p_sys->i_flags = A52_3F1R; break;
    case AOUT_CHAN_2F2R: p_sys->i_flags = A52_2F2R; break;
    case AOUT_CHAN_3F2R: p_sys->i_flags = A52_3F2R; break;
    default:
        msg_Err( p_filter, "unknow sample format !" );
        free( p_sys );
        return -1;
    }
    if ( p_filter->output.i_channels & AOUT_CHAN_LFE )
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
static void Interleave( float * p_out, const float * p_in, int i_channels )
{
    int i, j;

    for ( j = 0; j < i_channels; j++ )
    {
        for ( i = 0; i < 256; i++ )
        {
            p_out[i * i_channels + j] = p_in[j * 256 + i];
        }
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

    if ( (i_flags & A52_CHANNEL_MASK) != (p_sys->i_flags & A52_CHANNEL_MASK) )
    {
        msg_Err( p_filter,
                 "liba52 couldn't do the requested downmix 0x%x->0x%x",
                 p_sys->i_flags, i_flags );
        memset( p_out_buf->p_buffer, 0, i_bytes_per_block * 6 );
        return;
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

        /* Interleave the *$£%ù samples. */
        Interleave( (float *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                    p_samples, p_sys->i_nb_channels );
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

