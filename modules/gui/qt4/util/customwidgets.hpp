/*****************************************************************************
 * customwidgets.hpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * Copyright (C) 2004 Daniel Molkentin <molkentin@kde.org>
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 * The "ClickLineEdit" control is based on code by  Daniel Molkentin
 * <molkentin@kde.org> for libkdepim
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

#ifndef _CUSTOMWIDGETS_H_
#define _CUSTOMWIDGETS_H_

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QList>
#include <QTimer>
#include <QToolButton>
#include <QAbstractAnimation>

class QPixmap;
class QWidget;

class QFramelessButton : public QPushButton
{
    Q_OBJECT
public:
    QFramelessButton( QWidget *parent = NULL );
    virtual QSize sizeHint() const { return iconSize(); }
protected:
    virtual void paintEvent( QPaintEvent * event );
};

class QToolButtonExt : public QToolButton
{
    Q_OBJECT
public:
    QToolButtonExt( QWidget *parent = 0, int ms = 0 );
private:
    bool shortClick;
    bool longClick;
private slots:
    void releasedSlot();
    void clickedSlot();
signals:
    void shortClicked();
    void longClicked();
};

class QElidingLabel : public QLabel
{
public:
    QElidingLabel( const QString &s = QString(),
                      Qt::TextElideMode mode = Qt::ElideRight,
                      QWidget * parent = NULL );
    void setElideMode( Qt::TextElideMode );
protected:
    virtual void paintEvent( QPaintEvent * event );
private:
    Qt::TextElideMode elideMode;
};


class QVLCStackedWidget : public QStackedWidget
{
public:
    QVLCStackedWidget( QWidget *parent ) : QStackedWidget( parent ) { }
    QSize minimumSizeHint () const
    {
        return currentWidget() ? currentWidget()->minimumSizeHint() : QSize();
    }
};

class QVLCDebugLevelSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    QVLCDebugLevelSpinBox( QWidget *parent ) : QSpinBox( parent ) { };
protected:
    virtual QString textFromValue( int ) const;
    /* QVLCDebugLevelSpinBox is read-only */
    virtual int valueFromText( const QString& ) const { return -1; }
};

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
    void setFps( int _fps ) { fps = _fps; interval = 1000.0 / fps; };
    virtual int duration() const { return interval * pixmaps.count(); };
    virtual ~PixmapAnimator() { qDeleteAll( pixmaps ); };
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

/** This spinning icon, to the colors of the VLC cone, will show
 * that there is some background activity running
 **/
class SpinningIcon : public QLabel
{
    Q_OBJECT

public:
    SpinningIcon( QWidget *parent );
    void play( int loops = -1, int fps = 0 )
    {
        animator->setLoopCount( loops );
        if ( fps ) animator->setFps( fps );
        animator->start();
    }
    void stop() { animator->stop(); }
    bool isPlaying() { return animator->state() == PixmapAnimator::Running; }
private:
    PixmapAnimator *animator;
};

class YesNoCheckBox : public QCheckBox
{
    Q_OBJECT
public:
    YesNoCheckBox( QWidget *parent );
};

/* VLC Key/Wheel hotkeys interactions */

class QKeyEvent;
class QWheelEvent;
class QInputEvent;

int qtKeyModifiersToVLC( QInputEvent* e );
int qtEventToVLCKey( QKeyEvent *e );
int qtWheelEventToVLCKey( QWheelEvent *e );
QString VLCKeyToString( unsigned val );

#endif
