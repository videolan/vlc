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
#ifndef TEXTUREPROVIDERITEM_HPP
#define TEXTUREPROVIDERITEM_HPP

#include <QQuickItem>
#include <QSGTextureProvider>
#include <QMutex>

#include "util/qsgtextureview.hpp"

class QSGTextureViewProvider : public QSGTextureProvider
{
    Q_OBJECT

    mutable QSGTextureView m_textureView;

    const QSGTextureProvider* m_textureProvider = nullptr;

private:
    void adjustTexture();

public:
    QSGTextureViewProvider();

    QSGTexture *texture() const override;

    void setTextureProvider(const QSGTextureProvider *textureProvider);

    void setRect(const QRect& rect);

    void setMipmapFiltering(QSGTexture::Filtering filter);
    void setFiltering(QSGTexture::Filtering filter);
    void setAnisotropyLevel(QSGTexture::AnisotropyLevel level);
    void setHorizontalWrapMode(QSGTexture::WrapMode hwrap);
    void setVerticalWrapMode(QSGTexture::WrapMode vwrap);

    void requestDetachFromAtlas();
};

class TextureProviderItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(const QQuickItem* source MEMBER m_source NOTIFY sourceChanged FINAL)
    // NOTE: Although this is named as `textureSubRect`, it is allowed to provide a larger size than
    //       the texture size. In that case, the texture's wrap mode is going to be relevant, provided
    //       that the graphics backend supports it. Do note that if the source texture is already a
    //       sub-texture (such as a texture in the atlas), wrapping would only be applicable outside
    //       the boundaries of the whole texture and not the source sub-texture, and not considering
    //       this may expose irrelevant parts of the atlas (this means that wrap mode is effectively
    //       useless for sub- or atlas textures). In such a case, `detachAtlasTextures` can be used.
    Q_PROPERTY(QRect textureSubRect MEMBER m_rect NOTIFY rectChanged RESET resetTextureSubRect FINAL)
    Q_PROPERTY(bool detachAtlasTextures MEMBER m_detachAtlasTextures NOTIFY detachAtlasTexturesChanged FINAL)

    Q_PROPERTY(QSGTexture::AnisotropyLevel anisotropyLevel MEMBER m_anisotropyLevel NOTIFY anisotropyLevelChanged FINAL)
    Q_PROPERTY(QSGTexture::WrapMode horizontalWrapMode MEMBER m_horizontalWrapMode NOTIFY horizontalWrapModeChanged FINAL)
    Q_PROPERTY(QSGTexture::WrapMode verticalWrapMode MEMBER m_verticalWrapMode NOTIFY verticalWrapModeChanged FINAL)
    // Maybe we should use `Item::smooth` instead of these properties, or maybe not, as we should allow disabling filtering completely.
    Q_PROPERTY(QSGTexture::Filtering filtering MEMBER m_filtering NOTIFY filteringChanged FINAL)
    // WARNING: mipmap filtering is not respected if target texture has no mip maps:
    Q_PROPERTY(QSGTexture::Filtering mipmapFiltering MEMBER m_mipmapFiltering NOTIFY mipmapFilteringChanged FINAL)

    QML_ELEMENT
public:
    TextureProviderItem() = default;
    virtual ~TextureProviderItem();

    // These enumerations must be in sync with `QSGTexture`:
    // It appears that MOC is not clever enough to consider foreign enumerations with `Q_ENUM` (I tried)...
    enum _WrapMode {
        Repeat,
        ClampToEdge,
        MirroredRepeat
    };
    Q_ENUM(_WrapMode);

    enum _Filtering {
        None,
        Nearest,
        Linear
    };
    Q_ENUM(_Filtering);

    enum _AnisotropyLevel {
        AnisotropyNone,
        Anisotropy2x,
        Anisotropy4x,
        Anisotropy8x,
        Anisotropy16x
    };
    Q_ENUM(_AnisotropyLevel);

    bool isTextureProvider() const override;

    QSGTextureProvider *textureProvider() const override;

    void resetTextureSubRect();

public slots:
    void invalidateSceneGraph();

signals:
    void sourceChanged(const QQuickItem *source);
    void rectChanged(const QRect& rect);

    void anisotropyLevelChanged(QSGTexture::AnisotropyLevel);
    void filteringChanged(QSGTexture::Filtering);
    void mipmapFilteringChanged(QSGTexture::Filtering);
    void horizontalWrapModeChanged(QSGTexture::WrapMode);
    void verticalWrapModeChanged(QSGTexture::WrapMode);

    void detachAtlasTexturesChanged(bool);

protected:
    void releaseResources() override;

private:
    QPointer<const QQuickItem> m_source;
    QRect m_rect;

    mutable QPointer<QSGTextureViewProvider> m_textureProvider;

    std::atomic<QSGTexture::AnisotropyLevel> m_anisotropyLevel = QSGTexture::AnisotropyNone;
    std::atomic<QSGTexture::Filtering> m_filtering = (smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    std::atomic<QSGTexture::WrapMode> m_horizontalWrapMode = QSGTexture::ClampToEdge;
    std::atomic<QSGTexture::WrapMode> m_verticalWrapMode = QSGTexture::ClampToEdge;
    // When there are mip maps, no mip map filtering should be fine (unlike no mip maps with mip map filtering):
    // But we want to have mip map filtering by default if the texture has mip maps (if not, it won't be respected):
    std::atomic<QSGTexture::Filtering> m_mipmapFiltering = QSGTexture::Linear;
    std::atomic<bool> m_detachAtlasTextures = false;
};

#endif // TEXTUREPROVIDERITEM_HPP
