/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include "renderer_manager.hpp"

#include <QApplication>
#include <QTimer>
#include <QVector>
#include <QHash>

#include <vlc_common.h>
#include <vlc_renderer_discovery.h>
#include <vlc_player.h>
#include <vlc_cxx_helpers.hpp>

static void player_renderer_changed(vlc_player_t *, vlc_renderer_item_t *new_item, void *data);

using RendererItemPtr = ::vlc::vlc_shared_data_ptr<vlc_renderer_item_t, &vlc_renderer_item_hold, &vlc_renderer_item_release>;
using vlc_player_locker = vlc_locker<vlc_player_t, vlc_player_Lock, vlc_player_Unlock>;

struct ItemEntry
{
    ItemEntry(const RendererItemPtr& renderItem, bool current = true)
        : renderItem(renderItem)
        , currentScan(current)
    {}

    ItemEntry(){}

    RendererItemPtr renderItem;
    /**
     * we scan for 20 seconds, when the scan ends render discoverer are destroyed but
     * items are kept in the model, at this moment the items are marked as "old", if
     * a new scan starts and finish without finding them again, then they will be removed
     * from the model
     */
    bool currentScan = true;
};

class RendererManagerPrivate
{
public:
    RendererManagerPrivate(RendererManager* pub, qt_intf_t* const intf, vlc_player_t* const player)
        : p_intf(intf)
        , m_player(player)
        , q_ptr(pub)
    {
        assert(m_player);
        QObject::connect( &m_stop_scan_timer, &QTimer::timeout, q_ptr, [this](){ timerCountdown(); });

        static struct vlc_player_cbs cbs = {};
        cbs.on_renderer_changed = &player_renderer_changed;
        {
            vlc_player_locker lock{ m_player };
            m_playerListener = vlc_player_AddListener(m_player, &cbs, this);
        }
    }

    ~RendererManagerPrivate()
    {
        if (m_playerListener)
        {
            vlc_player_locker lock{ m_player };
            vlc_player_RemoveListener(m_player, m_playerListener);
        }
    }

    void timerCountdown()
    {
        Q_Q(RendererManager);
        if( m_stop_scan_timer.isActive() && m_scan_remain > 0 )
        {
            m_scan_remain--;
            emit q->scanRemainChanged();
        }
        else
        {
            q->StopScan();
        }
    }

    void setStatus(RendererManager::RendererStatus status)
    {
        Q_Q(RendererManager);
        if (status == m_status)
            return;
        m_status = status;
        emit q->statusChanged();
    }


    void onRendererAdded(const RendererItemPtr& renderItem) {
        Q_Q(RendererManager);
        QString key = getItemKey(renderItem);
        if( !m_items.contains( key ) )
        {
            emit q->beginInsertRows({}, m_itemsIndex.size(), m_itemsIndex.size());
            m_items.emplace( key, renderItem );
            m_itemsIndex.push_back(key);
            emit q->endInsertRows();
        }
        else
        {
            /* mark the item as seen during this scan */
            ItemEntry& entry = m_items[key];
            entry.currentScan = true;
        }
    }

    void onRendererRemoved(const RendererItemPtr& renderItem) {
        Q_Q(RendererManager);

        QString key = getItemKey(renderItem);
        if( m_items.contains( key ) )
        {
            //only remove the item if we are using it
            //and if we are actively scanning
            if( m_selectedItem != renderItem && m_status == RendererManager::RUNNING )
            {
                qsizetype index = m_itemsIndex.indexOf(key);
                emit q->beginRemoveRows({}, index, index);
                m_items.remove(key);
                m_itemsIndex.remove(index);
                emit q->endRemoveRows();
            }
        }
    }

    void onPlayerRendererChanged(const RendererItemPtr& renderItem)
    {
        Q_Q(RendererManager);

        if (m_selectedItem == renderItem)
            return;

        if (m_selectedItem)
        {
            int oldIndexRow = getItemRow(m_selectedItem);
            m_selectedItem.reset();
            if (oldIndexRow >= 0)
            {
                auto oldIndex = q->index(oldIndexRow);
                emit q->dataChanged(oldIndex, oldIndex, { Qt::CheckStateRole, RendererManager::SELECTED });
            }
        }

        if (renderItem)
        {
            m_selectedItem = renderItem;

            int newIndexRow= getItemRow(m_selectedItem);
            if (newIndexRow >= 0)
            {
                auto newIndex = q->index(newIndexRow);
                emit q->dataChanged(newIndex, newIndex, { Qt::CheckStateRole, RendererManager::SELECTED });
            }
            else
            {
                QString key = getItemKey(m_selectedItem);
                emit q->beginInsertRows({}, m_itemsIndex.size(), m_itemsIndex.size());
                //renderer is not part of the current scan, mark it like it
                m_items.emplace( key, renderItem, false );
                m_itemsIndex.push_back(key);
                emit q->endInsertRows();
            }
        }
        emit q->useRendererChanged();
    }

