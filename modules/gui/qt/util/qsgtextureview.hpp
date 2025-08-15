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
#ifndef QSGTEXTUREVIEW_HPP
#define QSGTEXTUREVIEW_HPP

#include <QSGTexture>

#include <optional>

class QSGTextureView : public QSGDynamicTexture
{
    Q_OBJECT

    QSGTexture* m_texture = nullptr;
    QRect m_rect;
    mutable std::optional<QRectF> m_normalRect;
    mutable bool m_normalRectChanged = false;

private slots:
    bool adjustNormalRect() const;

public slots:
    // Reset the view texture state to the target texture state.
    // NOTE: When `setTexture()` is called (regardless of if the texture pointer changes), this is called implicitly.
    //       The reason for that is, some states such as mipmap filtering, is only applicable when the target texture
    //       has mipmaps. Therefore it only makes sense to reset the state when the texture changes, because we do not
    //       know if the new texture has mipmaps or not. At the same time, it is observed that `QSGTextureProvider::textureChanged()`
    //       is also emitted when the texture pointer is the same but some states change (`QQuickImage` case), so for
    //       convenience reasons this is also called when the texture pointer does not change upon `setTexture()` call,
    //       as it is expected that if a texture provider uses this class, it would connect source texture change signal
    //       to `setTexture()` here (as done in `QSGTextureViewProvider` at the moment).
    // NOTE: This slot returns `true` if a state changes, and `false` otherwise.
    // NOTE: This slot does not emit `updateRequested()` itself if a state changes.
    bool resetState();

public:
    QSGTextureView() = default;
    explicit QSGTextureView(QSGTexture* texture);

    QSGTexture* texture() const;
    void setTexture(QSGTexture* texture);

    // Subtexturing:
    QRect rect() const;
    void setRect(const QRect& rect);

    qint64 comparisonKey() const override;
    QRhiTexture *rhiTexture() const override;
    QSize textureSize() const override;
    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override;

    QRectF normalizedTextureSubRect() const override;

    bool isAtlasTexture() const override;

    QSGTexture *removedFromAtlas(QRhiResourceUpdateBatch *resourceUpdates = nullptr) const override;

    void commitTextureOperations(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates) override;

    bool updateTexture() override;

signals:
    void updateRequested();
};

#endif // QSGTEXTUREVIEW_HPP
