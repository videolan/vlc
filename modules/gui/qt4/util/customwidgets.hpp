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


/*****************************************************************
 * Custom views
 *****************************************************************/
#include <QMouseEvent>
#include <QTreeView>
#include <QCursor>
#include <QPoint>
#include <QModelIndex>

class QVLCTreeView : public QTreeView
{
    Q_OBJECT;
public:
    QVLCTreeView( QWidget * parent ) : QTreeView( parent )
    {
    };
    virtual ~QVLCTreeView()   {};

    void mouseReleaseEvent(QMouseEvent* e )
    {
        if( e->button() & Qt::RightButton )
        {
            emit rightClicked( indexAt( QPoint( e->x(), e->y() ) ),
                               QCursor::pos() );
        }
    }
signals:
    void rightClicked( QModelIndex, QPoint  );
};

class QKeyEvent;
class QWheelEvent;
int qtKeyModifiersToVLC( QInputEvent* e );
int qtEventToVLCKey( QKeyEvent *e );
int qtWheelEventToVLCKey( QWheelEvent *e );
QString VLCKeyToString( int val );

#endif
