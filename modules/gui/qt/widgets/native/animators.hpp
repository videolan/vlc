/*****************************************************************************
 * animators.hpp: Object animators
 ****************************************************************************
 * Copyright (C) 2006-2014 the VideoLAN team
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

#ifndef ANIMATORS_HPP
#define ANIMATORS_HPP

#include "qt.hpp"

#include <QObject>
#include <QList>
#include <QString>
#include <QAbstractAnimation>
#include <QPixmap>
#include <QPersistentModelIndex>

class QWidget;
class QPixmap;
class QAbstractItemView;

class BasicAnimator : public QAbstractAnimation
{
    Q_OBJECT

public:
    BasicAnimator( QObject *parent = 0 );
    void setFps( int _fps ) { fps = _fps; interval = 1000.0 / fps; }
    int duration() const Q_DECL_OVERRIDE { return 1000; }

signals:
    void frameChanged();

protected:
    void updateCurrentTime ( int msecs ) Q_DECL_OVERRIDE;
    int fps;
    int interval;
    int current_frame;
};

/** An animated pixmap
     * Use this widget to display an animated icon based on a series of
     * pixmaps. The pixmaps will be stored in memory and should be kept small.
     * First, create the widget, add frames and then start playing. Looping
     * is supported.
     **/
class PixmapAnimator : public BasicAnimator
{
    Q_OBJECT

public:
    PixmapAnimator(QWidget *parent, QList<QString> _frames , int width, int height);
    int duration() const Q_DECL_OVERRIDE { return interval * pixmaps.count(); }
    virtual ~PixmapAnimator();
    const QPixmap& getPixmap() { return currentPixmap; }
protected:
    void updateCurrentTime ( int msecs ) Q_DECL_OVERRIDE;
    QList<QPixmap> pixmaps;
    QPixmap currentPixmap;
signals:
    void pixmapReady( const QPixmap & );
};

class DelegateAnimationHelper : public QObject
{
    Q_OBJECT

public:
    DelegateAnimationHelper( QAbstractItemView *view, BasicAnimator *animator = 0 );
    void setIndex( const QModelIndex &index );
    bool isRunning() const;
    const QPersistentModelIndex & getIndex() const;

public slots:
    void run( bool );

protected slots:
    void updateDelegate();

private:
    QAbstractItemView *view;
    BasicAnimator *animator;
    QPersistentModelIndex index;
};

#endif // ANIMATORS_HPP
