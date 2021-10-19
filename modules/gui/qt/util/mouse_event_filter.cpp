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
#include "mouse_event_filter.hpp"

#include <QMouseEvent>
#include <QQuickItem>

MouseEventFilter::MouseEventFilter(QObject *parent)
    : QObject(parent)
{

}

MouseEventFilter::~MouseEventFilter()
{
    detach();
}

QObject *MouseEventFilter::target() const
{
    return m_target;
}

void MouseEventFilter::setTarget(QObject *newTarget)
{
    if (m_target == newTarget)
        return;

    detach();
    m_target = newTarget;
    attach();

    emit targetChanged();
}

bool MouseEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    assert(watched == m_target);

    const auto mouse = dynamic_cast<QMouseEvent*>(event);
    if (!mouse)
        return false;

    if (!m_filterEventsSynthesizedByQt &&
        mouse->source() == Qt::MouseEventSource::MouseEventSynthesizedByQt)
        return false;

    switch (event->type())
    {
    case QEvent::MouseButtonDblClick:
        emit mouseButtonDblClick(mouse->localPos(),
                                 mouse->globalPos(),
                                 mouse->buttons(),
                                 mouse->modifiers(),
                                 mouse->source(),
                                 mouse->flags()); break;
    case QEvent::MouseButtonPress:
        emit mouseButtonPress(mouse->localPos(),
                              mouse->globalPos(),
                              mouse->buttons(),
                              mouse->modifiers(),
                              mouse->source(),
                              mouse->flags()); break;
    case QEvent::MouseButtonRelease:
        emit mouseButtonRelease(mouse->localPos(),
                                mouse->globalPos(),
                                mouse->button(),
                                mouse->modifiers(),
                                mouse->source(),
                                mouse->flags()); break;
    case QEvent::MouseMove:
        emit mouseMove(mouse->localPos(),
                       mouse->globalPos(),
                       mouse->buttons(),
                       mouse->modifiers(),
                       mouse->source(),
                       mouse->flags()); break;

    default:
        return false;
    }

    return true;
}

void MouseEventFilter::attach()
{
    if (m_target)
    {
        m_target->installEventFilter(this);
        const auto item = qobject_cast<QQuickItem*>(m_target);
        if (item)
        {
            m_targetItemInitialAcceptedMouseButtons = item->acceptedMouseButtons();
            item->setAcceptedMouseButtons(Qt::AllButtons);
        }
    }
}

void MouseEventFilter::detach()
{
    if (m_target)
    {
        m_target->removeEventFilter(this);
        const auto item = qobject_cast<QQuickItem*>(m_target);
        if (item)
            item->setAcceptedMouseButtons(m_targetItemInitialAcceptedMouseButtons);
    }
}
