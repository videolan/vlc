/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#include "fadingedgemask.hpp"

#include <QSGMaterial>

void FadingEdgeMask::setEdge(Qt::Edge edge)
{
    if (m_edge == edge)
        return;
    m_edge = edge;
    emit edgeChanged();
    update();
}

QSGMaterialType *FadingEdgeMask::materialType() const
{
    static QSGMaterialType type;
    return &type;
}

QString FadingEdgeMask::fragmentShader() const
{
    return QStringLiteral(":/shaders/FadingEdgeMask.frag.qsb");
}

QVector<float> FadingEdgeMask::uniforms() const
{
    const bool vertical = (m_edge == Qt::TopEdge || m_edge == Qt::BottomEdge);
    const bool flip = (m_edge == Qt::BottomEdge || m_edge == Qt::RightEdge);

    return { vertical ? 1.f : 0.f,
             flip ? 1.f : 0.f };
}
