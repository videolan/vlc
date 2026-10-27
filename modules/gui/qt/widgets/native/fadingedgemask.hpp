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

#ifndef FADINGEDGEMASK_HPP
#define FADINGEDGEMASK_HPP

#include "backdropblenditem.hpp"

// This item fades what is rendered beneath it into `color`, from the `edge`
// where `color` fully replaces the content, to the opposite edge where the
// content is left untouched (see `BackdropBlendItem`).
//
// When `color` is opaque, this is equivalent to a gradient rectangle from
// `color` to transparent. When it is not opaque, the content is faded into
// the color (as opposed to being blended over by it), this means that the
// result is correct if what is beneath the content is `color` itself, such as
// when the content is on top of a uniform translucent background.
class FadingEdgeMask : public BackdropBlendItem
{
    Q_OBJECT

    QML_ELEMENT

    Q_PROPERTY(Qt::Edge edge READ edge WRITE setEdge NOTIFY edgeChanged FINAL)

public:
    using BackdropBlendItem::BackdropBlendItem;

    Qt::Edge edge() const { return m_edge; }
    void setEdge(Qt::Edge edge);

protected:
    QSGMaterialType *materialType() const override;
    QString fragmentShader() const override;
    QVector<float> uniforms() const override;

private:
    Qt::Edge m_edge = Qt::TopEdge;

signals:
    void edgeChanged();
};

#endif
