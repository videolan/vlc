/*****************************************************************************
 * interpreter.hpp
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

#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include "../commands/cmd_generic.hpp"
#include <map>

class Theme;
class VarBool;
class VarList;
class VarTree;
class VarPercent;


/// Command interpreter for scripts in the XML
class Interpreter: public SkinObject
{
public:
    /// Get the instance of Interpreter
    static Interpreter *instance( intf_thread_t *pIntf );

    /// Delete the instance of Interpreter
    static void destroy( intf_thread_t *pIntf );

    /// Parse an action tag and returns a pointer on a command
    /// (the intepreter takes care of deleting it, don't do it
    ///  yourself !)
    CmdGeneric *parseAction( const std::string &rAction, Theme *pTheme );

    /// Returns the boolean variable corresponding to the given name
    VarBool *getVarBool( const std::string &rName, Theme *pTheme );


    /// Returns the percent variable corresponding to the given name
    VarPercent *getVarPercent( const std::string &rName, Theme *pTheme );

    /// Returns the list variable corresponding to the given name
    VarList *getVarList( const std::string &rName, Theme *pTheme );

    /// Returns the tree variable corresponding to the given name
    VarTree *getVarTree( const std::string &rName, Theme *pTheme );

    /// Get a constant value
    std::string getConstant( const std::string &rValue );

private:
    /// Map of global commands
    std::map<std::string, CmdGenericPtr> m_commandMap;

    // Private because it is a singleton
    Interpreter( intf_thread_t *pIntf );
    virtual ~Interpreter() { }
};

#endif
