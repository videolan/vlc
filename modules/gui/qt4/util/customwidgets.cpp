/*****************************************************************************
 * customwidgets.cpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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
#include "qt4.hpp"               /* needed for qtr,  but not necessary */

#include <QPainter>
#include <QRect>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QPixmap>
#include <QApplication>
#include <vlc_keys.h>

QFramelessButton::QFramelessButton( QWidget *parent )
                    : QPushButton( parent )
{
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
}

void QFramelessButton::paintEvent( QPaintEvent * )
{
    QPainter painter( this );
    QPixmap pix = icon().pixmap( size() );
    QPoint pos( (width() - pix.width()) / 2, (height() - pix.height()) / 2 );
    painter.drawPixmap( QRect( pos.x(), pos.y(), pix.width(), pix.height() ), pix );
}

QElidingLabel::QElidingLabel( const QString &s, Qt::TextElideMode mode, QWidget * parent )
                 : QLabel( s, parent ), elideMode( mode )
{ }

void QElidingLabel::setElideMode( Qt::TextElideMode mode )
{
    elideMode = mode;
    repaint();
}

void QElidingLabel::paintEvent( QPaintEvent * )
{
    QPainter p( this );
    int space = frameWidth() + margin();
    QRect r = rect().adjusted( space, space, -space, -space );
    p.drawText( r, fontMetrics().elidedText( text(), elideMode, r.width() ), alignment() );
}

