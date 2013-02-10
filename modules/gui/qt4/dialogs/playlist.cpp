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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/playlist.hpp"

#include "components/playlist/playlist.hpp"

#include "util/qt_dirs.hpp"

#include <QUrl>
#include <QMimeData>
#include <QHBoxLayout>

PlaylistDialog::PlaylistDialog( intf_thread_t *_p_intf )
                : QVLCMW( _p_intf )
{
    setWindowTitle( qtr( "Playlist" ) );
    setWindowRole( "vlc-playlist" );
    setWindowOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );

    playlistWidget = new PlaylistWidget( p_intf, this );
    setCentralWidget( playlistWidget );

    readSettings( "playlistdialog", QSize( 600,700 ) );
}

PlaylistWidget *PlaylistDialog::exportPlaylistWidget()
{
    Q_ASSERT( playlistWidget );
    PlaylistWidget *widget = playlistWidget;
    layout()->removeWidget( playlistWidget );
    playlistWidget = NULL;
    return widget;
}

void PlaylistDialog::importPlaylistWidget( PlaylistWidget *widget )
{
    Q_ASSERT( !playlistWidget );
    playlistWidget = widget;
    setCentralWidget( playlistWidget );
    playlistWidget->show();
}

bool PlaylistDialog::hasPlaylistWidget()
{
    return ( !! playlistWidget );
}

void PlaylistDialog::hideEvent( QHideEvent * event )
{
    QWidget::hideEvent( event );
    emit visibilityChanged( false );
}

PlaylistDialog::~PlaylistDialog()
{
    writeSettings( "playlistdialog" );
}

void PlaylistDialog::dropEvent( QDropEvent *event )
{
    playlistWidget->dropEvent(event);
}
void PlaylistDialog::dragEnterEvent( QDragEnterEvent *event )
{
     event->acceptProposedAction();
}
void PlaylistDialog::dragMoveEvent( QDragMoveEvent *event )
{
     event->acceptProposedAction();
}
void PlaylistDialog::dragLeaveEvent( QDragLeaveEvent *event )
{
     event->accept();
}

