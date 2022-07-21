/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#include "qsgroundedrectangularimagenode.hpp"

#include <QSGTextureMaterial>
#include <QSGOpaqueTextureMaterial>

#include <QCache>
#include <QVector>
#include <QPainterPath>

template<class T>
T QSGRoundedRectangularImageNode::material_cast(QSGMaterial* const material)
{
#ifdef NDEBUG
    return static_cast<T>(object);
#else
    const auto ret = dynamic_cast<T>(material);
    assert(ret); // incompatible material type
    return ret;
#endif
}

QSGRoundedRectangularImageNode::QSGRoundedRectangularImageNode()
{
    setFlags(QSGGeometryNode::OwnsMaterial |
             QSGGeometryNode::OwnsOpaqueMaterial |
             QSGGeometryNode::OwnsGeometry);

    setMaterial(new QSGTextureMaterial);
    setOpaqueMaterial(new QSGOpaqueTextureMaterial);

    setSmooth(m_smooth);

     // Useful for debugging:
#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QStringLiteral("RoundedRectangularImage"));
#endif
}

QSGTextureMaterial *QSGRoundedRectangularImageNode::material() const
{
    return material_cast<QSGTextureMaterial*>(QSGGeometryNode::material());
}

QSGOpaqueTextureMaterial *QSGRoundedRectangularImageNode::opaqueMaterial() const
{
    return material_cast<QSGOpaqueTextureMaterial*>(QSGGeometryNode::opaqueMaterial());
}

void QSGRoundedRectangularImageNode::setSmooth(const bool smooth)
{
    if (m_smooth == smooth)
        return;

    {
        const enum QSGTexture::Filtering filtering = smooth ? QSGTexture::Linear : QSGTexture::Nearest;
        material()->setFiltering(filtering);
        opaqueMaterial()->setFiltering(filtering);
    }

    {
        const enum QSGTexture::Filtering mipmapFiltering = smooth ? QSGTexture::Linear : QSGTexture::None;
        material()->setMipmapFiltering(mipmapFiltering);
        opaqueMaterial()->setMipmapFiltering(mipmapFiltering);
    }

    markDirty(QSGNode::DirtyMaterial);
}

void QSGRoundedRectangularImageNode::setTexture(const std::shared_ptr<QSGTexture>& texture)
{
    assert(texture);

    {
        const bool wasAtlas = (!m_texture || m_texture->isAtlasTexture());

        m_texture = texture;

        // Unless we operate on atlas textures, it should be
        // fine to not rebuild the geometry
        if (wasAtlas || texture->isAtlasTexture())
            rebuildGeometry(); // Texture coordinate mismatch
    }

    material()->setTexture(texture.get());
    opaqueMaterial()->setTexture(texture.get());

    markDirty(QSGNode::DirtyMaterial);
}

bool QSGRoundedRectangularImageNode::setShape(const Shape& shape)
{
    if (m_shape == shape)
        return false;

    const bool ret = rebuildGeometry(shape);

    if (ret)
        m_shape = shape;

    return ret;
}

bool QSGRoundedRectangularImageNode::rebuildGeometry(const Shape& shape)
{
    QSGGeometry* const geometry = this->geometry();
    QSGGeometry* const rebuiltGeometry = rebuildGeometry(shape,
                                                         geometry,
                                                         m_texture->isAtlasTexture() ? m_texture.get()
                                                                                     : nullptr);
    if (!rebuiltGeometry)
    {
        return false;
    }
    else if (rebuiltGeometry == geometry)
    {
        // Was able to reconstruct old geometry instance
        markDirty(QSGNode::DirtyGeometry);
    }
    else
    {
        // - Dirty bit set implicitly
        // - No need to remove the old geometry
        setGeometry(rebuiltGeometry);
    }

    return true;
}

