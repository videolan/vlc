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

        painter->setCompositionMode(QPainter::CompositionMode_Source);

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
    if (m_renderingEnabled || m_updateRenderPosition)
        setFlag(QQuickItem::ItemHasContents);
    connect(this, &ViewBlockingRectangle::colorChanged, this, &QQuickItem::update);
    connect(this, &ViewBlockingRectangle::windowChanged, this, [this] {
        if (window())
        {
            // The new window might use an appropriate graphics API:
            setFlag(QQuickItem::ItemHasContents);
            m_windowChanged = true;
        }
    });
}

QSGNode *ViewBlockingRectangle::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto rectangleNode = dynamic_cast<QSGRectangleNode*>(oldNode);
    auto softwareRenderNode = dynamic_cast<SoftwareRenderNode*>(oldNode);

    assert(window());
    const bool softwareMode = (window()->rendererInterface()->graphicsApi() == QSGRendererInterface::GraphicsApi::Software);

    if (m_renderingEnabled)
    {
        if (Q_UNLIKELY(oldNode && ((softwareMode && !softwareRenderNode)
                                   || (!softwareMode && !rectangleNode))))
        {
            delete oldNode;
            oldNode = nullptr;
        }
    }
    else
    {
        if (rectangleNode || softwareRenderNode || (!m_updateRenderPosition && oldNode))
        {
            // If `m_updateRenderPosition` is true, new node will be the observer node,
            // otherwise, we early return as a scene graph node is not necessary.
            // Currently we are not reparenting the observer node that could be reused
            // otherwise.
            delete oldNode;
            oldNode = nullptr;
        }

        if (!m_updateRenderPosition)
            return nullptr;
    }

    const auto createObserverNode = [this]() {
        const auto node = new MatrixChangeObserverNode([p = QPointer(this)](const QMatrix4x4& matrix) {
            if (Q_LIKELY(p))
            {
                p->m_renderPosition = {matrix.row(0)[3], // Viewport/scene X
                                       matrix.row(1)[3]}; // Viewport/scene y
                emit p->scenePositionHasChanged();
            }
        });
        node->setFlag(QSGNode::OwnedByParent);
        return node;
    };

    if (!oldNode)
    {
        MatrixChangeObserverNode *observerNode;

        if (m_updateRenderPosition)
        {
            observerNode = createObserverNode();
        }
        else
        {
            observerNode = nullptr;
        }

        // Initial position:
        m_renderPosition = mapToScene(QPointF(0,0));

        if (m_renderingEnabled)
        {
            if (softwareMode)
            {
                softwareRenderNode = new SoftwareRenderNode;
                softwareRenderNode->setWindow(window());
                if (observerNode)
                    softwareRenderNode->appendChildNode(observerNode);
            }
            else
            {
                rectangleNode = window()->createRectangleNode();
                assert(rectangleNode);
                if (observerNode)
                    rectangleNode->appendChildNode(observerNode);

                const auto material = rectangleNode->material();
                if (!material ||
                    material == reinterpret_cast<QSGMaterial*>(1) /* Qt may explicitly set the material pointer to 1 in OpenVG */)
                {
                    // Scene graph adaptation does not support shading
                    qmlDebug(this) << "ViewBlockingRectangle is being used under an incompatible scene graph adaptation.";
                    delete rectangleNode;
                    setFlag(QQuickItem::ItemHasContents, false);
                    return nullptr;
                }

                rectangleNode->material()->setFlag(QSGMaterial::Blending, false);
            }
        }
        else if (observerNode)
        {
            oldNode = observerNode;
        }
    }
    else if (m_renderingEnabled)
    {
        const auto observerNode = oldNode->childAtIndex(0);

        if (m_updateRenderPosition)
        {
            if (!observerNode)
            {
                oldNode->appendChildNode(createObserverNode());
            }
        }
        else
        {
            if (observerNode)
            {
                assert(dynamic_cast<MatrixChangeObserverNode*>(observerNode));
                observerNode->setFlag(QSGNode::OwnedByParent, false); // this may not be necessary
                delete observerNode;
            }
        }
    }

    const auto rect = boundingRect();

    m_renderSize = rect.size();

    if (m_renderingEnabled)
    {
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
    else
    {
        if (m_updateRenderPosition)
        {
            assert(oldNode);
            return oldNode; // observer node
        }
        else
        {
            setFlag(ItemHasContents, false);
            return nullptr;
        }
    }
}

QSizeF ViewBlockingRectangle::renderSize() const
{
    return m_renderSize;
}

QPointF ViewBlockingRectangle::renderPosition() const
{
    return m_renderPosition;
}

void ViewBlockingRectangle::setRenderingEnabled(bool enabled)
{
    if (m_renderingEnabled == enabled)
        return;

    m_renderingEnabled = enabled;

    if (enabled)
    {
        setFlag(ItemHasContents, true);
        if (isVisible())
            update();
    }
    else if (!m_updateRenderPosition)
    {
        setFlag(ItemHasContents, false);
    }

    emit renderingEnabledChanged();
}

void ViewBlockingRectangle::setUpdateRenderPosition(bool _update)
{
    if (m_updateRenderPosition == _update)
        return;

    m_updateRenderPosition = _update;

    if (_update)
    {
        setFlag(ItemHasContents, true);
        if (isVisible())
            update();
    }
    else if (!m_renderingEnabled)
    {
        setFlag(ItemHasContents, false);
    }
}

bool ViewBlockingRectangle::updateRenderPosition() const
{
    return m_updateRenderPosition;
}

