/*****************************************************************************
 * simple.c : simple channel mixer plug-in (only 7/7.1/5/5.1 -> Stereo for now)
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
    set_description( _("audio filter for simple channel mixing") );
    set_capability( "audio filter", 10 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate trivial channel mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( (p_filter->input.i_physical_channels
           == p_filter->output.i_physical_channels
           && p_filter->input.i_original_channels
               == p_filter->output.i_original_channels)
          || p_filter->input.i_format != p_filter->output.i_format
          || p_filter->input.i_rate != p_filter->output.i_rate
          || p_filter->input.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return -1;
    }

    /* Only conversion to Stereo and 4.0 right now */
    if( p_filter->output.i_physical_channels !=
        (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) 
        && p_filter->output.i_physical_channels !=
        ( AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
        AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT) )
        {
            return -1;
        }

    /* Only from 7/7.1/5/5.1 */
    if( (p_filter->input.i_physical_channels & ~AOUT_CHAN_LFE) !=
        (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
         AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT) &&
        (p_filter->input.i_physical_channels & ~AOUT_CHAN_LFE) !=
        (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
         AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
         AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT) )
    {
        return -1;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 0;

    return 0;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i_input_nb = aout_FormatNbChannels( &p_filter->input );
    int i_output_nb = aout_FormatNbChannels( &p_filter->output );
    float *p_dest = (float *)p_out_buf->p_buffer;
    float *p_src = (float *)p_in_buf->p_buffer;
    int i;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * i_output_nb / i_input_nb;

    if( p_filter->output.i_physical_channels ==
                            (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) )
    {
        if( p_filter->input.i_physical_channels & AOUT_CHAN_MIDDLELEFT )
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[6] + 0.5 * p_src[0] + p_src[2] / 4 + p_src[4] / 4;
            p_dest++;
            *p_dest = p_src[6] + 0.5 * p_src[1] + p_src[3] / 4 + p_src[5] / 4;
            p_dest++;

            p_src += 7;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }
        else
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[4] + 0.5 * p_src[0] + 0.33 * p_src[2];
            p_dest++;
            *p_dest = p_src[4] + 0.5 * p_src[1] + 0.33 * p_src[3];
            p_dest++;

            p_src += 5;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }
    }
    else
    {
        if( p_filter->input.i_physical_channels & AOUT_CHAN_MIDDLELEFT )
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[6] + 0.5 * p_src[0] + p_src[2] / 6;
            p_dest++;
            *p_dest = p_src[6] + 0.5 * p_src[1] + p_src[3] / 6;
            p_dest++;
            *p_dest = p_src[2] / 6 +  p_src[4];
            p_dest++;
            *p_dest = p_src[3] / 6 +  p_src[5];
            p_dest++;

            p_src += 7;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }
        else
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[4] + 0.5 * p_src[0];
            p_dest++;
            *p_dest = p_src[4] + 0.5 * p_src[1];
            p_dest++;
            *p_dest = p_src[2];
            p_dest++;
            *p_dest = p_src[3];
            p_dest++;

            p_src += 5;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }

    }
}
