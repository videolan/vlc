/*****************************************************************************
 * cmd_dialogs.hpp
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

#ifndef CMD_DIALOGS_HPP
#define CMD_DIALOGS_HPP

#include "cmd_generic.hpp"
#include "../src/dialogs.hpp"
#include "cmd_change_skin.hpp"

#include <vlc_interface.h>

#define DEFINE_DIALOGS \
    DEF( ChangeSkin,         showChangeSkin() ) \
    DEF( FileSimple,         showFileSimple( true ) ) \
    DEF( File,               showFile( true ) ) \
    DEF( Disc,               showDisc( true ) ) \
    DEF( Net,                showNet( true ) ) \
    DEF( Messages,           showMessages() ) \
    DEF( Prefs,              showPrefs() ) \
    DEF( FileInfo,           showFileInfo() ) \
    \
    DEF( Add,                showFile( false ) ) \
    DEF( PlaylistLoad,       showPlaylistLoad() ) \
    DEF( PlaylistSave,       showPlaylistSave() ) \
    DEF( Directory,          showDirectory( true ) ) \
    DEF( StreamingWizard,    showStreamingWizard() ) \
    DEF( Playlist,           showPlaylist() ) \
    \
    DEF( ShowPopupMenu,      showPopupMenu(true,INTF_DIALOG_POPUPMENU) ) \
    DEF( HidePopupMenu,      showPopupMenu(false,INTF_DIALOG_POPUPMENU) ) \
    DEF( ShowAudioPopupMenu, showPopupMenu(true,INTF_DIALOG_AUDIOPOPUPMENU) ) \
    DEF( HideAudioPopupMenu, showPopupMenu(false,INTF_DIALOG_AUDIOPOPUPMENU) ) \
    DEF( ShowVideoPopupMenu, showPopupMenu(true,INTF_DIALOG_VIDEOPOPUPMENU) ) \
    DEF( HideVideoPopupMenu, showPopupMenu(false,INTF_DIALOG_VIDEOPOPUPMENU) ) \
    DEF( ShowMiscPopupMenu,  showPopupMenu(true,INTF_DIALOG_MISCPOPUPMENU) ) \
    DEF( HideMiscPopupMenu,  showPopupMenu(false,INTF_DIALOG_MISCPOPUPMENU) )

#define DEF( a, c ) \
class CmdDlg##a: public CmdGeneric                              \
{   public:                                                     \
    CmdDlg##a( intf_thread_t *pIntf ): CmdGeneric( pIntf ) { }  \
    virtual ~CmdDlg##a() { }                                    \
    virtual void execute()                                      \
    {                                                           \
        Dialogs *dlg = Dialogs::instance( getIntf() );          \
        if( dlg ) dlg->c;                                       \
    }                                                           \
    virtual string getType() const { return #a" dialog"; }      \
};

DEFINE_DIALOGS

class CmdInteraction: public CmdGeneric
{
public:
    CmdInteraction( intf_thread_t *pIntf, interaction_dialog_t * p_dialog )
                  : CmdGeneric( pIntf ), m_pDialog( p_dialog ) { }
    virtual ~CmdInteraction() { }

    virtual void execute()
    {
        Dialogs *pDialogs = Dialogs::instance( getIntf() );
        if( pDialogs != NULL )
            pDialogs->showInteraction( m_pDialog );
    }
    virtual string getType() const { return "interaction"; }
private:
    interaction_dialog_t *m_pDialog;
};

#endif
