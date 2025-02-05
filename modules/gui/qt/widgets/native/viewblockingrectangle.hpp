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
    explicit MatrixChangeObserverNode(const std::function<void()>& callback)
        : m_callback(callback) { }

    void render(const RenderState *) override
    {
        assert(matrix());
        const QMatrix4x4 m = *matrix(); // matrix() is the combined matrix here
        if (m_lastCombinedMatrix != m)
        {
            m_callback();
            m_lastCombinedMatrix = m;
        }
    }

    RenderingFlags flags() const override
    {
        // Enable all optimizations, as we are not actually rendering anything in this node:
        return static_cast<RenderingFlags>(BoundedRectRendering | DepthAwareRendering | OpaqueRendering | NoExternalRendering);
    }
private:
    std::function<void()> m_callback;
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

private:
    QColor m_color;
    bool m_windowChanged = false;

signals:
    // NOTE: `scenePositionHasChanged()` signal is emitted from the render thread,
    //       and NOT during synchronization (meaning, GUI thread is not blocked):
    void scenePositionHasChanged();
    void colorChanged();
};

#endif
