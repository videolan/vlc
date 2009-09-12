/*****************************************************************************
 * var_manager.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VAR_MANAGER_HPP
#define VAR_MANAGER_HPP

#include "../utils/var_text.hpp"
#include <list>
#include <map>


class VarManager: public SkinObject
{
public:
    /// Get the instance of VarManager
    static VarManager *instance( intf_thread_t *pIntf );

    /// Delete the instance of VarManager
    static void destroy( intf_thread_t *pIntf );

    /// Register a named variable in the manager
    void registerVar( const VariablePtr &rcVar, const string &rName );

    /// Register an anonymous variable in the manager
    void registerVar( const VariablePtr &rcVar );

    /// Get a variable by its name (NULL if not found)
    Variable *getVar( const string &rName );

    /// Get a variable by its name and check the type (NULL if not found)
    Variable *getVar( const string &rName, const string &rType );

    /// Get the tooltip text variable
    VarText &getTooltipText() { return *m_pTooltipText; }

    /// Get the help text variable
    VarText &getHelpText() { return *m_pHelpText; }

    /// Register a constant value
    void registerConst( const string &rName, const string &rValue);

    /// Get a constant value by its name
    string getConst( const string &rName );

private:
    /// Tooltip text
    VarText *m_pTooltipText;
    /// Help text
    VarText *m_pHelpText;
    /// Map of named registered variables
    map<string, VariablePtr> m_varMap;
    /// List of named registed variables
    list<string> m_varList;
    /// List of anonymous registed variables
    list<VariablePtr> m_anonVarList;
    /// Map of constant values
    map<string, string> m_constMap;

    /// Private because it is a singleton
    VarManager( intf_thread_t *pIntf );
    virtual ~VarManager();
};


#endif
