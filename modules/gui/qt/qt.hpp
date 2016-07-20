/*****************************************************************************
 * qt.hpp : Qt interface
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

#include <qconfig.h>

#ifdef QT_STATIC
#define QT_STATICPLUGIN
#endif

#define QT_NO_CAST_TO_ASCII
#include <QString>

#if ( QT_VERSION < 0x040800 )
# error Update your Qt version to at least 4.8.0
#endif

#define HAS_QT5  ( QT_VERSION >= 0x050000 )
#define HAS_QT56 ( QT_VERSION >= 0x050600 )

/* Q_DECL_OVERRIDE is a Qt5 feature, add empty define to not break with Qt4 */
#if !HAS_QT5 && !defined(Q_DECL_OVERRIDE)
# define Q_DECL_OVERRIDE
#endif

enum {
    DialogEventTypeOffset = 0,
    IMEventTypeOffset     = 100,
    PLEventTypeOffset     = 200,
    MsgEventTypeOffset    = 300,
};

enum{
    NOTIFICATION_NEVER = 0,
    NOTIFICATION_MINIMIZED = 1,
    NOTIFICATION_ALWAYS = 2,
};

#define DPI_REF_VALUE 96.0

class QVLCApp;
class MainInterface;
class QSettings;
class PLModel;

struct intf_sys_t
{
    vlc_thread_t thread;

    QVLCApp *p_app;          /* Main Qt Application */

    MainInterface *p_mi;     /* Main Interface, NULL if DialogProvider Mode */

    QSettings *mainSettings; /* Qt State settings not messing main VLC ones */

    PLModel *pl_model;

    QString filepath;        /* Last path used in dialogs */

    unsigned voutWindowType; /* Type of vout_window_t provided */
    bool b_isDialogProvider; /* Qt mode or Skins mode */
    playlist_t *p_playlist;  /* playlist */
#ifdef _WIN32
    bool disable_volume_keys;
#endif
};

#define THEPL p_intf->p_sys->p_playlist

/**
 * This class may be used for scope-bound locking/unlocking
 * of a playlist_t*. As hinted, the playlist is locked when
 * the object is created, and unlocked when the object is
 * destroyed.
 */

struct vlc_playlist_locker {
    vlc_playlist_locker( playlist_t* p_playlist )
        : p_playlist( p_playlist )
    {
        playlist_Lock( p_playlist ); 
    }

    ~vlc_playlist_locker()
    {
        playlist_Unlock( p_playlist );
    }

    private:
        playlist_t* p_playlist;
};

#define THEDP DialogsProvider::getInstance()
#define THEMIM MainInputManager::getInstance( p_intf )
#define THEAM ActionsManager::getInstance( p_intf )

#define qfu( i ) QString::fromUtf8( i )
#define qfue( i ) QString::fromUtf8( i ).replace( "&", "&&" ) /* for actions/buttons */
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
