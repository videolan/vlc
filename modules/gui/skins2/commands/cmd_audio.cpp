/*****************************************************************************
 * cmd_audio.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#include "cmd_audio.hpp"
#include <vlc/aout.h>
#include "aout_internal.h"
#include <string>

void CmdSetEqualizer::execute()
{
    // Get the audio output
    aout_instance_t *pAout = (aout_instance_t *)vlc_object_find( getIntf(),
        VLC_OBJECT_AOUT, FIND_ANYWHERE );

    // XXX
    string filters;
    if( m_enable)
    {
        filters = "equalizer";
    }

    if( pAout )
    {
        var_SetString( pAout, "audio-filter", (char*)filters.c_str() );
        for( int i = 0; i < pAout->i_nb_inputs; i++ )
        {
            pAout->pp_inputs[i]->b_restart = VLC_TRUE;
        }
        vlc_object_release( pAout );
    }
    else
    {
        config_PutPsz( getIntf(), "audio-filter", filters.c_str() );
    }
}

