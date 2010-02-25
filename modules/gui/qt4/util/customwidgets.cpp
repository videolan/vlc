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
#include <QColorGroup>
#include <QRect>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QStyle>
#include <QStyleOption>
#include <vlc_intf_strings.h>
#include <vlc_keys.h>
#include <wctype.h> /* twolower() */

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

QVLCFramelessButton::QVLCFramelessButton( QWidget *parent )
  : QPushButton( parent )
{
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
}

void QVLCFramelessButton::paintEvent( QPaintEvent * event )
{
    QPainter painter( this );
    QPixmap pix = icon().pixmap( size() );
    QPoint pos( (width() - pix.width()) / 2, (height() - pix.height()) / 2 );
    painter.drawPixmap( QRect( pos.x(), pos.y(), pix.width(), pix.height() ), pix );
}

QSize QVLCFramelessButton::sizeHint() const
{
    return iconSize();
}

SearchLineEdit::SearchLineEdit( QWidget *parent ) : QLineEdit( parent )
{
    clearButton = new QVLCFramelessButton( this );
    clearButton->setIcon( QIcon( ":/toolbar/clear" ) );
    clearButton->setIconSize( QSize( 16, 16 ) );
    clearButton->setCursor( Qt::ArrowCursor );
    clearButton->setToolTip( qfu(vlc_pgettext("Tooltip|Clear", "Clear")) );
    clearButton->hide();

    CONNECT( clearButton, clicked(), this, clear() );

    int frameWidth = style()->pixelMetric( QStyle::PM_DefaultFrameWidth, 0, this );

    QFontMetrics metrics( font() );
    QString styleSheet = QString( "min-height: %1px; "
                                  "padding-top: 1px; "
                                  "padding-bottom: 1px; "
                                  "padding-right: %2px;" )
                                  .arg( metrics.height() + ( 2 * frameWidth ) )
                                  .arg( clearButton->sizeHint().width() + 1 );
    setStyleSheet( styleSheet );

    setMessageVisible( true );

    CONNECT( this, textEdited( const QString& ),
             this, updateText( const QString& ) );
}

void SearchLineEdit::clear()
{
    setText( QString() );
    clearButton->hide();
    setMessageVisible( true );
}

void SearchLineEdit::setMessageVisible( bool on )
{
    message = on;
    repaint();
    return;
}

void SearchLineEdit::updateText( const QString& text )
{
    clearButton->setVisible( !text.isEmpty() );
}

void SearchLineEdit::resizeEvent ( QResizeEvent * event )
{
  QLineEdit::resizeEvent( event );
  int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth,0,this);
  clearButton->resize( clearButton->sizeHint().width(), height() );
  clearButton->move( width() - clearButton->width() - frameWidth, 0 );
}

void SearchLineEdit::focusInEvent( QFocusEvent *event )
{
  if( message )
  {
      setMessageVisible( false );
  }
  QLineEdit::focusInEvent( event );
}

void SearchLineEdit::focusOutEvent( QFocusEvent *event )
{
  if( text().isEmpty() )
  {
      setMessageVisible( true );
  }
  QLineEdit::focusOutEvent( event );
}

void SearchLineEdit::paintEvent( QPaintEvent *event )
{
  QLineEdit::paintEvent( event );
  if( !message ) return;
  QStyleOption option;
  option.initFrom( this );
  QRect rect = style()->subElementRect( QStyle::SE_LineEditContents, &option, this )
                  .adjusted( 3, 0, clearButton->width() + 1, 0 );
  QPainter painter( this );
  painter.setPen( palette().color( QPalette::Disabled, QPalette::Text ) );
  painter.drawText( rect, Qt::AlignLeft | Qt::AlignVCenter, qtr( I_PL_FILTER ) );
}


QVLCElidingLabel::QVLCElidingLabel( const QString &s, Qt::TextElideMode mode, QWidget * parent )
  : elideMode( mode ), QLabel( s, parent )
{ }

void QVLCElidingLabel::setElideMode( Qt::TextElideMode mode )
{
    elideMode = mode;
    repaint();
}

