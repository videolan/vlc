/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#include "hover_handler_rev11.hpp"

HoverHandlerRev11::HoverHandlerRev11(QObject *parent)
    : QObject(parent)
{
}

HoverHandlerRev11::~HoverHandlerRev11()
{
    setTarget(nullptr);
}

void HoverHandlerRev11::setTarget(QQuickItem* target)
{
    if (m_target)
        m_target->removeEventFilter(this);

    m_target = target;

    if (m_target)
    {
        m_target->setAcceptHoverEvents(true);
        m_target->installEventFilter(this);
    }
}

bool HoverHandlerRev11::eventFilter(QObject*, QEvent* event)
{
    bool changed = true;

    switch (event->type())
    {
    case QEvent::HoverEnter:
        m_hovered = true;
        break;
    case QEvent::HoverLeave:
        m_hovered = false;
        break;
    default:
        changed = false;
        break;
    }

    if (changed)
        emit hoveredChanged();

    return changed;
}

void HoverHandlerRev11::classBegin()
{
}

void HoverHandlerRev11::componentComplete()
{
    if (!m_target)
    {
        auto parentItem = qobject_cast<QQuickItem*>(QObject::parent());
        if (parentItem)
            setTarget(parentItem);
    }
}
