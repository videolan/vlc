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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///utils/" as Utils
import "qrc:///playlist/" as PL
import "qrc:///menus/" as Menus

Utils.NavigableFocusScope {
    id: root

    //center image
    Rectangle {
        visible: !rootWindow.hasEmbededVideo
        focus: false
        color: VLCStyle.colors.bg
        anchors.fill: parent

        Image {
            id: cover
            source: (mainPlaylistController.currentItem.artwork && mainPlaylistController.currentItem.artwork.toString())
                    ? mainPlaylistController.currentItem.artwork
                    : VLCStyle.noArtCover
            fillMode: Image.PreserveAspectFit

            width: parent.width / 2
            height: parent.height / 2
            anchors {
                horizontalCenter:  parent.horizontalCenter
                top: parent.top
                topMargin: parent.height/4  - (VLCStyle.fontHeight_xxlarge + VLCStyle.fontHeight_xlarge + VLCStyle.margin_small*2 ) / 2
            }
        }

        DropShadow {
            anchors.fill: cover
            source: cover
            horizontalOffset: 3
            verticalOffset: 10
            radius: 12
            samples: 17
            color: "black"
        }


        Text {
            id: titleLabel
            anchors {
                top: cover.bottom
                topMargin: VLCStyle.margin_small
                horizontalCenter: cover.horizontalCenter
            }
            text: mainPlaylistController.currentItem.title
            font.pixelSize: VLCStyle.fontSize_xxlarge
            font.bold: true
            color: VLCStyle.colors.text
        }

        Text {
            id: artistLabel
            anchors {
                top: titleLabel.bottom
                topMargin: VLCStyle.margin_small
                horizontalCenter: titleLabel.horizontalCenter
            }

            text: mainPlaylistController.currentItem.artist
            font.pixelSize: VLCStyle.fontSize_xlarge
            color: VLCStyle.colors.text
        }
    }

    VideoSurface {
        id: videoSurface
        ctx: mainctx
        visible: rootWindow.hasEmbededVideo
        anchors.fill: parent

        property point mousePosition: Qt.point(0,0)

        Keys.onReleased: {
            if (event.key === Qt.Key_Menu) {
                toolbarAutoHide.toggleForceVisible()
            }
        }

        onMouseMoved:{
            //short interval for mouse events
            toolbarAutoHide.setVisible(1000)
            mousePosition = Qt.point(x, y)
        }

        Menus.PopupMenu {
            id: dialogMenu
        }
    }

    Utils.Drawer {
        id: playlistpopup
        anchors {
            top: parent.top
            right: parent.right
            bottom: controlBarView.top
        }
        focus: false
        expandHorizontally: true

        component: Rectangle {
            color: VLCStyle.colors.setColorAlpha(VLCStyle.colors.banner, 0.8)
            width: root.width/4
            height: playlistpopup.height

            PL.PlaylistListView {
                id: playlistView
                focus: true
                anchors.fill: parent
                onActionLeft: playlistpopup.quit()
                onActionCancel: playlistpopup.quit()
            }
        }
        function quit() {
            state = "hidden"
            controlBarView.focus = true
        }
        onStateChanged: {
            if (state === "hidden")
                toolbarAutoHide.restart()
        }
    }


    Utils.Drawer {
        id: controlBarView
        focus: true
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        property var  noAutoHide: controlBarView.contentItem.noAutoHide

        state: "visible"
        expandHorizontally: false

        component: Rectangle {
            id: controllerBarId
            color: VLCStyle.colors.setColorAlpha(VLCStyle.colors.banner, 0.8)
            width: controlBarView.width
            height: 90 * VLCStyle.scale
            property alias noAutoHide: controllerId.noAutoHide


            MouseArea {
                id: controllerMouseArea
                hoverEnabled: true
                anchors.fill: parent

                ModalControlBar {
                    id: controllerId
                    focus: true
                    anchors.fill: parent

                    forceNoAutoHide: playlistpopup.state === "visible" || !player.hasVideoOutput || !rootWindow.hasEmbededVideo || controllerMouseArea.containsMouse
                    onNoAutoHideChanged: {
                        if (!noAutoHide)
                            toolbarAutoHide.restart()
                    }
                    showPlaylistButton: true

                    onActionUp: root.actionUp(index)
                    onActionDown: root.actionDown(index)
                    onActionLeft: root.actionLeft(index)
                    onActionRight: root.actionRight(index)
                    onActionCancel: root.actionCancel(index)
                    onShowPlaylist: {
                        if (playlistpopup.state === "visible") {
                            playlistpopup.state = "hidden"
                        } else {
                            playlistpopup.state = "visible"
                            playlistpopup.focus = true
                        }
                    }

                    //unhandled keys are forwarded to the vout window
                    Keys.forwardTo: videoSurface

                    //filter global events to keep toolbar
                    //visible when user navigates within the control bar
                    EventFilter {
                        id: filter
                        source: rootQMLView
                        filterEnabled: controllerId.activeFocus
                        Keys.onPressed: toolbarAutoHide.setVisible(5000)
                    }
                }
            }
        }
    }

    Timer {
        id: toolbarAutoHide
        running: true
        repeat: false
        interval: 3000
        onTriggered: {
            _setVisible(false)
        }

        function _setVisible(visible) {
            if (visible)
            {
                controlBarView.state = "visible"
                controlBarView.forceActiveFocus()
                videoSurface.cursorShape = Qt.ArrowCursor
            }
            else
            {
                if (controlBarView.noAutoHide)
                    return;
                controlBarView.state = "hidden"
                videoSurface.forceActiveFocus()
                videoSurface.cursorShape = Qt.BlankCursor
            }
        }

        function setVisible(duration) {
            _setVisible(true)
            toolbarAutoHide.interval = duration
            toolbarAutoHide.restart()
        }

        function toggleForceVisible() {
            _setVisible(controlBarView.state !== "visible")
            toolbarAutoHide.stop()
        }
    }

    Connections {
        target: rootWindow
        onAskShow: {
            toolbarAutoHide.toggleForceVisible()
        }
        onAskPopupMenu: {
            dialogMenu.popup(videoSurface.mousePosition)
        }
    }
}
