/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <cassert>
#include "mlbasemodel.hpp"
#include "medialib.hpp"
#include <vlc_cxx_helpers.hpp>

MLBaseModel::MLBaseModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_ml(nullptr)
    , m_ml_event_handle( nullptr, [this](vlc_ml_event_callback_t* cb ) {
            assert( m_ml != nullptr );
            vlc_ml_event_unregister_callback( m_ml, cb );
        })
    , m_need_reset( false )
{
    m_sort = VLC_ML_SORTING_DEFAULT;
    m_sort_desc = false;

    connect( this, &MLBaseModel::resetRequested, this, &MLBaseModel::onResetRequested );
}

MLBaseModel::~MLBaseModel()
{
}

void MLBaseModel::sortByColumn(QByteArray name, Qt::SortOrder order)
{
    beginResetModel();
    m_sort_desc = (order == Qt::SortOrder::DescendingOrder);
    m_sort = nameToCriteria(name);
    clear();
    endResetModel();
}

QMap<QString, QVariant> MLBaseModel::getDataAt(int idx)
{
    QMap<QString, QVariant> dataDict;
    QHash<int,QByteArray> roles = roleNames();
    for (auto role: roles.keys()) {
        dataDict[roles[role]] = data(index(idx), role);
    }
    return dataDict;
}

void MLBaseModel::onResetRequested()
{
    beginResetModel();
    clear();
    endResetModel();
}

void MLBaseModel::onLocalSizeAboutToBeChanged(size_t size)
{
    (void) size;
    beginResetModel();
}

void MLBaseModel::onLocalSizeChanged(size_t size)
{
    (void) size;
    endResetModel();
    emit countChanged(size);
}

void MLBaseModel::onLocalDataChanged(size_t offset, size_t count)
{
    assert(count);
    auto first = index(offset);
    auto last = index(offset + count - 1);
    emit dataChanged(first, last);
}

void MLBaseModel::onVlcMlEvent(const MLEvent &event)
{
    switch(event.i_type)
    {
        case VLC_ML_EVENT_BACKGROUND_IDLE_CHANGED:
            if ( event.background_idle_changed.b_idle && m_need_reset )
            {
                emit resetRequested();
                m_need_reset = false;
            }
            break;
    }
}

QString MLBaseModel::getFirstSymbol(QString str)
{
    QString ret("#");
    if ( str.length() > 0 && str[0].isLetter() )
        ret = str[0].toUpper();
    return ret;
}

void MLBaseModel::onVlcMlEvent(void* data, const vlc_ml_event_t* event)
{
    auto self = static_cast<MLBaseModel*>(data);
    QMetaObject::invokeMethod(self, [self, event = MLEvent(event)] {
        self->onVlcMlEvent(event);
    });
}

MLItemId MLBaseModel::parentId() const
{
    return m_parent;
}

void MLBaseModel::setParentId(MLItemId parentId)
{
    beginResetModel();
    m_parent = parentId;
    clear();
    endResetModel();
    emit parentIdChanged();
}

void MLBaseModel::unsetParentId()
{
    beginResetModel();
    m_parent = MLItemId();
    clear();
    endResetModel();
    emit parentIdChanged();
}

MediaLib* MLBaseModel::ml() const
{
    return m_mediaLib;
}

void MLBaseModel::setMl(MediaLib* medialib)
{
    assert(medialib);
    m_ml = medialib->vlcMl();
    m_mediaLib = medialib;
    if ( m_ml_event_handle == nullptr )
        m_ml_event_handle.reset( vlc_ml_event_register_callback( m_ml, onVlcMlEvent, this ) );
}

const QString& MLBaseModel::searchPattern() const
{
    return m_search_pattern;
}

void MLBaseModel::setSearchPattern( const QString& pattern )
{
    QString patternToApply = pattern.length() < 3 ? nullptr : pattern;
    if (patternToApply == m_search_pattern)
        /* No changes */
        return;

    beginResetModel();
    m_search_pattern = patternToApply;
    clear();
    endResetModel();
}

Qt::SortOrder MLBaseModel::getSortOrder() const
{
    return m_sort_desc ? Qt::SortOrder::DescendingOrder : Qt::SortOrder::AscendingOrder;
}

void MLBaseModel::setSortOder(Qt::SortOrder order)
{
    beginResetModel();
    m_sort_desc = (order == Qt::SortOrder::DescendingOrder);
    clear();
    endResetModel();
    emit sortOrderChanged();
}

const QString MLBaseModel::getSortCriteria() const
{
    return criteriaToName(m_sort);
}

void MLBaseModel::setSortCriteria(const QString& criteria)
{
    beginResetModel();
    m_sort = nameToCriteria(criteria.toUtf8());
    clear();
    endResetModel();
    emit sortCriteriaChanged();
}

void MLBaseModel::unsetSortCriteria()
{
    beginResetModel();
    m_sort = VLC_ML_SORTING_DEFAULT;
    clear();
    endResetModel();
    emit sortCriteriaChanged();
}
