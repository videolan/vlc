/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
#ifndef MOUSEEVENTFILTER_HPP
#define MOUSEEVENTFILTER_HPP

#include <QObject>
#include <QPointF>
#include <QPointer>

class MouseEventFilter : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QObject* target READ target WRITE setTarget NOTIFY targetChanged FINAL)
    Q_PROPERTY(bool filterEventsSynthesizedByQt MEMBER m_filterEventsSynthesizedByQt NOTIFY filterEventsSynthesizedByQtChanged FINAL)

public:
    explicit MouseEventFilter(QObject *parent = nullptr);
    ~MouseEventFilter();

    QObject *target() const;
    void setTarget(QObject *newTarget);

signals:
    void targetChanged();
    void filterEventsSynthesizedByQtChanged();

    void mouseButtonDblClick(QPointF localPos, QPointF globalPos, int buttons, int modifiers, int source, int flags);
    void mouseButtonPress(QPointF localPos, QPointF globalPos, int buttons, int modifiers, int source, int flags);
    void mouseButtonRelease(QPointF localPos, QPointF globalPos, int button, int modifiers, int source, int flags);
    void mouseMove(QPointF localPos, QPointF globalPos, int buttons, int modifiers, int source, int flags);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    void attach();
    void detach();

private:
    QPointer<QObject> m_target;
    Qt::MouseButtons m_targetItemInitialAcceptedMouseButtons = Qt::NoButton;
    bool m_filterEventsSynthesizedByQt = false;
};

#endif // MOUSEEVENTFILTER_HPP
