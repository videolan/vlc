/*****************************************************************************
 * cmd_input.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef CMD_INPUT_HPP
#define CMD_INPUT_HPP

#include "cmd_generic.hpp"

/// Commands to control the input
DEFINE_COMMAND( Play, "play" )
DEFINE_COMMAND( Pause, "pause" )
DEFINE_COMMAND( Stop, "stop" )
DEFINE_COMMAND( Slower, "slower" )
DEFINE_COMMAND( Faster, "faster" )
DEFINE_COMMAND( Mute, "mute" )
DEFINE_COMMAND( VolumeUp, "volume up" )
DEFINE_COMMAND( VolumeDown, "volume down" )


#endif
