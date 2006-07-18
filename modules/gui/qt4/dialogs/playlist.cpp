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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#include "dialogs/playlist.hpp"
#include "util/qvlcframe.hpp"
#include "qt4.hpp"
#include "components/playlist/panels.hpp"
#include "components/playlist/selector.hpp"
#include <QHBoxLayout>

PlaylistDialog *PlaylistDialog::instance = NULL;

PlaylistDialog::PlaylistDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Playlist" ) );
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                     VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    QHBoxLayout *layout = new QHBoxLayout();
    selector = new PLSelector( this, p_intf );
    layout->addWidget( selector, 1 );
    
    rightPanel = qobject_cast<PLPanel *>(new StandardPLPanel( this, p_intf,
                                   p_playlist, p_playlist->p_root_category ) );
    layout->addWidget( rightPanel, 3 );
    readSettings( "playlist", QSize( 500,500 ) );
    setLayout( layout );
}

PlaylistDialog::~PlaylistDialog()
{
    writeSettings( "playlist" );
}
