/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
#ifndef TEXTUREPROVIDEROBSERVER_HPP
#define TEXTUREPROVIDEROBSERVER_HPP

#include <QObject>
#include <QReadWriteLock>
#include <QPointer>
#include <QSize>
#include <QQuickItem>

class QSGTextureProvider;

// This utility class observes a texture provider and exposes its texture's properties
// so that they can be accessed in QML where texture introspection is not possible.
class TextureProviderObserver : public QObject
{
    Q_OBJECT

    // NOTE: source must be a texture provider item.
    Q_PROPERTY(const QQuickItem* source MEMBER m_source WRITE setSource NOTIFY sourceChanged FINAL)

    // WARNING: Texture properties are updated in the rendering thread.
    // WARNING: Individual properties do not necessarily reflect the same texture at any arbitrary time.
    //          In other words, properties are not updated atomically as a whole but independently
    //          once the source texture changes. It depends on when you are reading the properties,
    //          such that if you read properties before `updateTextureSize()` returns, the properties
    //          may be inconsistent with each other (some reflecting the old texture, some the new
    //          texture). This is done to reduce blocking the GUI thread from the rendering thread,
    //          which is a problem unlike vice versa, and is considered acceptable because sampling
    //          is supposed to be done periodically anyway (the sampling point must be chosen carefully
    //          to not conflict with the updates, if the properties must reflect the immediately up-to-date
    //          texture and the properties change each frame, as otherwise it might end up in a
    //          "forever chase"), so by the time the sampling is done the properties should be consistent.
    // NOTE: These properties do not provide notify signal, dynamic textures such as layer may
    //       change rapidly (even though throttled by v-sync in the rendering thread), and if
    //       such signal is connected to a receiver that lives in the GUI thread, the queued
    //       invocations can easily backlog. Similar to the high precision timer, we moved
    //       away from event based approach in favor of sampling based approach here.
    Q_PROPERTY(QSize textureSize READ textureSize FINAL) // Scene graph texture size
    Q_PROPERTY(QSize nativeTextureSize READ nativeTextureSize FINAL) // Native texture size (e.g. for atlas textures, the atlas size)

public:
    explicit TextureProviderObserver(QObject *parent = nullptr);

    void setSource(const QQuickItem* source);
    QSize textureSize() const;
    QSize nativeTextureSize() const;

signals:
    void sourceChanged();

private slots:
    void updateTextureSize();

private:
    QPointer<const QQuickItem> m_source;
    QPointer<QSGTextureProvider> m_provider;

    // It is not clear when `QSGTextureProvider::textureChanged()` can be signalled.
    // If it is only signalled during SG synchronization where Qt blocks the GUI thread,
    // we do not need explicit synchronization (with atomic, or mutex) here. If it can be
    // signalled at any time, we can still rely on SG synchronization (instead of explicit
    // synchronization) by waiting until the next synchronization, but the delay might be
    // more in that case. At the same time, the source might be living in a different window
    // where the SG synchronization would not be blocking the (GUI) thread where this
    // observer lives.

    std::atomic<QSize> m_textureSize {{}}; // invalid by default
    std::atomic<QSize> m_nativeTextureSize {{}}; // invalid by default
};

#endif // TEXTUREPROVIDEROBSERVER_HPP
