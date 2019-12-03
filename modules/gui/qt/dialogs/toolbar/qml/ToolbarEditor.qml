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

                EditorDNDView {
                    id : playerBtnDND
                    Layout.preferredHeight: VLCStyle.heightBar_large
                    Layout.fillWidth: true
                    model: playerControlBarModel
                }

                EditorDNDView {
                    id : miniPlayerBtnDND
                    Layout.preferredHeight: VLCStyle.heightBar_large
                    Layout.fillWidth: true
                    model: miniPlayerModel
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
        return playerControlBarModel.getConfig() + "|" + miniPlayerModel.getConfig()
    }

    Connections{
        target: toolbareditor
        onUpdatePlayerModel: {
            playerBtnDND.currentIndex = -1
            miniPlayerBtnDND.currentIndex = -1
            if (toolbarName == "MainPlayerToolbar")
                playerControlBarModel.reloadConfig(config)
            else
                miniPlayerModel.reloadConfig(config)
        }
        onSaveConfig: {
            miniPlayerModel.saveConfig()
            playerControlBarModel.saveConfig()
        }
    }

    PlayerControlBarModel {
        id: playerControlBarModel
        mainCtx: mainctx
        configName: "MainPlayerToolbar"
    }

    PlayerControlBarModel {
        id: miniPlayerModel
        mainCtx: mainctx
        configName: "MiniPlayerToolbar"
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

