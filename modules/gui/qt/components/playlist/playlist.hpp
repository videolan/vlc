/*****************************************************************************
 * playlist.hpp : Playlist Widgets
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Rafaël Carré <funman@videolanorg>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_PLAYLIST_HPP_
#define VLC_QT_PLAYLIST_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

//#include <vlc_playlist_legacy.h>

#include <QSplitter>

#include <QPushButton>
#include <QSplitterHandle>
#include <QMouseEvent>

class StandardPLPanel;
class LocationBar;
class QSignalMapper;
class SearchLineEdit;
class QModelIndex;
class QStackedWidget;
class PLSelector;

class PlaylistWidget : public QWidget
{
    Q_OBJECT
public:
    virtual ~PlaylistWidget();

    void forceHide();
    void forceShow();
    void setSearchFieldFocus();
    QStackedWidget *artContainer;
    StandardPLPanel      *mainView;

private:
    QSplitter            *leftSplitter;
    QSplitter            *split;

    PLSelector           *selector;

    LocationBar          *locationBar;
    SearchLineEdit       *searchEdit;

    intf_thread_t *p_intf;

protected:
    PlaylistWidget( intf_thread_t *_p_i, QWidget * );
    void dropEvent( QDropEvent *) Q_DECL_OVERRIDE;
    void dragEnterEvent( QDragEnterEvent * ) Q_DECL_OVERRIDE;
    void closeEvent( QCloseEvent * ) Q_DECL_OVERRIDE;
private slots:
    void changeView( const QModelIndex& index );

    friend class PlaylistDialog;
};

class LocationButton : public QPushButton
{
public:
    LocationButton( const QString &, bool bold, bool arrow, QWidget * parent = NULL );
    QSize sizeHint() const Q_DECL_OVERRIDE;
protected:
    void paintEvent ( QPaintEvent * event ) Q_DECL_OVERRIDE;
private:
    bool b_arrow;
};

class VLCModel;

class LocationBar : public QWidget
{
    Q_OBJECT
public:
    LocationBar( VLCModel * );
    void setIndex( const QModelIndex & );
    void setModel( VLCModel * _model ) { model = _model; };
    QSize sizeHint() const Q_DECL_OVERRIDE;
protected:
    void resizeEvent ( QResizeEvent * event ) Q_DECL_OVERRIDE;

private:
    void layOut( const QSize& size );

    VLCModel *model;
    QSignalMapper *mapper;
    QWidgetList buttons;
    QList<QAction*> actions;
    LocationButton *btnMore;
    QMenu *menuMore;
    QList<int> widths;

public slots:
    void setRootIndex();
private slots:
    void invoke( int i_item_id );

signals:
    void invoked( const QModelIndex & );
};


#endif
