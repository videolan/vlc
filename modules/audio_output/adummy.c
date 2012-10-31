/*****************************************************************************
 * adummy.c : dummy audio output plugin
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_cpu.h>

static int Open( vlc_object_t * p_this );

vlc_module_begin ()
    set_shortname( N_("Dummy") )
    set_description( N_("Dummy audio output") )
    set_capability( "audio output", 0 )
    set_callbacks( Open, NULL )
    add_shortcut( "dummy" )
vlc_module_end ()

#define A52_FRAME_NB 1536

static void Play( audio_output_t *aout, block_t *block, mtime_t *drift )
{
    block_Release( block );
    (void) aout;
    (void) drift;
}

static int Open( vlc_object_t * p_this )
{
    audio_output_t * p_aout = (audio_output_t *)p_this;

    p_aout->play = Play;
    p_aout->pause = NULL;
    p_aout->flush = NULL;
    p_aout->volume_set = NULL;
    p_aout->mute_set = NULL;

    if( AOUT_FMT_SPDIF( &p_aout->format )
     && var_InheritBool( p_this, "spdif" ) )
    {
        p_aout->format.i_format = VLC_CODEC_SPDIFL;
        p_aout->format.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->format.i_frame_length = A52_FRAME_NB;
    }
    else
        p_aout->format.i_format = HAVE_FPU ? VLC_CODEC_FL32 : VLC_CODEC_S16N;

    return VLC_SUCCESS;
}
