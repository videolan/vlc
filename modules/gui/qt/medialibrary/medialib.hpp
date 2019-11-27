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

#include <memory>

#include "qt.hpp"
#include "mlqmltypes.hpp"

class MediaLib : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool gridView READ isGridView WRITE setGridView NOTIFY gridViewChanged)
    Q_PROPERTY(bool discoveryPending READ discoveryPending NOTIFY discoveryPendingChanged)
    Q_PROPERTY(int  parsingProgress READ parsingProgress NOTIFY parsingProgressChanged)
    Q_PROPERTY(QString discoveryEntryPoint READ discoveryEntryPoint NOTIFY discoveryEntryPointChanged)
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged)

public:
    MediaLib(intf_thread_t* _intf, QObject* _parent = nullptr );

    Q_INVOKABLE void addToPlaylist(const MLParentId &itemId);
    Q_INVOKABLE void addToPlaylist(const QString& mrl);
    Q_INVOKABLE void addToPlaylist(const QUrl& mrl);
    Q_INVOKABLE void addToPlaylist(const QVariantList& itemIdList);

    Q_INVOKABLE void addAndPlay(const MLParentId &itemId);
    Q_INVOKABLE void addAndPlay(const QString& mrl);
    Q_INVOKABLE void addAndPlay(const QUrl& mrl);
    Q_INVOKABLE void addAndPlay(const QVariantList&itemIdList);

    Q_INVOKABLE void reload();

    inline bool idle() const { return m_idle; }
    inline int discoveryPending() const { return m_discoveryPending; }
    inline QString discoveryEntryPoint() const { return m_discoveryEntryPoint; }
    inline int parsingProgress() const { return m_parsingProgress; }

    vlc_medialibrary_t* vlcMl();

signals:
    void gridViewChanged();
    void reloadStarted();
    void reloadCompleted();
    void discoveryStarted();
    void discoveryCompleted();
    void parsingProgressChanged( quint32 percent );
    void discoveryEntryPointChanged( QString entryPoint );
    void discoveryPendingChanged( bool state );
    void idleChanged();

private:
    bool isGridView() const;
    void setGridView(bool);
    static void onMediaLibraryEvent( void* data, const vlc_ml_event_t* event );

private:
    void openMRLFromMedia(const vlc_ml_media_t& media, bool start );

    intf_thread_t* m_intf;

    bool m_gridView;
    bool m_idle = false;
    bool m_discoveryPending = false;
    int m_parsingProgress = 0;
    QString m_discoveryEntryPoint;

    /* Medialibrary */
    vlc_medialibrary_t* m_ml;
    std::unique_ptr<vlc_ml_event_callback_t, std::function<void(vlc_ml_event_callback_t*)>> m_event_cb;

};
