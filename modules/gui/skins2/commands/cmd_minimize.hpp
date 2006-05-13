/*****************************************************************************
 * cmd_minimize.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Mohammed Adnène Trojette     <adn@via.ecp.fr>
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

#ifndef CMD_MINIMIZE_HPP
#define CMD_MINIMIZE_HPP

#include "cmd_generic.hpp"


/// Command to minimize VLC
DEFINE_COMMAND(Minimize, "minimize" )
DEFINE_COMMAND(Restore, "restore" )
DEFINE_COMMAND(AddInTray, "add in tray" )
DEFINE_COMMAND(RemoveFromTray, "remove from tray" )

#endif
