/*****************************************************************************
 * spdif.c : dummy mixer for S/PDIF output (1 input only)
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
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
#include <vlc_plugin.h>
#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );

static void DoWork    ( aout_instance_t *, aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_description( N_("Dummy S/PDIF audio mixer") );
    set_capability( "audio mixer", 1 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate spdif mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    if ( !AOUT_FMT_NON_LINEAR(&p_aout->mixer.mixer) )
    {
        return -1;
    }

    p_aout->mixer.pf_do_work = DoWork;
    /* This is a bit kludgy - do not ask for a new buffer, since the one
     * provided by the first input will be good enough. */
    p_aout->mixer.output_alloc.i_alloc_type = AOUT_ALLOC_NONE;

    return 0;
}

/*****************************************************************************
 * DoWork: mix a new output buffer - this does nothing, indeed
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_buffer_t * p_buffer )
{
    int i = 0;
    aout_input_t * p_input = p_aout->pp_inputs[i];
    while ( p_input->b_error )
    {
        p_input = p_aout->pp_inputs[++i];
    }
    aout_FifoPop( p_aout, &p_input->fifo );

    /* Empty other FIFOs to avoid a memory leak. */
    for ( i++; i < p_aout->i_nb_inputs; i++ )
    {
        aout_fifo_t * p_fifo;
        aout_buffer_t * p_deleted;

        p_input = p_aout->pp_inputs[i];
        if ( p_input->b_error ) continue;
        p_fifo = &p_input->fifo;
        p_deleted = p_fifo->p_first;
        while ( p_deleted != NULL )
        {
            aout_buffer_t * p_next = p_deleted->p_next;
            aout_BufferFree( p_deleted );
            p_deleted = p_next;
        }
        p_fifo->p_first = NULL;
        p_fifo->pp_last = &p_fifo->p_first;
    }
}

