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

#ifndef BACKDROPBLENDITEM_HPP
#define BACKDROPBLENDITEM_HPP

#include <QQuickItem>
#include <QColor>
#include <QVector>

class QSGMaterialType;

// Base class for items that replace what is rendered beneath them with `color`,
// weighted by a coverage computed by the fragment shader of the subclass:
//
//     dst = color * coverage + dst * (1 - coverage)
//
// This is done in all the channels, including the alpha channel. Where coverage
// is 1, the color is written as is (akin to `ViewBlockingRectangle`, it is not
// blended over what is beneath). Where coverage is 0, nothing is changed. This
// allows masking arbitrary content without rendering it into an intermediate
// texture, as long as this item is rendered after (above) the content.
//
// The color is provided to the graphics pipeline as blend constant, the fragment
// shader only outputs the coverage (multiplied by `qt_Opacity`) in all channels.
//
// Fragment shaders get the position in item coordinates (`coord`, at location 0)
// from `BackdropBlend.vert`, and must declare the following uniform block, where
// the members following `qt_Opacity` are the values returned by `uniforms()`:
//
//     layout(std140, binding = 0) uniform buf {
//         mat4 qt_Matrix;
//         float qt_Opacity;
//         float ...;
//     };
//
// This item requires a RHI based scene graph adaptation, nothing is rendered
// otherwise (see `supported`).
class BackdropBlendItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)

    Q_PROPERTY(bool supported READ supported NOTIFY supportedChanged FINAL)

public:
    explicit BackdropBlendItem(QQuickItem *parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor& color);

    bool supported() const { return m_supported; }

protected:
    // The material type must be unique per fragment shader:
    virtual QSGMaterialType *materialType() const = 0;
    virtual QString fragmentShader() const = 0;
    // Values of the uniform block members following `qt_Opacity`, in order.
    // Called from the rendering thread, while the GUI thread is blocked.
    // Call `update()` when they change:
    virtual QVector<float> uniforms() const { return {}; };

    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

private:
    void updateSupported();

    // We should not need to schedule update ourselves by calling
    // `::update()`, Qt should do it implicitly with width/height
    // change:
    void markDirtyGeometry() { m_dirtyGeometry = true; }

    QColor m_color {Qt::transparent};
    bool m_supported = false;

    bool m_dirtyGeometry = true;

signals:
    void colorChanged();
    void supportedChanged();
};

#endif
