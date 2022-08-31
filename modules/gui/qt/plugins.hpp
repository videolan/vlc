/*****************************************************************************
 * plugins.hpp : Qt static plugin integration
 ****************************************************************************
 * Copyright © 2006-2009 the VideoLAN team
 * Copyright © 2022 Videolabs
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Alexandre Janniaux <ajanni@videolabs.io>
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

#if !defined(QT_STATIC) && !defined(QT_SHARED)
# error "Make sure qconfig.h was included before"
#endif

#if !defined(Q_IMPORT_PLUGIN)
# error "Make sure QtPlugin was included before"
#endif

#ifdef QT_STATIC /* For static builds */
    Q_IMPORT_PLUGIN(QSvgIconPlugin)
    Q_IMPORT_PLUGIN(QSvgPlugin)
    Q_IMPORT_PLUGIN(QJpegPlugin)
    Q_IMPORT_PLUGIN(QtQuick2Plugin)
    Q_IMPORT_PLUGIN(QtQuickControls2Plugin)
    Q_IMPORT_PLUGIN(QtQuickLayoutsPlugin)
    Q_IMPORT_PLUGIN(QtQuick2WindowPlugin)
    Q_IMPORT_PLUGIN(QtQuickTemplates2Plugin)
    Q_IMPORT_PLUGIN(QtQmlModelsPlugin)
    Q_IMPORT_PLUGIN(QtGraphicalEffectsPlugin)
    Q_IMPORT_PLUGIN(QtGraphicalEffectsPrivatePlugin)
    Q_IMPORT_PLUGIN(QmlShapesPlugin)

    #if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
     Q_IMPORT_PLUGIN(QtQmlPlugin)
    #endif

    #ifdef _WIN32
     Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
     Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    #elif defined(Q_OS_MACOS)
     Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
    #endif

    #if defined(QT5_HAS_X11)
     Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
     Q_IMPORT_PLUGIN(QXcbGlxIntegrationPlugin)
    #endif

    #if defined(QT5_HAS_WAYLAND)
     Q_IMPORT_PLUGIN(QWaylandEglPlatformIntegrationPlugin)
     Q_IMPORT_PLUGIN(QWaylandIntegrationPlugin)
     Q_IMPORT_PLUGIN(QWaylandXdgShellIntegrationPlugin)
    #endif
#endif
