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

#include <QQuickWindow>
#include <QPainter>
#include <QPointer>
#include <QSGRectangleNode>
#include <QSGMaterial>
#include <QSGRenderNode>

class SoftwareRenderNode : public QSGRenderNode
{
public:
    void render(const RenderState *renderState) override
    {
        assert(m_window);
        const auto painter = static_cast<QPainter *>(m_window->rendererInterface()->getResource(m_window, QSGRendererInterface::PainterResource));
        assert(painter);

        const auto clipRegion = renderState->clipRegion();
        if (clipRegion && !clipRegion->isEmpty())
            painter->setClipRegion(*clipRegion, Qt::ReplaceClip);

        painter->setTransform(matrix()->toTransform());
        painter->setOpacity(inheritedOpacity());

        painter->fillRect(rect().toRect(), m_color);
    }

    RenderingFlags flags() const override
    {
        return BoundedRectRendering | DepthAwareRendering | OpaqueRendering;
    }

    QRectF rect() const override
    {
        return m_rect;
    }

    void setRect(const QRectF& rect)
    {
        if (m_rect != rect)
        {
            m_rect = rect;
            markDirty(DirtyGeometry);
        }
    }

    void setWindow(QQuickWindow* const window)
    {
        if (Q_LIKELY(m_window != window))
        {
            m_window = window;
            markDirty(DirtyForceUpdate);
        }
    }

    void setColor(const QColor& color)
    {
        if (m_color != color)
        {
            m_color = color;
            markDirty(DirtyMaterial);
        }
    }

private:
    QPointer<QQuickWindow> m_window = nullptr;
    QRectF m_rect;
    QColor m_color {Qt::transparent};
};

ViewBlockingRectangle::ViewBlockingRectangle(QQuickItem *parent)
    : QQuickItem(parent)
    , m_color(Qt::transparent)
{
    setFlag(QQuickItem::ItemHasContents);
    connect(this, &ViewBlockingRectangle::colorChanged, this, &QQuickItem::update);
    connect(this, &ViewBlockingRectangle::windowChanged, this, [this] { m_windowChanged = true; });
}

QSGNode *ViewBlockingRectangle::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto rectangleNode = dynamic_cast<QSGRectangleNode*>(oldNode);
    auto softwareRenderNode = dynamic_cast<SoftwareRenderNode*>(oldNode);

    assert(window());
    const bool softwareMode = (window()->rendererInterface()->graphicsApi() == QSGRendererInterface::GraphicsApi::Software);

    if (Q_UNLIKELY(oldNode && ((softwareMode && !softwareRenderNode)
                               || (!softwareMode && !rectangleNode))))
    {
        delete oldNode;
        oldNode = nullptr;
    }

    if (!oldNode)
    {
        if (softwareMode)
        {
            softwareRenderNode = new SoftwareRenderNode;
            softwareRenderNode->setWindow(window());
        }
        else
        {
            rectangleNode = window()->createRectangleNode();
            assert(rectangleNode);
            assert(rectangleNode->material());
            rectangleNode->material()->setFlag(QSGMaterial::Blending, false);
        }
    }

    const auto rect = boundingRect();

    if (softwareMode)
    {
        softwareRenderNode->setRect(rect);
        softwareRenderNode->setColor(m_color);

        if (Q_UNLIKELY(m_windowChanged))
        {
            softwareRenderNode->setWindow(window());
            m_windowChanged = false;
        }

        return softwareRenderNode;
    }
    else
    {
        if (rectangleNode->rect() != rect)
            rectangleNode->setRect(rect);

        if (rectangleNode->color() != m_color)
        {
            rectangleNode->setColor(m_color);
            assert(rectangleNode->material());
            rectangleNode->material()->setFlag(QSGMaterial::Blending, false);
        }

        return rectangleNode;
    }
}
