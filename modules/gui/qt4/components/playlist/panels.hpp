/*****************************************************************************
 * panels.hpp : Panels for the playlist
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

#ifndef _PLPANELS_H_
#define _PLPANELS_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"
#include "components/playlist/playlist.hpp"

#include <QModelIndex>
#include <QWidget>
#include <QString>

#include <vlc_playlist.h>

class QSignalMapper;
class QTreeView;
class PLModel;
class QPushButton;
class QKeyEvent;

class PLPanel: public QWidget
{
    Q_OBJECT;
public:
    PLPanel( PlaylistWidget *p, intf_thread_t *_p_intf ) : QWidget( p )
    {
        p_intf = _p_intf;
        parent = p;
    }
    virtual ~PLPanel() {};
protected:
    intf_thread_t *p_intf;
    QFrame *parent;
public slots:
    virtual void setRoot( playlist_item_t * ) = 0;
};


class StandardPLPanel: public PLPanel
{
    Q_OBJECT;
public:
    StandardPLPanel( PlaylistWidget *, intf_thread_t *,
                     playlist_t *,playlist_item_t * );
    virtual ~StandardPLPanel();
protected:
    virtual void keyPressEvent( QKeyEvent *e );
protected:
    PLModel *model;
    friend class PlaylistWidget;
private:
    QLabel *title;
    QTreeView *view;
    QPushButton *repeatButton, *randomButton, *addButton, *gotoPlayingButton;
    int currentRootId;
    QSignalMapper *selectColumnsSigMapper;
public slots:
    void removeItem( int );
    virtual void setRoot( playlist_item_t * );
private slots:
    void deleteSelection();
    void handleExpansion( const QModelIndex& );
    void toggleRandom();
    void toggleRepeat();
    void gotoPlayingItem();
    void doPopup( QModelIndex index, QPoint point );
    void search( const QString& searchText );
    void popupAdd();
    void popupSelectColumn( QPoint );
    void toggleColumnShown( int );
};

#endif