    QString getItemKey(const RendererItemPtr& item) const
    {
        const char* type = vlc_renderer_item_type(item.get());
        const char* sout = vlc_renderer_item_sout(item.get());
        return QString("%1-%2").arg(type).arg(sout);
    }

    int getItemRow(const RendererItemPtr& item) const
    {
        QString newKey = getItemKey(item);
        return m_itemsIndex.indexOf(newKey);
    }

    qt_intf_t* const p_intf;
    vlc_player_t* const m_player;

    vlc_player_listener_id* m_playerListener = nullptr;

    RendererManager::RendererStatus m_status = RendererManager::IDLE;

    std::vector<vlc_renderer_discovery_t*> m_rds;


    //the elements of the model (in model order), the string is the key of m_items
    QVector<QString> m_itemsIndex;

    QHash<QString, ItemEntry> m_items;

    //renderer currently used by the player
    RendererItemPtr m_selectedItem;

    //model automatically stops scanning after a while
    QTimer m_stop_scan_timer;
    //remaining time in seconds
    unsigned m_scan_remain = 0;

    RendererManager* q_ptr;
    Q_DECLARE_PUBLIC(RendererManager);
};

void RendererManager::disableRenderer()
{
    Q_D(RendererManager);
    if (!d->m_selectedItem)
        return;

    {
        vlc_player_locker lock{ d->m_player };
        //vlc_player synchronously report the renderer change
        vlc_player_SetRenderer( d->m_player, nullptr );
    }
}

QVariant RendererManager::data(const QModelIndex &index, int role) const
{
    Q_D(const RendererManager);
    if (index.row() >= d->m_itemsIndex.size())
        return {};

    const ItemEntry& entry = d->m_items.value(d->m_itemsIndex[index.row()]);
    vlc_renderer_item_t* item = entry.renderItem.get();
    switch (role)
    {
    case Qt::DisplayRole:
    case NAME:
        return QString(vlc_renderer_item_name(item));
    case TYPE:
        return QString(vlc_renderer_item_type(item));
    case DEMUX_FILTER:
        return QString(vlc_renderer_item_demux_filter(item));
    case SOUT:
        return QString(vlc_renderer_item_sout(item));
    case ICON_URI:
    case Qt::DecorationRole:
    {
        const char* iconUri = vlc_renderer_item_icon_uri(item);
        if (!iconUri)
        {
            if (vlc_renderer_item_flags( item ) & VLC_RENDERER_CAN_VIDEO)
                return QString("qrc://menu/movie.svg");
            else
                return QString("qrc://menu/music.svg");
        }
        return QString(iconUri);
    }
    case FLAGS:
        return vlc_renderer_item_flags(entry.renderItem.get());
    case SELECTED:
        return entry.renderItem == d->m_selectedItem;
    case Qt::CheckStateRole:
        return entry.renderItem == d->m_selectedItem ? Qt::Checked : Qt::Unchecked;
    }
    return {};
}

bool RendererManager::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_D(RendererManager);
    if (index.row() >= d->m_itemsIndex.size())
        return false;

    QString sout = d->m_itemsIndex[index.row()];
    ItemEntry& entry = d->m_items[sout];
    switch (role)
    {
    case Qt::CheckStateRole:
    case SELECTED:
    {
        bool enableRenderer;

        if ( value.canConvert<bool>() )
            enableRenderer = value.toBool();
        else
            return false;

        {
            vlc_player_locker lock{ d->m_player };
            //vlc_player synchronously report the renderer change
            vlc_player_SetRenderer(
                d->m_player,
                enableRenderer ? entry.renderItem.get() : nullptr
            );
        }
        return true;
    }
    default:
        return false;
    }
}

QHash<int,QByteArray> RendererManager::roleNames() const
{
    return {
        { NAME, "name" },
        { TYPE, "type" },
        { DEMUX_FILTER, "demuxFilter" },
        { SOUT, "sour" },
        { ICON_URI, "iconUri" },
        { FLAGS, "flags" },
    };
}

