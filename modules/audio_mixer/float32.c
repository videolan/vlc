/*****************************************************************************
 * float32.c : precise float32 audio volume implementation
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

#include <stddef.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_aout_volume.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Create( vlc_object_t * );
static void DoWork( audio_volume_t *, block_t *, float );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_description( N_("Single precision audio volume") )
    set_capability( "audio volume", 10 )
    set_callbacks( Create, NULL )
vlc_module_end ()

/**
 * Initializes the mixer
 */
static int Create( vlc_object_t *p_this )
{
    audio_volume_t *p_volume = (audio_volume_t *)p_this;

    if (p_volume->format != VLC_CODEC_FL32)
        return -1;

    p_volume->amplify = DoWork;
    return 0;
}

/**
 * Mixes a new output buffer
 */
static void DoWork( audio_volume_t *p_volume, block_t *p_buffer,
                    float f_multiplier )
{
    if( f_multiplier == 1.0 )
        return; /* nothing to do */

    float *p = (float *)p_buffer->p_buffer;
    for( size_t i = p_buffer->i_buffer / sizeof(float); i > 0; i-- )
        *(p++) *= f_multiplier;

    (void) p_volume;
}
