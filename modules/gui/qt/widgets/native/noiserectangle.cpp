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
#include "noiserectangle.hpp"

#include "util/qsgrectangularnoisenode.hpp"

NoiseRectangle::NoiseRectangle()
{
    const auto setDirty = [this]() {
        m_dirty = true;
        update();
    };

    connect(this, &QQuickItem::widthChanged, this, setDirty);
    connect(this, &QQuickItem::heightChanged, this, setDirty);
    connect(this, &NoiseRectangle::strengthChanged, this, setDirty);
}

QSGNode *NoiseRectangle::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!oldNode)
    {
        oldNode = new QSGRectangularNoiseNode;
    }

    assert(dynamic_cast<QSGRectangularNoiseNode*>(oldNode));

    const auto node = static_cast<QSGRectangularNoiseNode*>(oldNode);

    if (m_dirty)
    {
        // Synchronize the item with the node
        node->setStrength(m_strength);
        node->setRect(boundingRect());
        m_dirty = false;
    }

    return node;
}

qreal NoiseRectangle::strength() const
{
    return m_strength;
}

void NoiseRectangle::setStrength(qreal newStrength)
{
    if (qFuzzyCompare(m_strength, newStrength))
        return;

    m_strength = newStrength;

    setFlag(ItemHasContents, !qFuzzyIsNull(m_strength));

    emit strengthChanged();
}

void NoiseRectangle::componentComplete()
{
    if (!qFuzzyIsNull(m_strength))
    {
        setFlag(ItemHasContents);
        update();
    }
    QQuickItem::componentComplete();
}
