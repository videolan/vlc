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

#ifndef MLQMLTYPES_HPP
#define MLQMLTYPES_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QHash>
#include <QObject>
#include <vlc_common.h>
#include <vlc_media_library.h>

static constexpr int64_t INVALID_MLITEMID_ID = 0;

static const QHash<QStringView, vlc_ml_parent_type> ml_parent_map = {
    { QStringLiteral("VLC_ML_PARENT_ALBUM"), VLC_ML_PARENT_ALBUM },
    { QStringLiteral("VLC_ML_PARENT_ARTIST"), VLC_ML_PARENT_ARTIST },
    { QStringLiteral("VLC_ML_PARENT_SHOW"), VLC_ML_PARENT_SHOW },
    { QStringLiteral("VLC_ML_PARENT_GENRE"), VLC_ML_PARENT_GENRE },
    { QStringLiteral("VLC_ML_PARENT_GROUP"), VLC_ML_PARENT_GROUP },
    { QStringLiteral("VLC_ML_PARENT_FOLDER"), VLC_ML_PARENT_FOLDER },
    { QStringLiteral("VLC_ML_PARENT_PLAYLIST"), VLC_ML_PARENT_PLAYLIST }
};

class MLItemId
{
    Q_GADGET
public:
    MLItemId() : id(INVALID_MLITEMID_ID), type( VLC_ML_PARENT_UNKNOWN ) {}
    MLItemId( int64_t i, vlc_ml_parent_type t ) : id( i ), type( t ) {}
    bool operator==( const MLItemId& other ) const
    {
        return id == other.id && type == other.type;
    }
    bool operator!=( const MLItemId& other ) const
    {
        return !(*this == other);
    }
    bool operator<( const MLItemId& other ) const
    {
        return id < other.id;
    }

    int64_t id;
    vlc_ml_parent_type type;

    Q_INVOKABLE bool isValid() const {
        return (id != INVALID_MLITEMID_ID);
    }

    Q_INVOKABLE constexpr bool hasParent() const {
        return (type != VLC_ML_PARENT_UNKNOWN);
    }

    Q_INVOKABLE inline QString toString() const {

#define ML_PARENT_TYPE_CASE(type) case type: return QString("%1 - %2").arg(#type).arg(id)
        switch (type) {
            ML_PARENT_TYPE_CASE(VLC_ML_PARENT_ALBUM);
            ML_PARENT_TYPE_CASE(VLC_ML_PARENT_ARTIST);
            ML_PARENT_TYPE_CASE(VLC_ML_PARENT_SHOW);
            ML_PARENT_TYPE_CASE(VLC_ML_PARENT_GENRE);
            ML_PARENT_TYPE_CASE(VLC_ML_PARENT_GROUP);
            ML_PARENT_TYPE_CASE(VLC_ML_PARENT_FOLDER);
            ML_PARENT_TYPE_CASE(VLC_ML_PARENT_PLAYLIST);
        default:
            return QString("UNKNOWN - %2").arg(id);
        }
#undef ML_PARENT_TYPE_CASE
    }

    Q_INVOKABLE static inline MLItemId fromString(const QStringView& serialized_id) {
        const QList<QStringView> parts = serialized_id.split('-'); // Type, ID
        if (parts.length() != 2) {
            return {INVALID_MLITEMID_ID, VLC_ML_PARENT_UNKNOWN};
        }

        const QStringView type = parts[0].trimmed();
        bool conversionSuccessful = false;
        std::int64_t item_id = parts[1].trimmed().toLongLong(&conversionSuccessful);
        if (!conversionSuccessful) {
            return {INVALID_MLITEMID_ID, VLC_ML_PARENT_UNKNOWN};
        }

        return { item_id, ml_parent_map.value(type, VLC_ML_PARENT_UNKNOWN) };
    }
};


inline size_t qHash(const MLItemId& item, size_t seed = 0)
{
    return qHashMulti(seed, item.id, item.type);
}

class MLItem
{
public:
    MLItem(MLItemId id) : m_id(id) {}
    virtual ~MLItem() = default;

    MLItemId getId() const { return m_id; }

private:
    MLItemId m_id;
};

#endif // MLQMLTYPES_HPP
