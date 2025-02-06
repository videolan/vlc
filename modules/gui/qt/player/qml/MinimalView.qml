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
import VLC.Player
import VLC.Style
import VLC.Util
import VLC.Menus

FocusScope {
    id: root

    property bool _showCSD: MainCtx.clientSideDecoration
                            && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)
                            && (!MainCtx.hasEmbededVideo || _csdOnVideo)
    property bool _csdOnVideo: true

    VideoSurface {
        id: videoSurface

        focus: visible

        videoSurfaceProvider: MainCtx.videoSurfaceProvider

        visible: MainCtx.hasEmbededVideo

        anchors.fill: parent

        onMouseMoved: {
            mouseAutoHide.restart()
            videoSurface.cursorShape = Qt.ArrowCursor
            root._csdOnVideo = true
        }

        onVisibleChanged: {
            mouseAutoHide.restart()
            videoSurface.cursorShape = Qt.ArrowCursor
            root._csdOnVideo = true
        }

        Timer {
            // toggleControlBarButton's visibility depends on this timer
            id: mouseAutoHide
            running: true
            repeat: false
            interval: 3000

            onTriggered: {
                // Cursor hides when toggleControlBarButton is not visible
                videoSurface.forceActiveFocus()
                videoSurface.cursorShape = Qt.BlankCursor
                root._csdOnVideo = false
            }
        }
    }

    Rectangle {
        color: "#000000"

        anchors.fill: parent

        visible: !MainCtx.hasEmbededVideo
        focus: visible

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
            visible: MainCtx.bgCone
        }

        QmlAudioContextMenu {
            id: audioContextMenu
            ctx: MainCtx
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: (eventPoint, button) => {
                audioContextMenu.popup(eventPoint.globalPosition)
            }
        }
    }

    Item {
        id: csd

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        height: VLCStyle.globalToolbar_height

        //drag and dbl click the titlebar in CSD mode
        Loader {
            id: tapNDrag

            anchors.fill: parent
            active: root._showCSD
            source: "qrc:///qt/qml/VLC/Widgets/CSDTitlebarTapNDrapHandler.qml"
        }

        Loader {
            id: csdDecorations

            anchors {
                top: parent.top
                right: parent.right
                bottom: parent.bottom
            }

            focus: false
            active:  root._showCSD
            enabled: root._showCSD
            visible: root._showCSD
            source:  VLCStyle.palette.hasCSDImage
                     ? "qrc:///qt/qml/VLC/Widgets/CSDThemeButtonSet.qml"
                     : "qrc:///qt/qml/VLC/Widgets/CSDWindowButtonSet.qml"
        }
    }

    Widgets.FloatingNotification {
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: VLCStyle.margin_large
        }
    }

    Keys.onPressed: (event) => {
        if (event.accepted)
            return
        MainCtx.sendHotkey(event.key, event.modifiers);
    }
}
