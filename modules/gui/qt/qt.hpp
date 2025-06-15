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
#include <vlc_configuration.h>
#include <vlc_threads.h>

#include <memory>

#include <qconfig.h>

#include <QString>

enum {
    IMEventTypeOffset     = 0,
    MsgEventTypeOffset    = 100
};

enum{
    NOTIFICATION_NEVER = 0,
    NOTIFICATION_MINIMIZED = 1,
    NOTIFICATION_ALWAYS = 2,
};

///// forward declaration

extern "C" {
typedef struct intf_dialog_args_t intf_dialog_args_t;
typedef struct vlc_playlist vlc_playlist_t;
typedef struct intf_thread_t intf_thread_t;
typedef struct vlc_player_t vlc_player_t;
}

namespace vlc {
class Compositor;

namespace playlist {
class PlaylistController;
}

}
class PlayerController;

///// module internal

struct qt_intf_t
{
    struct vlc_object_t obj;

    /** pointer to the actual intf module */
    intf_thread_t* intf;

    /** Specific for dialogs providers */
    void ( *pf_show_dialog ) ( struct intf_thread_t *, int, int,
                               intf_dialog_args_t * );

    vlc_thread_t thread;
    vlc_sem_t wait_quit;

    class MainCtx *p_mi;     /* Main Interface, NULL if DialogProvider Mode */
    class QSettings *mainSettings; /* Qt State settings not messing main VLC ones */

    bool b_isDialogProvider; /* Qt mode or Skins mode */

    vlc_playlist_t *p_playlist;  /* playlist */
    vlc_player_t *p_player; /* player */
    vlc::playlist::PlaylistController* p_mainPlaylistController;
    PlayerController* p_mainPlayerController;
    std::unique_ptr<vlc::Compositor>  p_compositor;

    int refCount;
    bool isShuttingDown;
};

template <typename T, void (*LOCK)(T *), void (*UNLOCK)(T *)>
class vlc_locker {
    T * const ptr = nullptr;

public:
    explicit vlc_locker(T * const ptr)
        : ptr(ptr)
    {
        LOCK(ptr);
    }

    ~vlc_locker()
    {
        UNLOCK(ptr);
    }
};

#define THEDP DialogsProvider::getInstance()
#define THEMIM p_intf->p_mainPlayerController
#define THEMPL p_intf->p_mainPlaylistController

#define qfu( i ) QString::fromUtf8( i )
#define qfue( i ) QString::fromUtf8( i ).replace( "&", "&&" ) /* for actions/buttons */
#define qfut( i ) QString::fromUtf8( vlc_gettext(i) )
#define qtu( i ) ((i).toUtf8().constData())

/* For marking translatable static strings (like `_()`) */
#define qtr( i ) qfut( i )

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#define QtCheckboxChanged  QCheckBox::checkStateChanged
#else
#define QtCheckboxChanged  QCheckBox::stateChanged
#endif

#define BUTTONACT( b, a ) connect( b, &QAbstractButton::clicked, this, a )

#define getSettings() p_intf->mainSettings

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
