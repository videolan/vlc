/*****************************************************************************
 * filters.c : audio output filters management
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_dialog.h>

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#endif

#include <vlc_aout.h>
#include "aout_internal.h"
#include <libvlc.h>

/*****************************************************************************
 * FindFilter: find an audio filter for a specific transformation
 *****************************************************************************/
static aout_filter_t * FindFilter( aout_instance_t * p_aout,
                             const audio_sample_format_t * p_input_format,
                             const audio_sample_format_t * p_output_format )
{
    static const char typename[] = "audio output";
    aout_filter_t * p_filter;

    p_filter = vlc_custom_create( p_aout, sizeof(*p_filter),
                                  VLC_OBJECT_GENERIC, typename );

    if ( p_filter == NULL ) return NULL;
    vlc_object_attach( p_filter, p_aout );

    memcpy( &p_filter->input, p_input_format, sizeof(audio_sample_format_t) );
    memcpy( &p_filter->output, p_output_format,
            sizeof(audio_sample_format_t) );
    p_filter->p_module = module_need( p_filter, "audio filter", NULL, false );
    if ( p_filter->p_module == NULL )
    {
        vlc_object_detach( p_filter );
        vlc_object_release( p_filter );
        return NULL;
    }

    p_filter->b_continuity = false;

    return p_filter;
}

/*****************************************************************************
 * SplitConversion: split a conversion in two parts
 *****************************************************************************
 * Returns the number of conversions required by the first part - 0 if only
 * one conversion was asked.
 * Beware : p_output_format can be modified during this function if the
 * developer passed SplitConversion( toto, titi, titi, ... ). That is legal.
 * SplitConversion( toto, titi, toto, ... ) isn't.
 *****************************************************************************/
static int SplitConversion( const audio_sample_format_t * p_input_format,
                            const audio_sample_format_t * p_output_format,
                            audio_sample_format_t * p_middle_format )
{
    bool b_format =
             (p_input_format->i_format != p_output_format->i_format);
    bool b_rate = (p_input_format->i_rate != p_output_format->i_rate);
    bool b_channels =
        (p_input_format->i_physical_channels
          != p_output_format->i_physical_channels)
     || (p_input_format->i_original_channels
          != p_output_format->i_original_channels);
    int i_nb_conversions = b_format + b_rate + b_channels;

    if ( i_nb_conversions <= 1 ) return 0;

    memcpy( p_middle_format, p_output_format, sizeof(audio_sample_format_t) );

    if ( i_nb_conversions == 2 )
    {
        if ( !b_format || !b_channels )
        {
            p_middle_format->i_rate = p_input_format->i_rate;
            aout_FormatPrepare( p_middle_format );
            return 1;
        }

        /* !b_rate */
        p_middle_format->i_physical_channels
             = p_input_format->i_physical_channels;
        p_middle_format->i_original_channels
             = p_input_format->i_original_channels;
        aout_FormatPrepare( p_middle_format );
        return 1;
    }

    /* i_nb_conversion == 3 */
    p_middle_format->i_rate = p_input_format->i_rate;
    aout_FormatPrepare( p_middle_format );
    return 2;
}

static void ReleaseFilter( aout_filter_t * p_filter )
{
    module_unneed( p_filter, p_filter->p_module );
    vlc_object_detach( p_filter );
    vlc_object_release( p_filter );
}

/*****************************************************************************
 * aout_FiltersCreatePipeline: create a filters pipeline to transform a sample
 *                             format to another
 *****************************************************************************
 * pi_nb_filters must be initialized before calling this function
 *****************************************************************************/
