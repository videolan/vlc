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
#include "textureprovideritem.hpp"

#include <QSGTextureProvider>
#include <QRunnable>
#include <QMutexLocker>

class TextureProviderCleaner : public QRunnable
{
public:
    explicit TextureProviderCleaner(QSGTextureProvider *textureProvider)
        : m_textureProvider(textureProvider) { }

    void run() override
    {
        delete m_textureProvider;
    }

private:
    const QPointer<QSGTextureProvider> m_textureProvider;
};

TextureProviderItem::~TextureProviderItem()
{
    {
        if (m_textureProvider)
        {
            // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling

            // `QQuickItem::releaseResources()` is called before the item is disassociated from its window:
            // "This happens when the item is about to be removed from the window it was previously rendering to."
            // Therefore, we can not have a texture provider during destruction, but not the window:
            assert(window());
            window()->scheduleRenderJob(new TextureProviderCleaner(m_textureProvider), QQuickWindow::BeforeSynchronizingStage);
            m_textureProvider = nullptr;
        }
    }
}

bool TextureProviderItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *TextureProviderItem::textureProvider() const
{
    // This method is called from the rendering thread.

    if (!m_textureProvider)
    {
        m_textureProvider = new QSGTextureViewProvider;

        const auto adjustSource = [provider = m_textureProvider](const QQuickItem *source) {
            if (source)
            {
                assert(source->isTextureProvider() &&
                       "TextureProviderItem: " \
                       "TextureProviderItem's source item is not a texture provider. " \
                       "Layering can be enabled for the source item in order to make " \
                       "it a texture provider.");

                provider->setTextureProvider(source->textureProvider());
            }
            else
            {
                provider->setTextureProvider(nullptr);
            }
        };

        const auto synchronizeState = [weakThis = QPointer(this), provider = m_textureProvider]() {
            if (Q_UNLIKELY(!weakThis))
                return;

            provider->setFiltering(weakThis->m_filtering);
            provider->setMipmapFiltering(weakThis->m_mipmapFiltering);
            provider->setAnisotropyLevel(weakThis->m_anisotropyLevel);
            provider->setHorizontalWrapMode(weakThis->m_horizontalWrapMode);
            provider->setVerticalWrapMode(weakThis->m_verticalWrapMode);

            if (weakThis->m_detachAtlasTextures)
                provider->requestDetachFromAtlas();
        };

        // These are going to be queued when necessary:
        connect(this, &TextureProviderItem::sourceChanged, m_textureProvider, adjustSource);
        connect(this, &TextureProviderItem::rectChanged, m_textureProvider, &QSGTextureViewProvider::setRect, Qt::DirectConnection);

        connect(this, &TextureProviderItem::filteringChanged, m_textureProvider, &QSGTextureViewProvider::setFiltering);
        connect(this, &TextureProviderItem::mipmapFilteringChanged, m_textureProvider, &QSGTextureViewProvider::setMipmapFiltering);
        connect(this, &TextureProviderItem::anisotropyLevelChanged, m_textureProvider, &QSGTextureViewProvider::setAnisotropyLevel);
        connect(this, &TextureProviderItem::horizontalWrapModeChanged, m_textureProvider, &QSGTextureViewProvider::setHorizontalWrapMode);
        connect(this, &TextureProviderItem::verticalWrapModeChanged, m_textureProvider, &QSGTextureViewProvider::setVerticalWrapMode);

        connect(this, &TextureProviderItem::detachAtlasTexturesChanged, m_textureProvider, [provider = m_textureProvider](bool detach) {
            if (detach)
                provider->requestDetachFromAtlas();
        });

        // When the target texture changes, the texture view may reset its state, so we need to synchronize in that case:
        connect(m_textureProvider, &QSGTextureProvider::textureChanged, m_textureProvider, synchronizeState); // Executed in texture provider's thread

        // Initial adjustments:
        adjustSource(m_source);
        if (m_rect.isValid())
            m_textureProvider->setRect(m_rect);
        synchronizeState();
    }
    return m_textureProvider;
}

void TextureProviderItem::resetTextureSubRect()
{
    m_rect = {};
    emit rectChanged({});
}

