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

    property bool autoHide: _lockAutoHide === 0 && !lockAutoHide
    property bool lockAutoHide: false
    property int  _lockAutoHide: 0 //count the number of element locking the autoHide

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)
    onActionCancel: history.previous()

    implicitHeight: columnLayout.implicitHeight

    ColumnLayout {
        id: columnLayout
        anchors.fill: parent
        spacing: VLCStyle.margin_xsmall
        anchors.leftMargin: VLCStyle.margin_xlarge
        anchors.rightMargin: VLCStyle.margin_xlarge

        RowLayout {
            Text {
                text: player.time.toString()
                color: VLCStyle.colors.playerFg
                font.pixelSize: VLCStyle.fontSize_normal
                font.bold: true
                Layout.alignment: Qt.AlignLeft
            }

            Item {
                Layout.fillWidth: true
            }


            Text {
                text: (mainInterface.showRemainingTime && player.remainingTime.valid())
                      ? "-" + player.remainingTime.toString()
                      : player.length.toString()
                color: VLCStyle.colors.playerFg
                font.pixelSize: VLCStyle.fontSize_normal
                font.bold: true
                Layout.alignment: Qt.AlignRight
                MouseArea {
                    anchors.fill: parent
                    onClicked: mainInterface.showRemainingTime = !mainInterface.showRemainingTime
                }
            }

        }
        SliderBar {
            id: trackPositionSlider
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            Layout.fillWidth: true
            enabled: player.playingState == PlayerController.PLAYING_STATE_PLAYING || player.playingState == PlayerController.PLAYING_STATE_PAUSED
            Keys.onDownPressed: buttons_left.focus = true

            parentWindow: mainInterfaceRect
        }

        Item {
            Layout.fillWidth: true
            Layout.bottomMargin: VLCStyle.margin_xsmall
            Layout.preferredHeight: Math.max(buttons_left.implicitHeight, buttons_center.implicitHeight, buttons_right.implicitHeight)

            PlayerButtonsLayout {
                id: buttons_left

                model: playerControlBarModel_left
                forceColors: true

                focus: true

                anchors.left: parent.left

                visible: model.count > 0 && (playerControlBarModel_center.count > 0 ? ((x+width) < buttons_center.x) : true)

                navigationParent: root
                navigationUp: function(index) {
                    if (trackPositionSlider.enabled)
                        trackPositionSlider.focus = true
                    else
                        root.navigationUp(index)
                }

                navigationRightItem: buttons_center

                Keys.priority: Keys.AfterItem
                Keys.onPressed: defaultKeyAction(event, 0)
            }

            PlayerButtonsLayout {
                id: buttons_center

                model: playerControlBarModel_center
                forceColors: true

                focus: true

                anchors.horizontalCenter: parent.horizontalCenter

                navigationParent: root
                navigationUp: function(index) {
                    if (trackPositionSlider.enabled)
                        trackPositionSlider.focus = true
                    else
                        root.navigationUp(index)
                }

                navigationLeftItem: buttons_left
                navigationRightItem: buttons_right

                Keys.priority: Keys.AfterItem
                Keys.onPressed: defaultKeyAction(event, 0)
            }

            Item {
                Layout.fillWidth: true
            }

            PlayerButtonsLayout {
                id: buttons_right

                model: playerControlBarModel_right
                forceColors: true

                focus: true

                anchors.right: parent.right

                visible: model.count > 0 && (playerControlBarModel_center.count > 0 ? ((buttons_center.x + buttons_center.width) < x)
                                                                              : !(((buttons_left.x + buttons_left.width) > x) && playerControlBarModel_left.count > 0))

                navigationParent: root
                navigationUp: function(index) {
                    if (trackPositionSlider.enabled)
                        trackPositionSlider.focus = true
                    else
                        root.navigationUp(index)
                }

                navigationLeftItem: buttons_center

                Keys.priority: Keys.AfterItem
                Keys.onPressed: defaultKeyAction(event, 0)
            }
        }
    }
    Connections{
        target: mainInterface
        onToolBarConfUpdated: playerControlBarModel.reloadModel()
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

    ControlButtons{
        id:controlmodelbuttons

        parentWindow: mainInterfaceRect
    }

}
