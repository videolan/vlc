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
#include <QQmlInfo>

#if __has_include(<rhi/qrhi.h>) // RHI is semi-public since Qt 6.6
#define RHI_HEADER_AVAILABLE
#include <rhi/qrhi.h>
#elif __has_include(<QtGui/private/qrhi_p.h>)
#define RHI_HEADER_AVAILABLE
#include <QtGui/private/qrhi_p.h>
#endif

TextureProviderObserver::TextureProviderObserver(QObject *parent)
    : QObject{parent}
{

}

void TextureProviderObserver::setSource(const QQuickItem *source, bool enforce)
{
    if (!enforce && (m_source == source))
        return;

    if (m_source)
    {
        if (Q_LIKELY(m_provider))
        {
            disconnect(m_provider, nullptr, this, nullptr);
            m_provider = nullptr;

            // We would like to reset the properties in these conditions:
            // - There is a new valid source. We want to reset because updating the properties is
            //   asynchronous. Even if update occurs, it may take a while, and it may never happen.
            // - There is no more source.
            resetProperties(); // memory order does not matter, `setSource()` is not called frequently.
        }
        else
        {
            // source changed before we got its `QSGTextureProvider`
            disconnect(m_source, nullptr, this, nullptr);
        }
    }

    m_source = source;
    emit sourceChanged();

    if (m_source)
    {
        assert(m_source->isTextureProvider());

        const auto init = [this, enforce]() {
            const auto window = m_source->window();
            assert(window);

            connect(window, &QQuickWindow::beforeSynchronizing, this, [this, window, source = m_source, enforce]() {
                if (Q_UNLIKELY(source != m_source)) // either different or null pointer
                    return; // we can simply return here, as if new source is valid a new connection would be established (this slot becomes stale either way)

                if (Q_UNLIKELY(m_source->window() != window))
                {
                    // This may happen if the source item changes its window before its old window starts synchronizing in the current or the next frame.
                    // There may be two situations:
                    // - Source item has a new valid window. This may happen if the item is moved to another window (such as by adjusting visual parent).
                    //   In this case it would be fine to continue if multiple windows are bound to the same rendering thread, but better to not risk it.
                    // - Source item no longer has a window. This is more likely than the former.
                    qmlDebug(this) << "source item's window: " << m_source->window() << " is not matching with the captured one: " << window << ". Trying again...";
                    QMetaObject::invokeMethod(this, [this, enforce]() {
                        if (Q_UNLIKELY(enforce))
                        {
                            qmlWarning(this) << "source item changed its window again, bailing out.";
                            setSource(nullptr); // bail out, tried once
                        }
                        else
                            setSource(m_source, true);
                    }, Qt::QueuedConnection);
                    return;
                }

                const auto provider = m_source->textureProvider(); // This can only be called in the rendering thread.
                assert(provider);

                assert(!m_provider || (provider == m_provider)); // If providers are different, source must be different too (which is handled above).
                if (Q_UNLIKELY(provider == m_provider)) // Unlikely, source set to something else (such as null), and then set back to its original value really fast.
                {
                    updateProperties(); // Not sure if really necessary, but we do not lose anything.
                    return;
                }

                m_provider = provider;

                connect(m_provider, &QSGTextureProvider::textureChanged, this, &TextureProviderObserver::updateProperties, Qt::DirectConnection);

                updateProperties();
            }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::DirectConnection));
        };

        if (m_source->window())
            init();
        else
            connect(m_source, &QQuickItem::windowChanged, this, init, Qt::SingleShotConnection);
    }
}

QSize TextureProviderObserver::textureSize() const
{
    // This is likely called in the QML/GUI thread.
    // QML/GUI thread can freely block the rendering thread to the extent the time is reasonable and a
    // fraction of `1/FPS`, because it is already throttled by v-sync (so it would just throttle less).
    return m_textureSize.load(std::memory_order_acquire);
}

QSize TextureProviderObserver::nativeTextureSize() const
{
    return m_nativeTextureSize.load(std::memory_order_acquire);
}

bool TextureProviderObserver::hasAlphaChannel() const
{
    return m_hasAlphaChannel.load(std::memory_order_acquire);
}

bool TextureProviderObserver::hasMipmaps() const
{
    return m_hasMipmaps.load(std::memory_order_acquire);
}

