/*****************************************************************************
 * interpreter.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: interpreter.cpp,v 1.2 2004/01/05 22:17:32 asmax Exp $
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
#include "../src/vlcproc.hpp"
#include "../vars/playlist.hpp"
#include "../vars/vlcvars.hpp"
#include "../vars/time.hpp"
#include "../vars/volume.hpp"


Interpreter::Interpreter( intf_thread_t *pIntf ): SkinObject( pIntf )
{
}


CmdGeneric *Interpreter::parseAction( const string &rAction, Theme *pTheme )
{
    // XXX should not be so hardcoded!
    CmdGeneric *pCommand = NULL;
    if( rAction == "none" )
    {
        pCommand = new CmdDummy( getIntf() );
    }
    else if( rAction == "dialogs.changeSkin()" )
    {
        pCommand = new CmdDlgChangeSkin( getIntf() );
    }
    else if( rAction == "dialogs.fileSimple()" )
    {
        pCommand = new CmdDlgFileSimple( getIntf() );
    }
    else if( rAction == "dialogs.file()" )
    {
        pCommand = new CmdDlgFile( getIntf() );
    }
    else if( rAction == "dialogs.disc()" )
    {
        pCommand = new CmdDlgDisc( getIntf() );
    }
    else if( rAction == "dialogs.net()" )
    {
        pCommand = new CmdDlgNet( getIntf() );
    }
    else if( rAction == "dialogs.messages()" )
    {
        pCommand = new CmdDlgMessages( getIntf() );
    }
    else if( rAction == "dialogs.prefs()" )
    {
        pCommand = new CmdDlgPrefs( getIntf() );
    }
    else if( rAction == "dialogs.fileInfo()" )
    {
        pCommand = new CmdDlgFileInfo( getIntf() );
    }
    else if( rAction == "dialogs.popup()" )
    {
        pCommand = new CmdDlgPopupMenu( getIntf() );
    }
    else if( rAction == "playlist.add()" )
    {
        pCommand = new CmdDlgAdd( getIntf() );
    }
    else if( rAction == "playlist.del()" )
    {
        VarList &rVar = VlcProc::instance( getIntf() )->getPlaylistVar();
        pCommand = new CmdPlaylistDel( getIntf(), rVar );
    }
    else if( rAction == "playlist.next()" )
    {
        pCommand = new CmdPlaylistNext( getIntf() );
    }
    else if( rAction == "playlist.previous()" )
    {
        pCommand = new CmdPlaylistPrevious( getIntf() );
    }
    else if( rAction == "playlist.sort()" )
    {
        pCommand = new CmdPlaylistSort( getIntf() );
    }
    else if( rAction == "vlc.fullscreen()" )
    {
        pCommand = new CmdFullscreen( getIntf() );
    }
    else if( rAction == "vlc.quit()" )
    {
        pCommand = new CmdQuit( getIntf() );
    }
    else if( rAction == "vlc.stop()" )
    {
        pCommand = new CmdStop( getIntf() );
    }
    else if( rAction == "vlc.slower()" )
    {
        pCommand = new CmdSlower( getIntf() );
    }
    else if( rAction == "vlc.faster()" )
    {
        pCommand = new CmdFaster( getIntf() );
    }
    else if( rAction.find( ".setLayout(" ) != string::npos )
    {
        int leftPos = rAction.find( ".setLayout(" );
        string windowId = rAction.substr( 0, leftPos );
        // 11 is the size of ".setLayout("
        int rightPos = rAction.find( ")", windowId.size() + 11 );
        string layoutId = rAction.substr( windowId.size() + 11,
                                          rightPos - (windowId.size() + 11) );
        // XXX check the IDs (isalpha())
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
    VarBool *pVar = NULL;

    if( rName == "vlc.isPlaying" )
    {
        pVar = &VlcProc::instance( getIntf() )->getIsPlayingVar();
    }
    else if( rName == "vlc.isSeekablePlaying" )
    {
        pVar = &VlcProc::instance( getIntf() )->getIsSeekablePlayingVar();
    }
    else if( rName == "vlc.isMute" )
    {
        pVar = &VlcProc::instance( getIntf() )->getIsMuteVar();
    }
    else if( rName.find( ".isVisible" ) != string::npos )
    {
        int leftPos = rName.find( ".isVisible" );
        string windowId = rName.substr( 0, leftPos );
        // XXX Need to check the IDs (isalpha())?
        GenericWindow *pWin = pTheme->getWindowById( windowId );
        if( pWin )
        {
            pVar = &pWin->getVisibleVar();
        }
        else
        {
            msg_Warn( getIntf(), "Unknown window (%s)", windowId.c_str() );
        }
    }

    return pVar;
}


VarPercent *Interpreter::getVarPercent( const string &rName, Theme *pTheme )
{
    VarPercent *pVar = NULL;

    if( rName == "time" )
    {
        pVar = &VlcProc::instance( getIntf() )->getTimeVar();
    }
    else if( rName == "volume" )
    {
        pVar = &VlcProc::instance( getIntf() )->getVolumeVar();
    }
    else if( rName == "playlist.slider" )
    {
        pVar = &VlcProc::instance( getIntf() )->
                getPlaylistVar().getPositionVar();
    }

    return pVar;
}


VarList *Interpreter::getVarList( const string &rName, Theme *pTheme )
{
    VarList *pVar = NULL;

    if( rName == "playlist" )
    {
        pVar = &VlcProc::instance( getIntf() )->getPlaylistVar();
    }

    return pVar;
}

