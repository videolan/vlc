/*****************************************************************************
 * aout_dummy.c : dummy audio output plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: aout.c,v 1.3 2002/08/12 22:12:51 massiot Exp $
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
#include <string.h>
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>

#include "aout_internal.h"

#define FRAME_SIZE 2048

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     SetFormat   ( aout_instance_t * );
static void    Play        ( aout_instance_t *, aout_buffer_t * );

/*****************************************************************************
 * OpenAudio: open a dummy audio device
 *****************************************************************************/
int E_(OpenAudio) ( vlc_object_t * p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    p_aout->output.pf_setformat = SetFormat;
    p_aout->output.pf_play = Play;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SetFormat: pretend to set the dsp output format
 *****************************************************************************/
static int SetFormat( aout_instance_t * p_aout )
{
    p_aout->output.i_nb_samples = FRAME_SIZE;
    return 0;
}

/*****************************************************************************
 * Play: pretend to play a sound
 *****************************************************************************/
static void Play( aout_instance_t * p_aout, aout_buffer_t * p_buffer )
{
    aout_BufferFree( p_buffer );
}

