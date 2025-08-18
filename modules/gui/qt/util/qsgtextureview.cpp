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
#include "qsgtextureview.hpp"

QSGTextureView::QSGTextureView(QSGTexture *texture)
{
    setTexture(texture);
}

QSGTexture *QSGTextureView::texture() const
{
    return m_texture;
}

void QSGTextureView::setTexture(QSGTexture *texture)
{
    if (m_texture == texture)
    {
        if (m_texture)
        {
            if (resetState())
                emit updateRequested();
        }

        return;
    }

    if (m_texture)
    {
        disconnect(m_texture, nullptr, this, nullptr);
    }

    m_texture = texture;

    if (texture)
    {
        resetState();

        // Maybe use `QMetaObject::indexOfSignal()` instead to probe `updateRequested()`?
        if (qobject_cast<QSGTextureView*>(texture) || texture->inherits("QSGLayer"))
        {
            // Since Qt 5, it is guaranteed that slots are executed in the order they
            // are connected. The order is important here, we want to emit `updateRequested()`
            // only after necessary adjustments:
            connect(texture, SIGNAL(updateRequested()), this, SLOT(resetState()));
            connect(texture, SIGNAL(updateRequested()), this, SLOT(adjustNormalRect()));
            connect(texture, SIGNAL(updateRequested()), this, SIGNAL(updateRequested()));
        }

        adjustNormalRect();
    }
    else
    {
        // Invalidate the normal rect, so that it is calculated via
        // `normalizedTextureSubRect()` when there is a texture:
        m_normalRect.reset();
    }

    emit updateRequested();
}

bool QSGTextureView::adjustNormalRect() const
{
    if (!m_rect.isValid())
        return false;

    assert(m_texture);

    QSizeF size = m_texture->textureSize();

    if (size.isValid())
    {
        m_normalRect = QRectF(m_rect.x() / size.width(),
                              m_rect.y() / size.height(),
                              m_rect.width() / size.width(),
                              m_rect.height() / size.height());
        m_normalRectChanged = true;
        return true;
    }

    // Do not emit `updateRequested()` here, as this method may be called
    // from `normalizedTextureSubRect()`. At the same time, this method is
    // called when `updateRequested()` is emitted, it should not emit the
    // same signal. Instead, `setRect()` emits `updateRequested()` when
    // applicable.

    return false;
}

QRect QSGTextureView::rect() const
{
    return m_rect;
}

void QSGTextureView::setRect(const QRect &rect)
{
    if (m_rect == rect)
        return;

    if (!rect.isValid())
        return;

    m_rect = rect;

    // We need the source texture in order to calculate the normal rect.
    // If texture is not available when the rect is set, it will be done
    // later in `normalizedTextureSubRect()`.
    if (m_texture)
    {
        adjustNormalRect();
        emit updateRequested();
    }
    else
    {
        // Invalidate the normal rect, so that it is calculated via
        // `normalizedTextureSubRect()` when there is a texture:
        m_normalRect.reset();
    }
}

bool QSGTextureView::resetState()
{
    assert(m_texture); // This method must not be called when there is no target texture.

    bool changeMade = false;

    if (const auto newFiltering = m_texture->filtering(); filtering() != newFiltering)
    {
        setFiltering(newFiltering);
        changeMade = true;
    }

    {
        // Qt bug: source `QSGTexture` has mipmap filtering, but has no mipmaps. Depending on the graphics backend,
        //         this may cause the texture to be not rendered (OpenGL case). As a workaround, we must disable
        //         mipmap filtering here.

        // Testing `QRhiTexture::flags()` with `QRhiTexture::MipMapped` would make more sense, as that is expected to
        // actually reflect if the underlying native texture actually has mip maps. But this should be okay also as
        // RHI stuff are still semi-public as of Qt 6.9, and private before Qt 6.6 (we are supporting Qt 6.2).
        if (m_texture->hasMipmaps())
        {
            if (const auto newMipmapFiltering = m_texture->mipmapFiltering(); mipmapFiltering() != newMipmapFiltering)
            {
                setMipmapFiltering(newMipmapFiltering);
                changeMade = true;
            }
        }
        else if (mipmapFiltering() != QSGTexture::Filtering::None)
        {
            setMipmapFiltering(QSGTexture::Filtering::None);
            changeMade = true;
        }
    }

    if (const auto newAnisotropyLevel = m_texture->anisotropyLevel(); anisotropyLevel() != newAnisotropyLevel)
    {
        setAnisotropyLevel(newAnisotropyLevel);
        changeMade = true;
    }

    if (const auto newHWrapMode = m_texture->horizontalWrapMode(); horizontalWrapMode() != newHWrapMode)
    {
        setHorizontalWrapMode(newHWrapMode);
        changeMade = true;
    }

    if (const auto newVWrapMode = m_texture->verticalWrapMode(); verticalWrapMode() != newVWrapMode)
    {
        setHorizontalWrapMode(newVWrapMode);
        changeMade = true;
    }

    return changeMade;
}

