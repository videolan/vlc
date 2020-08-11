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
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///playlist/" as PL
import "qrc:///menus/" as Menus

Widgets.NavigableFocusScope {
    id: rootPlayer

    //menu/overlay to dismiss
    property var _menu: undefined

    function dismiss() {
        if (_menu)
            _menu.dismiss()
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: {
        if (event.accepted)
            return
        defaultKeyAction(event, 0)

        //unhandled keys are forwarded as hotkeys
        if (!event.accepted || controlBarView.state !== "visible")
            mainInterface.sendHotkey(event.key, event.modifiers);
    }

    Keys.onReleased: {
        if (event.accepted)
            return
        if (event.key === Qt.Key_Menu) {
            toolbarAutoHide.toggleForceVisible()
        } else {
            defaultKeyReleaseAction(event, 0)
        }
    }

    navigationCancel: function() {
        if (mainInterface.hasEmbededVideo && controlBarView.state === "visible") {
            toolbarAutoHide._setVisibleControlBar(false)
        } else {
            if (player.hasVideoOutput) {
               mainPlaylistController.stop()
            }
            history.previous()
        }
    }

    //property alias centralLayout: mainLayout.centralLayout
    ColumnLayout {
        id: mainLayout
        z: 1
        anchors.fill: parent

        Widgets.DrawerExt{
            id: topcontrolView

            Layout.preferredHeight: height
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop

            edge: Widgets.DrawerExt.Edges.Top
            property var autoHide: topcontrolView.contentItem.autoHide

            state: "visible"

            component: FocusScope {
                width: topcontrolView.width
                height: topbar.implicitHeight
                focus: true
                property bool autoHide: topbar.autoHide && !resumeDialog.visible

                TopBar{
                    id: topbar

                    anchors.fill: parent

                    focus: true
                    visible: !resumeDialog.visible

                    onAutoHideChanged: {
                        if (autoHide)
                            toolbarAutoHide.restart()
                    }

                    lockAutoHide: playlistpopup.state === "visible"

                    onTogglePlaylistVisiblity:  {
                        if (mainInterface.playlistDocked)
                            playlistpopup.showPlaylist = !playlistpopup.showPlaylist
                        else
                            mainInterface.playlistVisible = !mainInterface.playlistVisible
                    }

                    title: mainPlaylistController.currentItem.title

                    navigationParent: rootPlayer
                    navigationDownItem: playlistpopup.showPlaylist ? playlistpopup : controlBarView
                }

                ResumeDialog {
                    id: resumeDialog

                    anchors.fill: parent

                    navigationParent: rootPlayer

                    onHidden: {
                        if (activeFocus) {
                            topbar.focus = true
                            controlBarView.forceActiveFocus()
                        }
                    }
                }
            }
        }

        Item {
            id: centralLayout

            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: VLCStyle.margin_small


                visible: !mainInterface.hasEmbededVideo

                Item {
                    Layout.fillHeight: true
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.maximumHeight: rootPlayer.height / 2
                    Layout.minimumHeight: 1
                    Layout.topMargin: albumLabel.Layout.preferredHeight + artistLabel.Layout.preferredHeight

                    Image {
                        id: cover
                        source: (mainPlaylistController.currentItem.artwork && mainPlaylistController.currentItem.artwork.toString())
                                ? mainPlaylistController.currentItem.artwork
                                : VLCStyle.noArtCover
                        fillMode: Image.PreserveAspectFit

                        //source aspect ratio
                        readonly property real sar: cover.sourceSize.width / cover.sourceSize.height

                        height: Math.min(parent.height, parent.width - VLCStyle.margin_small * 2)
                        width: height  * sar
                        anchors.centerIn: parent
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
                }

                Label {
                    id: albumLabel

                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    Layout.alignment: Qt.AlignHCenter

                    text: mainPlaylistController.currentItem.album
                    font.pixelSize: VLCStyle.fontSize_xxlarge
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    color: VLCStyle.colors.playerFg
                    Accessible.description: i18n.qtr("album")
                }

                Label {
                    id: artistLabel

                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    Layout.alignment: Qt.AlignHCenter

                    text: mainPlaylistController.currentItem.artist
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: VLCStyle.fontSize_xlarge
                    color: VLCStyle.colors.playerFg
                    Accessible.description: i18n.qtr("artist")
                }

                Item {
                    Layout.fillHeight: true
                }
            }

            Widgets.DrawerExt {
                id: playlistpopup

                z: 2

                anchors {
                    top: centralLayout.top
                    right: parent.right
                    bottom: centralLayout.bottom
                }
                property bool showPlaylist: false
                property var previousFocus: undefined
                focus: false
                edge: Widgets.DrawerExt.Edges.Right
                state: showPlaylist && mainInterface.playlistDocked ? "visible" : "hidden"
                component: Rectangle {
                    color: VLCStyle.colors.setColorAlpha(VLCStyle.colors.banner, 0.8)
                    width: rootPlayer.width/4
                    height: playlistpopup.height

                    PL.PlaylistListView {
                        id: playlistView
                        focus: true
                        anchors.fill: parent

                        forceDark: true
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
        }

        Widgets.DrawerExt {
            id: controlBarView

            Layout.preferredHeight: height
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBottom

            focus: true

            property var autoHide: controlBarView.contentItem.autoHide

            state: "visible"
            edge: Widgets.DrawerExt.Edges.Bottom

            component: Rectangle {
                id: controllerBarId
                gradient: Gradient {
                    GradientStop { position: 1.0; color: VLCStyle.colors.playerBg }
                    GradientStop { position: 0.0; color: "transparent" }
                }

                width: controlBarView.width
                height: controllerId.implicitHeight + controllerId.anchors.bottomMargin
                property alias autoHide: controllerId.autoHide

                MouseArea {
                    id: controllerMouseArea
                    hoverEnabled: true
                    anchors.fill: parent

                    ControlBar {
                        id: controllerId
                        focus: true
                        anchors.fill: parent
                        anchors.leftMargin: VLCStyle.applicationHorizontalMargin
                        anchors.rightMargin: VLCStyle.applicationHorizontalMargin
                        anchors.bottomMargin: VLCStyle.applicationVerticalMargin

                        lockAutoHide: playlistpopup.state === "visible"
                            || !player.hasVideoOutput
                            || !mainInterface.hasEmbededVideo
                            || controllerMouseArea.containsMouse
                        onAutoHideChanged: {
                            if (autoHide)
                                toolbarAutoHide.restart()
                        }

                        navigationParent: rootPlayer
                        navigationUpItem: playlistpopup.showPlaylist ? playlistpopup : topcontrolView
                    }
                }
            }
        }

    }

    //center image
    Rectangle {
        visible: !mainInterface.hasEmbededVideo
        focus: false
        color: VLCStyle.colors.bg
        anchors.fill: parent

        z: 0

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
    }

    VideoSurface {
        id: videoSurface

        z: 0

        ctx: mainctx
        visible: mainInterface.hasEmbededVideo
        anchors.fill: parent

        property point mousePosition: Qt.point(0,0)

        onMouseMoved:{
            //short interval for mouse events
            toolbarAutoHide.setVisible(1000)
            mousePosition = Qt.point(x, y)
        }

        Menus.PopupMenu {
            id: dialogMenu
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
                if (!controlBarView.autoHide || !topcontrolView.autoHide)
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
        source: topWindow
        filterEnabled: controlBarView.state === "visible"
                       && (controlBarView.focus || topcontrolView.focus)
        Keys.onPressed: toolbarAutoHide.setVisible(5000)
    }

    Connections {
        target: mainInterface
        onAskShow: {
            toolbarAutoHide.toggleForceVisible()
        }
        onAskPopupMenu: {
            dialogMenu.popup(videoSurface.mousePosition)
        }
    }
}
