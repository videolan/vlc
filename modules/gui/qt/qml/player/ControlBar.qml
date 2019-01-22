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

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///utils/" as Utils
import "qrc:///menus/" as Menus


Utils.NavigableFocusScope {
    id: root

    signal showTrackBar()
    signal showPlaylist()

    property bool noAutoHide: mainMenu.opened
    property bool showPlaylistButton: false

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)

    onActionCancel: mainPlaylistController.stop()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        SliderBar {
            id: trackPositionSlider
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            Layout.fillWidth: true
            enabled: player.playingState == PlayerController.PLAYING_STATE_PLAYING || player.playingState == PlayerController.PLAYING_STATE_PAUSED
            Keys.onDownPressed: buttons.focus = true

        }

        Utils.NavigableFocusScope {
            id: buttons
            Layout.fillHeight: true
            Layout.fillWidth: true

            focus: true

            onActionUp: {
                if (trackPositionSlider.enabled)
                    trackPositionSlider.focus = true
                else
                    root.actionUp(index)
            }
            onActionDown: root.actionDown(index)
            onActionLeft: root.actionLeft(index)
            onActionRight: root.actionRight(index)
            onActionCancel: root.actionCancel(index)

            Keys.priority: Keys.AfterItem
            Keys.onPressed: defaultKeyAction(event, 0)

            Utils.IconToolButton {
                id: fullscreenBtn
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                KeyNavigation.right: randomBtn
                size: VLCStyle.icon_large
                text: VLCIcons.exit
                onClicked: mainPlaylistController.stop()
            }

            ToolBar {
                id: centerbuttons
                anchors.centerIn: parent

                focusPolicy: Qt.StrongFocus
                focus: true

                background: Rectangle {
                    color: "transparent"
                }

                Component.onCompleted: {
                    playBtn.focus= true
                }

                RowLayout {
                    focus: true
                    anchors.fill: parent
                    Utils.IconToolButton {
                        id: randomBtn
                        size: VLCStyle.icon_large
                        checked: mainPlaylistController.random
                        text: VLCIcons.shuffle_on
                        onClicked: mainPlaylistController.toggleRandom()
                        KeyNavigation.right: prevBtn
                    }

                    Utils.IconToolButton {
                        id: prevBtn
                        size: VLCStyle.icon_large
                        text: VLCIcons.previous
                        onClicked: mainPlaylistController.prev()
                        KeyNavigation.right: playBtn
                    }

                    Utils.IconToolButton {
                        id: playBtn
                        size: VLCStyle.icon_large
                        text: (player.playingState !== PlayerController.PLAYING_STATE_PAUSED
                               && player.playingState !== PlayerController.PLAYING_STATE_STOPPED)
                                     ? VLCIcons.pause
                                     : VLCIcons.play
                        onClicked: mainPlaylistController.togglePlayPause()
                        focus: true
                        KeyNavigation.right: nextBtn
                    }

                    Utils.IconToolButton {
                        id: nextBtn
                        size: VLCStyle.icon_large
                        text: VLCIcons.next
                        onClicked: mainPlaylistController.next()
                        KeyNavigation.right: repeatBtn
                    }

                    Utils.IconToolButton {
                        id: repeatBtn
                        size: VLCStyle.icon_large
                        checked: mainPlaylistController.repeatMode !== PlaylistControllerModel.PLAYBACK_REPEAT_NONE
                        text: (mainPlaylistController.repeatMode == PlaylistControllerModel.PLAYBACK_REPEAT_CURRENT)
                                     ? VLCIcons.repeat_one
                                     : VLCIcons.repeat_all
                        onClicked: mainPlaylistController.toggleRepeatMode()
                        KeyNavigation.right: langBtn
                    }
                }
            }

            ToolBar {
                id: rightButtons
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter

                focusPolicy: Qt.StrongFocus
                background: Rectangle {
                    color: "transparent"
                }
                Component.onCompleted: {
                    rightButtons.contentItem.focus= true
                }

                RowLayout {
                    anchors.fill: parent


                    Utils.IconToolButton {
                        id: langBtn
                        size: VLCStyle.icon_large
                        text: VLCIcons.audiosub
                        onClicked: root.showTrackBar()
                        KeyNavigation.right: showPlaylistButton ? playlistBtn : menuBtn
                    }

                    Utils.IconToolButton {
                        id: playlistBtn
                        visible: showPlaylistButton
                        size: VLCStyle.icon_large
                        text: VLCIcons.playlist
                        onClicked: root.showPlaylist()
                        KeyNavigation.right: menuBtn
                    }

                    Utils.IconToolButton {
                        id: menuBtn
                        size: VLCStyle.icon_large
                        text: VLCIcons.menu
                        onClicked: mainMenu.openAbove(this)

                        Menus.MainDropdownMenu {
                            id: mainMenu
                            onClosed: {
                                menuBtn.forceActiveFocus()
                            }
                        }
                    }

                }
            }
        }
    }
}
