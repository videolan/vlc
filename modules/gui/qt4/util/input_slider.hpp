/*****************************************************************************
 * input_slider.hpp : A slider that controls an input
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

#ifndef _INPUTSLIDER_H_
#define _INPUTSLIDER_H_

#include "util/directslider.hpp"

class InputSlider : public DirectSlider
{
    Q_OBJECT
public:
    InputSlider( QWidget *_parent );
    InputSlider( Qt::Orientation q,QWidget *_parent );
    virtual ~InputSlider()   {};
protected:
    void mouseMoveEvent(QMouseEvent *event);
private:
    bool mymove;
    int inputLength;
public slots:
    void setPosition( float, int, int );
private slots:
    void userDrag( int );
signals:
    void sliderDragged( float );
};


class QPaintEvent;
#include <QAbstractSlider>

class SoundSlider : public QAbstractSlider
{
    Q_OBJECT
public:
    SoundSlider( QWidget *_parent, int _i_step, bool b_softamp );
    virtual ~SoundSlider() {};
protected:
    int padding;
    virtual void paintEvent(QPaintEvent *);
    virtual void wheelEvent( QWheelEvent *event );
    virtual void mousePressEvent( QMouseEvent * );
    virtual void mouseMoveEvent( QMouseEvent * );
    virtual void mouseReleaseEvent( QMouseEvent * );
private:
    bool b_sliding;
    bool b_outside;
    int i_oldvalue;
    float f_step;
    void changeValue( int x );
    QPixmap pixGradient;
    QPixmap pixOutside;
};

#endif
