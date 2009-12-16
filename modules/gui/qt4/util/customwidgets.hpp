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
#include <QLabel>
#include <QIcon>

/**
  This class provides a QLineEdit which contains a greyed-out hinting
  text as long as the user didn't enter any text

  @short LineEdit with customizable "Click here" text
  @author Daniel Molkentin
*/
class ClickLineEdit : public QLineEdit
{
    Q_OBJECT
    Q_PROPERTY( QString clickMessage READ clickMessage WRITE setClickMessage )
public:
    ClickLineEdit( const QString &msg, QWidget *parent );
    virtual ~ClickLineEdit() {};
    void setClickMessage( const QString &msg );
    QString clickMessage() const { return mClickMessage; }
    virtual void setText( const QString& txt );
protected:
    virtual void paintEvent( QPaintEvent *e );
    virtual void dropEvent( QDropEvent *ev );
    virtual void focusInEvent( QFocusEvent *ev );
    virtual void focusOutEvent( QFocusEvent *ev );
private:
    QString mClickMessage;
    bool mDrawClickMsg;
};

class QToolButton;
class SearchLineEdit : public QFrame
{
    Q_OBJECT
public:
    SearchLineEdit( QWidget *parent );

private:
    ClickLineEdit *searchLine;
    QToolButton   *clearButton;

private slots:
    void updateText( const QString& );

signals:
    void textChanged( const QString& );
};

class QVLCIconLabel : public QLabel
{
    Q_OBJECT
public:
    QVLCIconLabel( const QIcon&, QWidget *parent = 0 );
    void setIcon( const QIcon& );
signals:
    void clicked();
protected:
    virtual void enterEvent( QEvent * );
    virtual void leaveEvent( QEvent * );
    virtual void mouseReleaseEvent( QMouseEvent * );
    virtual void resizeEvent( QResizeEvent * );
private:
    inline void updatePixmap( );
    QIcon icon;
    QIcon::Mode iconMode;
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

