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
#include <QToolBar>

#include <vlc_playlist.h>

class QSignalMapper;
class QTreeView;
class QListView;
class PLModel;
class QPushButton;
class QKeyEvent;
class QWheelEvent;
class PlIconView;
class LocationBar;

class StandardPLPanel: public QWidget
{
    Q_OBJECT

public:
    StandardPLPanel( PlaylistWidget *, intf_thread_t *,
                     playlist_t *,playlist_item_t * );
    virtual ~StandardPLPanel();
protected:
    friend class PlaylistWidget;

    virtual void keyPressEvent( QKeyEvent *e );
    virtual void wheelEvent( QWheelEvent *e );

    PLModel *model;
private:
    intf_thread_t *p_intf;

    QWidget     *parent;
    QLabel      *title;
    QPushButton *addButton;
    QGridLayout *layout;
    LocationBar *locationBar;

    QTreeView   *treeView;
    PlIconView  *iconView;
    QAbstractItemView *currentView;

    int currentRootId;
    QSignalMapper *selectColumnsSigMapper;
    QSignalMapper *viewSelectionMapper;

    int last_activated_id;

    enum {
      TREE_VIEW = 0,
      ICON_VIEW,
      COVER_VIEW,
    };

    void createTreeView();
    void createIconView();

public slots:
    void removeItem( int );
    virtual void setRoot( playlist_item_t * );
private slots:
    void deleteSelection();
    void handleExpansion( const QModelIndex& );
    void gotoPlayingItem();
    void search( const QString& searchText );
    void popupAdd();
    void popupSelectColumn( QPoint );
    void popupPlView( const QPoint & );
    void toggleColumnShown( int );
    void showView( int );
    void activate( const QModelIndex & );
    void handleInputChange( input_thread_t * );
};

class LocationBar : public QToolBar
{
    Q_OBJECT;
public:
    LocationBar( PLModel * );
    void setIndex( const QModelIndex & );
signals:
    void invoked( const QModelIndex & );
private slots:
    void invoke( int i_item_id );
private:
    PLModel *model;
    QSignalMapper *mapper;
};

#endif
