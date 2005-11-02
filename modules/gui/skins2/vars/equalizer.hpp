/*****************************************************************************
 * equalizer.hpp
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef EQUALIZER_HPP
#define EQUALIZER_HPP

#include "../utils/var_percent.hpp"
#include <string>


/// Variable for graphical equalizer
class EqualizerBands: public SkinObject, public Observer<VarPercent>
{
    public:
        /// Number of bands
        static const int kNbBands = 10;

        EqualizerBands( intf_thread_t *pIntf );
        virtual ~EqualizerBands();

        /// Set the equalizer bands from a configuration string,
        /// e.g. "1 5.2 -3.6 0 0 2.5 0 0 0 0"
        void set( string bands );

        /// Return the variable for a specific band
        VariablePtr getBand( int band );

    private:
        /// Array of equalizer bands
        VariablePtr m_cBands[kNbBands];
        /// Flag set when an update is in progress
        bool m_isUpdating;

        /// Callback for band updates
        virtual void onUpdate( Subject<VarPercent> &rBand );
};


#endif
