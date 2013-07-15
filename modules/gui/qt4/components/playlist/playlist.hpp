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

#ifndef _PLAYLISTWIDGET_H_
#define _PLAYLISTWIDGET_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"

//#include <vlc_playlist.h>

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
    virtual void dropEvent( QDropEvent *);
    virtual void dragEnterEvent( QDragEnterEvent * );
    virtual void closeEvent( QCloseEvent * );
private slots:
    void changeView( const QModelIndex& index );
    void clearPlaylist();

    friend class PlaylistDialog;
};

#ifdef Q_OS_MAC
class PlaylistSplitter : public QSplitter
{
public:
    PlaylistSplitter( QWidget *_parent ) : QSplitter( _parent ){}
protected:
    virtual QSplitterHandle *createHandle();
};
#else
 #define PlaylistSplitter QSplitter
#endif

#ifdef Q_OS_MAC
class SplitterHandle : public QSplitterHandle
{
public:
    SplitterHandle( Qt::Orientation orientation, QSplitter * parent );

protected:
    virtual void paintEvent ( QPaintEvent * );

private:
    virtual QSize sizeHint () const;
};
#endif /* __APPLE__ */

class LocationButton : public QPushButton
{
public:
    LocationButton( const QString &, bool bold, bool arrow, QWidget * parent = NULL );
    virtual QSize sizeHint() const;
protected:
    virtual void paintEvent ( QPaintEvent * event );
private:
    bool b_arrow;
};

class PLModel;
class QHBoxLayout;
class LocationBar : public QWidget
{
    Q_OBJECT
public:
    LocationBar( PLModel * );
    void setIndex( const QModelIndex & );
    virtual QSize sizeHint() const;
protected:
    virtual void resizeEvent ( QResizeEvent * event );

private:
    void layOut( const QSize& size );

    PLModel *model;
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
