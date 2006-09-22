/*****************************************************************************
 * playlist.cpp : Playlist dialog
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 ******************************************************************************/

#include "dialogs/playlist.hpp"

#include "qt4.hpp"
#include "main_interface.hpp"
#include "util/qvlcframe.hpp"
#include "components/playlist/panels.hpp"
#include "components/playlist/selector.hpp"
#include "dialogs_provider.hpp"
#include "menus.hpp"

#include <QHBoxLayout>
#include <QSignalMapper>
#include <QMenu>
#include <QAction>
#include <QMenuBar>

PlaylistDialog *PlaylistDialog::instance = NULL;

PlaylistDialog::PlaylistDialog( intf_thread_t *_p_intf ) : QVLCMW( _p_intf )
{
    QWidget *main = new QWidget( this );
    setCentralWidget( main );
    setWindowTitle( qtr( "Playlist" ) );

   createPlMenuBar( menuBar(), p_intf );

    selector = new PLSelector( centralWidget(), p_intf, THEPL );
    selector->setMaximumWidth( 130 );

    playlist_item_t *p_root = playlist_GetPreferredNode( THEPL,
                                                THEPL->p_local_category );

    rightPanel = qobject_cast<PLPanel *>(new StandardPLPanel( centralWidget(),
                              p_intf, THEPL, p_root ) );
    CONNECT( selector, activated( int ), rightPanel, setRoot( int ) );

    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget( selector, 0 );
    layout->addWidget( rightPanel, 10 );
    centralWidget()->setLayout( layout );
    readSettings( "playlist", QSize( 600,700 ) );
}

PlaylistDialog::~PlaylistDialog()
{
    writeSettings( "playlist" );
}

void PlaylistDialog::createPlMenuBar( QMenuBar *bar, intf_thread_t *p_intf )
{
    QMenu *manageMenu = new QMenu();
    manageMenu->setTitle( qtr("Add") );

    QMenu *subPlaylist = new QMenu();
    subPlaylist->setTitle( qtr("Add to current playlist") );
    subPlaylist->addAction( "&File...", THEDP,
                           SLOT( simplePLAppendDialog() ) );
    subPlaylist->addAction( "&Advanced add...", THEDP,
                           SLOT( PLAppendDialog() ) );
    manageMenu->addMenu( subPlaylist );
    manageMenu->addSeparator();

    QMenu *subML = new QMenu();
    subML->setTitle( qtr("Add to Media library") );
    subML->addAction( "&File...", THEDP,
                           SLOT( simpleMLAppendDialog() ) );
    subML->addAction( "Directory", THEDP, SLOT( openMLDirectory() ));
    subML->addAction( "&Advanced add...", THEDP,
                           SLOT( MLAppendDialog() ) );
    manageMenu->addMenu( subML );
    manageMenu->addAction( "Open playlist file", THEDP, SLOT( openPlaylist() ));

    manageMenu->addAction( "Dock playlist", this, SLOT( dock() ) );
    bar->addMenu( manageMenu );
    bar->addMenu( QVLCMenu::SDMenu( p_intf ) );
}

void PlaylistDialog::dock()
{
    hide();
    QEvent *event = new QEvent( (QEvent::Type)(PLDockEvent_Type) );
    QApplication::postEvent( p_intf->p_sys->p_mi, event );
}
