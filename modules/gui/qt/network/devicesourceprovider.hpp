/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qt.hpp"

#include <QString>
#include <QSet>
#include <vector>
#include <QMutex>

#include "networkdevicemodel.hpp"
#include "mediatreelistener.hpp"
#include "util/shared_input_item.hpp"
#include "vlcmediasourcewrapper.hpp"
#include "util/singleton.hpp"

static inline std::size_t qHash(const SharedInputItem& item, size_t seed = 0) noexcept
{
    QString name = qfu(item->psz_name);
    QUrl mainMrl = QUrl::fromEncoded(item->psz_uri);
    QString protocol = mainMrl.scheme();
    return qHash(name, seed) ^ qHash(protocol);;
}

class MediaSourceModel : public QObject
{
    Q_OBJECT
public:

    MediaSourceModel(MediaSourcePtr& mediaSource);
    ~MediaSourceModel();

public:
    void init();

    const std::vector<SharedInputItem>& getMedias() const;

    QString getDescription() const;
    MediaTreePtr getTree() const;

signals:
    void mediaAdded(SharedInputItem media);
    void mediaRemoved(SharedInputItem media);

private:
    void addItems(const std::vector<SharedInputItem>& inputList,
                  const MediaSourcePtr& mediaSource,
                  bool clear);

    void removeItems(const std::vector<SharedInputItem>& inputList,
                     const MediaSourcePtr& mediaSource);

    struct ListenerCb;
    std::unique_ptr<MediaTreeListener> m_listenner;
    MediaSourcePtr m_mediaSource;
    std::vector<SharedInputItem> m_medias;
};
typedef QSharedPointer<MediaSourceModel> SharedMediaSourceModel;


class MediaSourceCache : public Singleton<MediaSourceCache>
{
public:
    SharedMediaSourceModel getMediaSourceModel(vlc_media_source_provider_t* provider, const char* name);

private:
    MediaSourceCache() = default;
    ~MediaSourceCache() = default;
    friend class Singleton<MediaSourceCache>;

private:
    //we keep a weak pointer to the model sources, if no other party is
    //referencing the source, then it will be freed,
    std::map<QString, QWeakPointer<MediaSourceModel>> m_cache;
    QMutex m_mutex;
};

class DeviceSourceProvider : public QObject
{
    Q_OBJECT

public:
    DeviceSourceProvider(NetworkDeviceModel::SDCatType sdSource,
                         const QString &sourceName,
                         MainCtx* ctx,
                         QObject *parent = nullptr);
    virtual ~DeviceSourceProvider();

    void init();

    const std::vector<SharedMediaSourceModel>& getMediaSources() const;

signals:
    void failed();
    void nameUpdated( QString name );
    void itemsUpdated();

private:
    MainCtx* m_ctx = nullptr;
    quint64 m_taskId = 0;

    NetworkDeviceModel::SDCatType m_sdSource;
    QString m_sourceName; // '*' -> all sources
    QString m_name; // source long name

    std::vector<SharedMediaSourceModel> m_mediaSources;
};
