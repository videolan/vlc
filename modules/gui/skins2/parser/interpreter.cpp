/*****************************************************************************
 * interpreter.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: interpreter.cpp,v 1.3 2004/01/11 17:12:17 asmax Exp $
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

#include "interpreter.hpp"
#include "../commands/cmd_playlist.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_dummy.hpp"
#include "../commands/cmd_layout.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_input.hpp"
#include "../commands/cmd_fullscreen.hpp"
#include "../src/theme.hpp"
#include "../src/var_manager.hpp"
#include "../src/vlcproc.hpp"
#include "../vars/playlist.hpp"
#include "../vars/vlcvars.hpp"
#include "../vars/time.hpp"
#include "../vars/volume.hpp"


Interpreter::Interpreter( intf_thread_t *pIntf ): SkinObject( pIntf )
{
    /// Create the generic commands
#define REGISTER_CMD( name, cmd ) \
    m_commandMap[name] = CmdGenericPtr( new cmd( getIntf() ) );

    REGISTER_CMD( "none", CmdDummy )
    REGISTER_CMD( "dialogs.changeSkin()", CmdDlgChangeSkin )
    REGISTER_CMD( "dialogs.fileSimple()", CmdDlgFileSimple )
    REGISTER_CMD( "dialogs.file()", CmdDlgFile )
    REGISTER_CMD( "dialogs.disc()", CmdDlgDisc )
    REGISTER_CMD( "dialogs.net()", CmdDlgNet )
    REGISTER_CMD( "dialogs.messages()", CmdDlgMessages )
    REGISTER_CMD( "dialogs.prefs()", CmdDlgPrefs )
    REGISTER_CMD( "dialogs.fileInfo()", CmdDlgFileInfo )
    REGISTER_CMD( "dialogs.popup()", CmdDlgPopupMenu )
    REGISTER_CMD( "playlist.add()", CmdDlgAdd )
    VarList &rVar = VlcProc::instance( getIntf() )->getPlaylistVar();
    m_commandMap["playlist.del()"] =
        CmdGenericPtr( new CmdPlaylistDel( getIntf(), rVar ) );
    REGISTER_CMD( "playlist.next()", CmdPlaylistNext )
    REGISTER_CMD( "playlist.previous()", CmdPlaylistPrevious )
    REGISTER_CMD( "playlist.sort()", CmdPlaylistSort )
    REGISTER_CMD( "vlc.fullscreen()", CmdFullscreen )
    REGISTER_CMD( "vlc.quit()", CmdQuit )
    REGISTER_CMD( "vlc.faster()", CmdFaster )
    REGISTER_CMD( "vlc.slower()", CmdSlower )
    REGISTER_CMD( "vlc.stop()", CmdStop )
}


Interpreter *Interpreter::instance( intf_thread_t *pIntf )
{
    if( ! pIntf->p_sys->p_interpreter )
    {
        Interpreter *pInterpreter;
        pInterpreter = new Interpreter( pIntf );
        if( pInterpreter )
        {
            pIntf->p_sys->p_interpreter = pInterpreter;
        }
    }
    return pIntf->p_sys->p_interpreter;
}


void Interpreter::destroy( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_interpreter )
    {
        delete pIntf->p_sys->p_interpreter;
        pIntf->p_sys->p_interpreter = NULL;
    }
}


CmdGeneric *Interpreter::parseAction( const string &rAction, Theme *pTheme )
{
    // Try to find the command in the global command map
    if( m_commandMap.find( rAction ) != m_commandMap.end() )
    {
        return m_commandMap[rAction].get();
    }

    CmdGeneric *pCommand = NULL;

    if( rAction.find( ".setLayout(" ) != string::npos )
    {
        int leftPos = rAction.find( ".setLayout(" );
        string windowId = rAction.substr( 0, leftPos );
        // 11 is the size of ".setLayout("
        int rightPos = rAction.find( ")", windowId.size() + 11 );
        string layoutId = rAction.substr( windowId.size() + 11,
                                          rightPos - (windowId.size() + 11) );
        pCommand = new CmdLayout( getIntf(), windowId, layoutId );
    }

    if( pCommand )
    {
        // Add the command in the pool
        pTheme->m_commands.push_back( CmdGenericPtr( pCommand ) );
    }

    return pCommand;
}


VarBool *Interpreter::getVarBool( const string &rName, Theme *pTheme )
{
    // Try to get the variable from the variable manager
    VarManager *pVarManager = VarManager::instance( getIntf() );
    VarBool *pVar = (VarBool*)pVarManager->getVar( rName, "bool" );

    if( pVar )
    {
        return pVar;
    }
    else if( rName.find( ".isVisible" ) != string::npos )
    {
        int leftPos = rName.find( ".isVisible" );
        string windowId = rName.substr( 0, leftPos );
        // XXX Need to check the IDs (isalpha())?
        GenericWindow *pWin = pTheme->getWindowById( windowId );
        if( pWin )
        {
            return &pWin->getVisibleVar();
        }
        else
        {
            msg_Warn( getIntf(), "Unknown window (%s)", windowId.c_str() );
            return NULL;
        }
    }
    else
    {
        return NULL;
    }
}


VarPercent *Interpreter::getVarPercent( const string &rName, Theme *pTheme )
{
    // Try to get the variable from the variable manager
    VarManager *pVarManager = VarManager::instance( getIntf() );
    VarPercent *pVar = (VarPercent*)pVarManager->getVar( rName, "percent" );
    return pVar;
}


VarList *Interpreter::getVarList( const string &rName, Theme *pTheme )
{
    // Try to get the variable from the variable manager
    VarManager *pVarManager = VarManager::instance( getIntf() );
    VarList *pVar = (VarList*)pVarManager->getVar( rName, "list" );
    return pVar;
}

