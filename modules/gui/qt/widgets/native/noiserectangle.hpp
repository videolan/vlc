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
#ifndef NOISERECTANGLE_HPP
#define NOISERECTANGLE_HPP

#include <QQuickItem>

class NoiseRectangle : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(qreal strength READ strength WRITE setStrength NOTIFY strengthChanged FINAL)

    QML_ELEMENT
public:
    NoiseRectangle();

    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

    qreal strength() const;
    void setStrength(qreal newStrength);

signals:
    void strengthChanged();
private:
    bool m_dirty = false;
    qreal m_strength = 0.2;
protected:
    void componentComplete() override;
};

#endif // NOISERECTANGLE_HPP
