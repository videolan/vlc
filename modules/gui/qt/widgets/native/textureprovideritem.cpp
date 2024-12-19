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
        QMutexLocker lock(&m_textureProviderMutex);
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

    QMutexLocker lock(&m_textureProviderMutex);
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

        const auto adjustRect = [provider = m_textureProvider](const QRect& rect) {
            if (rect.isValid())
                provider->setRect(rect);
        };

        connect(this, &TextureProviderItem::sourceChanged, m_textureProvider, adjustSource);
        connect(this, &TextureProviderItem::rectChanged, m_textureProvider, adjustRect);

        // Initial adjustments:
        adjustSource(m_source);
        adjustRect(m_rect);
    }
    return m_textureProvider;
}

void TextureProviderItem::invalidateSceneGraph()
{
    // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling

    // This slot is called from the rendering thread.
    {
        QMutexLocker lock(&m_textureProviderMutex);
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
        QMutexLocker lock(&m_textureProviderMutex);
        if (m_textureProvider)
        {
            window()->scheduleRenderJob(new TextureProviderCleaner(m_textureProvider), QQuickWindow::BeforeSynchronizingStage);
            m_textureProvider = nullptr;
        }
    }

    QQuickItem::releaseResources();
}

void TextureProviderItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemDevicePixelRatioHasChanged)
    {
        emit dprChanged();
    }

    QQuickItem::itemChange(change, value);
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
