/*****************************************************************************
 * input_slider.hpp : A slider that controls an input
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include "qt4.hpp"

#include <QAbstractSlider>
#include <QSlider>

#include <QMouseEvent>
#include <QWheelEvent>

/* Input Slider derived from QSlider */
class InputSlider : public QSlider
{
    Q_OBJECT
public:
    InputSlider( QWidget *_parent );
    InputSlider( Qt::Orientation q,QWidget *_parent );
    virtual ~InputSlider()   {};
protected:
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void wheelEvent(QWheelEvent *event);
private:
    bool b_sliding;
    int inputLength;
    char psz_length[MSTRTIME_MAX_SIZE];
public slots:
    void setPosition( float, int, int );
private slots:
    void userDrag( int );
signals:
    void sliderDragged( float );
};


/* Sound Slider inherited directly from QAbstractSlider */

class QPaintEvent;

class SoundSlider : public QAbstractSlider
{
    Q_OBJECT
public:
    SoundSlider( QWidget *_parent, int _i_step, bool b_softamp, char * );
    virtual ~SoundSlider() {};

protected:
    const static int paddingL = 3;
    const static int paddingR = 2;
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
