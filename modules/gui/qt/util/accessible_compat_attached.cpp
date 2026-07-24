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

#include "accessible_compat_attached.hpp"

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0) && defined(QT_DECLARATIVE_PRIVATE)
#include <private/qquickaccessibleattached_p.h>
#define ACCESSIBLE_COMPAT_OK
#endif

AccessibleCompatAttached::AccessibleCompatAttached(QObject* parent)
    : QObject(parent)
{
}

void AccessibleCompatAttached::setId(const QString& id) {
#ifdef ACCESSIBLE_COMPAT_OK
    QQuickAccessibleAttached* accessible = qobject_cast<QQuickAccessibleAttached*>(
            qmlAttachedPropertiesObject<QQuickAccessibleAttached>(parent()));
    accessible->setId(id);
    emit IdChanged();
#endif
}

QString AccessibleCompatAttached::getId() {
#ifdef ACCESSIBLE_COMPAT_OK
    QQuickAccessibleAttached* accessible = qobject_cast<QQuickAccessibleAttached*>(
        qmlAttachedPropertiesObject<QQuickAccessibleAttached>(parent()));
    return accessible->id();
#endif
    return {};
}

AccessibleCompatAttached* AccessibleCompatAttached::qmlAttachedProperties(QObject* parent)
{
    return new AccessibleCompatAttached(parent);
}

void AccessibleCompatAttached::setId(QWidget* wd, const QString& id)
{
    #if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        wd->setAccessibleIdentifier(id);
    #endif
}