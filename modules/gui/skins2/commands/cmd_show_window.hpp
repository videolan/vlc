/*****************************************************************************
 * cmd_show_window.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: cmd_show_window.hpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#ifndef CMD_SHOW_WINDOW_HPP
#define CMD_SHOW_WINDOW_HPP

#include "cmd_generic.hpp"
#include "../utils/var_bool.hpp"


template<bool newValue> class CmdShowHideWindow;

typedef CmdShowHideWindow<true> CmdShowWindow;
typedef CmdShowHideWindow<false> CmdHideWindow;


/// "Show/Hide window" command
template<bool newValue>
class CmdShowHideWindow: public CmdGeneric
{
    public:
        CmdShowHideWindow( intf_thread_t *pIntf, VarBool &rVariable ):
            CmdGeneric( pIntf ), m_rVariable( rVariable ) {}
        virtual ~CmdShowHideWindow() {}

        /// This method does the real job of the command
        virtual void execute() { m_rVariable.set( newValue ); }

        /// Return the type of the command
        virtual string getType() const { return "show/hide window"; }

    private:
        /// Reference to the observed variable
        VarBool &m_rVariable;
};

#endif
