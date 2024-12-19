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
};

class TextureProviderItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(const QQuickItem* source MEMBER m_source NOTIFY sourceChanged FINAL)
    Q_PROPERTY(QRect textureSubRect MEMBER m_rect NOTIFY rectChanged FINAL)

    QML_ELEMENT
public:
    TextureProviderItem() = default;
    virtual ~TextureProviderItem();

    bool isTextureProvider() const override;

    QSGTextureProvider *textureProvider() const override;

public slots:
    void invalidateSceneGraph();

signals:
    void sourceChanged(const QQuickItem *source);
    void rectChanged(const QRect& rect);
    void dprChanged();

protected:
    void releaseResources() override;
    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value) override;

private:
    QPointer<const QQuickItem> m_source;
    QRect m_rect;

    mutable QPointer<QSGTextureViewProvider> m_textureProvider;
    mutable QMutex m_textureProviderMutex; // I'm not sure if this mutex is necessary
};

#endif // TEXTUREPROVIDERITEM_HPP
