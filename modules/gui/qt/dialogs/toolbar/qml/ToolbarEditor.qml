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
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///player/" as Player
import "qrc:///style/"

Rectangle{
    id: root
    color: VLCStyle.colors.bg

    ColumnLayout{
        anchors.fill: parent
        spacing: 0
        TabBar {
            id: bar
            Layout.preferredHeight: VLCStyle.heightBar_normal
            Layout.preferredWidth: VLCStyle.button_width_large * bar.count

            EditorTabButton {
                id: mainPlayerTab
                index: 0
                text: i18n.qtr("Mainplayer")
            }

            EditorTabButton {
                id: miniPlayerTab
                index: 1
                text: i18n.qtr("Miniplayer")
            }
        }
        Rectangle{
            Layout.preferredHeight: VLCStyle.heightBar_large
            Layout.fillWidth: true
            radius: 2
            color: VLCStyle.colors.bgAlt

            StackLayout{
                anchors.fill: parent
                currentIndex: bar.currentIndex

                RowLayout {
                    Layout.preferredHeight: VLCStyle.heightBar_large
                    Layout.fillWidth: true

                    spacing: VLCStyle.margin_xlarge

                    TextMetrics {
                        id: leftMetric
                        text: i18n.qtr("L   E   F   T")
                        font.pixelSize: VLCStyle.fontSize_xxlarge
                    }

                    TextMetrics {
                        id: centerMetric
                        text: i18n.qtr("C   E   N   T   E   R")
                        font.pixelSize: VLCStyle.fontSize_xxlarge
                    }

                    TextMetrics {
                        id: rightMetric
                        text: i18n.qtr("R   I   G   H   T")
                        font.pixelSize: VLCStyle.fontSize_xxlarge
                    }

                    EditorDNDView {
                        id : playerBtnDND_left
                        Layout.preferredHeight: VLCStyle.heightBar_large

                        Layout.alignment: Qt.AlignLeft
                        Layout.fillWidth: count > 0 || (playerBtnDND_left.count === 0 && playerBtnDND_center.count === 0 && playerBtnDND_right.count === 0)
                        Layout.minimumWidth: leftMetric.width

                        model: playerControlBarModel_left

                        Text {
                            anchors.centerIn: parent

                            height: parent.height
                            width: parent.width / 2
                            text: leftMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }

                    EditorDNDView {
                        id : playerBtnDND_center
                        Layout.preferredHeight: VLCStyle.heightBar_large

                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: count > 0 || (playerBtnDND_left.count === 0 && playerBtnDND_center.count === 0 && playerBtnDND_right.count === 0)
                        Layout.minimumWidth: centerMetric.width

                        model: playerControlBarModel_center

                        Text {
                            anchors.centerIn: parent

                            height: parent.height
                            width: parent.width / 2
                            text: centerMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }

                    EditorDNDView {
                        id : playerBtnDND_right
                        Layout.preferredHeight: VLCStyle.heightBar_large

                        Layout.alignment: Qt.AlignRight
                        Layout.fillWidth: count > 0 || (playerBtnDND_left.count === 0 && playerBtnDND_center.count === 0 && playerBtnDND_right.count === 0)
                        Layout.minimumWidth: rightMetric.width

                        model: playerControlBarModel_right

                        Text {
                            anchors.centerIn: parent

                            height: parent.height
                            width: parent.width / 2
                            text: rightMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }
                }

                RowLayout {
                    Layout.preferredHeight: VLCStyle.heightBar_large
                    Layout.fillWidth: true

                    spacing: VLCStyle.margin_xlarge

                    EditorDNDView {
                        id : miniPlayerBtnDND_left
                        Layout.preferredHeight: VLCStyle.heightBar_large

                        Layout.alignment: Qt.AlignLeft
                        Layout.fillWidth: count > 0 || (miniPlayerBtnDND_left.count === 0 && miniPlayerBtnDND_center.count === 0 && miniPlayerBtnDND_right.count === 0)
                        Layout.minimumWidth: leftMetric.width

                        model: miniPlayerModel_left

                        Text {
                            anchors.centerIn: parent

                            height: parent.height
                            width: parent.width / 2
                            text: leftMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }

                    EditorDNDView {
                        id : miniPlayerBtnDND_center
                        Layout.preferredHeight: VLCStyle.heightBar_large

                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: count > 0 || (miniPlayerBtnDND_left.count === 0 && miniPlayerBtnDND_center.count === 0 && miniPlayerBtnDND_right.count === 0)
                        Layout.minimumWidth: centerMetric.width

                        model: miniPlayerModel_center

                        Text {
                            anchors.centerIn: parent

                            height: parent.height
                            width: parent.width / 2
                            text: centerMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }

                    EditorDNDView {
                        id : miniPlayerBtnDND_right
                        Layout.preferredHeight: VLCStyle.heightBar_large

                        Layout.alignment: Qt.AlignRight
                        Layout.fillWidth: count > 0 || (miniPlayerBtnDND_left.count === 0 && miniPlayerBtnDND_center.count === 0 && miniPlayerBtnDND_right.count === 0)
                        Layout.minimumWidth: rightMetric.width

                        model: miniPlayerModel_right

                        Text {
                            anchors.centerIn: parent

                            height: parent.height
                            width: parent.width / 2
                            text: rightMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }
                }
            }
        }

        Rectangle{
            id : allBtnsGrid
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.margins: VLCStyle.margin_xxsmall
            color: VLCStyle.colors.bgAlt

            ColumnLayout{
                anchors.fill: parent

                Text {
                    Layout.margins: VLCStyle.margin_xxsmall
                    text: i18n.qtr("Drag items below to add them above: ")
                    font.pointSize: VLCStyle.fontHeight_xsmall
                    color: VLCStyle.colors.buttonText
                }

                ToolbarEditorButtonList {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: VLCStyle.margin_xxsmall
                }
            }
        }
    }

    function getProfileConfig(){
        return "%1#%2#%3 | %4#%5#%6".arg(playerControlBarModel_left.getConfig())
                                    .arg(playerControlBarModel_center.getConfig())
                                    .arg(playerControlBarModel_right.getConfig())
                                    .arg(miniPlayerModel_left.getConfig())
                                    .arg(miniPlayerModel_center.getConfig())
                                    .arg(miniPlayerModel_right.getConfig())
    }

    Connections{
        target: toolbareditor
        onUpdatePlayerModel: {
            playerBtnDND_left.currentIndex = -1
            playerBtnDND_center.currentIndex = -1
            playerBtnDND_right.currentIndex = -1

            miniPlayerBtnDND_left.currentIndex = -1
            miniPlayerBtnDND_center.currentIndex = -1
            miniPlayerBtnDND_right.currentIndex = -1

            if (toolbarName == "MainPlayerToolbar-left")
                playerControlBarModel_left.reloadConfig(config)
            else if (toolbarName == "MainPlayerToolbar-center")
                playerControlBarModel_center.reloadConfig(config)
            else if (toolbarName == "MainPlayerToolbar-right")
                playerControlBarModel_right.reloadConfig(config)
            else if (toolbarName == "MiniPlayerToolbar-left")
                miniPlayerModel_left.reloadConfig(config)
            else if (toolbarName == "MiniPlayerToolbar-center")
                miniPlayerModel_center.reloadConfig(config)
            else if (toolbarName == "MiniPlayerToolbar-right")
                miniPlayerModel_right.reloadConfig(config)
        }

        onSaveConfig: {
            miniPlayerModel_left.saveConfig()
            miniPlayerModel_center.saveConfig()
            miniPlayerModel_right.saveConfig()

            playerControlBarModel_left.saveConfig()
            playerControlBarModel_center.saveConfig()
            playerControlBarModel_right.saveConfig()
        }
    }

    PlayerControlBarModel {
        id: playerControlBarModel_left
        mainCtx: mainctx
        configName: "MainPlayerToolbar-left"
    }

    PlayerControlBarModel {
        id: playerControlBarModel_center
        mainCtx: mainctx
        configName: "MainPlayerToolbar-center"
    }

    PlayerControlBarModel {
        id: playerControlBarModel_right
        mainCtx: mainctx
        configName: "MainPlayerToolbar-right"
    }

    PlayerControlBarModel {
        id: miniPlayerModel_left
        mainCtx: mainctx
        configName: "MiniPlayerToolbar-left"
    }

    PlayerControlBarModel {
        id: miniPlayerModel_center
        mainCtx: mainctx
        configName: "MiniPlayerToolbar-center"
    }

    PlayerControlBarModel {
        id: miniPlayerModel_right
        mainCtx: mainctx
        configName: "MiniPlayerToolbar-right"
    }

    Player.ControlButtons{
        id: controlButtons
    }

    PlaylistControllerModel {
        id: mainPlaylistController
        playlistPtr: mainctx.playlist
    }

    EditorDummyButton{
        id: buttonDragItem
        visible: false
        Drag.active: visible
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2
        color: VLCStyle.colors.buttonText

        function updatePos(x, y) {
            var pos = root.mapFromGlobal(x, y)
            this.x = pos.x - 20
            this.y = pos.y - 20
        }
    }

    /*
      Match the QML theme to
      native part. Using Qt Style Sheet to
      set the theme.
    */
    Component.onCompleted: toolbareditor.setStyleSheet(
                               "background-color:"+VLCStyle.colors.bg+
                               ";color:"+VLCStyle.colors.buttonText+
                               ";selection-background-color:"+
                               VLCStyle.colors.bgHover);
}

