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

#include "viewblockingrectangle.hpp"

#include <QSGRectangleNode>
#include <QQuickWindow>
#include <QSGMaterial>

ViewBlockingRectangle::ViewBlockingRectangle(QQuickItem *parent)
    : QQuickItem(parent)
    , m_color(Qt::transparent)
{
    setFlag(QQuickItem::ItemHasContents);
    connect(this, &ViewBlockingRectangle::colorChanged, this, &QQuickItem::update);
}

QSGNode *ViewBlockingRectangle::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto node = static_cast<QSGRectangleNode*>(oldNode);

    const auto rect = boundingRect();

    const auto disableBlending = [&node]() {
        assert(node->material());
        // Software backend check: (Qt bug)
        if (node->material() != reinterpret_cast<QSGMaterial*>(1))
            node->material()->setFlag(QSGMaterial::Blending, false);
    };

    if (!node)
    {
        assert(window());
        node = window()->createRectangleNode();
        assert(node);

        disableBlending();
    }

    // Geometry:
    if (node->rect() != rect)
        node->setRect(rect);

    // Material:
    if (node->color() != m_color)
    {
        node->setColor(m_color);
        disableBlending();
    }

    return node;
}
