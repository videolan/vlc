/*****************************************************************************
 * volume.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VOLUME_HPP
#define VOLUME_HPP


#include "../utils/var_percent.hpp"
#include <string>


/// Variable for VLC volume
class Volume: public VarPercent
{
public:
    Volume( intf_thread_t *pIntf );
    virtual ~Volume() { }

    virtual void set( float percentage, bool updateVLC );
    virtual void set( float percentage ) { set( percentage, true ); }

    virtual float getVolume() const;
    virtual void setVolume( float volume, bool updateVLC );

    virtual float getStep() const { return m_step; }

    virtual std::string getAsStringPercent() const;

private:
    // preferred volume step on [0., 1.]
    float m_step;
};


#endif
