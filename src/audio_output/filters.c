/*****************************************************************************
 * filters.c : audio output filters management
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: filters.c,v 1.2 2002/08/09 23:47:23 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#ifdef HAVE_ALLOCA_H
#   include <alloca.h> 
#endif

#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * aout_FiltersCreatePipeline: create a filters pipeline to transform a sample
 *                             format to another
 *****************************************************************************
 * TODO : allow the user to add/remove specific filters
 *****************************************************************************/
int aout_FiltersCreatePipeline( aout_instance_t * p_aout,
                                aout_filter_t ** pp_filters,
                                int * pi_nb_filters,
                                audio_sample_format_t * p_input_format,
                                audio_sample_format_t * p_output_format )
{
    if ( AOUT_FMTS_IDENTICAL( p_input_format, p_output_format ) )
    {
        msg_Dbg( p_aout, "no need for any filter" );
        *pi_nb_filters = 0;
        return 0;
    }

    pp_filters[0] = vlc_object_create( p_aout, sizeof(aout_filter_t) );
    if ( pp_filters[0] == NULL )
    {
        return -1;
    }
    vlc_object_attach( pp_filters[0], p_aout );

    /* Try to find a filter to do the whole conversion. */
    memcpy( &pp_filters[0]->input, p_input_format,
            sizeof(audio_sample_format_t) );
    memcpy( &pp_filters[0]->output, p_output_format,
            sizeof(audio_sample_format_t) );
    pp_filters[0]->p_module = module_Need( pp_filters[0], "audio filter",
                                           NULL );
    if ( pp_filters[0]->p_module != NULL )
    {
        msg_Dbg( p_aout, "found a filter for the whole conversion" );
        *pi_nb_filters = 1;
        return 0;
    }

    /* Split the conversion : format | rate, or rate | format. */
    pp_filters[0]->output.i_format = pp_filters[0]->input.i_format;
    pp_filters[0]->p_module = module_Need( pp_filters[0], "audio filter",
                                           NULL );
    if ( pp_filters[0]->p_module == NULL )
    {
        /* Then, start with the format conversion. */
        memcpy( &pp_filters[0]->output, p_output_format,
                sizeof(audio_sample_format_t) );
        pp_filters[0]->output.i_rate = pp_filters[0]->input.i_rate;
        pp_filters[0]->p_module = module_Need( pp_filters[0], "audio filter",
                                               NULL );
        if ( pp_filters[0]->p_module == NULL )
        {
            msg_Err( p_aout, "couldn't find a filter for any conversion" );
            vlc_object_detach_all( pp_filters[0] );
            vlc_object_destroy( pp_filters[0] );
            return -1;
        }
    }

    /* Find a filter for the rest. */
    pp_filters[1] = vlc_object_create( p_aout, sizeof(aout_filter_t) );
    if ( pp_filters[1] == NULL )
    {
        vlc_object_detach_all( pp_filters[0] );
        vlc_object_destroy( pp_filters[0] );
        return -1;
    }
    vlc_object_attach( pp_filters[1], p_aout );

    memcpy( &pp_filters[1]->input, &pp_filters[0]->output,
            sizeof(audio_sample_format_t) );
    memcpy( &pp_filters[1]->output, p_output_format,
            sizeof(audio_sample_format_t) );
    pp_filters[1]->p_module = module_Need( pp_filters[1], "audio filter",
                                           NULL );
    if ( pp_filters[1]->p_module == NULL )
    {
        msg_Err( p_aout,
                 "couldn't find a filter for the 2nd part of the conversion" );
        vlc_object_detach_all( pp_filters[0] );
        vlc_object_destroy( pp_filters[0] );
        vlc_object_detach_all( pp_filters[1] );
        vlc_object_destroy( pp_filters[1] );
        return -1;
    }

    msg_Dbg( p_aout, "filter pipeline made of two filters" );
    *pi_nb_filters = 2;

    return 0;
}

/*****************************************************************************
 * aout_FiltersDestroyPipeline: deallocate a filters pipeline
 *****************************************************************************/
void aout_FiltersDestroyPipeline( aout_instance_t * p_aout,
                                  aout_filter_t ** pp_filters,
                                  int i_nb_filters )
{
    int i;

    for ( i = 0; i < i_nb_filters; i++ )
    {
        module_Unneed( pp_filters[i], pp_filters[i]->p_module );
        vlc_object_detach_all( pp_filters[i] );
        vlc_object_destroy( pp_filters[i] );
    }
}

/*****************************************************************************
 * aout_FiltersHintBuffers: fill in aout_alloc_t structures to optimize
 *                          buffer allocations
 *****************************************************************************/
void aout_FiltersHintBuffers( aout_instance_t * p_aout,
                              aout_filter_t ** pp_filters,
                              int i_nb_filters, aout_alloc_t * p_first_alloc )
{
    int i;

    for ( i = i_nb_filters - 1; i >= 0; i-- )
    {
        aout_filter_t * p_filter = pp_filters[i];

        int i_output_size = aout_FormatToByterate( &p_filter->output,
                                                   p_filter->output.i_rate );
        int i_input_size = aout_FormatToByterate( &p_filter->input,
                                                  p_filter->input.i_rate );

        p_first_alloc->i_bytes_per_sec = __MAX( p_first_alloc->i_bytes_per_sec,
                                                i_output_size );

        if ( p_filter->b_in_place )
        {
            p_first_alloc->i_bytes_per_sec = __MAX(
                                         p_first_alloc->i_bytes_per_sec,
                                         i_input_size );
        }
        else
        {
            /* We're gonna need a buffer allocation. */
            memcpy( &p_filter->output_alloc, p_first_alloc,
                    sizeof(aout_alloc_t) );
            p_first_alloc->i_alloc_type = AOUT_ALLOC_STACK;
            p_first_alloc->i_bytes_per_sec = i_input_size;
        }
    }
}

/*****************************************************************************
 * aout_FiltersPlay: play a buffer
 *****************************************************************************/
void aout_FiltersPlay( aout_instance_t * p_aout,
                       aout_filter_t ** pp_filters,
                       int i_nb_filters, aout_buffer_t ** pp_input_buffer )
{
    int i;

    for ( i = 0; i < i_nb_filters; i++ )
    {
        aout_filter_t * p_filter = pp_filters[i];
        aout_buffer_t * p_output_buffer;

        aout_BufferAlloc( &p_filter->output_alloc,
                          (u64)((*pp_input_buffer)->i_nb_samples * 1000000)
                            / p_filter->output.i_rate, *pp_input_buffer,
                          p_output_buffer );
        if ( p_output_buffer == NULL )
        {
            msg_Err( p_aout, "out of memory" );
            return;
        }
        /* Please note that p_output_buffer->i_nb_samples shall be set by
         * the filter plug-in. */

        p_filter->pf_do_work( p_aout, p_filter, *pp_input_buffer,
                              p_output_buffer );

        if ( !p_filter->b_in_place )
        {
            aout_BufferFree( *pp_input_buffer );
            *pp_input_buffer = p_output_buffer;
        }
    }
}