void TextureProviderItem::invalidateSceneGraph()
{
    // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling

    // This slot is called from the rendering thread.
    {
        if (m_textureProvider)
        {
            delete m_textureProvider;
        }
    }
}

void TextureProviderItem::releaseResources()
{
    // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling

    // This method is called from the GUI thread.

    // QQuickItem::releaseResources() is guaranteed to have a valid window when it is called:
    assert(window());
    {
        if (m_textureProvider)
        {
            window()->scheduleRenderJob(new TextureProviderCleaner(m_textureProvider), QQuickWindow::BeforeSynchronizingStage);
            m_textureProvider = nullptr;
        }
    }

    QQuickItem::releaseResources();
}

void QSGTextureViewProvider::adjustTexture()
{
    if (m_textureProvider)
        m_textureView.setTexture(m_textureProvider->texture());
    else
        m_textureView.setTexture(nullptr);

    // `textureChanged()` is emitted implicitly, no need to emit here explicitly again.
}

QSGTextureViewProvider::QSGTextureViewProvider()
    : QSGTextureProvider()
{
    connect(&m_textureView, &QSGTextureView::updateRequested, this, &QSGTextureProvider::textureChanged);
}

QSGTexture *QSGTextureViewProvider::texture() const
{
    if (m_textureProvider && m_textureView.texture())
        return &m_textureView;
    else
        return nullptr;
}

void QSGTextureViewProvider::setTextureProvider(const QSGTextureProvider *textureProvider)
{
    if (m_textureProvider == textureProvider)
        return;

    if (m_textureProvider)
        disconnect(m_textureProvider, &QSGTextureProvider::textureChanged, this, &QSGTextureViewProvider::adjustTexture);

    m_textureProvider = textureProvider;

    if (m_textureProvider)
    {
        connect(m_textureProvider, &QSGTextureProvider::textureChanged, this, &QSGTextureViewProvider::adjustTexture);
        connect(m_textureProvider, &QObject::destroyed, this, [this]() { setTextureProvider(nullptr); });
    }

    adjustTexture();
}

void QSGTextureViewProvider::setRect(const QRect &rect)
{
    m_textureView.setRect(rect);
    // `textureChanged()` is emitted implicitly, no need to emit here explicitly again
}

void QSGTextureViewProvider::setMipmapFiltering(QSGTexture::Filtering filter)
{
    if (m_textureView.mipmapFiltering() == filter)
        return;

    if (filter != QSGTexture::Filtering::None)
    {
        const auto targetTexture = m_textureView.texture();
        // If there is no target texture, we can accept mipmap filtering. When there becomes a target texture, `QSGTextureView` should
        // consider this itself anyway if the new target texture has no mipmaps. Workarounds should probably not be over-conservative,
        // we should not dismiss the case if there is no target texture now but the upcoming texture has mip maps.
        if (targetTexture && !targetTexture->hasMipmaps())
        {
            // Having mip map filtering when there are no mip maps may be problematic with certain graphics backends (like OpenGL).
            return;
        }
    }

    m_textureView.setMipmapFiltering(filter);
    emit textureChanged();
}

void QSGTextureViewProvider::setFiltering(QSGTexture::Filtering filter)
{
    if (m_textureView.filtering() == filter)
        return;

    m_textureView.setFiltering(filter);
    emit textureChanged();
}

void QSGTextureViewProvider::setAnisotropyLevel(QSGTexture::AnisotropyLevel level)
{
    if (m_textureView.anisotropyLevel() == level)
        return;

    m_textureView.setAnisotropyLevel(level);
    emit textureChanged();
}

void QSGTextureViewProvider::setHorizontalWrapMode(QSGTexture::WrapMode hwrap)
{
    if (m_textureView.horizontalWrapMode() == hwrap)
        return;

    m_textureView.setHorizontalWrapMode(hwrap);
    emit textureChanged();
}

void QSGTextureViewProvider::setVerticalWrapMode(QSGTexture::WrapMode vwrap)
{
    if (m_textureView.verticalWrapMode() == vwrap)
        return;

    m_textureView.setVerticalWrapMode(vwrap);
    emit textureChanged();
}

void QSGTextureViewProvider::requestDetachFromAtlas()
{
    m_textureView.requestDetachFromAtlas();
}
