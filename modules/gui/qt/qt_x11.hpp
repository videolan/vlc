/*****************************************************************************
 * qt_x11.hpp : X11 helper functions
 ****************************************************************************
 * Copyright (C) 2006-2026 the VideoLAN team
 * $Id$
 *
 * Authors: Nathan E. Egge <unlord@videolan.org>
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

#ifndef QVLC_X11_H_
#define QVLC_X11_H_

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
/* QNativeInterface::QX11Application returns nullptr when platform is not X11 */
static inline bool vlcQtIsX11()
{
    return qApp->nativeInterface<QNativeInterface::QX11Application>() != nullptr;
}
#else
#include <QX11Info>
static inline bool vlcQtIsX11()
{
    return QX11Info::isPlatformX11();
}
#endif

#endif
