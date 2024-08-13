/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
import QtQuick

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

Rectangle {
    id: minimalViewRec

    property bool _showCSD: MainCtx.clientSideDecoration && !(MainCtx.intfMainWindow.visibility === Window.FullScreen)

    color: "#000000"

    ColorContext {
        id: theme

        palette: (MainCtx.hasEmbededVideo && MainCtx.pinVideoControls === false)
                 ? VLCStyle.darkPalette
                 : VLCStyle.palette

        colorSet: ColorContext.Window
    }

    Image {
        id: logo

        source: MainCtx.useXmasCone() 
                ? "qrc:///logo/vlc48-xmas.png" 
                : SVGColorImage.colorize("qrc:///misc/cone.svg").accent(theme.accent).uri()
        
        anchors.centerIn: parent
        width: Math.min(parent.width / 2, sourceSize.width)
        height: Math.min(parent.height / 2, sourceSize.height)
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    //drag and dbl click the titlebar in CSD mode
    Loader {
        id: tapNDrag

        anchors.fill: parent
        active: minimalViewRec._showCSD
        source: "qrc:///qt/qml/VLC/Widgets/CSDTitlebarTapNDrapHandler.qml"
    }

    Loader {
        id: csdDecorations

        anchors.top: parent.top
        anchors.right: parent.right
        
        focus: false
        height: VLCStyle.icon_normal
        active:  _showCSD
        enabled: _showCSD
        visible: _showCSD
        source:  VLCStyle.palette.hasCSDImage
                 ? "qrc:///qt/qml/VLC/Widgets/CSDThemeButtonSet.qml"
                 : "qrc:///qt/qml/VLC/Widgets/CSDWindowButtonSet.qml"
    }
}

    
