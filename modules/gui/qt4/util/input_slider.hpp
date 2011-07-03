/*****************************************************************************
 * input_slider.hpp : VolumeSlider and SeekSlider
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _INPUTSLIDER_H_
#define _INPUTSLIDER_H_

#include <vlc_common.h>
#include "timetooltip.hpp"

#include <QSlider>

#define MSTRTIME_MAX_SIZE 22

class QMouseEvent;
class QWheelEvent;
class QHideEvent;
class QTimer;
class SeekPoints;

/* Input Slider derived from QSlider */
class SeekSlider : public QSlider
{
    Q_OBJECT
public:
    SeekSlider( QWidget *_parent );
    SeekSlider( Qt::Orientation q, QWidget *_parent );
    ~SeekSlider();
    void setChapters( SeekPoints * );

protected:
    virtual void mouseMoveEvent( QMouseEvent *event );
    virtual void mousePressEvent( QMouseEvent* event );
    virtual void mouseReleaseEvent( QMouseEvent *event );
    virtual void wheelEvent( QWheelEvent *event );
    virtual void enterEvent( QEvent * );
    virtual void leaveEvent( QEvent * );
    virtual void hideEvent( QHideEvent * );

    virtual void paintEvent( QPaintEvent* event );
    virtual bool eventFilter( QObject *obj, QEvent *event );

    QSize handleSize() const;
    QSize sizeHint() const;

private:
    bool b_isSliding;       /* Whether we are currently sliding by user action */
    bool b_is_jumping;      /* if we requested a jump to another chapter */
    int inputLength;        /* InputLength that can change */
    char psz_length[MSTRTIME_MAX_SIZE]; /* Used for the ToolTip */
    QTimer *seekLimitTimer;
    TimeTooltip *mTimeTooltip;
    float f_buffering;
    SeekPoints* chapters;

public slots:
    void setPosition( float, int64_t, int );
    void updateBuffering( float );

private slots:
    void startSeekTimer();
    void updatePos();

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
    void setMuted( bool ); /* Set Mute status */

protected:
    const static int paddingL = 3;
    const static int paddingR = 2;

    virtual void paintEvent( QPaintEvent *);
    virtual void wheelEvent( QWheelEvent *event );
    virtual void mousePressEvent( QMouseEvent * );
    virtual void mouseMoveEvent( QMouseEvent * );
    virtual void mouseReleaseEvent( QMouseEvent * );

private:
    bool b_isSliding; /* Whether we are currently sliding by user action */
    bool b_mouseOutside; /* Whether the mouse is outside or inside the Widget */
    int i_oldvalue; /* Store the old Value before changing */
    float f_step; /* How much do we increase each time we wheel */
    bool b_isMuted;

    QPixmap pixGradient; /* Gradient pix storage */
    QPixmap pixGradient2; /* Muted Gradient pix storage */
    QPixmap pixOutside; /* OutLine pix storage */

    void changeValue( int x ); /* Function to modify the value from pixel x() */
};

#endif
