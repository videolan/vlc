/*****************************************************************************
 * searchlineedit.hpp: Custom widgets
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

#ifndef _SEARCHLINEEDIT_H_
#define _SEARCHLINEEDIT_H_

#ifdef __APPLE__
#include "../qt4.hpp"
#else
#include "qt4.hpp"
#endif
#include <QLineEdit>

#if HAS_QT47
class ClickLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    ClickLineEdit( const QString &msg, QWidget *parent ) : QLineEdit( parent )
    {
        QLineEdit::setPlaceholderText ( msg );
    }
};
#else
/**
  This class provides a QLineEdit which contains a greyed-out hinting
  text as long as the user didn't enter any text

  @short LineEdit with customizable "Click here" text
  @author Daniel Molkentin
*/
class ClickLineEdit : public QLineEdit
{
    Q_OBJECT
    Q_PROPERTY( QString clickMessage READ placeholderText WRITE setPlaceholderText )
public:
    ClickLineEdit( const QString &msg, QWidget *parent );
    void setPlaceholderText( const QString &msg );
    const QString& placeholderText() const { return mClickMessage; }
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
#endif

#ifndef Q_OS_MAC
class QFramelessButton;
class SearchLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    SearchLineEdit( QWidget *parent = NULL );

private:
    void resizeEvent ( QResizeEvent * event );
    void focusInEvent( QFocusEvent *event );
    void focusOutEvent( QFocusEvent *event );
    void paintEvent( QPaintEvent *event );
    void setMessageVisible( bool on );
    QFramelessButton   *clearButton;
    bool message;

public slots:
    void clear();

private slots:
    void updateText( const QString& );
    void searchEditingFinished();

signals:
    void searchDelayedChanged( const QString& );
};
#else

/* On Mac, we try to use the native NSSearchField */
#include <QMacCocoaViewContainer>

class SearchLineEdit : public QMacCocoaViewContainer
{
    Q_OBJECT

public:
    SearchLineEdit(QWidget *parent = 0);
    virtual ~SearchLineEdit() {}

    virtual QSize sizeHint() const { return QSize(150, 40); }

public slots:
    void clear() {}

signals:
    void searchDelayedChanged( const QString& );
    void textEdited( const QString& );
};
#endif

#endif

