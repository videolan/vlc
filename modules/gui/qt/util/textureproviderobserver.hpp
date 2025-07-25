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
#include <QMutex>
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
    Q_PROPERTY(QSize textureSize READ textureSize NOTIFY textureChanged FINAL)

public:
    explicit TextureProviderObserver(QObject *parent = nullptr);

    void setSource(const QQuickItem* source);
    QSize textureSize() const;

signals:
    void sourceChanged();
    void textureChanged();

private slots:
    void updateTextureSize();

private:
    QPointer<const QQuickItem> m_source;
    QPointer<QSGTextureProvider> m_provider;

    // It is not clear when `QSGTextureProvider::textureChanged()` can be signalled.
    // If it is only signalled during SG synchronization where Qt blocks the GUI thread,
    // we do not need explicit synchronization here. If it can be signalled at any time,
    // we can still rely on SG synchronization (instead of explicit synchronization) by
    // waiting until the next synchronization, but the delay might be more in that case.
    // At the same time, the source might be living in a different window where the SG
    // synchronization would not be blocking the (GUI) thread where this observer lives.
    mutable QMutex m_textureMutex; // Maybe QReadWriteLock would be better.

    QSize m_textureSize; // invalid by default
};

#endif // TEXTUREPROVIDEROBSERVER_HPP
