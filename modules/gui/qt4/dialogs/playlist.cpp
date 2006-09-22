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

    SDMapper = new QSignalMapper();
    CONNECT( SDMapper, mapped (QString), this, SDMenuAction( QString ) );
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
    bar->addMenu( SDMenu() );
}

QMenu *PlaylistDialog::SDMenu()
{
    QMenu *menu = new QMenu();
    menu->setTitle( qtr( "Additional sources" ) );
    vlc_list_t *p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE,
                                        FIND_ANYWHERE );
    int i_num = 0;
    for( int i_index = 0 ; i_index < p_list->i_count; i_index++ )
    {
        module_t * p_parser = (module_t *)p_list->p_values[i_index].p_object ;
        if( !strcmp( p_parser->psz_capability, "services_discovery" ) )
            i_num++;
    }
    for( int i_index = 0 ; i_index < p_list->i_count; i_index++ )
    {
        module_t * p_parser = (module_t *)p_list->p_values[i_index].p_object;
        if( !strcmp( p_parser->psz_capability, "services_discovery" ) )
        {
            QAction *a = new QAction( qfu( p_parser->psz_longname ), menu );
            a->setCheckable( true );
            /* hack to handle submodules properly */
            int i = -1;
            while( p_parser->pp_shortcuts[++i] != NULL );
            i--;
            if( playlist_IsServicesDiscoveryLoaded( THEPL,
                 i>=0?p_parser->pp_shortcuts[i] : p_parser->psz_object_name ) )
            {
                a->setChecked( true );
            }
            CONNECT( a , triggered(), SDMapper, map() );
            SDMapper->setMapping( a, i>=0? p_parser->pp_shortcuts[i] :
                                            p_parser->psz_object_name );
            menu->addAction( a );
        }
    }
    vlc_list_release( p_list );
    return menu;
}

void PlaylistDialog::dock()
{
    hide();
    QEvent *event = new QEvent( (QEvent::Type)(PLDockEvent_Type) );
    QApplication::postEvent( p_intf->p_sys->p_mi, event );
}

void PlaylistDialog::SDMenuAction( QString data )
{
    char *psz_sd = data.toUtf8().data();
    if( !playlist_IsServicesDiscoveryLoaded( THEPL, psz_sd ) )
        playlist_ServicesDiscoveryAdd( THEPL, psz_sd );
    else
        playlist_ServicesDiscoveryRemove( THEPL, psz_sd );
}
