/*****************************************************************************
 * qt4.hpp : QT4 interface
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef QVLC_H_
#define QVLC_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>    /* VLC_COMMON_MEMBERS for vlc_interface.h */
#include <vlc_interface.h> /* intf_thread_t */
#include <vlc_playlist.h>  /* playlist_t */

#define QT_NO_CAST_TO_ASCII
#include <QString>

#if ( QT_VERSION < 0x040400 )
# error Update your Qt version to at least 4.4.0
#endif
#if QT_VERSION == 0x040500
# error Please update Qt version to 4.5.1. 4.5.0 is too buggy
#endif

#define HAS_QT45 ( QT_VERSION >= 0x040500 )
#define HAS_QT47 ( QT_VERSION >= 0x040700 )

enum {
    DialogEventType = 0,
    IMEventType     = 100,
    PLEventType     = 200,
    MsgEventType    = 300,
};

class QVLCApp;
class QMenu;
class MainInterface;
class QSettings;

struct intf_sys_t
{
    vlc_thread_t thread;

    QVLCApp *p_app;          /* Main Qt Application */
    MainInterface *p_mi;     /* Main Interface, NULL if DialogProvider Mode */

    QSettings *mainSettings; /* Qt State settings not messing main VLC ones */

    bool b_isDialogProvider; /* Qt mode or Skins mode */

    int  i_screenHeight;     /* Detection of Small screens */

    QString filepath;        /* Last path used in dialogs */

#ifdef WIN32
    bool disable_volume_keys;
#endif
};

#define THEPL pl_Get(p_intf)
#define QPL_LOCK playlist_Lock( THEPL );
#define QPL_UNLOCK playlist_Unlock( THEPL );

#define THEDP DialogsProvider::getInstance()
#define THEMIM MainInputManager::getInstance( p_intf )
#define THEAM ActionsManager::getInstance( p_intf )

#define qfu( i ) QString::fromUtf8( i )
#define qtr( i ) QString::fromUtf8( vlc_gettext(i) )
#define qtu( i ) ((i).toUtf8().constData())

#define CONNECT( a, b, c, d ) \
        connect( a, SIGNAL(b), c, SLOT(d) )
#define DCONNECT( a, b, c, d ) \
        connect( a, SIGNAL(b), c, SLOT(d), Qt::DirectConnection )
#define BUTTONACT( b, a ) connect( b, SIGNAL(clicked()), this, SLOT(a) )

#define BUTTON_SET( button, text, tooltip )  \
    button->setText( text );                 \
    button->setToolTip( tooltip );

#define BUTTON_SET_ACT( button, text, tooltip, thisslot ) \
    BUTTON_SET( button, text, tooltip );                  \
    BUTTONACT( button, thisslot );

#define BUTTON_SET_IMG( button, text, image, tooltip )    \
    BUTTON_SET( button, text, tooltip );                  \
    button->setIcon( QIcon( ":/"#image ) );

#define BUTTON_SET_ACT_I( button, text, image, tooltip, thisslot ) \
    BUTTON_SET_IMG( button, text, image, tooltip );                \
    BUTTONACT( button, thisslot );

#define VISIBLE(i) (i && i->isVisible())

#define TOGGLEV( x ) { if( x->isVisible() ) x->hide();          \
            else  x->show(); }

/* for widgets which must not follow the RTL auto layout changes */
#define RTL_UNAFFECTED_WIDGET setLayoutDirection( Qt::LeftToRight );

#define getSettings() p_intf->p_sys->mainSettings

#define QT_VOLUME_DEFAULT AOUT_VOLUME_DEFAULT
#define QT_VOLUME_MAX (AOUT_VOLUME_DEFAULT * 2)

static inline QString QVLCUserDir( vlc_userdir_t type )
{
    char *dir = config_GetUserDir( type );
    if( !dir )
        return "";
    QString res = qfu( dir );
    free( dir );
    return res;
}

/* After this day of the year, the usual VLC cone is replaced by another cone
 * wearing a Father Xmas hat.
 * Note this icon doesn't represent an endorsment of Coca-Cola company.
 */
#define QT_XMAS_JOKE_DAY 354

#endif
