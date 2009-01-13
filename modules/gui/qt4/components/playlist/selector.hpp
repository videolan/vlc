/*****************************************************************************
 * selector.hpp : Playlist source selector
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
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

#ifndef _PLSEL_H_
#define _PLSEL_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/playlist/playlist_model.hpp"

#include <QWidget>

class QTreeView;
class PlaylistWidget;

class PLSelector: public QWidget
{
    Q_OBJECT;
public:
    PLSelector( QWidget *p, intf_thread_t *_p_intf );
    virtual ~PLSelector();
protected:
    PLModel *model;
    friend class PlaylistWidget;
private:
    intf_thread_t *p_intf;
    QTreeView *view;
private slots:
    void setSource( const QModelIndex& );
signals:
    void activated( int );
    void shouldRemove( int );
};

#endif
