/*****************************************************************************
 * playlist.hpp: Playlist dialog
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
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
#include "components/playlist_legacy/playlist.hpp"
#include "util/singleton.hpp"

#include <QModelIndex>

class QHideEvent;

class PlaylistDialog : public QVLCMW, public Singleton<PlaylistDialog>
{
    Q_OBJECT

public:
    PlaylistWidget *exportPlaylistWidget( );
    void importPlaylistWidget( PlaylistWidget * );
    bool hasPlaylistWidget();

protected:
    void hideEvent( QHideEvent * ) Q_DECL_OVERRIDE;

private:
    PlaylistWidget *playlistWidget;
    PlaylistDialog( intf_thread_t * );
    virtual ~PlaylistDialog();

    virtual void dropEvent( QDropEvent *) Q_DECL_OVERRIDE;
    virtual void dragEnterEvent( QDragEnterEvent * ) Q_DECL_OVERRIDE;
    virtual void dragMoveEvent( QDragMoveEvent * ) Q_DECL_OVERRIDE;
    virtual void dragLeaveEvent( QDragLeaveEvent * ) Q_DECL_OVERRIDE;

    friend class    Singleton<PlaylistDialog>;

signals:
    void visibilityChanged( bool );
};


#endif
