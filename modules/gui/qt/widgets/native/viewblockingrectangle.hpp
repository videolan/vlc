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

#ifndef VIEWBLOCKINGRECTANGLE_HPP
#define VIEWBLOCKINGRECTANGLE_HPP

#include <QQuickItem>
#include <QColor>
#include <QMatrix4x4>
#include <QSGRenderNode>

// WARNING: Do not use QSGTransformNode here with preprocess, Qt does not
//          necessarily call preprocess when the combined matrix changes!
class MatrixChangeObserverNode : public QSGRenderNode
{
public:
    explicit MatrixChangeObserverNode(const std::function<void(const QMatrix4x4&)>& callback)
        : m_callback(callback) { }

    // Use prepare(), because matrix() returns dangling pointer when render() is called from Qt 6.2 to 6.5 (QTBUG-97589):
    void prepare() override
    {
        QSGRenderNode::prepare(); // The documentation says that the default implementation is empty, but still call it just in case.

        assert(matrix());
        const QMatrix4x4 m = *matrix(); // matrix() is the combined matrix here
        if (m_lastCombinedMatrix != m)
        {
            m_callback(m);
            m_lastCombinedMatrix = m;
        }
    }

    void render(const RenderState *) override
    {
        // We do not render anything in this node.
    }

    RenderingFlags flags() const override
    {
        // Enable all optimizations, as we are not actually rendering anything in this node:
        return static_cast<RenderingFlags>(BoundedRectRendering | DepthAwareRendering | OpaqueRendering | NoExternalRendering);
    }
private:
    std::function<void(const QMatrix4x4& newMatrix)> m_callback;
    QMatrix4x4 m_lastCombinedMatrix;
};

class ViewBlockingRectangle : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QColor color MEMBER m_color NOTIFY colorChanged FINAL)

public:
    explicit ViewBlockingRectangle(QQuickItem *parent = nullptr);

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

    // For now these are protected, but can be made public if necessary:
    // These methods should only be called from the rendering thread.
    QSizeF renderSize() const;
    QPointF renderPosition() const;

private:
    QColor m_color;
    bool m_windowChanged = false;

    // Although these members belong to the class instance's thread, they
    // are updated in the rendering thread:
    QSizeF m_renderSize;
    QPointF m_renderPosition {-1., -1.};

signals:
    // NOTE: `scenePositionHasChanged()` signal is emitted from the render thread,
    //       and NOT during synchronization (meaning, GUI thread is not blocked):
    void scenePositionHasChanged();
    void colorChanged();
};

#endif
