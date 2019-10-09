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
    id: rootPlayer

    //menu/overlay to dismiss
    property var _menu: undefined

    function dismiss() {
        if (_menu)
            _menu.dismiss()
    }

    //center image
    Rectangle {
        visible: !rootWindow.hasEmbededVideo
        focus: false
        color: VLCStyle.colors.bg
        anchors.fill: parent

        FastBlur {
            //destination aspect ration
            readonly property real dar: parent.width / parent.height

            anchors.centerIn: parent
            width: (cover.sar < dar) ? parent.width :  parent.height * cover.sar
            height: (cover.sar < dar) ? parent.width / cover.sar :  parent.height
            source: cover
            radius: 64

            //darken background
            Rectangle {
                color: "#80000000"
                anchors.fill: parent
            }
        }

        Image {
            id: cover
            source: (mainPlaylistController.currentItem.artwork && mainPlaylistController.currentItem.artwork.toString())
                    ? mainPlaylistController.currentItem.artwork
                    : VLCStyle.noArtCover
            fillMode: Image.PreserveAspectFit

            //source aspect ratio
            property real sar: cover.sourceSize.width / cover.sourceSize.height

            width: (parent.height * sar) / 2
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
            color: VLCStyle.colors.playerFg
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
            color: VLCStyle.colors.playerFg
        }
    }

    VideoSurface {
        id: videoSurface
        ctx: mainctx
        visible: rootWindow.hasEmbededVideo
        anchors.fill: parent

        property point mousePosition: Qt.point(0,0)

        Keys.onPressed: {
            if (event.key === Qt.Key_Menu
                    || event.key === Qt.Key_Back
                    || event.key === Qt.Key_Backspace
                    || event.matches(StandardKey.Back)
                    || event.matches(StandardKey.Cancel)) {
                toolbarAutoHide.toggleForceVisible()
            } else {
                rootWindow.sendHotkey(event.key, event.modifiers);
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

    Utils.DrawerExt{
        id: topcontrolView
        anchors{
            left: parent.left
            right: parent.right
            top: parent.top
        }
        edge: Utils.DrawerExt.Edges.Top
        property var noAutoHide: topcontrolView.contentItem.noAutoHide

        state: "visible"

        component: TopBar{
            focus: true
            width: topcontrolView.width
            noAutoHide: noAutoHideInt ||  playlistpopup.state === "visible"
            onNoAutoHideChanged: {
                if (!noAutoHide)
                    toolbarAutoHide.restart()
            }

            onTogglePlaylistVisiblity:  {
                if (rootWindow.playlistDocked)
                    playlistpopup.showPlaylist = !playlistpopup.showPlaylist
                else
                    rootWindow.playlistVisible = !rootWindow.playlistVisible
            }

            navigationParent: rootPlayer
            navigationDownItem: playlistpopup.showPlaylist ? playlistpopup : controlBarView

            Keys.onPressed: {
                if (event.accepted)
                    return
                if (event.key === Qt.Key_Menu
                        || event.key === Qt.Key_Backspace
                        || event.matches(StandardKey.Back)
                        || event.matches(StandardKey.Cancel)) {
                    toolbarAutoHide.toggleForceVisible()
                } else {
                    rootWindow.sendHotkey(event.key, event.modifiers);
                }
            }
        }
    }

    Utils.DrawerExt {
        id: playlistpopup
        anchors {
            top: topcontrolView.bottom
            right: parent.right
            bottom: controlBarView.top
        }
        property bool showPlaylist: false
        property var previousFocus: undefined
        focus: false
        edge: Utils.DrawerExt.Edges.Right
        state: showPlaylist && rootWindow.playlistDocked ? "visible" : "hidden"
        component: Rectangle {
            color: VLCStyle.colors.setColorAlpha(VLCStyle.colors.banner, 0.8)
            width: rootPlayer.width/4
            height: playlistpopup.height

            PL.PlaylistListView {
                id: playlistView
                focus: true
                anchors.fill: parent

                navigationParent: rootPlayer
                navigationUpItem: topcontrolView
                navigationDownItem: controlBarView
                navigationLeft: function() {
                    playlistpopup.showPlaylist = false
                    controlBarView.forceActiveFocus()
                }
                navigationCancel: function() {
                    playlistpopup.showPlaylist = false
                    controlBarView.forceActiveFocus()
                }
            }
        }
        onStateChanged: {
            if (state === "hidden")
                toolbarAutoHide.restart()
        }
    }

    Utils.DrawerExt {
        id: controlBarView
        focus: true
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        property var  noAutoHide: controlBarView.contentItem.noAutoHide

        state: "visible"
        edge: Utils.DrawerExt.Edges.Bottom

        component: Rectangle {
            id: controllerBarId
            gradient: Gradient {
                GradientStop { position: 1.0; color: VLCStyle.colors.playerBg }
                GradientStop { position: 0.0; color: "transparent" }
            }

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

                    navigationParent: rootPlayer
                    navigationUpItem: playlistpopup.showPlaylist ? playlistpopup : topcontrolView

                    //unhandled keys are forwarded as hotkeys
                    Keys.onPressed: {
                        if (event.accepted)
                            return
                        if (event.key === Qt.Key_Menu
                                || event.key === Qt.Key_Backspace
                                || event.matches(StandardKey.Back)
                                || event.matches(StandardKey.Cancel))
                            toolbarAutoHide.toggleForceVisible()
                        else
                            rootWindow.sendHotkey(event.key, event.modifiers);
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
            _setVisibleControlBar(false)
        }

        function _setVisibleControlBar(visible) {
            if (visible)
            {
                controlBarView.state = "visible"
                topcontrolView.state = "visible"
                if (!controlBarView.focus && !topcontrolView.focus)
                    controlBarView.forceActiveFocus()

                videoSurface.cursorShape = Qt.ArrowCursor
            }
            else
            {
                if (controlBarView.noAutoHide || topcontrolView.noAutoHide)
                    return;
                controlBarView.state = "hidden"
                topcontrolView.state = "hidden"
                videoSurface.forceActiveFocus()
                videoSurface.cursorShape = Qt.BlankCursor
            }
        }

        function setVisible(duration) {
            _setVisibleControlBar(true)
            toolbarAutoHide.interval = duration
            toolbarAutoHide.restart()
        }

        function toggleForceVisible() {
            _setVisibleControlBar(controlBarView.state !== "visible")
            toolbarAutoHide.stop()
        }

    }

    //filter global events to keep toolbar
    //visible when user navigates within the control bar
    EventFilter {
        id: filter
        source: rootQMLView
        filterEnabled: controlBarView.state === "visible"
                       && (controlBarView.focus || topcontrolView.focus)
        Keys.onPressed: toolbarAutoHide.setVisible(5000)
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
