/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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
#pragma once

#include <vlc_media_library.h>

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>

#include <memory>

#include "mlmedia.hpp"

class MediaLib;
class MLItemId;
class MLEvent;

class MLMediaStore : public QObject
{
    Q_OBJECT

public:
    MLMediaStore(MediaLib *ml, QObject *parent = nullptr);
    ~MLMediaStore();

    void insert(const QString &mrl);
    void remove(const MLItemId &id);
    void clear();

    bool contains(const QString &mrl) const;

signals:
    void updated(const QString &mrl, const MLMedia &media);

private:
    static void onVlcMlEvent(void* data
                             , const vlc_ml_event_t* event);

    // updates associated Media
    void update(const MLItemId &id);

    void setMedia(const QString &mrl, MLMedia media);

    MediaLib *m_ml;

    std::unique_ptr<vlc_ml_event_callback_t,
                    std::function<void(vlc_ml_event_callback_t*)>> m_ml_event_handle;

    // maintain set of MRL to handle media creation events
    QSet<QString> m_files;

    // MLEvent doesn't provide media mrl
    // maintain a map for faster handling of ml events
    QHash<MLItemId, QString> m_mrls;
};
