/*****************************************************************************
 * cmd_generic.hpp
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

#ifndef CMD_GENERIC_HPP
#define CMD_GENERIC_HPP

#include <string>

#include "../src/skin_common.hpp"
#include "../utils/pointer.hpp"


/// Macro to define the prototype of simple commands
#define DEFINE_COMMAND( name, type )                           \
class Cmd##name: public CmdGeneric                             \
{   public:                                                    \
    Cmd##name( intf_thread_t *pIntf ): CmdGeneric( pIntf ) { } \
    virtual ~Cmd##name() { }                                   \
    virtual void execute();                                    \
    virtual string getType() const { return type; }            \
};


/// Macro to define a "callback" command inside a class
#define DEFINE_CALLBACK( parent, action )                            \
class Cmd##action: public CmdGeneric                                 \
{                                                                    \
public:                                                              \
    Cmd##action( parent *pParent ):                                  \
        CmdGeneric( pParent->getIntf() ), m_pParent( pParent ) { }   \
    virtual ~Cmd##action() { }                                       \
    virtual void execute();                                          \
    virtual string getType() const { return "Cmd" #parent #action; } \
private:                                                             \
    parent *m_pParent;                                               \
} m_cmd##action;                                                     \
friend class Cmd##action;


/// Base class for skins commands
class CmdGeneric: public SkinObject
{
public:
    virtual ~CmdGeneric() { }

    /// This method does the real job of the command
    virtual void execute() = 0;

    /// Return the type of the command
    virtual string getType() const { return ""; }

    /// During queue reductions, check if we really want to remove
    /// this command.
    virtual bool checkRemove( CmdGeneric * ) const { return true; }

protected:
    CmdGeneric( intf_thread_t *pIntf ): SkinObject( pIntf ) { }
};


typedef CountedPtr<CmdGeneric> CmdGenericPtr;


#endif
