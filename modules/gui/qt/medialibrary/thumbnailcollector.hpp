/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#ifndef THUMBNAILCOLLECTOR_HPP
#define THUMBNAILCOLLECTOR_HPP


#include <QObject>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <memory>

#include <functional>

class MediaLib;
struct vlc_medialibrary_t;
struct vlc_ml_event_callback_t;
struct vlc_ml_event_t;


class ThumbnailCollector : public QObject
{
    Q_OBJECT

public:
    ThumbnailCollector(QObject *parent = nullptr);

    void start(MediaLib *ml, const QSet<int64_t> &mlIds);

    QHash<int64_t, QString> allGenerated() { return m_thumbnails; }

signals:
    void finished();

private:
    static void onVlcMLEvent(void *data, const vlc_ml_event_t *event);

    QMutex m_mut;
    MediaLib *m_ml {};
    std::unique_ptr<vlc_ml_event_callback_t,
                    std::function<void(vlc_ml_event_callback_t*)>> m_ml_event_handle {};

    QSet<int64_t> m_pending;
    QHash<int64_t, QString> m_thumbnails;
};


#endif // THUMBNAILCOLLECTOR_HPP
