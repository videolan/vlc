/*****************************************************************************
 * spdif.c : dummy mixer for S/PDIF output (1 input only)
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: spdif.c,v 1.2 2002/08/12 07:40:23 massiot Exp $
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
#include <errno.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );

static void DoWork    ( aout_instance_t *, aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("dummy spdif audio mixer module") );
    set_capability( "audio mixer", 1 );
    add_shortcut( "spdif" );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate spdif mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    if ( p_aout->mixer.output.i_format != AOUT_FMT_SPDIF )
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
    aout_input_t * p_input = p_aout->pp_inputs[0];
    aout_FifoPop( p_aout, &p_input->fifo );
}