QString QVLCDebugLevelSpinBox::textFromValue( int v ) const
{
    QString const texts[] = {
    /* Note that min level 0 is 'errors' in Qt Ui
       FIXME: fix debug levels accordingly to documentation */
    /*  qtr("infos"),*/
        qtr("errors"),
        qtr("warnings"),
        qtr("debug")
    };
    if ( v < 0 ) v = 0;
    if ( v >= 2 ) v = 2;

    return QString( "%1 (%2)" ).arg( v ).arg( texts[v] );
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
    /* F1 - F35 - Qt goes to F35, VLC stops at F12 */
    { Qt::Key_F1,                    KEY_F1 },
    { Qt::Key_F2,                    KEY_F2 },
    { Qt::Key_F3,                    KEY_F3 },
    { Qt::Key_F4,                    KEY_F4 },
    { Qt::Key_F5,                    KEY_F5 },
    { Qt::Key_F6,                    KEY_F6 },
    { Qt::Key_F7,                    KEY_F7 },
    { Qt::Key_F8,                    KEY_F8 },
    { Qt::Key_F9,                    KEY_F9 },
    { Qt::Key_F10,                   KEY_F10 },
    { Qt::Key_F11,                   KEY_F11 },
    { Qt::Key_F12,                   KEY_F12 },
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
    {
        /* VLC and X11 use lowercase whereas Qt uses uppercase, this
         * method should be equal to towlower in case of latin1 */
        if( qtk >= 'A' && qtk <= 'Z' ) i_vlck = qtk+32;
        else if( qtk >= 0xC0 && qtk <= 0xDE && qtk != 0xD7) i_vlck = qtk+32;
        else i_vlck = qtk;
    }
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

QString VLCKeyToString( unsigned val )
{
    char *base = vlc_keycode2str (val);
    if (base == NULL)
        return qtr( "Unset" );

    QString r = qfu( base );

    free( base );
    return r;
}


/* Animated Icon implementation */

AnimatedIcon::AnimatedIcon( QWidget *parent )
    : QLabel( parent ), mTimer( this ), mIdleFrame( NULL )
{
    mCurrentFrame = mRemainingLoops = 0;
    connect( &mTimer, SIGNAL( timeout() ), this, SLOT( onTimerTick() ) );
}

AnimatedIcon::~AnimatedIcon()
{
    // We don't need to destroy the timer, he's our child
    delete mIdleFrame;
    foreach( QPixmap *frame, mFrames )
        delete frame;
}

void AnimatedIcon::addFrame( const QPixmap &pxm, int index )
{
    if( index == 0 )
    {
        // Replace idle frame
        delete mIdleFrame;
        mIdleFrame = new QPixmap( pxm );
        setPixmap( *mIdleFrame );
        return;
    }
    QPixmap *copy = new QPixmap( pxm );
    mFrames.insert( ( index < 0 || index > mFrames.count() ) ? mFrames.count() :
                    index, copy );
    if( !pixmap() )
        setPixmap( *copy );
}

void AnimatedIcon::play( int loops, int interval )
{
    if( interval < 20 )
    {
        interval = 20;
    }

    if( !mIdleFrame && (mFrames.isEmpty() || loops != 0 ) )
    {
        return;
    }

    if( loops == 0 )
    {
        // Stop playback
        mCurrentFrame = mRemainingLoops = 0;
        mTimer.stop();
        setPixmap( mIdleFrame != NULL ? *mIdleFrame : *mFrames.last() );
        return;
    }

    if( loops <= -1 )
        loops = -1;

    mCurrentFrame = 1;
    mRemainingLoops = loops;
    mTimer.start( interval );
    setPixmap( *mFrames.first() );
}

// private slot
void AnimatedIcon::onTimerTick()
{
    //assert( !mFrames.isEmpty() );
    if( ++mCurrentFrame > mFrames.count() )
    {
        if( mRemainingLoops != -1 )
        {
            if( --mRemainingLoops == 0 )
            {
                mTimer.stop();
                setPixmap( mIdleFrame ? *mIdleFrame : *mFrames.last() );
                return;
            }
        }
        mCurrentFrame = 1;
    }
    //assert( mCurrentFrame >= 1 && mCurrentFrame <= mFrames.count() );
    setPixmap( *mFrames.at( mCurrentFrame - 1 ) );
}


/* SpinningIcon implementation */

SpinningIcon::SpinningIcon( QWidget *parent, bool noIdleFrame )
    : AnimatedIcon( parent )
{
    if( noIdleFrame )
        addFrame( QPixmap(), 0 );
    else
        addFrame( QPixmap( ":/util/wait0" ), 0 );
    addFrame( QPixmap( ":/util/wait1" ) );
    addFrame( QPixmap( ":/util/wait2" ) );
    addFrame( QPixmap( ":/util/wait3" ) );
    addFrame( QPixmap( ":/util/wait4" ) );
    setScaledContents( true );
    setFixedSize( 16, 16 );
}

QToolButtonExt::QToolButtonExt(QWidget *parent, int ms )
    : QToolButton( parent ),
      shortClick( false ),
      longClick( false )
{
    setAutoRepeat( true );
    /* default to twice the doubleclick delay */
    setAutoRepeatDelay( ( ms > 0 )? ms : 2 * QApplication::doubleClickInterval() );
    setAutoRepeatInterval( 100 );
    connect( this, SIGNAL(released()), this, SLOT(releasedSlot()) );
    connect( this, SIGNAL(clicked()), this, SLOT(clickedSlot()) );
}

/* table illustrating the different scenarios and the events generated
 * ====================
 *
 *  event     isDown()
 *
 *  released  false   }
 *  clicked   false   }= short click
 *
 *  released  false    = cancelled click (mouse released outside of button area,
 *                                        before long click delay kicks in)
 *
 *  released  true    }
 *  clicked   true    }= long click (multiple of these generated)
 *  released  false    = stop long click (mouse released / moved outside of
 *                                        button area)
 * (clicked   false)   = stop long click (additional event if mouse released
 *                                        inside of button area)
 */

void QToolButtonExt::releasedSlot()
{
    if( isDown() )
    {
        // we are beginning a long click
        longClick = true;
        shortClick = false;
    }
    else
    {
        if( longClick )
        {
            // we are stopping a long click
            longClick = false;
            shortClick = false;
        }
        else
        {
            // we are generating a short click
            longClick = false;
            shortClick = true;
        }
    }
}

void QToolButtonExt::clickedSlot()
{
    if( longClick )
        emit longClicked();
    else if( shortClick )
        emit shortClicked();
}
