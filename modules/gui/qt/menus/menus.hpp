/*****************************************************************************
 * menus.hpp : Menus handling
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
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

#ifndef QVLC_MENUS_H_
#define QVLC_MENUS_H_

#include "qt.hpp"

#include "custom_menus.hpp"

#include <QObject>
#include <QMenu>
#include <QVector>
#include <QAbstractListModel>
#include <QMetaProperty>

class VLCMenuBar : public QObject
{
    Q_OBJECT
    friend class MenuFunc;

public:
    /* Main bar creation */
    static void createMenuBar( MainInterface *mi, intf_thread_t * );

    /* Popups Menus */
    static QMenu* PopupMenu( intf_thread_t *, bool );
    static QMenu* AudioPopupMenu( intf_thread_t *, bool );
    static QMenu* VideoPopupMenu( intf_thread_t *, bool );
    static QMenu* MiscPopupMenu( intf_thread_t *, bool );

    /* Systray */
    static void updateSystrayMenu( MainInterface *, intf_thread_t  *,
                                   bool b_force_visible = false);

    /* destructor for parentless Menus (kept in static variables) */
    static void freeRendererMenu(){ delete rendererMenu; rendererMenu = NULL; }
    static void freeRecentsMenu(){ delete recentsMenu; recentsMenu = NULL; }

private:
    /* All main Menus */
    static QMenu *FileMenu( intf_thread_t *, QWidget *, MainInterface * mi = NULL );

    static QMenu *ToolsMenu( intf_thread_t *, QMenu * );
    static QMenu *ToolsMenu( intf_thread_t * p_intf, QWidget *parent )
        { return ToolsMenu( p_intf, new QMenu( parent ) ); }

    static QMenu *ViewMenu( intf_thread_t *, QMenu *, MainInterface * mi = NULL );

    static QMenu *InterfacesMenu( intf_thread_t *p_intf, QMenu * );
    static void ExtensionsMenu( intf_thread_t *p_intf, QMenu * );

    static QMenu *NavigMenu( intf_thread_t *, QMenu * );
    static QMenu *NavigMenu( intf_thread_t *p_intf, QWidget *parent ) {
        return NavigMenu( p_intf, new QMenu( parent ) );
    }
    static QMenu *RebuildNavigMenu(intf_thread_t *, QMenu *);

    static QMenu *VideoMenu( intf_thread_t *, QMenu * );
    static QMenu *VideoMenu( intf_thread_t *p_intf, QWidget *parent ) {
        return VideoMenu( p_intf, new QMenu( parent ) );
    }
    static QMenu *SubtitleMenu( intf_thread_t *, QMenu *current, bool b_popup = false );
    static QMenu *SubtitleMenu( intf_thread_t *p_intf, QWidget *parent) {
        return SubtitleMenu( p_intf, new QMenu( parent ) );
    }

    static QMenu *AudioMenu( intf_thread_t *, QMenu * );
    static QMenu *AudioMenu( intf_thread_t *p_intf, QWidget *parent ) {
        return AudioMenu( p_intf, new QMenu( parent ) );
    }

    static QMenu *HelpMenu( QWidget * );

    /* Popups Menus */
    static void PopupMenuStaticEntries( QMenu *menu );
    static void PopupMenuPlaylistEntries( QMenu *menu, intf_thread_t *p_intf );
    static void PopupMenuPlaylistControlEntries( QMenu *menu, intf_thread_t *p_intf );
    static void PopupMenuControlEntries( QMenu *menu, intf_thread_t *p_intf, bool b = true );

    /* recentMRL menu */
    static QMenu *recentsMenu;

    static RendererMenu *rendererMenu;

    static void updateAudioDevice(intf_thread_t *, QMenu* );

public slots:
    static void updateRecents( intf_thread_t * );
};

#endif
