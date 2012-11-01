/*****************************************************************************
 * equalizer.cpp
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_aout.h>
#include "equalizer.hpp"
#include "../utils/var_percent.hpp"
#include <ios>
#include <iomanip>
#include <sstream>

EqualizerBands::EqualizerBands( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_isUpdating( false )
{
    for( int i = 0; i < kNbBands; i++ )
    {
        // Create and observe the band variables
        VarPercent *pVar = new VarPercent( pIntf );
        m_cBands[i] = VariablePtr( pVar );
        pVar->set( 0.5f );
        pVar->addObserver( this );
    }
}


EqualizerBands::~EqualizerBands()
{
    for( int i = 0; i < kNbBands; i++ )
    {
        ((VarPercent*)m_cBands[i].get())->delObserver( this );
    }
}


void EqualizerBands::set( string bands )
{
    float val;
    stringstream ss( bands );

    m_isUpdating = true;
    // Parse the string
    for( int i = 0; i < kNbBands; i++ )
    {
        ss >> val;
        // Set the band value in percent
        ((VarPercent*)m_cBands[i].get())->set( (val + 20) / 40 );
    }
    m_isUpdating = false;
}


VariablePtr EqualizerBands::getBand( int band )
{
    return m_cBands[band];
}


void EqualizerBands::onUpdate( Subject<VarPercent> &rBand, void *arg )
{
    (void)rBand; (void)arg;
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    audio_output_t *pAout = playlist_GetAout( pPlaylist );

    // Make sure we are not called from set()
    if (!m_isUpdating)
    {
        float val;
        stringstream ss;
        // Write one digit after the floating point
        ss << setprecision( 1 ) << setiosflags( ios::fixed );

        // Convert the band values to a string
        val = 40 * ((VarPercent*)m_cBands[0].get())->get() - 20;
        ss << val;
        for( int i = 1; i < kNbBands; i++ )
        {
            val = 40 * ((VarPercent*)m_cBands[i].get())->get() - 20;
            ss << " " << val;
        }

        string bands = ss.str();

        config_PutPsz( getIntf(), "equalizer-bands", bands.c_str() );
        if( pAout )
        {
            // Update the audio output
            var_SetString( pAout, "equalizer-bands", (char*)bands.c_str() );
        }
    }

    if( pAout )
        vlc_object_release( pAout );
}


EqualizerPreamp::EqualizerPreamp( intf_thread_t *pIntf ): VarPercent( pIntf )
{
    // Initial value
    VarPercent::set( 0.8 );
}


void EqualizerPreamp::set( float percentage, bool updateVLC )
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    audio_output_t *pAout = playlist_GetAout( pPlaylist );

    VarPercent::set( percentage );

    // Avoid infinite loop
    if( updateVLC )
    {
        float val = 40 * percentage - 20;

        config_PutFloat( getIntf(), "equalizer-preamp", val );
        if( pAout )
        {
            // Update the audio output
            var_SetFloat( pAout, "equalizer-preamp", val );
        }
    }

    if( pAout )
        vlc_object_release( pAout );
}
