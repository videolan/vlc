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

#ifndef QSGROUNDEDRECTANGULARIMAGENODE_HPP
#define QSGROUNDEDRECTANGULARIMAGENODE_HPP

#include <QSGGeometryNode>

#include <memory>
#include <cmath>

class QSGTextureMaterial;
class QSGOpaqueTextureMaterial;
class QSGTexture;

class QSGRoundedRectangularImageNode : public QSGGeometryNode
{
    template<class T>
    static T material_cast(QSGMaterial* const material);

public:
    struct Shape
    {
        QRectF rect;
        qreal radius = 0.0;

        constexpr bool operator ==(const Shape& b) const
        {
            return (rect == b.rect && qFuzzyCompare(radius, b.radius));
        }

        constexpr bool isValid() const
        {
            return (!rect.isEmpty() && std::isgreaterequal(radius, 0.0));
        }
    };

    QSGRoundedRectangularImageNode();

    // For convenience:
    QSGTextureMaterial* material() const;
    QSGOpaqueTextureMaterial* opaqueMaterial() const;

    void setSmooth(const bool smooth);
    void setTexture(const std::shared_ptr<QSGTexture>& texture);

    inline constexpr Shape shape() const
    {
        return m_shape;
    }

    bool setShape(const Shape& shape);

    inline bool rebuildGeometry()
    {
        return rebuildGeometry(m_shape);
    }

    bool rebuildGeometry(const Shape& shape);

    // Constructs a geometry denoting rounded rectangle using QPainterPath
    static QSGGeometry* rebuildGeometry(const Shape& shape,
                                        QSGGeometry* geometry,
                                        const QSGTexture* const atlasTexture = nullptr);

private:
    std::shared_ptr<QSGTexture> m_texture;
    Shape m_shape;
    bool m_smooth = true;
};

#endif // QSGROUNDEDRECTANGULARIMAGENODE_HPP
