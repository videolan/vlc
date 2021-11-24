/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
#include "item_key_event_filter.hpp"
#include <QEvent>

ItemKeyEventFilter::ItemKeyEventFilter(QQuickItem *parent)
    : QQuickItem(parent)
{
}

ItemKeyEventFilter::~ItemKeyEventFilter()
{
    if (m_target)
        m_target->removeEventFilter(this);
}

void ItemKeyEventFilter::setTarget(QObject* target)
{
    assert(target);

    target->installEventFilter(this);
    m_target = target;
}

void ItemKeyEventFilter::keyPressEvent(QKeyEvent* event)
{
    // This is actually called when the QML event handler hasn't accepted the event
    m_qmlAccepted = false;
    // Ensure the event won't be propagated further
    event->setAccepted(true);
}

void ItemKeyEventFilter::keyReleaseEvent(QKeyEvent* event)
{
    m_qmlAccepted = false;
    event->setAccepted(true);
}

bool ItemKeyEventFilter::eventFilter(QObject*, QEvent* event)
{
    if (!m_enabled)
        return false;

    bool ret = false;
    switch (event->type())
    {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        m_qmlAccepted = true;
        QCoreApplication::sendEvent(this, event);
        ret = m_qmlAccepted;
        break;
    default:
        break;
    }
    return ret;
}
