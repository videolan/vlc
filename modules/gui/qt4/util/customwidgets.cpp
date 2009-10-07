/*****************************************************************************
 * customwidgets.cpp: Custom widgets
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "customwidgets.hpp"
#include "qt4.hpp" /*needed for qtr and CONNECT, but not necessary */

#include <QPainter>
#include <QLineEdit>
#include <QColorGroup>
#include <QRect>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QToolButton>
#include <QHBoxLayout>
#include <vlc_intf_strings.h>


#include <vlc_keys.h>

ClickLineEdit::ClickLineEdit( const QString &msg, QWidget *parent) : QLineEdit( parent )
{
    mDrawClickMsg = true;
    setClickMessage( msg );
}

void ClickLineEdit::setClickMessage( const QString &msg )
{
    mClickMessage = msg;
    repaint();
}


void ClickLineEdit::setText( const QString &txt )
{
    mDrawClickMsg = txt.isEmpty();
    repaint();
    QLineEdit::setText( txt );
}

void ClickLineEdit::paintEvent( QPaintEvent *pe )
{
    QLineEdit::paintEvent( pe );
    if ( mDrawClickMsg == true && !hasFocus() ) {
        QPainter p( this );
        QPen tmp = p.pen();
        p.setPen( palette().color( QPalette::Disabled, QPalette::Text ) );
        QRect cr = contentsRect();
        // Add two pixel margin on the left side
        cr.setLeft( cr.left() + 3 );
        p.drawText( cr, Qt::AlignLeft | Qt::AlignVCenter, mClickMessage );
        p.setPen( tmp );
        p.end();
    }
}

void ClickLineEdit::dropEvent( QDropEvent *ev )
{
    mDrawClickMsg = false;
    QLineEdit::dropEvent( ev );
}

void ClickLineEdit::focusInEvent( QFocusEvent *ev )
{
    if ( mDrawClickMsg == true ) {
        mDrawClickMsg = false;
        repaint();
    }
    QLineEdit::focusInEvent( ev );
}

void ClickLineEdit::focusOutEvent( QFocusEvent *ev )
{
    if ( text().isEmpty() ) {
        mDrawClickMsg = true;
        repaint();
    }
    QLineEdit::focusOutEvent( ev );
}

SearchLineEdit::SearchLineEdit( QWidget *parent ) : QFrame( parent )
{
    setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
    setLineWidth( 0 );

    QHBoxLayout *frameLayout = new QHBoxLayout( this );
    frameLayout->setMargin( 0 );
    frameLayout->setSpacing( 0 );

    QPalette palette;
    QBrush brush( QColor(255, 255, 255, 255) );
    brush.setStyle(Qt::SolidPattern);
    palette.setBrush(QPalette::Active, QPalette::Window, brush); //Qt::white

    setPalette(palette);
    setAutoFillBackground(true);

    searchLine = new  ClickLineEdit( qtr(I_PL_FILTER), 0 );
    searchLine->setFrame( false );
    searchLine->setMinimumWidth( 80 );

    CONNECT( searchLine, textChanged( const QString& ),
             this, updateText( const QString& ) );
    frameLayout->addWidget( searchLine );

    clearButton = new QToolButton;
    clearButton->setAutoRaise( true );
    clearButton->setMaximumWidth( 30 );
    clearButton->setIcon( QIcon( ":/toolbar/clear" ) );
    clearButton->setToolTip( qfu(vlc_pgettext("Tooltip|Clear", "Clear")) );
    clearButton->hide();

    CONNECT( clearButton, clicked(), searchLine, clear() );
    frameLayout->addWidget( clearButton );
}

void SearchLineEdit::updateText( const QString& text )
{
    clearButton->setVisible( !text.isEmpty() );
    emit textChanged( text );
}

/***************************************************************************
 * Hotkeys converters
 ***************************************************************************/
