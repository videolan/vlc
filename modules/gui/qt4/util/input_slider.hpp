/*****************************************************************************
 * input_slider.hpp : VolumeSlider and SeekSlider
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ludovic Fauvet <etix@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "timetooltip.hpp"
#include "styles/seekstyle.hpp"

#include <QSlider>
#include <QPainter>

#define MSTRTIME_MAX_SIZE 22

class QMouseEvent;
class QWheelEvent;
class QHideEvent;
class QTimer;
class SeekPoints;
class QPropertyAnimation;
class QStyleOption;
class QCommonStyle;

/* Input Slider derived from QSlider */
class SeekSlider : public QSlider
{
    Q_OBJECT
    Q_PROPERTY(qreal handleOpacity READ handleOpacity WRITE setHandleOpacity)
public:
    SeekSlider( Qt::Orientation q, QWidget *_parent = 0, bool _classic = false );
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

    virtual bool eventFilter( QObject *obj, QEvent *event );

    virtual QSize sizeHint() const;

    void processReleasedButton();
    bool isAnimationRunning() const;
    qreal handleOpacity() const;
    void setHandleOpacity( qreal opacity );
    int handleLength();

private:
    bool isSliding;        /* Whether we are currently sliding by user action */
    bool isJumping;              /* if we requested a jump to another chapter */
    int inputLength;                           /* InputLength that can change */
    char psz_length[MSTRTIME_MAX_SIZE];               /* Used for the ToolTip */
    QTimer *seekLimitTimer;
    TimeTooltip *mTimeTooltip;
    float f_buffering;
    SeekPoints* chapters;
    bool b_classic;
    bool b_seekable;
    int mHandleLength;

    /* Colors & gradients */
    QSize gradientsTargetSize;
    QLinearGradient backgroundGradient;
    QLinearGradient foregroundGradient;
    QLinearGradient handleGradient;
    QColor tickpointForeground;
    QColor shadowDark;
    QColor shadowLight;
    QCommonStyle *alternativeStyle;

    /* Handle's animation */
    qreal mHandleOpacity;
    QPropertyAnimation *animHandle;
    QTimer *hideHandleTimer;

public slots:
    void setPosition( float, int64_t, int );
    void setSeekable( bool b ) { b_seekable = b ; }
    void updateBuffering( float );
    void hideHandle();

private slots:
    void startSeekTimer();
    void updatePos();

signals:
    void sliderDragged( float );


    friend class SeekStyle;
};

/* Sound Slider inherited directly from QAbstractSlider */
class QPaintEvent;

#define SOUNDMAX  125 // % (+6dB)

class SoundSlider : public QAbstractSlider
{
    Q_OBJECT
public:
    SoundSlider(QWidget *_parent, float _i_step, char *psz_colors, int max = SOUNDMAX );
    void setMuted( bool ); /* Set Mute status */

protected:
    const static int paddingL = 3;
    const static int paddingR = 2;

    virtual void paintEvent( QPaintEvent *);
    virtual void wheelEvent( QWheelEvent *event );
    virtual void mousePressEvent( QMouseEvent * );
    virtual void mouseMoveEvent( QMouseEvent * );
    virtual void mouseReleaseEvent( QMouseEvent * );

    void processReleasedButton();

private:
    bool isSliding; /* Whether we are currently sliding by user action */
    bool b_mouseOutside; /* Whether the mouse is outside or inside the Widget */
    int i_oldvalue; /* Store the old Value before changing */
    float f_step; /* How much do we increase each time we wheel */
    bool b_isMuted;

    QPixmap pixGradient; /* Gradient pix storage */
    QPixmap pixGradient2; /* Muted Gradient pix storage */
    QPixmap pixOutside; /* OutLine pix storage */
    QPainter painter;
    QColor background;
    QColor foreground;
    QFont textfont;
    QRect textrect;

    void changeValue( int x ); /* Function to modify the value from pixel x() */
};

#endif
