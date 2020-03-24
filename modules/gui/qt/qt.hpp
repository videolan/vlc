/*****************************************************************************
 * qt.hpp : Qt interface
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
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

#include <vlc_common.h>
#include <vlc_interface.h> /* intf_thread_t */
#include <vlc_playlist.h>  /* vlc_playlist_t */
#include <vlc_player.h>  /* vlc_player_t */

#include <qconfig.h>

#ifdef QT_STATIC
#define QT_STATICPLUGIN
#endif

#define QT_NO_CAST_TO_ASCII
#include <QString>
#include <QUrl>

#if ( QT_VERSION < 0x050900 )
# error Update your Qt version to at least 5.9.0
#endif

#define HAS_QT56 ( QT_VERSION >= 0x050600 )
#define HAS_QT510 ( QT_VERSION >= 0x051000 )

enum {
    IMEventTypeOffset     = 0,
    MsgEventTypeOffset    = 100
};

enum{
    NOTIFICATION_NEVER = 0,
    NOTIFICATION_MINIMIZED = 1,
    NOTIFICATION_ALWAYS = 2,
};

namespace vlc {
class Compositor;

namespace playlist {
class PlaylistControllerModel;
}

}
class PlayerController;
struct intf_sys_t
{
    vlc_thread_t thread;

    class QVLCApp *p_app;          /* Main Qt Application */
    class MainInterface *p_mi;     /* Main Interface, NULL if DialogProvider Mode */
    class QSettings *mainSettings; /* Qt State settings not messing main VLC ones */

    QUrl filepath;        /* Last path used in dialogs */

    unsigned voutWindowType; /* Type of vout_window_t provided */
    bool b_isDialogProvider; /* Qt mode or Skins mode */

    vlc_playlist_t *p_playlist;  /* playlist */
    vlc_player_t *p_player; /* player */
    vlc::playlist::PlaylistControllerModel* p_mainPlaylistController;
    PlayerController* p_mainPlayerController;
    vlc::Compositor*  p_compositor;

#ifdef _WIN32
    bool disable_volume_keys;
#endif
};

/**
 * This class may be used for scope-bound locking/unlocking
 * of a player_t*. As hinted, the player is locked when
 * the object is created, and unlocked when the object is
 * destroyed.
 */
struct vlc_player_locker {
    vlc_player_locker( vlc_player_t* p_player )
        : p_player( p_player )
    {
        vlc_player_Lock( p_player );
    }

    ~vlc_player_locker()
    {
        vlc_player_Unlock( p_player );
    }

    private:
        vlc_player_t* p_player;
};

#define THEDP DialogsProvider::getInstance()
#define THEMIM p_intf->p_sys->p_mainPlayerController
#define THEMPL p_intf->p_sys->p_mainPlaylistController

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
    button->setIcon( QIcon( ":/"#image ".svg") );

#define BUTTON_SET_ACT_I( button, text, image, tooltip, thisslot ) \
    BUTTON_SET_IMG( button, text, image, tooltip );                \
    BUTTONACT( button, thisslot );

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