QSGGeometry* QSGRoundedRectangularImageNode::rebuildGeometry(const Shape& shape,
                                                             QSGGeometry* geometry,
                                                             const QSGTexture* const atlasTexture)
{
    if (!shape.isValid())
        return nullptr;

    int vertexCount;

    QVector<QPointF> *path;
    std::unique_ptr<QVector<QPointF>> upPath;

    if (qFuzzyIsNull(shape.radius))
    {
        // 4 vertices are needed to construct
        // a rectangle using triangle strip.
        vertexCount = 4;
        path = nullptr; // unused
    }
    else
    {
        using SizePair = QPair<qreal, qreal>;
        using ShapePair = QPair<SizePair, qreal>;

        // We could cache QSGGeometry itself, but
        // it would not be really useful for atlas
        // textures.
        static QCache<ShapePair, QVector<QPointF>> paths;

        ShapePair key {{shape.rect.width(), shape.rect.height()}, {shape.radius}};
        if (paths.contains(key))
        {
            // There is no cache manipulation after this point,
            // so path is assumed to be valid within its scope
            path = paths.object(key);
        }
        else
        {
            QPainterPath painterPath;
            painterPath.addRoundedRect(0, 0, key.first.first, key.first.second, key.second, key.second);
            painterPath = painterPath.simplified();

            path = new QVector<QPointF>(painterPath.elementCount());

            const int elementCount = painterPath.elementCount();
            for (int i = 0; i < elementCount; ++i)
            {
                // QPainterPath is not necessarily compatible with
                // with GPU primitives. However, simplified painter path
                // with rounded rectangle shape consists of painter path
                // elements of types which can be drawn using primitives.
                assert(painterPath.elementAt(i).type == QPainterPath::ElementType::MoveToElement
                       || painterPath.elementAt(i).type == QPainterPath::ElementType::LineToElement);

                // Symmetry based triangulation based on ordered painter path.
                (*path)[i] = (painterPath.elementAt((i % 2) ? (i)
                                                            : (elementCount - i - 1)));
            }

            if (!paths.insert(key, path))
                upPath.reset(path); // Own the path so there is no leak
        }

        vertexCount = path->count();
    }

    if (!geometry)
    {
        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), vertexCount);
        geometry->setIndexDataPattern(QSGGeometry::StaticPattern); // Is this necessary? Indexing is not used
        geometry->setVertexDataPattern(QSGGeometry::StaticPattern);
        geometry->setDrawingMode(QSGGeometry::DrawingMode::DrawTriangleStrip);
    }
    else
    {
        // Size check is implicitly done:
        geometry->allocate(vertexCount);

        // Assume the passed geometry is not a stray one.
        // It is possible to check and create a new QSGGeometry
        // if it is incompatible, but it should not be necessary.
        // So instead, just pass QSGGeometry to this function that
        // is either inherently compatible, or that is created by
        // this function.

        // These two are not required for compatibility,
        // but lets still assert them for performance reasons.
        assert(geometry->indexDataPattern() == QSGGeometry::StaticPattern);
        assert(geometry->vertexDataPattern() == QSGGeometry::StaticPattern);

        assert(geometry->drawingMode() == QSGGeometry::DrawingMode::DrawTriangleStrip);
        assert(geometry->attributes() == QSGGeometry::defaultAttributes_TexturedPoint2D().attributes);
        assert(geometry->sizeOfVertex() == QSGGeometry::defaultAttributes_TexturedPoint2D().stride);
    }

    QRectF texNormalSubRect;
    if (atlasTexture)
    {
        // The texture might not be in the atlas, but it is okay.
        texNormalSubRect = atlasTexture->normalizedTextureSubRect();
    }
    else
    {
        // In case no texture is given at all:
        texNormalSubRect = {0.0, 0.0, 1.0, 1.0};
    }

    if (qFuzzyIsNull(shape.radius))
    {
        // Use the helper function to reconstruct the pure rectangular geometry:
        QSGGeometry::updateTexturedRectGeometry(geometry, shape.rect, texNormalSubRect);
    }
    else
    {
        const auto mapToAtlasTexture = [texNormalSubRect] (const QPointF& nPoint) -> QPointF {
            return {texNormalSubRect.x() + (texNormalSubRect.width() * nPoint.x()),
                texNormalSubRect.y() + (texNormalSubRect.height() * nPoint.y())};
        };

        QSGGeometry::TexturedPoint2D* const points = geometry->vertexDataAsTexturedPoint2D();

        const qreal dx = shape.rect.x();
        const qreal dy = shape.rect.y();
        for (int i = 0; i < geometry->vertexCount(); ++i)
        {
            const QPointF& pos = path->at(i);
            QPointF tPos = {pos.x() / shape.rect.width(), pos.y() / shape.rect.height()};
            if (atlasTexture)
                tPos = mapToAtlasTexture(tPos);

            points[i].set(pos.x() + dx, pos.y() + dy, tPos.x(), tPos.y());
        }
    }

    geometry->markIndexDataDirty();
    geometry->markVertexDataDirty();

    return geometry;
}