bool TextureProviderObserver::isAtlasTexture() const
{
    return m_isAtlasTexture.load(std::memory_order_acquire);
}

bool TextureProviderObserver::isValid() const
{
    return m_isValid.load(std::memory_order_acquire);
}

qint64 TextureProviderObserver::comparisonKey() const
{
    return m_comparisonKey.load(std::memory_order_acquire);
}

QRectF TextureProviderObserver::normalizedTextureSubRect() const
{
    return m_normalizedTextureSubRect.load(std::memory_order_acquire);
}

void TextureProviderObserver::updateProperties()
{
    // This is likely called in the rendering thread.
    // Rendering thread should avoid blocking the QML/GUI thread. In this case, unlike the high precision
    // timer case, it should be fine because the size may be inaccurate in the worst case until the next
    // frame when the size is sampled again. In high precision timer case, accuracy is favored over
    // potential stuttering.
    constexpr auto memoryOrder = std::memory_order_relaxed;

    if (m_provider)
    {
        if (const auto texture = m_provider->texture())
        {
            {
                // SG and native texture size

                // SG texture size:
                const auto textureSize = texture->textureSize();
                m_textureSize.store(textureSize, memoryOrder);

                {
                    // Native texture size

                    const auto legacyUpdateNativeTextureSize = [&]() {
                        const auto ntsr = texture->normalizedTextureSubRect();
                        m_nativeTextureSize.store({static_cast<int>(textureSize.width() / ntsr.width()),
                                                   static_cast<int>(textureSize.height() / ntsr.height())},
                                                  memoryOrder);
                    };

#ifdef RHI_HEADER_AVAILABLE
                    const QRhiTexture* const rhiTexture = texture->rhiTexture();
                    if (Q_LIKELY(rhiTexture))
                        m_nativeTextureSize.store(rhiTexture->pixelSize(), memoryOrder);
                    else
                        legacyUpdateNativeTextureSize();
#else
                    legacyUpdateNativeTextureSize();
#endif
                }
            }

            {
                // Normal rect
                const QRectF& normalizedTextureSubRect = texture->normalizedTextureSubRect();

                m_normalizedTextureSubRect.store(normalizedTextureSubRect, memoryOrder);
            }

            {
                // Alpha channel
                const bool hasAlphaChannel = texture->hasAlphaChannel();

                if (m_hasAlphaChannel.exchange(hasAlphaChannel, memoryOrder) != hasAlphaChannel)
                    emit hasAlphaChannelChanged(hasAlphaChannel);
            }

            {
                // Mipmaps
                const bool hasMipmaps = texture->hasMipmaps();

                if (m_hasMipmaps.exchange(hasMipmaps, memoryOrder) != hasMipmaps)
                    emit hasMipmapsChanged(hasMipmaps);
            }

            {
                // Atlas texture
                const bool isAtlasTexture = texture->isAtlasTexture();

                if (m_isAtlasTexture.exchange(isAtlasTexture, memoryOrder) != isAtlasTexture)
                    emit isAtlasTextureChanged(isAtlasTexture);
            }

            if (!m_isValid.exchange(true, memoryOrder))
                emit isValidChanged(true);

            {
                // Comparison key
                const qint64 comparisonKey = texture->comparisonKey();

                if (m_comparisonKey.exchange(comparisonKey, memoryOrder) != comparisonKey)
                    emit comparisonKeyChanged(comparisonKey);
            }

            return;
        }
    }

    resetProperties(memoryOrder);
}

void TextureProviderObserver::resetProperties(std::memory_order memoryOrder)
{
    m_textureSize.store({}, memoryOrder);
    m_nativeTextureSize.store({}, memoryOrder);
    m_normalizedTextureSubRect.store({}, memoryOrder);

    if (m_hasAlphaChannel.exchange(false, memoryOrder))
        emit hasAlphaChannelChanged(false);

    if (m_hasMipmaps.exchange(false, memoryOrder))
        emit hasMipmapsChanged(false);

    if (m_isAtlasTexture.exchange(false, memoryOrder))
        emit isAtlasTextureChanged(false);

    if (m_isValid.exchange(false, memoryOrder))
        emit isValidChanged(false);

    if (m_comparisonKey.exchange(-1, memoryOrder) != -1)
        emit comparisonKeyChanged(-1);
}
