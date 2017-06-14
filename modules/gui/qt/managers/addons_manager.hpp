/*****************************************************************************
 * addons_manager.hpp: Addons manager for Qt
 ****************************************************************************
 * Copyright (C) 2013 VideoLAN and authors
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

#ifndef ADDONS_MANAGER_HPP
#define ADDONS_MANAGER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "util/singleton.hpp"

#include <vlc_addons.h>

#include <QObject>
#include <QEvent>


class AddonManagerEvent : public QEvent
{
public:
    static const QEvent::Type AddedEvent;
    static const QEvent::Type ChangedEvent;
    static const QEvent::Type DiscoveryEndedEvent;

    AddonManagerEvent( QEvent::Type type, addon_entry_t *_p_entry )
        : QEvent( type ), p_entry( _p_entry )
    {
        addon_entry_Hold( p_entry );
    }
    virtual ~AddonManagerEvent()
    {
        addon_entry_Release( p_entry );
    }

    addon_entry_t *entry() const { return p_entry; }

private:
    addon_entry_t *p_entry;
};

class AddonsManager : public QObject, public Singleton<AddonsManager>
{
    Q_OBJECT
    friend class Singleton<AddonsManager>;

public:
    AddonsManager( intf_thread_t * );
    virtual ~AddonsManager();
    static void addonFoundCallback( addons_manager_t *, addon_entry_t * );
    static void addonsDiscoveryEndedCallback( addons_manager_t * );
    static void addonChangedCallback( addons_manager_t *, addon_entry_t * );
    void customEvent( QEvent * );
    void install( QByteArray id );
    void remove( QByteArray id );
    static QString getAddonType( int );

signals:
    void addonAdded( addon_entry_t * );
    void addonChanged( const addon_entry_t * );
    void discoveryEnded();

public slots:
    void findNewAddons();
    void findDesignatedAddon( QString uri );
    void findInstalled();

private:
    addons_manager_t* p_manager;
};

#endif // ADDONS_MANAGER_HPP
