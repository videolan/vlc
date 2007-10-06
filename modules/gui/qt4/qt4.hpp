/*****************************************************************************
 * qt4.hpp : QT4 interface
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef _QVLC_H_
#define _QVLC_H_

#include <vlc/vlc.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>

#include <QEvent>

#define HAS_QT43 ( QT_VERSION >= 0x040300 )

/* Add define for duration, VLC_META_ENGINE doesn't include it */
#define VLC_META_ENGINE_DURATION   0x00000002
#define VLC_META_DURATION          N_("Duration")

class QApplication;
class QMenu;
class MainInterface;
class DialogsProvider;
class VideoWidget;

struct intf_sys_t
{
    QApplication *p_app;
    MainInterface *p_mi;
    playlist_t *p_playlist;
    msg_subscription_t *p_sub; ///< Subscription to the message bank

    VideoWidget *p_video;
    int i_saved_height, i_saved_width;

    QMenu * p_popup_menu;
};

#define THEPL p_intf->p_sys->p_playlist
#define QPL_LOCK vlc_mutex_lock( &THEPL->object_lock );
#define QPL_UNLOCK vlc_mutex_unlock( &THEPL->object_lock );

#define THEDP DialogsProvider::getInstance()
#define THEMIM MainInputManager::getInstance( p_intf )

#define qfu( i ) QString::fromUtf8( i )
#define qtr( i ) QString::fromUtf8( _(i) )
#define qtu( i ) i.toUtf8().data()
#define qta( i ) i.toAscii().data()

#define CONNECT( a, b, c, d ) connect( a, SIGNAL( b ), c, SLOT(d) )
#define BUTTONACT( b, a ) connect( b, SIGNAL( clicked() ), this, SLOT(a) )
#define ON_TIMEOUT( act ) CONNECT( THEDP->fixed_timer, timeout(), this, act )

#define BUTTON_SET( button, text, tooltip )  \
    button->setText( text );                 \
    button->setToolTip( tooltip );

#define BUTTON_SET_ACT( button, text, tooltip, thisslot ) \
    BUTTON_SET( button, text, tooltip );                  \
    BUTTONACT( button, thisslot );

#define BUTTON_SET_IMG( button, text, image, tooltip )    \
    BUTTON_SET( button, text, tooltip );                  \
    button->setIcon( QIcon( ":/pixmaps/"#image ) );

#define BUTTON_SET_ACT_I( button, text, image, tooltip, thisslot ) \
    BUTTON_SET_IMG( button, text, image, tooltip );                \
    BUTTONACT( button, thisslot );

#define VISIBLE(i) (i && i->isVisible())

#define TOGGLEV( x ) { if( x->isVisible() ) x->hide();          \
            else  x->show(); }

static int DialogEvent_Type = QEvent::User + 1;
static int PLUndockEvent_Type = QEvent::User + 2;
static int PLDockEvent_Type = QEvent::User + 3;
static int SetVideoOnTopEvent_Type = QEvent::User + 4;

class DialogEvent : public QEvent
{
public:
    DialogEvent( int _i_dialog, int _i_arg, intf_dialog_args_t *_p_arg ) :
                 QEvent( (QEvent::Type)(DialogEvent_Type) )
    {
        i_dialog = _i_dialog;
        i_arg = _i_arg;
        p_arg = _p_arg;
    };
    virtual ~DialogEvent() {};

    int i_arg, i_dialog;
    intf_dialog_args_t *p_arg;
};

#endif
