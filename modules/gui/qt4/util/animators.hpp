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

#include <QObject>
#include <QList>
#include <QString>
#include <QAbstractAnimation>
class QWidget;
class QPixmap;

/** An animated pixmap
     * Use this widget to display an animated icon based on a series of
     * pixmaps. The pixmaps will be stored in memory and should be kept small.
     * First, create the widget, add frames and then start playing. Looping
     * is supported.
     **/
class PixmapAnimator : public QAbstractAnimation
{
    Q_OBJECT

public:
    PixmapAnimator( QWidget *parent, QList<QString> _frames );
    void setFps( int _fps ) { fps = _fps; interval = 1000.0 / fps; }
    virtual int duration() const { return interval * pixmaps.count(); }
    virtual ~PixmapAnimator() { qDeleteAll( pixmaps ); }
    QPixmap *getPixmap() { return currentPixmap; }
protected:
    virtual void updateCurrentTime ( int msecs );
    QList<QPixmap *> pixmaps;
    QPixmap *currentPixmap;
    int fps;
    int interval;
    int lastframe_msecs;
    int current_frame;
signals:
    void pixmapReady( const QPixmap & );
};

#endif // ANIMATORS_HPP
