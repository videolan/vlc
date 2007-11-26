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

#include "main_interface.hpp"
#include "components/interface_widgets.hpp"
#include "dialogs_provider.hpp"
#include "menus.hpp"

#include <QUrl>
#include <QHBoxLayout>
#include <QSignalMapper>
#include <QMenu>
#include <QAction>
#include <QMenuBar>

PlaylistDialog *PlaylistDialog::instance = NULL;

PlaylistDialog::PlaylistDialog( intf_thread_t *_p_intf )
                : QVLCMW( _p_intf )
{
    QWidget *main = new QWidget( this );
    setCentralWidget( main );
    setWindowTitle( qtr( "Playlist" ) );
    setWindowOpacity( config_GetFloat( p_intf, "qt-opacity" ) );

    QHBoxLayout *l = new QHBoxLayout( centralWidget() );
    PlaylistWidget *plw = new PlaylistWidget( p_intf, NULL );
    l->addWidget( plw );

    readSettings( "playlist", QSize( 600,700 ) );
}

PlaylistDialog::~PlaylistDialog()
{
    writeSettings( "playlist" );
}

void PlaylistDialog::dropEvent(QDropEvent *event)
{
     const QMimeData *mimeData = event->mimeData();
     foreach( QUrl url, mimeData->urls() ) {
        QString s = url.toString();
        if( s.length() > 0 ) {
            playlist_Add( THEPL, qtu(s), NULL,
                          PLAYLIST_APPEND, PLAYLIST_END, VLC_TRUE, VLC_FALSE );
        }
     }
     event->acceptProposedAction();
}
void PlaylistDialog::dragEnterEvent(QDragEnterEvent *event)
{
     event->acceptProposedAction();
}
void PlaylistDialog::dragMoveEvent(QDragMoveEvent *event)
{
     event->acceptProposedAction();
}
void PlaylistDialog::dragLeaveEvent(QDragLeaveEvent *event)
{
     event->accept();
}