int aout_FiltersCreatePipeline( aout_instance_t * p_aout,
                                aout_filter_t ** pp_filters_start,
                                int * pi_nb_filters,
                                const audio_sample_format_t * p_input_format,
                                const audio_sample_format_t * p_output_format )
{
    aout_filter_t** pp_filters = pp_filters_start + *pi_nb_filters;
    audio_sample_format_t temp_format;
    int i_nb_conversions;

    if ( AOUT_FMTS_IDENTICAL( p_input_format, p_output_format ) )
    {
        msg_Dbg( p_aout, "no need for any filter" );
        return 0;
    }

    aout_FormatsPrint( p_aout, "filter(s)", p_input_format, p_output_format );

    if( *pi_nb_filters + 1 > AOUT_MAX_FILTERS )
    {
        msg_Err( p_aout, "max filter reached (%d)", AOUT_MAX_FILTERS );
        dialog_Fatal( p_aout, _("Audio filtering failed"),
                      _("The maximum number of filters (%d) was reached."),
                      AOUT_MAX_FILTERS );
        return -1;
    }

    /* Try to find a filter to do the whole conversion. */
    pp_filters[0] = FindFilter( p_aout, p_input_format, p_output_format );
    if ( pp_filters[0] != NULL )
    {
        msg_Dbg( p_aout, "found a filter for the whole conversion" );
        ++*pi_nb_filters;
        return 0;
    }

    /* We'll have to split the conversion. We always do the downmixing
     * before the resampling, because the audio decoder can probably do it
     * for us. */
    i_nb_conversions = SplitConversion( p_input_format,
                                        p_output_format, &temp_format );
    if ( !i_nb_conversions )
    {
        /* There was only one conversion to do, and we already failed. */
        msg_Err( p_aout, "couldn't find a filter for the conversion" );
        return -1;
    }

    pp_filters[0] = FindFilter( p_aout, p_input_format, &temp_format );
    if ( pp_filters[0] == NULL && i_nb_conversions == 2 )
    {
        /* Try with only one conversion. */
        SplitConversion( p_input_format, &temp_format, &temp_format );
        pp_filters[0] = FindFilter( p_aout, p_input_format, &temp_format );
    }
    if ( pp_filters[0] == NULL )
    {
        msg_Err( p_aout,
              "couldn't find a filter for the first part of the conversion" );
        return -1;
    }

    /* We have the first stage of the conversion. Find a filter for
     * the rest. */
    if( *pi_nb_filters + 2 > AOUT_MAX_FILTERS )
    {
        ReleaseFilter( pp_filters[0] );
        msg_Err( p_aout, "max filter reached (%d)", AOUT_MAX_FILTERS );
        dialog_Fatal( p_aout, _("Audio filtering failed"),
                      _("The maximum number of filters (%d) was reached."),
                      AOUT_MAX_FILTERS );
        return -1;
    }
    pp_filters[1] = FindFilter( p_aout, &pp_filters[0]->output,
                                p_output_format );
    if ( pp_filters[1] == NULL )
    {
        /* Try to split the conversion. */
        i_nb_conversions = SplitConversion( &pp_filters[0]->output,
                                           p_output_format, &temp_format );
        if ( !i_nb_conversions )
        {
            ReleaseFilter( pp_filters[0] );
            msg_Err( p_aout,
              "couldn't find a filter for the second part of the conversion" );
            return -1;
        }
        if( *pi_nb_filters + 3 > AOUT_MAX_FILTERS )
        {
            ReleaseFilter( pp_filters[0] );
            msg_Err( p_aout, "max filter reached (%d)", AOUT_MAX_FILTERS );
            dialog_Fatal( p_aout, _("Audio filtering failed"),
                          _("The maximum number of filters (%d) was reached."),
                          AOUT_MAX_FILTERS );
            return -1;
        }
        pp_filters[1] = FindFilter( p_aout, &pp_filters[0]->output,
                                    &temp_format );
        pp_filters[2] = FindFilter( p_aout, &temp_format,
                                    p_output_format );

        if ( pp_filters[1] == NULL || pp_filters[2] == NULL )
        {
            ReleaseFilter( pp_filters[0] );
            if ( pp_filters[1] != NULL )
            {
                ReleaseFilter( pp_filters[1] );
            }
            if ( pp_filters[2] != NULL )
            {
                ReleaseFilter( pp_filters[2] );
            }
            msg_Err( p_aout,
               "couldn't find filters for the second part of the conversion" );
            return -1;
        }
        *pi_nb_filters += 3;
        msg_Dbg( p_aout, "found 3 filters for the whole conversion" );
    }
    else
    {
        *pi_nb_filters += 2;
        msg_Dbg( p_aout, "found 2 filters for the whole conversion" );
    }

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
    (void)p_aout;

    for ( i = 0; i < i_nb_filters; i++ )
    {
        aout_filter_t *p_filter = pp_filters[i];

        module_unneed( p_filter, p_filter->p_module );
        free( p_filter->p_owner );
        vlc_object_detach( p_filter );
        vlc_object_release( p_filter );
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

    (void)p_aout; /* unused */

    for ( i = i_nb_filters - 1; i >= 0; i-- )
    {
        aout_filter_t * p_filter = pp_filters[i];

        int i_output_size = p_filter->output.i_bytes_per_frame
                             * p_filter->output.i_rate * AOUT_MAX_INPUT_RATE
                             / p_filter->output.i_frame_length;
        int i_input_size = p_filter->input.i_bytes_per_frame
                             * p_filter->input.i_rate * AOUT_MAX_INPUT_RATE
                             / p_filter->input.i_frame_length;

        p_first_alloc->i_bytes_per_sec = __MAX( p_first_alloc->i_bytes_per_sec,
                                                i_output_size );

        if ( p_filter->b_in_place )
        {
            p_first_alloc->i_bytes_per_sec = __MAX(
                                         p_first_alloc->i_bytes_per_sec,
                                         i_input_size );
            p_filter->output_alloc.i_alloc_type = AOUT_ALLOC_NONE;
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

    for( i = 0; i < i_nb_filters; i++ )
    {
        aout_filter_t * p_filter = pp_filters[i];
        aout_buffer_t * p_output_buffer;

        /* Resamplers can produce slightly more samples than (i_in_nb *
         * p_filter->output.i_rate / p_filter->input.i_rate) so we need
         * slightly bigger buffers. */
        aout_BufferAlloc( &p_filter->output_alloc,
                          ((mtime_t)(*pp_input_buffer)->i_nb_samples + 2)
                          * 1000000 / p_filter->input.i_rate,
                          *pp_input_buffer, p_output_buffer );
        if( p_output_buffer == NULL )
            return;

        /* Please note that p_output_buffer->i_nb_samples & i_nb_bytes
         * shall be set by the filter plug-in. */
        if( (*pp_input_buffer)->i_nb_samples > 0 )
        {
            p_filter->pf_do_work( p_aout, p_filter, *pp_input_buffer,
                                  p_output_buffer );
        }
        else
        {
            p_output_buffer->i_nb_bytes = 0;
            p_output_buffer->i_nb_samples = 0;
        }

        if( !p_filter->b_in_place )
        {
            aout_BufferFree( *pp_input_buffer );
            *pp_input_buffer = p_output_buffer;
        }
    }

    assert( (*pp_input_buffer) == NULL || (*pp_input_buffer)->i_alloc_type != AOUT_ALLOC_STACK );
}

/*****************************************************************************
 * aout_filter_RequestVout
 *****************************************************************************/
vout_thread_t *aout_filter_RequestVout( aout_filter_t *p_filter,
                                        vout_thread_t *p_vout, video_format_t *p_fmt )
{
    if( !p_filter->request_vout.pf_request_vout )
        return NULL;
    return p_filter->request_vout.pf_request_vout( p_filter->request_vout.p_private,
                                                   p_vout, p_fmt, true );
}

