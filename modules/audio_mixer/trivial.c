/*****************************************************************************
 * trivial.c : trivial mixer plug-in (1 input, no downmixing)
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: trivial.c,v 1.7 2002/09/20 23:27:03 massiot Exp $
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
    set_description( _("trivial audio mixer module") );
    set_capability( "audio mixer", 1 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate trivial mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    if ( p_aout->mixer.mixer.i_format != AOUT_FMT_FLOAT32
          && p_aout->mixer.mixer.i_format != AOUT_FMT_FIXED32 )
    {
        return -1;
    }

    p_aout->mixer.pf_do_work = DoWork;

    return 0;
}

/*****************************************************************************
 * DoWork: mix a new output buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_buffer_t * p_buffer )
{
    aout_input_t * p_input = p_aout->pp_inputs[0];
    int i_nb_bytes = p_buffer->i_nb_samples * sizeof(s32)
                      * p_aout->mixer.mixer.i_channels;
    byte_t * p_in = p_input->p_first_byte_to_mix;
    byte_t * p_out = p_buffer->p_buffer;

    if ( p_input->b_error ) return;

    for ( ; ; )
    {
        ptrdiff_t i_available_bytes = (p_input->fifo.p_first->p_buffer
                                        - p_in)
                                        + p_input->fifo.p_first->i_nb_samples
                                           * sizeof(s32)
                                           * p_aout->mixer.mixer.i_channels;

        if ( i_available_bytes < i_nb_bytes )
        {
            aout_buffer_t * p_old_buffer;

            if ( i_available_bytes > 0 )
                p_aout->p_vlc->pf_memcpy( p_out, p_in, i_available_bytes );
            i_nb_bytes -= i_available_bytes;
            p_out += i_available_bytes;

            /* Next buffer */
            p_old_buffer = aout_FifoPop( p_aout, &p_input->fifo );
            aout_BufferFree( p_old_buffer );
            if ( p_input->fifo.p_first == NULL )
            {
                msg_Err( p_aout, "internal amix error" );
                return;
            }
            p_in = p_input->fifo.p_first->p_buffer;
        }
        else
        {
            if ( i_nb_bytes > 0 )
                p_aout->p_vlc->pf_memcpy( p_out, p_in, i_nb_bytes );
            p_input->p_first_byte_to_mix = p_in + i_nb_bytes;
            break;
        }
    }
}

