/*****************************************************************************
 * cmd_playtree.hpp
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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

#ifndef CMD_PLAYTREE_HPP
#define CMD_PLAYTREE_HPP

#include "cmd_generic.hpp"
#include "../utils/var_tree.hpp"

// TODO : implement branch specific stuff

/// Command to delete the selected items from a tree
class CmdPlaytreeDel: public CmdGeneric
{
    public:
        CmdPlaytreeDel( intf_thread_t *pIntf, VarTree &rTree ):
            CmdGeneric( pIntf ), m_rTree( rTree ) {}
        virtual ~CmdPlaytreeDel() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree del"; }

    private:
        /// Tree
        VarTree &m_rTree;
};

/// Command to sort the playtree
DEFINE_COMMAND( PlaytreeSort, "playtree sort" )

/// Command to jump to the next item
DEFINE_COMMAND( PlaytreeNext, "playtree next" )

/// Command to jump to the previous item
DEFINE_COMMAND( PlaytreePrevious, "playtree previous" )

/// Command to set the random state
class CmdPlaytreeRandom: public CmdGeneric
{
    public:
        CmdPlaytreeRandom( intf_thread_t *pIntf, bool value ):
            CmdGeneric( pIntf ), m_value( value ) {}
        virtual ~CmdPlaytreeRandom() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree random"; }

    private:
        /// Random state
        bool m_value;
};

/// Command to set the loop state
class CmdPlaytreeLoop: public CmdGeneric
{
    public:
        CmdPlaytreeLoop( intf_thread_t *pIntf, bool value ):
            CmdGeneric( pIntf ), m_value( value ) {}
        virtual ~CmdPlaytreeLoop() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree loop"; }

    private:
        /// Loop state
        bool m_value;
};

/// Command to set the repeat state
class CmdPlaytreeRepeat: public CmdGeneric
{
    public:
        CmdPlaytreeRepeat( intf_thread_t *pIntf, bool value ):
            CmdGeneric( pIntf ), m_value( value ) {}
        virtual ~CmdPlaytreeRepeat() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree repeat"; }

    private:
        /// Loop state
        bool m_value;
};

/// Command to load a playlist
class CmdPlaytreeLoad: public CmdGeneric
{
    public:
        CmdPlaytreeLoad( intf_thread_t *pIntf, bool value ):
            CmdGeneric( pIntf ), m_value( value ) {}
        virtual ~CmdPlaytreeLoad() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree load"; }

    private:
        /// Loop state
        bool m_value;
};

/// Command to save a playlist
class CmdPlaytreeSave: public CmdGeneric
{
    public:
        CmdPlaytreeSave( intf_thread_t *pIntf, bool value ):
            CmdGeneric( pIntf ), m_value( value ) {}
        virtual ~CmdPlaytreeSave() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree save"; }

    private:
        /// Loop state
        bool m_value;
};

#endif
