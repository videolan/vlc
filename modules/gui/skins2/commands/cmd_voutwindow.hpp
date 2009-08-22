/*****************************************************************************
 * cmd_voutwindow.hpp
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Author: Erwan Tulou      <erwan10 aT videolan doT org >
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

#ifndef CMD_VOUTWINDOW_HPP
#define CMD_VOUTWINDOW_HPP

#include "cmd_generic.hpp"
#include <vlc_vout_window.h>


/// Command to create a vout window
class CmdNewVoutWindow: public CmdGeneric
{
    public:
        /// Create a vout window
        CmdNewVoutWindow( intf_thread_t *pIntf, vout_window_t *pWnd );
        virtual ~CmdNewVoutWindow() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "new vout window"; }

    private:
        vout_window_t* m_pWnd;
};


/// Command to release a vout window
class CmdReleaseVoutWindow: public CmdGeneric
{
    public:
        /// Release a vout window
        CmdReleaseVoutWindow( intf_thread_t *pIntf, vout_window_t *pWnd );
        virtual ~CmdReleaseVoutWindow() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "new vout window"; }

    private:
        vout_window_t* m_pWnd;
};

#endif
