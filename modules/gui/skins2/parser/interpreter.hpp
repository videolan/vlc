/*****************************************************************************
 * interpreter.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: interpreter.hpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include "../commands/cmd_generic.hpp"

class Theme;
class VarBool;
class VarList;
class VarPercent;


/// Command interpreter for scripts in the XML
class Interpreter: public SkinObject
{
    public:
        Interpreter( intf_thread_t *pIntf );
        virtual ~Interpreter() {}

        /// Parse an action tag and returns a pointer on a command
        /// (the intepreter takes care of deleting it, don't do it
        ///  yourself !)
        CmdGeneric *parseAction( const string &rAction, Theme *pTheme );

        /// Returns the boolean variable corresponding to the given name
        VarBool *getVarBool( const string &rName, Theme *pTheme );


        /// Returns the percent variable corresponding to the given name
        VarPercent *getVarPercent( const string &rName, Theme *pTheme );

        /// Returns the list variable corresponding to the given name
        VarList *getVarList( const string &rName, Theme *pTheme );
};

#endif