int qtKeyModifiersToVLC( QInputEvent* e )
{
    int i_keyModifiers = 0;
    if( e->modifiers() & Qt::ShiftModifier ) i_keyModifiers |= KEY_MODIFIER_SHIFT;
    if( e->modifiers() & Qt::AltModifier ) i_keyModifiers |= KEY_MODIFIER_ALT;
    if( e->modifiers() & Qt::ControlModifier ) i_keyModifiers |= KEY_MODIFIER_CTRL;
    if( e->modifiers() & Qt::MetaModifier ) i_keyModifiers |= KEY_MODIFIER_META;
    return i_keyModifiers;
}

int qtEventToVLCKey( QKeyEvent *e )
{
    int i_vlck = 0;
    /* Handle modifiers */
    i_vlck |= qtKeyModifiersToVLC( e );

    bool found = false;
    /* Look for some special keys */
#define HANDLE( qt, vk ) case Qt::qt : i_vlck |= vk; found = true;break
    switch( e->key() )
    {
        HANDLE( Key_Left, KEY_LEFT );
        HANDLE( Key_Right, KEY_RIGHT );
        HANDLE( Key_Up, KEY_UP );
        HANDLE( Key_Down, KEY_DOWN );
        HANDLE( Key_Space, ' ' );
        HANDLE( Key_Escape, KEY_ESC );
        HANDLE( Key_Return, KEY_ENTER );
        HANDLE( Key_Enter, KEY_ENTER );
        HANDLE( Key_F1, KEY_F1 );
        HANDLE( Key_F2, KEY_F2 );
        HANDLE( Key_F3, KEY_F3 );
        HANDLE( Key_F4, KEY_F4 );
        HANDLE( Key_F5, KEY_F5 );
        HANDLE( Key_F6, KEY_F6 );
        HANDLE( Key_F7, KEY_F7 );
        HANDLE( Key_F8, KEY_F8 );
        HANDLE( Key_F9, KEY_F9 );
        HANDLE( Key_F10, KEY_F10 );
        HANDLE( Key_F11, KEY_F11 );
        HANDLE( Key_F12, KEY_F12 );
        HANDLE( Key_PageUp, KEY_PAGEUP );
        HANDLE( Key_PageDown, KEY_PAGEDOWN );
        HANDLE( Key_Home, KEY_HOME );
        HANDLE( Key_End, KEY_END );
        HANDLE( Key_Insert, KEY_INSERT );
        HANDLE( Key_Delete, KEY_DELETE );
        HANDLE( Key_VolumeDown, KEY_VOLUME_DOWN);
        HANDLE( Key_VolumeUp, KEY_VOLUME_UP );
        HANDLE( Key_VolumeMute, KEY_VOLUME_MUTE );
        HANDLE( Key_MediaPlay, KEY_MEDIA_PLAY_PAUSE );
        HANDLE( Key_MediaStop, KEY_MEDIA_STOP );
        HANDLE( Key_MediaPrevious, KEY_MEDIA_PREV_TRACK );
        HANDLE( Key_MediaNext, KEY_MEDIA_NEXT_TRACK );

    }
    if( !found )
    {
        /* Force lowercase */
        if( e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z )
            i_vlck += e->key() + 32;
        /* Rest of the ascii range */
        else if( e->key() >= Qt::Key_Space && e->key() <= Qt::Key_AsciiTilde )
            i_vlck += e->key();
    }
    return i_vlck;
}

int qtWheelEventToVLCKey( QWheelEvent *e )
{
    int i_vlck = 0;
    /* Handle modifiers */
    i_vlck |= qtKeyModifiersToVLC( e );
    if ( e->delta() > 0 )
        i_vlck |= KEY_MOUSEWHEELUP;
    else
        i_vlck |= KEY_MOUSEWHEELDOWN;
    return i_vlck;
}

QString VLCKeyToString( int val )
{
    char *base = KeyToString (val & ~KEY_MODIFIER);

    QString r = "";
    if( val & KEY_MODIFIER_CTRL )
        r+= qfu( "Ctrl+" );
    if( val & KEY_MODIFIER_ALT )
        r+= qfu( "Alt+" );
    if( val & KEY_MODIFIER_SHIFT )
        r+= qfu( "Shift+" );

    if (base)
    {
        r += qfu( base );
        free( base );
    }
    else
        r += qtr( "Unset" );
    return r;
}

