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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );

static void DoWork    ( aout_mixer_t *, aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_description( N_("Dummy S/PDIF audio mixer") )
    set_capability( "audio mixer", 1 )
    set_callbacks( Create, NULL )
vlc_module_end ()

/*****************************************************************************
 * Create: allocate spdif mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_mixer_t *p_mixer = (aout_mixer_t *)p_this;

    if ( !AOUT_FMT_NON_LINEAR(&p_mixer->fmt) )
    {
        return -1;
    }

    p_mixer->mix = DoWork;
    /* This is a bit kludgy - do not ask for a new buffer, since the one
     * provided by the first input will be good enough. */
    p_mixer->allocation.b_alloc = false;

    return 0;
}

/*****************************************************************************
 * DoWork: mix a new output buffer - this does nothing, indeed
 *****************************************************************************/
static void DoWork( aout_mixer_t * p_mixer, aout_buffer_t * p_buffer )
{
    VLC_UNUSED( p_buffer );

    unsigned i = 0;
    aout_mixer_input_t * p_input = p_mixer->input[i];
    while ( p_input->is_invalid )
        p_input = p_mixer->input[++i];

    aout_buffer_t * p_old_buffer = aout_FifoPop( NULL, &p_input->fifo );
    assert( p_old_buffer == p_buffer );

    /* Empty other FIFOs to avoid a memory leak. */
    for ( i++; i < p_mixer->input_count; i++ )
    {
        p_input = p_mixer->input[i];
        if ( p_input->is_invalid )
            continue;
        while ((p_old_buffer = aout_FifoPop( NULL, &p_input->fifo )))
            aout_BufferFree( p_old_buffer );
    }
}

