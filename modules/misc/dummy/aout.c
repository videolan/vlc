/*****************************************************************************
 * aout.c : dummy audio output plugin
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
#include <vlc_aout.h>
#include <vlc_cpu.h>

#include "dummy.h"

#define FRAME_SIZE 2048
#define A52_FRAME_NB 1536

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void    Play        ( aout_instance_t * );

/*****************************************************************************
 * OpenAudio: open a dummy audio device
 *****************************************************************************/
int OpenAudio ( vlc_object_t * p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    p_aout->output.pf_play = Play;
    p_aout->output.pf_pause = NULL;
    aout_VolumeSoftInit( p_aout );

    if( AOUT_FMT_NON_LINEAR( &p_aout->output.output )
     && var_InheritBool( p_this, "spdif" ) )
    {
        p_aout->output.output.i_format = VLC_CODEC_SPDIFL;
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->output.output.i_frame_length = A52_FRAME_NB;
    }
    else
        p_aout->output.output.i_format =
            HAVE_FPU ? VLC_CODEC_FL32 : VLC_CODEC_S16N;
    p_aout->output.i_nb_samples = A52_FRAME_NB;

    /* Create the variable for the audio-device */
    var_Create( p_aout, "audio-device", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Play: pretend to play a sound
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
    aout_buffer_t * p_buffer = aout_FifoPop( &p_aout->output.fifo );
    aout_BufferFree( p_buffer );
}

