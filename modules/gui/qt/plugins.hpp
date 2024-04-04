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

#include <QtQml/qqmlextensionplugin.h>

#if !defined(QT_STATIC) && !defined(QT_SHARED)
# error "Make sure qconfig.h was included before"
#endif

#if !defined(Q_IMPORT_PLUGIN)
# error "Make sure QtPlugin was included before"
#endif

#if !defined(Q_IMPORT_QML_PLUGIN)
# error "Make sure QtPlugin was included before"
#endif

#ifdef QT_STATIC /* For static builds */

// Mandatory plugins:
    Q_IMPORT_PLUGIN(QSvgIconPlugin)
    Q_IMPORT_PLUGIN(QSvgPlugin)
    Q_IMPORT_PLUGIN(QJpegPlugin)
    Q_IMPORT_QML_PLUGIN(QtQuick2Plugin)
    Q_IMPORT_QML_PLUGIN(QtQuickControls2Plugin)
    Q_IMPORT_QML_PLUGIN(QtQuickControls2BasicStylePlugin)
    Q_IMPORT_QML_PLUGIN(QtQuickControls2BasicStyleImplPlugin)
    Q_IMPORT_QML_PLUGIN(QtQuickControls2FusionStylePlugin)
    Q_IMPORT_QML_PLUGIN(QtQuickControls2FusionStyleImplPlugin)
    Q_IMPORT_QML_PLUGIN(QtQuickControls2ImplPlugin)
    Q_IMPORT_QML_PLUGIN(QtQuickLayoutsPlugin)
    Q_IMPORT_QML_PLUGIN(QtQuick_WindowPlugin)
    Q_IMPORT_QML_PLUGIN(QtQuickTemplates2Plugin)
    Q_IMPORT_QML_PLUGIN(QtQmlModelsPlugin)
    Q_IMPORT_QML_PLUGIN(QtGraphicalEffectsPlugin)
    Q_IMPORT_QML_PLUGIN(QtGraphicalEffectsPrivatePlugin)
    Q_IMPORT_QML_PLUGIN(QtQmlPlugin)
    Q_IMPORT_QML_PLUGIN(QtQmlWorkerScriptPlugin)
    Q_IMPORT_QML_PLUGIN(QtQmlMetaPlugin)

#ifdef _WIN32
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    Q_IMPORT_PLUGIN(QModernWindowsStylePlugin)
#else
    Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
#endif
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    Q_IMPORT_QML_PLUGIN(QtQuickControls2WindowsStylePlugin)
    Q_IMPORT_QML_PLUGIN(QtQuickControls2NativeStylePlugin)
    // Q_IMPORT_PLUGIN(QWindowsDirect2DIntegrationPlugin)
#elif defined(Q_OS_MACOS)
    Q_IMPORT_PLUGIN(QMacStylePlugin)
    Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#elif defined(__linux__)
    Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
    Q_IMPORT_PLUGIN(QXcbGlxIntegrationPlugin)
    Q_IMPORT_PLUGIN(QXcbEglIntegrationPlugin)
    Q_IMPORT_PLUGIN(QWaylandEglPlatformIntegrationPlugin)
    Q_IMPORT_PLUGIN(QWaylandIntegrationPlugin)
    Q_IMPORT_PLUGIN(QWaylandXdgShellIntegrationPlugin)
    Q_IMPORT_PLUGIN(QGtk3ThemePlugin)
    Q_IMPORT_PLUGIN(QXdgDesktopPortalThemePlugin)
#endif
#endif
