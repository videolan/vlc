/*****************************************************************************
 * cmd_audio.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef CMD_AUDIO_HPP
#define CMD_AUDIO_HPP

#include "cmd_generic.hpp"

/// Command to enable/disable the equalizer
class CmdSetEqualizer: public CmdGeneric
{
public:
    CmdSetEqualizer( intf_thread_t *pIntf, bool iEnable )
                   : CmdGeneric( pIntf ), m_enable( iEnable ) { }
    virtual ~CmdSetEqualizer() { }
    virtual void execute();
    virtual std::string getType() const { return "set equalizer"; }

private:
    /// Enable or disable the equalizer
    bool m_enable;
};


#endif
