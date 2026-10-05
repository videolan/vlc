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

#ifndef ROUNDEDCORNERMASK_HPP
#define ROUNDEDCORNERMASK_HPP

#include "backdropblenditem.hpp"

// This item replaces the area outside of a rounded corner arc with `color`,
// with anti-aliasing at the arc edge (see `BackdropBlendItem`). This allows
// rounding the corner of arbitrary content without rendering it into an
// intermediate texture, as long as this item is rendered after (above) the
// content.
//
// The item is expected to be square, its size being the radius of the arc. The
// arc center is placed at the opposite of the `corner` of the item.
class RoundedCornerMask : public BackdropBlendItem
{
    Q_OBJECT

    QML_ELEMENT

    Q_PROPERTY(Qt::Corner corner READ corner WRITE setCorner NOTIFY cornerChanged FINAL)

public:
    using BackdropBlendItem::BackdropBlendItem;

    Qt::Corner corner() const { return m_corner; }
    void setCorner(Qt::Corner corner);

protected:
    QSGMaterialType *materialType() const override;
    QString fragmentShader() const override;
    QVector<float> uniforms() const override;

private:
    Qt::Corner m_corner = Qt::TopLeftCorner;

signals:
    void cornerChanged();
};

#endif
