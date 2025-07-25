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
#include "textureproviderobserver.hpp"

#include <QSGTextureProvider>

TextureProviderObserver::TextureProviderObserver(QObject *parent)
    : QObject{parent}
{

}

void TextureProviderObserver::setSource(const QQuickItem *source)
{
    if (m_source == source)
        return;

    {
        QMutexLocker locker(&m_textureMutex);
        m_textureSize = {};

        if (m_source)
        {
            if (Q_LIKELY(m_provider))
            {
                disconnect(m_provider, nullptr, this, nullptr);
                m_provider = nullptr;
            }
            else
            {
                // source changed before we got its `QSGTextureProvider`
                disconnect(m_source, nullptr, this, nullptr);
            }
        }
    }

    m_source = source;

    if (m_source)
    {
        assert(m_source->isTextureProvider());

        const auto init = [this]() {
            const auto window = m_source->window();
            assert(window);

            connect(window, &QQuickWindow::beforeSynchronizing, this, [this, window]() {
                assert(m_source->window() == window);
                assert(!m_provider);

                m_provider = m_source->textureProvider(); // This can only be called in the rendering thread.
                assert(m_provider);

                connect(m_provider, &QSGTextureProvider::textureChanged, this, &TextureProviderObserver::updateTextureSize, Qt::DirectConnection);
                connect(m_provider, &QSGTextureProvider::textureChanged, this, &TextureProviderObserver::textureChanged);

                updateTextureSize();

                emit textureChanged(); // This should be safe if QML engine uses auto connection (default), otherwise we need to queue ourselves.
            }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::DirectConnection));
        };

        if (m_source->window())
            init();
        else
            connect(m_source, &QQuickItem::windowChanged, this, init, Qt::SingleShotConnection);
    }

    emit sourceChanged();
}

QSize TextureProviderObserver::textureSize() const
{
    QMutexLocker locker(&m_textureMutex);
    return m_textureSize;
}

void TextureProviderObserver::updateTextureSize()
{
    // Better to lock as early as possible, QML can wait until the
    // update completes to get the up-to-date size (if it requests
    // read for any reason meanwhile).
    QMutexLocker locker(&m_textureMutex);

    if (m_provider)
    {
        if (const auto texture = m_provider->texture())
        {
            m_textureSize = texture->textureSize();
            return;
        }
    }

    m_textureSize = {};
}