qint64 QSGTextureView::comparisonKey() const
{
    assert(m_texture);
    return m_texture->comparisonKey();
}

QRhiTexture *QSGTextureView::rhiTexture() const
{
    assert(m_texture);
    return m_texture->rhiTexture();
}

QSize QSGTextureView::textureSize() const
{
    assert(m_texture);
    if (m_rect.isNull())
        return m_texture->textureSize();
    else
        return m_rect.size();
}

bool QSGTextureView::hasAlphaChannel() const
{
    assert(m_texture);
    return m_texture->hasAlphaChannel();
}

bool QSGTextureView::hasMipmaps() const
{
    assert(m_texture);
    return m_texture->hasMipmaps();
}

QRectF QSGTextureView::normalizedTextureSubRect() const
{
    assert(m_texture);

    if (!m_normalRect.has_value())
        adjustNormalRect();

    QRectF subRect = m_texture->normalizedTextureSubRect();

    if (m_normalRect.has_value() && m_normalRect->isValid())
    {
        // Sub texture:
        // NOTE: The source texture might be in the atlas.
        subRect = QRectF(subRect.x() + m_normalRect->x() * subRect.width(),
                         subRect.y() + m_normalRect->y() * subRect.height(),
                         m_normalRect->width() * subRect.width(),
                         m_normalRect->height() * subRect.height());
    }

    return subRect;
}

bool QSGTextureView::isAtlasTexture() const
{
    assert(m_texture);
    // If Qt does not respect normalizedTextureSubRect() of a QSGTexture
    // that does not report to be in the atlas texture, we should return
    // true regardless. Experiments show that Qt cares about the normalized
    // sub rect even if the texture is not in the atlas.
    return m_texture->isAtlasTexture();
}

QSGTexture *QSGTextureView::removedFromAtlas(QRhiResourceUpdateBatch *batch) const
{
    assert(m_texture); // it is absurd to call removedFromAtlas() when there is no target

    // We can "remove" the target texture from the atlas, and re-attach the view to the detached texture.
    // It should be noted that this method does not mutate the target texture, which can be guessed by
    // the method signature (being a const method), but rather creates a new texture which is an independent
    // texture that has its content copied from the atlas texture (still owned by the atlas texture).
    // This would not cause a leak, because:
    // 1) we are not owning the atlas texture (current view target) here.
    // 2) the returned (detached) texture is owned by the atlas texture (current view target).
    QSGTexture *const detachedTexture = m_texture->removedFromAtlas(batch);
    if (Q_LIKELY(detachedTexture))
    {
        // This virtual method is const, which is fine for QSGTexture (removing
        // from atlas means returning a new image which is a copy from the relevant
        // part of the atlas), but not for QSGTextureView, because the texture view
        // needs to mutate to point to the detached texture. There is not much to
        // do besides 1) creating a new QSGTextureView and returning it, and leaking
        // this instance 2) using const cast.
        const_cast<QSGTextureView*>(this)->setTexture(detachedTexture);
        return const_cast<QSGTextureView*>(this);
    }

    return nullptr;
}

void QSGTextureView::commitTextureOperations(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates)
{
    if (m_texture)
        m_texture->commitTextureOperations(rhi, resourceUpdates);
}

bool QSGTextureView::updateTexture()
{
    bool ret = false;

    if (const auto dynamicTexture = qobject_cast<QSGDynamicTexture*>(m_texture))
    {
        if (dynamicTexture->updateTexture())
            ret = true;
    }

    if (m_normalRectChanged)
    {
        ret = true;
        m_normalRectChanged = false;
    }

    return ret;
}

