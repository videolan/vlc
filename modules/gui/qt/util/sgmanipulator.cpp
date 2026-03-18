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
#include "sgmanipulator.hpp"

#include <QSGRenderNode>
#include <QSGGeometryNode>
#include <QSGTransformNode>
#include <QSGMaterial>


SGManipulator::SGManipulator(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents);

    connect(this, &SGManipulator::constantUpdateChanged, this, [this](bool constantUpdate) {
        if (constantUpdate)
            update();
    });
}

QJSValue SGManipulator::blending() const
{
    if (m_blending)
        return *m_blending;
    else
        return {};
}

void SGManipulator::setBlending(const QJSValue& blending)
{
    if ((!m_blending && blending.isNull()) || (m_blending == blending.toBool()))
        return;

    m_blending = blending.toBool();

    emit blendingChanged(blending);

    update();
}

void SGManipulator::resetBlending()
{
    m_blending.reset();
}

QSGNode *SGManipulator::updatePaintNode(QSGNode *, UpdatePaintNodeData *data)
{
    assert(data);

    const auto transformNode = data->transformNode;
    assert(transformNode);

    QSGNode *targetNode = nullptr;
    QSGGeometryNode *targetGeometryNode = nullptr;
    QSGRenderNode *targetRenderNode = nullptr;

    QSGNode *i = transformNode;
    while (i)
    {
        // NOTE: The target node must not be a transform node.
        // NOTE: The transform nodes parent sibling item's rendering nodes.
        if (dynamic_cast<QSGTransformNode*>(i))
        {
            i = i->previousSibling();
            continue;
        }

        targetNode = i;
        targetGeometryNode = dynamic_cast<QSGGeometryNode*>(i);
        targetRenderNode = dynamic_cast<QSGRenderNode*>(i);

        break;
    }

    if (targetNode)
    {
        if (targetGeometryNode)
        {
            bool materialChangeMade = false;

            if (const auto material = targetGeometryNode->material())
            {
                if (m_blending)
                {
                    if (*m_blending != material->flags().testFlag(QSGMaterial::Blending))
                    {
                        qDebug() << "SGManipulator: Blending is overridden as" << *m_blending << "for target material" << material;
                        material->setFlag(QSGMaterial::Blending, *m_blending);
                        materialChangeMade = true;
                    }
                }

                if (materialChangeMade)
                    targetGeometryNode->markDirty(QSGNode::DirtyMaterial);
            }
        }
    }

    if (m_constantUpdate)
        update(); // re-schedule an update

    return nullptr;
}
