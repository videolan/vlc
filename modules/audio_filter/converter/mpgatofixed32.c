/*****************************************************************************
 * mpgatofixed32.c: MPEG-1 & 2 audio layer I, II, III + MPEG 2.5 decoder,
 * using MAD (MPEG Audio Decoder)
 *****************************************************************************
 * Copyright (C) 2001 by Jean-Paul Saman
 * $Id: mpgatofixed32.c,v 1.5 2003/02/21 14:17:46 hartman Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman@wxs.nl>
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
#include <string.h>                                              /* strdup() */

#include <mad.h>

#include <vlc/vlc.h>
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
    struct mad_stream mad_stream;
    struct mad_frame mad_frame;
    struct mad_synth mad_synth;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL, VLC_TRUE );
    set_description( _("MPEG audio decoder module") );
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

    if ( (p_filter->input.i_format != VLC_FOURCC('m','p','g','a')
           && p_filter->input.i_format != VLC_FOURCC('m','p','g','3'))
            || (p_filter->output.i_format != VLC_FOURCC('f','l','3','2')
                 && p_filter->output.i_format != VLC_FOURCC('f','i','3','2')) )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
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

    /* Initialize libmad */
    mad_stream_init( &p_sys->mad_stream );
    mad_frame_init( &p_sys->mad_frame );
    mad_synth_init( &p_sys->mad_synth );
    mad_stream_options( &p_sys->mad_stream, MAD_OPTION_IGNORECRC );

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 0;

    return 0;
}

/*****************************************************************************
 * DoWork: decode an MPEG audio frame.
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    struct aout_filter_sys_t * p_sys = p_filter->p_sys;


    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_samples * sizeof(vlc_fixed_t);

    /* Do the actual decoding now. */
    mad_stream_buffer( &p_sys->mad_stream, p_in_buf->p_buffer,
                       p_in_buf->i_nb_bytes );
    if ( mad_frame_decode( &p_sys->mad_frame, &p_sys->mad_stream ) == -1 )
    {
        msg_Warn( p_aout, "libmad error: %s",
                  mad_stream_errorstr( &p_sys->mad_stream ) );
        if( p_filter->output.i_format == VLC_FOURCC('f','l','3','2') )
        {
            int i;
	    float * a = (float *)p_out_buf->p_buffer;
            for ( i = 0 ; i < p_out_buf->i_nb_samples ; i++ )
                *a++ = 0.0;
            return;
        }
        else
        {
            memset( p_out_buf->p_buffer, 0, p_out_buf->i_nb_bytes );
        } 
    }
    mad_synth_frame( &p_sys->mad_synth, &p_sys->mad_frame );

    if ( p_filter->output.i_format == VLC_FOURCC('f','i','3','2') )
    {
        /* Interleave and keep buffers in mad_fixed_t format */
        mad_fixed_t * p_samples = (mad_fixed_t *)p_out_buf->p_buffer;
        struct mad_pcm * p_pcm = &p_sys->mad_synth.pcm;
        unsigned int i_samples = p_pcm->length;
        mad_fixed_t const * p_left = p_pcm->samples[0];
        mad_fixed_t const * p_right = p_pcm->samples[1];

        switch ( p_pcm->channels )
        {
        case 2:
            while ( i_samples-- )
            {
                *p_samples++ = *p_left++;
                *p_samples++ = *p_right++;
            }
            break;

        case 1:
            p_filter->p_vlc->pf_memcpy( p_samples, p_left,
                                        i_samples * sizeof(mad_fixed_t) );
            break;

        default:
            msg_Err( p_filter, "cannot interleave %i channels",
                     p_pcm->channels );
        }
    }
    else
    {
        /* float32 */
        float * p_samples = (float *)p_out_buf->p_buffer;
        struct mad_pcm * p_pcm = &p_sys->mad_synth.pcm;
        unsigned int i_samples = p_pcm->length;
        mad_fixed_t const * p_left = p_pcm->samples[0];
        mad_fixed_t const * p_right = p_pcm->samples[1];

        switch ( p_pcm->channels )
        {
        case 2:
            while ( i_samples-- )
            {
                *p_samples++ = (float)*p_left++ / (float)FIXED32_ONE;
                *p_samples++ = (float)*p_right++ / (float)FIXED32_ONE;
            }
            break;

        case 1:
            while ( i_samples-- )
            {
                *p_samples++ = (float)*p_left++ / (float)FIXED32_ONE;
            }
            break;

        default:
            msg_Err( p_filter, "cannot interleave %i channels",
                     p_pcm->channels );
        }
    }
}

/*****************************************************************************
 * Destroy : deallocate data structures
 *****************************************************************************/
static void Destroy( vlc_object_t * _p_filter )
{
    aout_filter_t * p_filter = (aout_filter_t *)_p_filter;
    struct aout_filter_sys_t * p_sys = p_filter->p_sys;

    mad_synth_finish( &p_sys->mad_synth );
    mad_frame_finish( &p_sys->mad_frame );
    mad_stream_finish( &p_sys->mad_stream );
    free( p_sys );
}

