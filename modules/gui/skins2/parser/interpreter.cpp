/*****************************************************************************
 * interpreter.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "interpreter.hpp"
#include "expr_evaluator.hpp"
#include "../commands/cmd_audio.hpp"
#include "../commands/cmd_muxer.hpp"
#include "../commands/cmd_playlist.hpp"
#include "../commands/cmd_playtree.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_dummy.hpp"
#include "../commands/cmd_dvd.hpp"
#include "../commands/cmd_layout.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_minimize.hpp"
#include "../commands/cmd_input.hpp"
#include "../commands/cmd_fullscreen.hpp"
#include "../commands/cmd_on_top.hpp"
#include "../commands/cmd_show_window.hpp"
#include "../commands/cmd_snapshot.hpp"
#include "../src/theme.hpp"
#include "../src/var_manager.hpp"
#include "../src/vlcproc.hpp"


Interpreter::Interpreter( intf_thread_t *pIntf ): SkinObject( pIntf )
{
    /// Create the generic commands
#define REGISTER_CMD( name, cmd ) \
    m_commandMap[name] = CmdGenericPtr( new cmd( getIntf() ) );
#define REGISTER_CMD1( name, cmd, a1 ) \
    m_commandMap[name] = CmdGenericPtr( new cmd( getIntf(), a1 ) );

    REGISTER_CMD( "none", CmdDummy )
    REGISTER_CMD( "dialogs.changeSkin()", CmdDlgChangeSkin )
    REGISTER_CMD( "dialogs.fileSimple()", CmdDlgFileSimple )
    REGISTER_CMD( "dialogs.file()", CmdDlgFile )
    REGISTER_CMD( "dialogs.directory()", CmdDlgDirectory )
    REGISTER_CMD( "dialogs.disc()", CmdDlgDisc )
    REGISTER_CMD( "dialogs.net()", CmdDlgNet )
    REGISTER_CMD( "dialogs.playlist()", CmdDlgPlaylist )
    REGISTER_CMD( "dialogs.messages()", CmdDlgMessages )
    REGISTER_CMD( "dialogs.prefs()", CmdDlgPrefs )
    REGISTER_CMD( "dialogs.fileInfo()", CmdDlgFileInfo )
    REGISTER_CMD( "dialogs.streamingWizard()", CmdDlgStreamingWizard )

    REGISTER_CMD( "dialogs.popup()", CmdDlgShowPopupMenu )
    REGISTER_CMD( "dialogs.audioPopup()", CmdDlgShowAudioPopupMenu )
    REGISTER_CMD( "dialogs.videoPopup()", CmdDlgShowVideoPopupMenu )
    REGISTER_CMD( "dialogs.miscPopup()", CmdDlgShowMiscPopupMenu )

    REGISTER_CMD( "dvd.nextTitle()", CmdDvdNextTitle )
    REGISTER_CMD( "dvd.previousTitle()", CmdDvdPreviousTitle )
    REGISTER_CMD( "dvd.nextChapter()", CmdDvdNextChapter )
    REGISTER_CMD( "dvd.previousChapter()", CmdDvdPreviousChapter )
    REGISTER_CMD( "dvd.rootMenu()", CmdDvdRootMenu )
    REGISTER_CMD( "playlist.load()", CmdDlgPlaylistLoad )
    REGISTER_CMD( "playlist.save()", CmdDlgPlaylistSave )
    REGISTER_CMD( "playlist.add()", CmdDlgAdd )
    REGISTER_CMD( "playlist.next()", CmdPlaylistNext )
    REGISTER_CMD( "playlist.previous()", CmdPlaylistPrevious )
    REGISTER_CMD1( "playlist.setRandom(true)", CmdPlaylistRandom, true )
    REGISTER_CMD1( "playlist.setRandom(false)", CmdPlaylistRandom, false )
    REGISTER_CMD1( "playlist.setLoop(true)", CmdPlaylistLoop, true )
    REGISTER_CMD1( "playlist.setLoop(false)", CmdPlaylistLoop, false )
    REGISTER_CMD1( "playlist.setRepeat(true)", CmdPlaylistRepeat, true )
    REGISTER_CMD1( "playlist.setRepeat(false)", CmdPlaylistRepeat, false )
    VarTree &rVarTree = VlcProc::instance( getIntf() )->getPlaytreeVar();
    REGISTER_CMD1( "playlist.del()", CmdPlaytreeDel, rVarTree )
    REGISTER_CMD1( "playtree.del()", CmdPlaytreeDel, rVarTree )
    REGISTER_CMD( "playlist.sort()", CmdPlaytreeSort )
    REGISTER_CMD( "playtree.sort()", CmdPlaytreeSort )
    REGISTER_CMD( "vlc.fullscreen()", CmdFullscreen )
    REGISTER_CMD( "vlc.play()", CmdPlay )
    REGISTER_CMD( "vlc.pause()", CmdPause )
    REGISTER_CMD( "vlc.stop()", CmdStop )
    REGISTER_CMD( "vlc.faster()", CmdFaster )
    REGISTER_CMD( "vlc.slower()", CmdSlower )
    REGISTER_CMD( "vlc.mute()", CmdMute )
    REGISTER_CMD( "vlc.volumeUp()", CmdVolumeUp )
    REGISTER_CMD( "vlc.volumeDown()", CmdVolumeDown )
    REGISTER_CMD( "vlc.minimize()", CmdMinimize )
    REGISTER_CMD( "vlc.onTop()", CmdOnTop )
    REGISTER_CMD( "vlc.snapshot()", CmdSnapshot )
    REGISTER_CMD( "vlc.toggleRecord()", CmdToggleRecord )
    REGISTER_CMD( "vlc.nextFrame()", CmdNextFrame )
    REGISTER_CMD( "vlc.quit()", CmdQuit )
    REGISTER_CMD1( "equalizer.enable()", CmdSetEqualizer, true )
    REGISTER_CMD1( "equalizer.disable()", CmdSetEqualizer, false )

    // Register the constant bool variables in the var manager
    VarManager *pVarManager = VarManager::instance( getIntf() );
    VarBool *pVarTrue = new VarBoolTrue( getIntf() );
    pVarManager->registerVar( VariablePtr( pVarTrue ), "true" );
    VarBool *pVarFalse = new VarBoolFalse( getIntf() );
    pVarManager->registerVar( VariablePtr( pVarFalse ), "false" );

#undef  REGISTER_CMD
#undef  REGISTER_CMD1
}


Interpreter *Interpreter::instance( intf_thread_t *pIntf )
{
    if( ! pIntf->p_sys->p_interpreter )
    {
        Interpreter *pInterpreter = new (std::nothrow) Interpreter( pIntf );
        if( pInterpreter )
            pIntf->p_sys->p_interpreter = pInterpreter;
    }
    return pIntf->p_sys->p_interpreter;
}


void Interpreter::destroy( intf_thread_t *pIntf )
{
    delete pIntf->p_sys->p_interpreter;
    pIntf->p_sys->p_interpreter = NULL;
}


CmdGeneric *Interpreter::parseAction( const std::string &rAction, Theme *pTheme )
{
    // Try to find the command in the global command map
    if( m_commandMap.find( rAction ) != m_commandMap.end() )
    {
        return m_commandMap[rAction].get();
    }

    CmdGeneric *pCommand = NULL;

    if( rAction.find( ";" ) != std::string::npos )
    {
        // Several actions are defined...
        std::list<CmdGeneric *> actionList;
        std::string rightPart = rAction;
        std::string::size_type pos = rightPart.find( ";" );
        while( pos != std::string::npos )
        {
            std::string leftPart = rightPart.substr( 0, pos );
            // Remove any whitespace at the end of the left part, and parse it
            leftPart =
                leftPart.substr( 0, leftPart.find_last_not_of( " \t" ) + 1 );
            actionList.push_back( parseAction( leftPart, pTheme ) );
            // Now remove any whitespace at the beginning of the right part,
            // and go on checking for further actions in it...
            rightPart = rightPart.substr( pos, rightPart.size() );
            if ( rightPart.find_first_not_of( " \t;" ) == std::string::npos )
            {
                // The right part is completely buggy, it's time to leave the
                // loop...
                rightPart = "";
                break;
            }

            rightPart =
                rightPart.substr( rightPart.find_first_not_of( " \t;" ),
                                  rightPart.size() );
            pos = rightPart.find( ";" );
        }
        actionList.push_back( parseAction( rightPart, pTheme ) );

        // The list is filled, we remove NULL pointers from it, just in case...
        actionList.remove( NULL );

        pCommand = new CmdMuxer( getIntf(), actionList );
    }
    else if( rAction.find( ".setLayout(" ) != std::string::npos )
    {
        int leftPos = rAction.find( ".setLayout(" );
        std::string windowId = rAction.substr( 0, leftPos );
        // 11 is the size of ".setLayout("
        int rightPos = rAction.find( ")", windowId.size() + 11 );
        std::string layoutId = rAction.substr( windowId.size() + 11,
                                          rightPos - (windowId.size() + 11) );

        TopWindow *pWin = pTheme->getWindowById( windowId );
        GenericLayout *pLayout = pTheme->getLayoutById( layoutId );
        if( !pWin )
        {
            msg_Err( getIntf(), "unknown window (%s)", windowId.c_str() );
        }
        else if( !pLayout )
        {
            msg_Err( getIntf(), "unknown layout (%s)", layoutId.c_str() );
        }
        // Check that the layout doesn't correspond to another window
        else if( pWin != pLayout->getWindow() )
        {
            msg_Err( getIntf(), "layout %s is not associated to window %s",
                     layoutId.c_str(), windowId.c_str() );
        }
        else
        {
            pCommand = new CmdLayout( getIntf(), *pWin, *pLayout );
        }
    }
    else if( rAction.find( ".maximize()" ) != std::string::npos )
    {
        int leftPos = rAction.find( ".maximize()" );
        std::string windowId = rAction.substr( 0, leftPos );

        TopWindow *pWin = pTheme->getWindowById( windowId );
        if( !pWin )
        {
            msg_Err( getIntf(), "unknown window (%s)", windowId.c_str() );
        }
        else
        {
            pCommand = new CmdMaximize( getIntf(),
                                        pTheme->getWindowManager(),
                                        *pWin );
        }
    }
    else if( rAction.find( ".unmaximize()" ) != std::string::npos )
    {
        int leftPos = rAction.find( ".unmaximize()" );
        std::string windowId = rAction.substr( 0, leftPos );

        TopWindow *pWin = pTheme->getWindowById( windowId );
        if( !pWin )
        {
            msg_Err( getIntf(), "unknown window (%s)", windowId.c_str() );
        }
        else
        {
            pCommand = new CmdUnmaximize( getIntf(),
                                          pTheme->getWindowManager(),
                                          *pWin );
        }
    }
    else if( rAction.find( ".show()" ) != std::string::npos )
    {
        int leftPos = rAction.find( ".show()" );
        std::string windowId = rAction.substr( 0, leftPos );

        if( windowId == "playlist_window" &&
            !var_InheritBool( getIntf(), "skinned-playlist") )
        {
            std::list<CmdGeneric *> list;
            list.push_back( new CmdDlgPlaylist( getIntf() ) );
            TopWindow *pWin = pTheme->getWindowById( windowId );
            if( pWin )
                list.push_back( new CmdHideWindow( getIntf(),
                                                   pTheme->getWindowManager(),
                                                   *pWin ) );
            pCommand = new CmdMuxer( getIntf(), list );
        }
        else
        {
            TopWindow *pWin = pTheme->getWindowById( windowId );
            if( pWin )
            {
                pCommand = new CmdShowWindow( getIntf(),
                                              pTheme->getWindowManager(),
                                              *pWin );
            }
            else
            {
                // It was maybe the id of a popup
                Popup *pPopup = pTheme->getPopupById( windowId );
                if( pPopup )
                {
                    pCommand = new CmdShowPopup( getIntf(), *pPopup );
                }
                else
                {
                    msg_Err( getIntf(), "unknown window or popup (%s)",
                             windowId.c_str() );
                }
            }
        }
    }
    else if( rAction.find( ".hide()" ) != std::string::npos )
    {
        int leftPos = rAction.find( ".hide()" );
        std::string windowId = rAction.substr( 0, leftPos );
        if( windowId == "playlist_window" &&
           !var_InheritBool( getIntf(), "skinned-playlist") )
        {
            std::list<CmdGeneric *> list;
            list.push_back( new CmdDlgPlaylist( getIntf() ) );
            TopWindow *pWin = pTheme->getWindowById( windowId );
            if( pWin )
                list.push_back( new CmdHideWindow( getIntf(),
                                                   pTheme->getWindowManager(),
                                                   *pWin ) );
            pCommand = new CmdMuxer( getIntf(), list );
        }
        else
        {
            TopWindow *pWin = pTheme->getWindowById( windowId );
            if( pWin )
            {
                pCommand = new CmdHideWindow( getIntf(),
                                              pTheme->getWindowManager(),
                                              *pWin );
            }
            else
            {
                msg_Err( getIntf(), "unknown window (%s)", windowId.c_str() );
            }
        }
    }

    if( pCommand )
    {
        // Add the command in the pool
        pTheme->m_commands.push_back( CmdGenericPtr( pCommand ) );
    }
    else
    {
        msg_Warn( getIntf(), "unknown action: %s", rAction.c_str() );
    }

    return pCommand;
}


VarBool *Interpreter::getVarBool( const std::string &rName, Theme *pTheme )
{
    VarManager *pVarManager = VarManager::instance( getIntf() );

    // Convert the expression into Reverse Polish Notation
    ExprEvaluator evaluator( getIntf() );
    evaluator.parse( rName );

    std::list<VarBool*> varStack;

    // Get the first token from the RPN stack
    std::string token = evaluator.getToken();
    while( !token.empty() )
    {
        if( token == "and" )
        {
            // Get the 2 last variables on the stack
            if( varStack.empty() )
            {
                msg_Err( getIntf(), "invalid boolean expression: %s",
                         rName.c_str());
                return NULL;
            }
            VarBool *pVar1 = varStack.back();
            varStack.pop_back();
            if( varStack.empty() )
            {
                msg_Err( getIntf(), "invalid boolean expression: %s",
                         rName.c_str());
                return NULL;
            }
            VarBool *pVar2 = varStack.back();
            varStack.pop_back();

            // Create a composite boolean variable
            VarBool *pNewVar = new VarBoolAndBool( getIntf(), *pVar1, *pVar2 );
            varStack.push_back( pNewVar );
            // Register this variable in the manager
            pVarManager->registerVar( VariablePtr( pNewVar ) );
        }
        else if( token == "or" )
        {
            // Get the 2 last variables on the stack
            if( varStack.empty() )
            {
                msg_Err( getIntf(), "invalid boolean expression: %s",
                         rName.c_str());
                return NULL;
            }
            VarBool *pVar1 = varStack.back();
            varStack.pop_back();
            if( varStack.empty() )
            {
                msg_Err( getIntf(), "invalid boolean expression: %s",
                         rName.c_str());
                return NULL;
            }
            VarBool *pVar2 = varStack.back();
            varStack.pop_back();

            // Create a composite boolean variable
            VarBool *pNewVar = new VarBoolOrBool( getIntf(), *pVar1, *pVar2 );
            varStack.push_back( pNewVar );
            // Register this variable in the manager
            pVarManager->registerVar( VariablePtr( pNewVar ) );
        }
        else if( token == "not" )
        {
            // Get the last variable on the stack
            if( varStack.empty() )
            {
                msg_Err( getIntf(), "invalid boolean expression: %s",
                         rName.c_str());
                return NULL;
            }
            VarBool *pVar = varStack.back();
            varStack.pop_back();

            // Create a composite boolean variable
            VarBool *pNewVar = new VarNotBool( getIntf(), *pVar );
            varStack.push_back( pNewVar );
            // Register this variable in the manager
            pVarManager->registerVar( VariablePtr( pNewVar ) );
        }
        else
        {
            // Try first to get the variable from the variable manager
            // Indeed, if the skin designer is stupid enough to call a layout
            // "dvd", we want "dvd.isActive" to resolve as the built-in action
            // and not as the "layoutId.isActive" one.
            VarBool *pVar = (VarBool*)pVarManager->getVar( token, "bool" );
            if( pVar )
            {
                varStack.push_back( pVar );
            }
            else if( token.find( ".isVisible" ) != std::string::npos )
            {
                int leftPos = token.find( ".isVisible" );
                std::string windowId = token.substr( 0, leftPos );
                TopWindow *pWin = pTheme->getWindowById( windowId );
                if( pWin )
                {
                    // Push the visibility variable onto the stack
                    varStack.push_back( &pWin->getVisibleVar() );
                }
                else
                {
                    msg_Err( getIntf(), "unknown window (%s)",
                             windowId.c_str() );
                    return NULL;
                }
            }
            else if( token.find( ".isMaximized" ) != std::string::npos )
            {
                int leftPos = token.find( ".isMaximized" );
                std::string windowId = token.substr( 0, leftPos );
                TopWindow *pWin = pTheme->getWindowById( windowId );
                if( pWin )
                {
                    // Push the "maximized" variable onto the stack
                    varStack.push_back( &pWin->getMaximizedVar() );
                }
                else
                {
                    msg_Err( getIntf(), "unknown window (%s)",
                             windowId.c_str() );
                    return NULL;
                }
            }
            else if( token.find( ".isActive" ) != std::string::npos )
            {
                int leftPos = token.find( ".isActive" );
                std::string layoutId = token.substr( 0, leftPos );
                GenericLayout *pLayout = pTheme->getLayoutById( layoutId );
                if( pLayout )
                {
                    // Push the isActive variable onto the stack
                    varStack.push_back( &pLayout->getActiveVar() );
                }
                else
                {
                    msg_Err( getIntf(), "unknown layout (%s)",
                             layoutId.c_str() );
                    return NULL;
                }
            }
            else
            {
                msg_Err( getIntf(), "cannot resolve boolean variable: %s",
                         token.c_str());
                return NULL;
            }
        }
        // Get the first token from the RPN stack
        token = evaluator.getToken();
    }

    // The stack should contain a single variable
    if( varStack.size() != 1 )
    {
        msg_Err( getIntf(), "invalid boolean expression: %s", rName.c_str() );
        return NULL;
    }
    return varStack.back();
}


VarPercent *Interpreter::getVarPercent( const std::string &rName, Theme *pTheme )
{
    (void)pTheme;
    VarManager *pVarManager = VarManager::instance( getIntf() );
    return static_cast<VarPercent*>(pVarManager->getVar( rName, "percent" ));
}


VarList *Interpreter::getVarList( const std::string &rName, Theme *pTheme )
{
    (void)pTheme;
    VarManager *pVarManager = VarManager::instance( getIntf() );
    return static_cast<VarList*>(pVarManager->getVar( rName, "list" ));
}


VarTree *Interpreter::getVarTree( const std::string &rName, Theme *pTheme )
{
    (void)pTheme;
    VarManager *pVarManager = VarManager::instance( getIntf() );
    return static_cast<VarTree*>(pVarManager->getVar( rName, "tree" ));
}


std::string Interpreter::getConstant( const std::string &rValue )
{
    // Check if the value is a registered constant; if not, keep as is.
    std::string val = VarManager::instance( getIntf() )->getConst( rValue );
    return val.empty() ? rValue : val;
}

