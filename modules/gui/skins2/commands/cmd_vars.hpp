/*****************************************************************************
 * cmd_vars.hpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#ifndef CMD_VARS_HPP
#define CMD_VARS_HPP

#include "cmd_generic.hpp"
#include "../utils/ustring.hpp"

class EqualizerBands;
class EqualizerPreamp;
class VarText;

/// Command to notify the playlist of a change
DEFINE_COMMAND( NotifyPlaylist, "notify playlist" )

/// Command to notify the playlist of a change
DEFINE_COMMAND( PlaytreeChanged, "playtree changed" )

/// Command to notify the playtree of an item update
class CmdPlaytreeUpdate: public CmdGeneric
{
    public:
        CmdPlaytreeUpdate( intf_thread_t *pIntf, int id ):
            CmdGeneric( pIntf ), m_id( id ) {}
        virtual ~CmdPlaytreeUpdate() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree update"; }

        /// Only accept removal of command if they concern the same item
        virtual bool checkRemove( CmdGeneric * ) const;

    private:
        /// Playlist item ID
        int m_id;
};

/// Command to notify the playtree of an item append
class CmdPlaytreeAppend: public CmdGeneric
{
    public:
        CmdPlaytreeAppend( intf_thread_t *pIntf, playlist_add_t *p_add ) :
            CmdGeneric( pIntf ), m_pAdd( p_add ) {}
        virtual ~CmdPlaytreeAppend() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree append"; }

    private:
        playlist_add_t * m_pAdd;
};

/// Command to notify the playtree of an item deletion
class CmdPlaytreeDelete: public CmdGeneric
{
    public:
        CmdPlaytreeDelete( intf_thread_t *pIntf, int i_id ) :
            CmdGeneric( pIntf ), m_id( i_id ) {}
        virtual ~CmdPlaytreeDelete() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree append"; }

    private:
        int m_id;
};




/// Command to set a text variable
class CmdSetText: public CmdGeneric
{
    public:
        CmdSetText( intf_thread_t *pIntf, VarText &rText,
                    const UString &rValue ):
            CmdGeneric( pIntf ), m_rText( rText ), m_value( rValue ) {}
        virtual ~CmdSetText() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "set text"; }

    private:
        /// Text variable to set
        VarText &m_rText;
        /// Value to set
        const UString m_value;
};


/// Command to set the equalizer preamp
class CmdSetEqPreamp: public CmdGeneric
{
    public:
        CmdSetEqPreamp( intf_thread_t *pIntf, EqualizerPreamp &rPreamp,
                       float value ):
            CmdGeneric( pIntf ), m_rPreamp( rPreamp ), m_value( value ) {}
        virtual ~CmdSetEqPreamp() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "set equalizer preamp"; }

    private:
        /// Preamp variable to set
        EqualizerPreamp &m_rPreamp;
        /// Value to set
        float m_value;
};


/// Command to set the equalizerbands
class CmdSetEqBands: public CmdGeneric
{
    public:
        CmdSetEqBands( intf_thread_t *pIntf, EqualizerBands &rEqBands,
                       const string &rValue ):
            CmdGeneric( pIntf ), m_rEqBands( rEqBands ), m_value( rValue ) {}
        virtual ~CmdSetEqBands() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "set equalizer bands"; }

    private:
        /// Equalizer variable to set
        EqualizerBands &m_rEqBands;
        /// Value to set
        const string m_value;
};


#endif
