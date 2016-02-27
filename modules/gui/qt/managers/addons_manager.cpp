/*****************************************************************************
 * addons_manager.cpp: Addons manager for Qt
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

#include "addons_manager.hpp"
#include <QApplication>

const QEvent::Type AddonManagerEvent::AddedEvent =
        (QEvent::Type)QEvent::registerEventType();
const QEvent::Type AddonManagerEvent::ChangedEvent =
        (QEvent::Type)QEvent::registerEventType();
const QEvent::Type AddonManagerEvent::DiscoveryEndedEvent =
        (QEvent::Type)QEvent::registerEventType();

AddonsManager::AddonsManager( intf_thread_t *p_intf )
{
    p_manager = addons_manager_New( VLC_OBJECT(p_intf) );
    if ( !p_manager ) return;

    vlc_event_manager_t *p_em = p_manager->p_event_manager;
    vlc_event_attach( p_em, vlc_AddonFound, addonsEventsCallback, this );
    vlc_event_attach( p_em, vlc_AddonsDiscoveryEnded, addonsEventsCallback, this );
    vlc_event_attach( p_em, vlc_AddonChanged, addonsEventsCallback, this );
}

AddonsManager::~AddonsManager()
{
    if ( p_manager )
        addons_manager_Delete( p_manager );
}

void AddonsManager::findNewAddons()
{
    addons_manager_Gather( p_manager, "repo://" );
}

void AddonsManager::findDesignatedAddon( QString uri )
{
    addons_manager_Gather( p_manager, qtu(uri) );
}

void AddonsManager::findInstalled()
{
    addons_manager_LoadCatalog( p_manager );
}

void AddonsManager::install( QByteArray id )
{
    Q_ASSERT( id.size() == sizeof(addon_uuid_t) );
    addon_uuid_t addonid;
    memcpy( &addonid, id.constData(), sizeof(addon_uuid_t) );
    addons_manager_Install( p_manager, addonid );
}

void AddonsManager::remove( QByteArray id )
{
    Q_ASSERT( id.size() == sizeof(addon_uuid_t) );
    addon_uuid_t addonid;
    memcpy( &addonid, id.constData(), sizeof(addon_uuid_t) );
    addons_manager_Remove( p_manager, addonid );
}

QString AddonsManager::getAddonType( int i_type )
{
    switch ( i_type )
    {
    case ADDON_SKIN2:
        return qtr( "Skins" );
    case ADDON_PLAYLIST_PARSER:
        return qtr("Playlist parsers");
    case ADDON_SERVICE_DISCOVERY:
        return qtr("Service Discovery");
    case ADDON_INTERFACE:
        return qtr("Interfaces");
    case ADDON_META:
        return qtr("Art and meta fetchers");
    case ADDON_EXTENSION:
        return qtr("Extensions");
    default:
        return qtr("Unknown");
    }
}

void AddonsManager::addonsEventsCallback( const vlc_event_t *event, void *data )
{
    AddonsManager *me = ( AddonsManager * ) data;
    QEvent *ev = NULL;

    if ( event->type == vlc_AddonFound )
    {
        ev = new AddonManagerEvent( AddonManagerEvent::AddedEvent,
                                    event->u.addon_generic_event.p_entry );
    }
    else if ( event->type == vlc_AddonsDiscoveryEnded )
    {
        ev = new QEvent( AddonManagerEvent::DiscoveryEndedEvent );
    }
    else if ( event->type == vlc_AddonChanged )
    {
        ev = new AddonManagerEvent( AddonManagerEvent::ChangedEvent,
                                    event->u.addon_generic_event.p_entry );
    }

    if ( ev ) QApplication::postEvent( me, ev );
}

void AddonsManager::customEvent( QEvent *event )
{
    if ( event->type() == AddonManagerEvent::AddedEvent )
    {
        AddonManagerEvent *ev = static_cast<AddonManagerEvent *>(event);
        emit addonAdded( ev->entry() );
    }
    else if ( event->type() == AddonManagerEvent::ChangedEvent )
    {
        AddonManagerEvent *ev = static_cast<AddonManagerEvent *>(event);
        emit addonChanged( ev->entry() );
    }
    else if ( event->type() == AddonManagerEvent::DiscoveryEndedEvent )
    {
        emit discoveryEnded();
    }
}
