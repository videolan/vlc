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

class ViewBlockingRectangle : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QColor color MEMBER m_color NOTIFY colorChanged FINAL)

public:
    ViewBlockingRectangle(QQuickItem *parent = nullptr);

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

private:
    QColor m_color;

signals:
    void colorChanged();
};

#endif
