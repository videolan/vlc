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
#include "qmleventfilter.hpp"
#include <QEvent>

QmlEventFilter::QmlEventFilter(QQuickItem *parent)
    : QQuickItem(parent)
{
}

QmlEventFilter::~QmlEventFilter()
{
    if (m_source != nullptr)
        m_source->removeEventFilter(this);
}

void QmlEventFilter::setSource(QObject* source)
{
    source->installEventFilter(this);
    m_source = source;
}

void QmlEventFilter::keyPressEvent(QKeyEvent* event)
{
    // This is actually called when the QML event handler hasn't accepted the event
    m_qmlAccepted = false;
    // Ensure the event won't be propagated further
    event->setAccepted(true);
}

void QmlEventFilter::keyReleaseEvent(QKeyEvent* event)
{
    m_qmlAccepted = false;
    event->setAccepted(true);
}

bool QmlEventFilter::eventFilter(QObject*, QEvent* event)
{
    if (!m_filterEnabled)
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
