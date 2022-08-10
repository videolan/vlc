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
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


T.Pane {
    id: root

    property VLCColors colors: VLCStyle.colors

    implicitWidth: Math.max(background ? background.implicitWidth : 0,
                            contentWidth + leftPadding + rightPadding)

    implicitHeight: Math.max(background ? background.implicitHeight : 0,
                             contentHeight + topPadding + bottomPadding)

    contentWidth: teleWidget.implicitWidth
    contentHeight: teleWidget.y + teleWidget.implicitHeight

    Keys.priority: Keys.AfterItem
    Keys.onPressed: Navigation.defaultKeyAction(event)

    Column {
        spacing: VLCStyle.margin_small

        Widgets.SubtitleLabel {
            text: I18n.qtr("Teletext")

            color: root.colors.text
        }

        Row {
            id: teleWidget

            Widgets.IconControlButton{
                id: teleActivateBtn

                checked: Player.teletextEnabled

                focus: true

                iconText: VLCIcons.tv
                text: I18n.qtr("Teletext activate")

                colors: root.colors
                color: colors.text

                T.ToolTip.visible: hovered || visualFocus

                Navigation.parentItem: root
                Navigation.rightItem: teleTransparencyBtn

                onClicked: Player.teletextEnabled = !Player.teletextEnabled
            }

            Widgets.IconControlButton{
                id: teleTransparencyBtn

                enabled: teleActivateBtn.checked

                opacity: 0.5

                iconText: VLCIcons.tvtelx
                text: I18n.qtr("Teletext transparency")

                colors: root.colors
                color: colors.text

                T.ToolTip.visible: hovered || visualFocus

                Navigation.parentItem: root
                Navigation.leftItem: teleActivateBtn
                Navigation.rightItem: telePageNumber

                onClicked: Player.teletextTransparency = !Player.teletextTransparency
            }

            Widgets.SpinBoxExt{
                id: telePageNumber

                // NOTE: We want a fixed size for the TextInput.
                width: VLCStyle.dp(64, VLCStyle.scale)

                enabled: teleActivateBtn.checked

                from: 100
                to: 899

                validator: IntValidator {
                    bottom: telePageNumber.from
                    top: telePageNumber.to
                }

                editable: true
                textColor: colors.text
                bgColor: colors.bg

                Navigation.parentItem: root
                Navigation.leftItem: teleTransparencyBtn
                Navigation.rightItem: indexKeyBtn

                //only update the player teletext page when the user change the value manually
                property bool inhibitPageUpdate: true

                onValueChanged: {
                    if (inhibitPageUpdate)
                        return
                    Player.teletextPage = value
                }

                Component.onCompleted: {
                    value = Player.teletextPage
                    inhibitPageUpdate = false
                }

                Connections {
                    target: Player
                    onTeletextPageChanged: {
                        telePageNumber.inhibitPageUpdate = true
                        telePageNumber.value = Player.teletextPage
                        telePageNumber.inhibitPageUpdate = false
                    }
                }
            }

            Widgets.IconControlButton{
                id: indexKeyBtn

                enabled: teleActivateBtn.checked

                iconText: VLCIcons.record
                text: I18n.qtr("Index key")

                colors: root.colors
                color: "grey"
                colorDisabled: "grey"

                T.ToolTip.visible: hovered || visualFocus

                Navigation.parentItem: root
                Navigation.leftItem: telePageNumber
                Navigation.rightItem: redKeyBtn

                onClicked: Player.teletextPage = Player.TELE_INDEX
            }

            Widgets.IconControlButton{
                id: redKeyBtn

                enabled: teleActivateBtn.checked

                iconText: VLCIcons.record
                text: I18n.qtr("Red key")

                colors: root.colors
                color: "red"
                colorDisabled: "grey"

                T.ToolTip.visible: hovered || visualFocus

                Navigation.parentItem: root
                Navigation.leftItem: indexKeyBtn
                Navigation.rightItem: greenKeyBtn

                onClicked: Player.teletextPage = Player.TELE_RED
            }

            Widgets.IconControlButton{
                id: greenKeyBtn

                enabled: teleActivateBtn.checked

                iconText: VLCIcons.record
                text: I18n.qtr("Green key")

                colors: root.colors
                color: "green"
                colorDisabled: "grey"

                T.ToolTip.visible: hovered || visualFocus

                Navigation.parentItem: root
                Navigation.leftItem: redKeyBtn
                Navigation.rightItem: yellowKeyBtn

                onClicked: Player.teletextPage = Player.TELE_GREEN
            }

            Widgets.IconControlButton{
                id: yellowKeyBtn

                enabled: teleActivateBtn.checked

                iconText: VLCIcons.record
                text: I18n.qtr("Yellow key")

                colors: root.colors
                color: "yellow"
                colorDisabled: "grey"

                T.ToolTip.visible: hovered || visualFocus

                Navigation.parentItem: root
                Navigation.leftItem: greenKeyBtn
                Navigation.rightItem: blueKeyBtn

                onClicked: Player.teletextPage = Player.TELE_YELLOW
            }

            Widgets.IconControlButton{
                id: blueKeyBtn

                enabled: teleActivateBtn.checked

                iconText: VLCIcons.record
                text: I18n.qtr("Blue key")

                colors: root.colors
                color: "blue"
                colorDisabled: "grey"

                T.ToolTip.visible: hovered || visualFocus

                Navigation.parentItem: root
                Navigation.leftItem: yellowKeyBtn

                onClicked: Player.teletextPage = Player.TELE_BLUE
            }
        }
    }
}
