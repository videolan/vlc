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
#ifndef SGMANIPULATOR_H
#define SGMANIPULATOR_H

#include <QQuickItem>
#include <QJSValue>

#include <optional>

// This class allows manipulating the parent item's scene graph node.
// Visibility can be set to to false to disable manipulation.
// It is the user's responsibility to call `QQuickItem::update()`
// to update the settings. Currently an update is scheduled as soon
// as a manipulating property is changed. This means that if the
// target node is altered without the control of this class, an
// update should be scheduled (unless `constantUpdate` is set),
// this can be for example `Image` switching from a opaque image
// to a translucent one, or `Rectangle` switching from a opaque
// color to a transparent one. The same applies when the target
// node itself changes (such as due to re-parenting).
class SGManipulator : public QQuickItem
{
    Q_OBJECT

    // This is not really expensive, but still avoid constant updates if possible:
    Q_PROPERTY(bool constantUpdate MEMBER m_constantUpdate NOTIFY constantUpdateChanged FINAL)

    Q_PROPERTY(QJSValue blending READ blending WRITE setBlending RESET resetBlending NOTIFY blendingChanged FINAL)

    QML_ELEMENT

public:
    explicit SGManipulator(QQuickItem* parent = nullptr);

    QJSValue blending() const;
    void setBlending(const QJSValue& blending);
    void resetBlending();

signals:
    void blendingChanged(const QJSValue&);
    void constantUpdateChanged(bool);

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

private:
    std::optional<bool> m_blending;
    bool m_constantUpdate = false;
};

#endif // SGMANIPULATOR_H
