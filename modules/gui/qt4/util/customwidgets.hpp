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
#include <QToolButton>
#include <QDial>

#include "animators.hpp"

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

class VLCQDial : public QDial
{
    Q_OBJECT
public:
    VLCQDial( QWidget *parent = NULL );
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
QString VLCKeyToString( unsigned val, bool );

#endif
