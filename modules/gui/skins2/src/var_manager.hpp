/*****************************************************************************
 * var_manager.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: var_manager.hpp,v 1.1 2004/01/03 23:31:34 asmax Exp $
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

#ifndef VAR_MANAGER_HPP
#define VAR_MANAGER_HPP

#include "../utils/var_text.hpp"


class VarManager: public SkinObject
{
    public:
        /// Get the instance of VarManager
        static VarManager *instance( intf_thread_t *pIntf );

        /// Delete the instance of VarManager
        static void destroy( intf_thread_t *pIntf );

        /// Get the tooltip text variable
        VarText &getTooltipText() { return m_tooltip; }

        /// Get the help text variable
        VarText &getHelpText() { return m_help; }

    private:
        /// Tooltip text
        VarText m_tooltip;
        /// Help text
        VarText m_help;

        /// Private because it is a singleton
        VarManager( intf_thread_t *pIntf );
        virtual ~VarManager() {}
};


#endif
