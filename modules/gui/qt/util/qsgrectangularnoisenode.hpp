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
#ifndef QSGRECTANGULARNOISENODE_HPP
#define QSGRECTANGULARNOISENODE_HPP

#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QSGMaterial>

class QSGRectangularNoiseMaterial : public QSGMaterial
{
public:
    QSGRectangularNoiseMaterial();

    float strength() const { return m_strength; };
    void setStrength(qreal strength) { m_strength = strength; };

    QSGMaterialType *type() const override;

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode renderMode) const override;
    int compare(const QSGMaterial *other) const override;

private:
    float m_strength = 0.2;
};

class QSGRectangularNoiseNode : public QSGGeometryNode
{
public:
    QSGRectangularNoiseNode();

    void setRect(const QRectF &rect);

    QRectF rect() const;

    float strength() const;
    void setStrength(float strength);

private:
    QSGGeometry m_geometry;
    QSGRectangularNoiseMaterial m_material;
};

#endif // QSGRECTANGULARNOISENODE_HPP
