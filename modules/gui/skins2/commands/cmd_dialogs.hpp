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


#define DEFC( a, c ) \
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

DEFC( ChangeSkin,         showChangeSkin() )
DEFC( FileSimple,         showFileSimple( true ) )
DEFC( File,               showFile( true ) )
DEFC( Disc,               showDisc( true ) )
DEFC( Net,                showNet( true ) )
DEFC( Messages,           showMessages() )
DEFC( Prefs,              showPrefs() )
DEFC( FileInfo,           showFileInfo() )

DEFC( Add,                showFile( false ) )
DEFC( PlaylistLoad,       showPlaylistLoad() )
DEFC( PlaylistSave,       showPlaylistSave() )
DEFC( Directory,          showDirectory( true ) )
DEFC( StreamingWizard,    showStreamingWizard() )
DEFC( Playlist,           showPlaylist() )

DEFC( ShowPopupMenu,      showPopupMenu(true,INTF_DIALOG_POPUPMENU) )
DEFC( HidePopupMenu,      showPopupMenu(false,INTF_DIALOG_POPUPMENU) )
DEFC( ShowAudioPopupMenu, showPopupMenu(true,INTF_DIALOG_AUDIOPOPUPMENU) )
DEFC( HideAudioPopupMenu, showPopupMenu(false,INTF_DIALOG_AUDIOPOPUPMENU) )
DEFC( ShowVideoPopupMenu, showPopupMenu(true,INTF_DIALOG_VIDEOPOPUPMENU) )
DEFC( HideVideoPopupMenu, showPopupMenu(false,INTF_DIALOG_VIDEOPOPUPMENU) )
DEFC( ShowMiscPopupMenu,  showPopupMenu(true,INTF_DIALOG_MISCPOPUPMENU) )
DEFC( HideMiscPopupMenu,  showPopupMenu(false,INTF_DIALOG_MISCPOPUPMENU) )

#undef DEFC

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
