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

#include "roundedcornermask.hpp"

#include <QSGMaterial>

void RoundedCornerMask::setCorner(Qt::Corner corner)
{
    if (m_corner == corner)
        return;
    m_corner = corner;
    emit cornerChanged();
    update();
}

QSGMaterialType *RoundedCornerMask::materialType() const
{
    static QSGMaterialType type;
    return &type;
}

QString RoundedCornerMask::fragmentShader() const
{
    return QStringLiteral(":/shaders/Arc.frag.qsb");
}

QVector<float> RoundedCornerMask::uniforms() const
{
    const bool right = (m_corner == Qt::TopRightCorner || m_corner == Qt::BottomRightCorner);
    const bool bottom = (m_corner == Qt::BottomLeftCorner || m_corner == Qt::BottomRightCorner);

    return { right ? 1.f : 0.f,
             bottom ? 1.f : 0.f };
}
