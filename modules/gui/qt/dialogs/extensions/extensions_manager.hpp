/*****************************************************************************
 * extensions_manager.hpp: Extensions manager for Qt
 ****************************************************************************
 * Copyright (C) 2009-2010 VideoLAN and authors
 *
 * Authors: Jean-Philippe André < jpeg # videolan.org >
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

#ifndef EXTENSIONS_MANAGER_HPP
#define EXTENSIONS_MANAGER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include <player/player_controller.hpp>
#include "util/singleton.hpp"

#include <QObject>

class QMenu;
class QSignalMapper;

class ExtensionsDialogProvider;

extern "C" {
    struct extensions_manager_t;
};

class ExtensionsManager : public QObject, public Singleton<ExtensionsManager>
{
    friend class Singleton<ExtensionsManager>;

    Q_OBJECT
public:
    inline bool isLoaded() { return p_extensions_manager != NULL; }
    inline bool cannotLoad() { return b_unloading || b_failed; }
    inline bool isUnloading() { return b_unloading; }
    void menu( QMenu *current );

    void openVLsub();

    /** Get the extensions_manager_t if it is loaded */
    extensions_manager_t* getManager()
    {
        return p_extensions_manager;
    }

private:
    ExtensionsManager( qt_intf_t *p_intf, QObject *parent = nullptr );
    virtual ~ExtensionsManager();

public slots:
    bool loadExtensions();
    void unloadExtensions();
    void reloadExtensions();

private slots:
    void triggerMenu( int id );
    void inputChanged( );
    void playingChanged(PlayerController::PlayingState );
    void metaChanged( input_item_t *p_input );

private:
    static ExtensionsManager* instance;
    qt_intf_t *p_intf;
    extensions_manager_t *p_extensions_manager;
    ExtensionsDialogProvider *p_edp;

    QSignalMapper *menuMapper;
    bool b_unloading;  ///< Work around threads + emit issues, see isUnloading
    bool b_failed; ///< Flag set to true if we could not load the module

signals:
    void extensionsUpdated();
};

#endif // EXTENSIONS_MANAGER_HPP
