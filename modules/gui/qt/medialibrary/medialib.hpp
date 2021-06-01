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
#pragma once

#include <Qt>
#include <QAbstractListModel>
#include <QVariant>
#include <QHash>
#include <QByteArray>
#include <QList>
#include <QQuickWidget>
#include <QQuickItem>
#include <QMetaObject>
#include <QMetaMethod>
#include <QQmlEngine>
#include <QThreadPool>

#include <memory>

#include "qt.hpp"
#include "mlqmltypes.hpp"

class MediaLib : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool discoveryPending READ discoveryPending NOTIFY discoveryPendingChanged)
    Q_PROPERTY(int  parsingProgress READ parsingProgress NOTIFY parsingProgressChanged)
    Q_PROPERTY(QString discoveryEntryPoint READ discoveryEntryPoint NOTIFY discoveryEntryPointChanged)
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged)

public:
    MediaLib(qt_intf_t* _intf, QObject* _parent = nullptr );

    Q_INVOKABLE void addToPlaylist(const MLItemId &itemId, const QStringList &options = {});
    Q_INVOKABLE void addToPlaylist(const QString& mrl, const QStringList &options = {});
    Q_INVOKABLE void addToPlaylist(const QUrl& mrl, const QStringList &options = {});
    Q_INVOKABLE void addToPlaylist(const QVariantList& itemIdList, const QStringList &options = {});

    Q_INVOKABLE void addAndPlay(const MLItemId &itemId, const QStringList &options = {});
    Q_INVOKABLE void addAndPlay(const QString& mrl, const QStringList &options = {});
    Q_INVOKABLE void addAndPlay(const QUrl& mrl, const QStringList &options = {});
    Q_INVOKABLE void addAndPlay(const QVariantList&itemIdList, const QStringList &options = {});
    Q_INVOKABLE void insertIntoPlaylist(size_t index, const QVariantList &itemIds /*QList<MLParentId>*/, const QStringList &options = {});

    Q_INVOKABLE void reload();

    inline bool idle() const { return m_idle; }
    inline int discoveryPending() const { return m_discoveryPending; }
    inline QString discoveryEntryPoint() const { return m_discoveryEntryPoint; }
    inline int parsingProgress() const { return m_parsingProgress; }

    vlc_medialibrary_t* vlcMl();

    QThreadPool &threadPool() { return m_threadPool; }

signals:
    void discoveryStarted();
    void discoveryCompleted();
    void parsingProgressChanged( quint32 percent );
    void discoveryEntryPointChanged( QString entryPoint );
    void discoveryPendingChanged( bool state );
    void idleChanged();

private:
    static void onMediaLibraryEvent( void* data, const vlc_ml_event_t* event );

private:
    qt_intf_t* m_intf;

    bool m_idle = false;
    bool m_discoveryPending = false;
    int m_parsingProgress = 0;
    QString m_discoveryEntryPoint;

    /* Medialibrary */
    vlc_medialibrary_t* m_ml;
    std::unique_ptr<vlc_ml_event_callback_t, std::function<void(vlc_ml_event_callback_t*)>> m_event_cb;

    QThreadPool m_threadPool;
};
