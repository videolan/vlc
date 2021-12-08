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
#include "keyhelper.hpp"

#include <QQmlEngine>

QmlKeyHelper::QmlKeyHelper(QObject* parent)
    : QObject(parent)
{}

bool QmlKeyHelper::call(bool (*fun)(const QKeyEvent *), const QObject *event)
{
    QKeyEvent fakeEvent(QKeyEvent::KeyPress,
                        event->property("key").toInt(),
                        static_cast<Qt::KeyboardModifiers>(event->property("modifiers").toInt()),
                        event->property("text").toString(),
                        event->property("isAutoRepeat").toBool(),
                        event->property("count").toInt()
                        );

    return fun(&fakeEvent);
}
