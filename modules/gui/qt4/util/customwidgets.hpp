/*****************************************************************************
 * customwidgets.h: Custom widgets
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

class QVLCFramelessButton : public QPushButton
{
    Q_OBJECT
public:
    QVLCFramelessButton( QWidget *parent = NULL );
    virtual QSize sizeHint() const { return iconSize(); }
protected:
    virtual void paintEvent( QPaintEvent * event );
};


class QVLCElidingLabel : public QLabel
{
public:
    QVLCElidingLabel( const QString &s = QString(),
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

class DebugLevelSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    DebugLevelSpinBox( QWidget *parent ) : QSpinBox( parent ) { };
protected:
    virtual QString textFromValue( int ) const;
    /* DebugLevelSpinBox is read-only */
    virtual int valueFromText( const QString& ) const { return -1; }
};

/* VLC Key/Wheel hotkeys interactions */

class QKeyEvent;
class QWheelEvent;
class QInputEvent;

int qtKeyModifiersToVLC( QInputEvent* e );
int qtEventToVLCKey( QKeyEvent *e );
int qtWheelEventToVLCKey( QWheelEvent *e );
QString VLCKeyToString( int val );

#endif

