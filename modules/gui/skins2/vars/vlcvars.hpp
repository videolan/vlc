/*****************************************************************************
 * vlcvars.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: vlcvars.hpp,v 1.1 2004/01/03 23:31:34 asmax Exp $
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef VLCVARS_HPP
#define VLCVARS_HPP

#include "../utils/var_bool.hpp"


class VlcIsMute: public VarBool
{
    public:
        VlcIsMute( intf_thread_t *pIntf ): VarBool( pIntf ) {}
        virtual ~VlcIsMute() {}

        virtual void set( bool value );
};


class VlcIsPlaying: public VarBool
{
    public:
        VlcIsPlaying( intf_thread_t *pIntf ): VarBool( pIntf ) {}
        virtual ~VlcIsPlaying() {}

        virtual void set( bool value, bool updateVLC );

        virtual void set( bool value ) { set( value, true ); }
};


class VlcIsSeekablePlaying: public VarBool
{
    public:
        VlcIsSeekablePlaying( intf_thread_t *pIntf ): VarBool( pIntf ) {}
        virtual ~VlcIsSeekablePlaying() {}
};


#endif