void QVLCElidingLabel::paintEvent( QPaintEvent * event )
{
    QPainter p( this );
    int space = frameWidth() + margin();
    QRect r = rect().adjusted( space, space, -space, -space );
    p.drawText( r, fontMetrics().elidedText( text(), elideMode, r.width() ), alignment() );
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

typedef struct
{
    int      qt;
    uint32_t vlc;
} vlc_qt_key_t;

static const vlc_qt_key_t keys[] =
{
    { Qt::Key_Escape,                KEY_ESC },
    { Qt::Key_Tab,                   '\t', },
    // Qt::Key_Backtab
    { Qt::Key_Backspace,             '\b' },
    { Qt::Key_Return,                '\r' },
    { Qt::Key_Enter,                 '\r' }, // numeric pad
    { Qt::Key_Insert,                KEY_INSERT },
    { Qt::Key_Delete,                KEY_DELETE },
    // Qt::Key_Pause
    // Qt::Key_Print
    // Qt::Key_SysReq
    // Qt::Key_Clear
    { Qt::Key_Home,                  KEY_HOME },
    { Qt::Key_End,                   KEY_END },
    { Qt::Key_Left,                  KEY_LEFT },
    { Qt::Key_Up,                    KEY_UP },
    { Qt::Key_Right,                 KEY_RIGHT },
    { Qt::Key_Down,                  KEY_DOWN },
    { Qt::Key_PageUp,                KEY_PAGEUP },
    { Qt::Key_PageDown,              KEY_PAGEDOWN },
    // Qt::Key_Shift
    // Qt::Key_Control
    // Qt::Key_Meta
    // Qt::Key_Alt
    // Qt::Key_CapsLock
    // Qt::Key_NumLock
    // Qt::Key_ScrollLock
    /* F1 - F35 */
    // Qt::Key_Super_L
    // Qt::Key_Super_R
    { Qt::Key_Menu,                  KEY_MENU },
    // Qt::Key_Hyper_L
    // Qt::Key_Hyper_R
    // Qt::Key_Help
    // Qt::Key_Direction_L
    // Qt::Key_Direction_R

    // Qt::Key_Multi_key
    // Qt::Key_Codeinput
    // Qt::Key_SingleCandidate
    // Qt::Key_MultipleCandidate
    // Qt::Key_PreviousCandidate
    // Qt::Key_Mode_switch
    // Qt::Key_Kanji
    // Qt::Key_Muhenkan
    // Qt::Key_Henkan
    // Qt::Key_Romaji
    // Qt::Key_Hiragana
    // Qt::Key_Katakana
    // Qt::Key_Hiragana_Katakana
    // Qt::Key_Zenkaku
    // Qt::Key_Hankaku
    // Qt::Key_Zenkaku_Hankaku
    // Qt::Key_Touroku
    // Qt::Key_Massyo
    // Qt::Key_Kana_Lock
    // Qt::Key_Kana_Shift
    // Qt::Key_Eisu_Shift
    // Qt::Key_Eisu_toggle
    // Qt::Key_Hangul
    // Qt::Key_Hangul_Start
    // Qt::Key_Hangul_End
    // Qt::Key_Hangul_Hanja
    // Qt::Key_Hangul_Jamo
    // Qt::Key_Hangul_Romaja
    // Qt::Key_Hangul_Jeonja
    // Qt::Key_Hangul_Banja
    // Qt::Key_Hangul_PreHanja
    // Qt::Key_Hangul_PostHanja
    // Qt::Key_Hangul_Special
    // Qt::Key_Dead_Grave
    // Qt::Key_Dead_Acute
    // Qt::Key_Dead_Circumflex
    // Qt::Key_Dead_Tilde
    // Qt::Key_Dead_Macron
    // Qt::Key_Dead_Breve
    // Qt::Key_Dead_Abovedot
    // Qt::Key_Dead_Diaeresis
    // Qt::Key_Dead_Abovering
    // Qt::Key_Dead_Doubleacute
    // Qt::Key_Dead_Caron
    // Qt::Key_Dead_Cedilla
    // Qt::Key_Dead_Ogonek
    // Qt::Key_Dead_Iota
    // Qt::Key_Dead_Voiced_Sound
    // Qt::Key_Dead_Semivoiced_Sound
    // Qt::Key_Dead_Belowdot
    // Qt::Key_Dead_Hook
    // Qt::Key_Dead_Horn
    { Qt::Key_Back,                  KEY_BROWSER_BACK },
    { Qt::Key_Forward,               KEY_BROWSER_FORWARD },
    { Qt::Key_Stop,                  KEY_BROWSER_STOP },
    { Qt::Key_Refresh,               KEY_BROWSER_REFRESH },
    { Qt::Key_VolumeDown,            KEY_VOLUME_DOWN },
    { Qt::Key_VolumeMute,            KEY_VOLUME_MUTE },
    { Qt::Key_VolumeUp,              KEY_VOLUME_UP },
    // Qt::Key_BassBoost
    // Qt::Key_BassUp
    // Qt::Key_BassDown
    // Qt::Key_TrebleUp
    // Qt::Key_TrebleDown
    { Qt::Key_MediaPlay,             KEY_MEDIA_PLAY_PAUSE },
    { Qt::Key_MediaStop,             KEY_MEDIA_STOP },
    { Qt::Key_MediaPrevious,         KEY_MEDIA_PREV_TRACK },
    { Qt::Key_MediaNext,             KEY_MEDIA_NEXT_TRACK },
    // Qt::Key_MediaRecord
    { Qt::Key_HomePage,              KEY_BROWSER_HOME },
    { Qt::Key_Favorites,             KEY_BROWSER_FAVORITES },
    { Qt::Key_Search,                KEY_BROWSER_SEARCH },
    // Qt::Key_Standby
    // Qt::Key_OpenUrl
    // Qt::Key_LaunchMail
    // Qt::Key_LaunchMedia
    /* Qt::Key_Launch0 through Qt::Key_LaunchF */
    // Qt::Key_MediaLast
};

static int keycmp( const void *a, const void *b )
{
    const int *q = (const int *)a;
    const vlc_qt_key_t *m = (const vlc_qt_key_t *)b;

    return *q - m->qt;
}

int qtEventToVLCKey( QKeyEvent *e )
{
    int qtk = e->key();
    uint32_t i_vlck = 0;

    if( qtk <= 0xff )
        /* VLC and X11 use lowercase whereas Qt uses uppercase */
#if defined( __STDC_ISO_10646__ ) || defined( _WIN32 )
        i_vlck = towlower( qtk );
#else
# error FIXME
#endif
    else /* Qt and X11 go to F35, but VLC stops at F12 */
    if( qtk >= Qt::Key_F1 && qtk <= Qt::Key_F12 )
        i_vlck = qtk - Qt::Key_F1 + KEY_F1;
    else
    {
        const vlc_qt_key_t *map;

        map = (const vlc_qt_key_t *)
              bsearch( &qtk, (const void *)keys, sizeof(keys)/sizeof(keys[0]),
                       sizeof(*keys), keycmp );
        if( map != NULL )
            i_vlck = map->vlc;
    }

    /* Handle modifiers */
    i_vlck |= qtKeyModifiersToVLC( e );
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
    if( val & KEY_MODIFIER_META )
        r+= qfu( "Meta+" );

    if (base)
    {
        r += qfu( base );
        free( base );
    }
    else
        r += qtr( "Unset" );
    return r;
}

