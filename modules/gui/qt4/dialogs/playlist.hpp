/*****************************************************************************
 * playlist.hpp: Playlist dialog
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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

#ifndef _PLAYLIST_DIALOG_H_
#define _PLAYLIST_DIALOG_H_

#include "util/qvlcframe.hpp"

class PlaylistDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static PlaylistDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance) instance = new PlaylistDialog( p_intf );
        return instance;
    }
    virtual ~PlaylistDialog();
private:
    PlaylistDialog( intf_thread_t * );
    intf_thread_t *p_intf;
    static PlaylistDialog *instance;
public slots:
};


#endif
