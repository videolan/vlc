/*****************************************************************************
 * cmd_resize.hpp
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

#ifndef CMD_RESIZE_HPP
#define CMD_RESIZE_HPP

#include "cmd_generic.hpp"

class GenericLayout;


/// Command to resize a layout
class CmdResize: public CmdGeneric
{
    public:
        /// Resize the given layout
        CmdResize( intf_thread_t *pIntf, GenericLayout &rLayout, int width,
                   int height );
        virtual ~CmdResize() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "resize"; }

    private:
        GenericLayout &m_rLayout;
        int m_width, m_height;
};


/// Command to resize the vout window
class CmdResizeVout: public CmdGeneric
{
    public:
        /// Resize the given layout
        CmdResizeVout( intf_thread_t *pIntf, void *pWindow, int width,
                       int height );
        virtual ~CmdResizeVout() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "resize vout"; }

    private:
        void *m_pWindow;
        int m_width, m_height;
};


#endif
