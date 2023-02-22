/*****************************************************************************
 * Copyright (C) 2023 the VideoLAN team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef COMPOSITOR_ACCESSIBLITY_H
#define COMPOSITOR_ACCESSIBLITY_H

#include <QString>

class QObject;
class QAccessibleInterface;
class QQuickWindow;

namespace vlc {

class AccessibleRenderWindow
{
public:
    virtual ~AccessibleRenderWindow()  = default;
    virtual QQuickWindow* getOffscreenWindow() const = 0;
};


#if !defined(QT_NO_ACCESSIBILITY) && defined(QT5_DECLARATIVE_PRIVATE)

QAccessibleInterface* compositionAccessibleFactory(const QString &classname, QObject *object);

#endif

}

#endif /* COMPOSITOR_ACCESSIBLITY_H */