int RendererManager::rowCount(const QModelIndex&) const
{
    Q_D(const RendererManager);
    return d->m_itemsIndex.size();
}


static void renderer_event_item_added( vlc_renderer_discovery_t* p_rd, vlc_renderer_item_t *p_item )
{
    RendererManagerPrivate *self = reinterpret_cast<RendererManagerPrivate*>( p_rd->owner.sys );
    RendererItemPtr renderItem(p_item);
    QMetaObject::invokeMethod(self->q_func(), [self, renderItem = std::move(renderItem)](){
        self->onRendererAdded(renderItem);
    });
}

static void renderer_event_item_removed( vlc_renderer_discovery_t *p_rd,
                                 vlc_renderer_item_t *p_item )
{
    RendererManagerPrivate *self = reinterpret_cast<RendererManagerPrivate*>( p_rd->owner.sys );
    RendererItemPtr renderItem(p_item);
    QMetaObject::invokeMethod(self->q_func(), [self, renderItem = std::move(renderItem)](){
        self->onRendererRemoved(renderItem);
    });
}

static void player_renderer_changed(vlc_player_t *, vlc_renderer_item_t *new_item, void *data)
{
    RendererManagerPrivate *self = reinterpret_cast<RendererManagerPrivate*>( data);
    RendererItemPtr renderItem(new_item);
    QMetaObject::invokeMethod(self->q_func(), [self, renderItem = std::move(renderItem)](){
        self->onPlayerRendererChanged(renderItem);
    });
}

RendererManager::RendererManager( qt_intf_t *p_intf_, vlc_player_t* player )
    : d_ptr(new RendererManagerPrivate(this, p_intf_, player))
{
}

RendererManager::~RendererManager()
{
    StopScan();
}

int RendererManager::getScanRemain() const
{
    Q_D(const RendererManager);
    return d->m_scan_remain;
}

RendererManager::RendererStatus RendererManager::getStatus() const
{
    Q_D(const RendererManager);
    return d->m_status;
}

bool RendererManager::useRenderer() const
{
    Q_D(const RendererManager);
    return d->m_selectedItem.get() != nullptr;
}

void RendererManager::StartScan()
{
    Q_D(RendererManager);
    if( d->m_stop_scan_timer.isActive() )
        return;

    /* SD subnodes */
    char **ppsz_longnames;
    char **ppsz_names;
    if( vlc_rd_get_names( d->p_intf, &ppsz_names, &ppsz_longnames ) != VLC_SUCCESS )
    {
        d->setStatus( RendererManager::RendererStatus::FAILED );
        return;
    }

    struct vlc_renderer_discovery_owner owner =
    {
        d,
        renderer_event_item_added,
        renderer_event_item_removed,
    };

    char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
    for( ; *ppsz_name; ppsz_name++, ppsz_longname++ )
    {
        msg_Dbg( d->p_intf, "starting renderer discovery service %s", *ppsz_longname );
        vlc_renderer_discovery_t* p_rd = vlc_rd_new( VLC_OBJECT(d->p_intf), *ppsz_name, &owner );
        if( p_rd != NULL )
            d->m_rds.push_back( p_rd );
        free( *ppsz_name );
        free( *ppsz_longname );
    }
    free( ppsz_names );
    free( ppsz_longnames );

    d->m_scan_remain = 20;
    d->m_stop_scan_timer.setInterval( 1000 );
    d->m_stop_scan_timer.start();
    d->setStatus( RendererManager::RendererStatus::RUNNING );
    emit scanRemainChanged();
}

void RendererManager::StopScan()
{
    Q_D(RendererManager);
    d->m_stop_scan_timer.stop();

    for ( vlc_renderer_discovery_t* p_rd : d->m_rds )
        vlc_rd_release( p_rd );
    d->m_rds.clear();

    /* Cleanup of outdated items, and notify removal */
    for (int i = d->m_itemsIndex.size() - 1; i >= 0; --i)
    {
        QString key = d->m_itemsIndex[i];
        ItemEntry& entry = d->m_items[key];
        if( !entry.currentScan && entry.renderItem != d->m_selectedItem )
        {
            //remove items from previous scans
            emit beginRemoveRows({}, i, i);
            d->m_items.remove(key);
            d->m_itemsIndex.remove(i);
            emit endRemoveRows();
        }
        else
        {
            /* don't keep if not updated by next detect */
            entry.currentScan = false;
        }
    }
    d->setStatus( RendererManager::RendererStatus::IDLE );
}
