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

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///playlist/" as PL


Widgets.NavigableFocusScope {
    id: root

    signal showTrackBar()

    property VLCColors colors: VLCStyle.nightColors
    property bool autoHide: _lockAutoHide === 0 && !lockAutoHide
    property bool lockAutoHide: false
    property int  _lockAutoHide: 0 //count the number of element locking the autoHide

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)
    onActionCancel: history.previous()

    onActiveFocusChanged: if (activeFocus) trackPositionSlider.forceActiveFocus()

    implicitHeight: columnLayout.implicitHeight

    ColumnLayout {
        id: columnLayout
        anchors.fill: parent
        spacing: VLCStyle.margin_small

        RowLayout {
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: VLCStyle.margin_normal

            Label {
                text: player.time.toString()
                color: root.colors.playerFg
                font.pixelSize: VLCStyle.fontSize_normal
                Layout.alignment: Qt.AlignLeft
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: (mainInterface.showRemainingTime && player.remainingTime.valid())
                      ? "-" + player.remainingTime.toString()
                      : player.length.toString()
                color: root.colors.playerFg
                font.pixelSize: VLCStyle.fontSize_normal
                Layout.alignment: Qt.AlignRight
                MouseArea {
                    anchors.fill: parent
                    onClicked: mainInterface.showRemainingTime = !mainInterface.showRemainingTime
                }
            }

        }
        SliderBar {
            id: trackPositionSlider

            backgroundColor: Qt.lighter(colors.playerBg, 1.6180)
            progressBarColor: activeFocus ? colors.accent : colors.playerControlBarFg
            barHeight: VLCStyle.heightBar_xxsmall
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            Layout.fillWidth: true
            enabled: player.playingState == PlayerController.PLAYING_STATE_PLAYING || player.playingState == PlayerController.PLAYING_STATE_PAUSED
            Keys.onDownPressed: playerButtonsLayout.focus = true

            parentWindow: g_root

            colors: root.colors
        }

        Item {
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: VLCStyle.margin_normal
            Layout.bottomMargin: VLCStyle.margin_xsmall
            Layout.preferredHeight: playerButtonsLayout.implicitHeight

            PlayerButtonsLayout {
                id: playerButtonsLayout

                anchors {
                    fill: parent

                    leftMargin: VLCStyle.applicationHorizontalMargin
                    rightMargin: VLCStyle.applicationHorizontalMargin
                    bottomMargin: VLCStyle.applicationVerticalMargin
                }

                models: [playerControlBarModel_left, playerControlBarModel_center, playerControlBarModel_right]

                navigationUpItem: trackPositionSlider.enabled ? trackPositionSlider : root.navigationUpItem

                colors: root.colors
            }
        }
    }

    PlayerControlBarModel{
        id:playerControlBarModel_left
        mainCtx: mainctx
        configName: "MainPlayerToolbar-left"
    }

    PlayerControlBarModel{
        id:playerControlBarModel_center
        mainCtx: mainctx
        configName: "MainPlayerToolbar-center"
    }

    PlayerControlBarModel{
        id:playerControlBarModel_right
        mainCtx: mainctx
        configName: "MainPlayerToolbar-right"
    }
}
