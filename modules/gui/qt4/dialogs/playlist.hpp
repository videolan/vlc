/*****************************************************************************
 * playlist.hpp: Playlist dialog
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
 *****************************************************************************/

#ifndef QVLC_PLAYLIST_DIALOG_H_
#define QVLC_PLAYLIST_DIALOG_H_ 1

#include "util/qvlcframe.hpp"
#include "../components/playlist/playlist.hpp"

#include <QModelIndex>

class QSignalMapper;
class PLSelector;
class PLPanel;
class QSettings;

class PlaylistDialog : public QVLCMW
{
    Q_OBJECT;
private:
    PlaylistWidget *playlistWidget;

public:
    static PlaylistDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance) instance = new PlaylistDialog( p_intf );
        return instance;
    }
    static void killInstance()
    {
        delete instance;
        instance = NULL;
    }
private:
    PlaylistDialog( intf_thread_t * );
    virtual ~PlaylistDialog();

    void dropEvent( QDropEvent *);
    void dragEnterEvent( QDragEnterEvent * );
    void dragMoveEvent( QDragMoveEvent * );
    void dragLeaveEvent( QDragLeaveEvent * );

    static PlaylistDialog *instance;
};


#endif
