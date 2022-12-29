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
import QtQuick.Layouts 1.11
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

    contentWidth: column.implicitWidth
    contentHeight: column.implicitHeight

    Keys.priority: Keys.AfterItem
    Keys.onPressed: Navigation.defaultKeyAction(event)

    function _teletextButtonColor(item, base)
    {
        if (!item.enabled)
            return VLCStyle.colors.setColorAlpha(base, 0.2)
        else if (item.hovered && !item.down)
            return Qt.lighter(base)
        else
            return base
    }

    Column {
        id: column

        spacing: VLCStyle.margin_small

        Item {
            // NOTE: This makes sure that we can deal with long translations for our itemText.
            width: Math.max(itemText.implicitWidth + teleActivateBtn.width + VLCStyle.margin_small,
                            parent.width)

            height: teleActivateBtn.height

            Widgets.SubtitleLabel {
                id: itemText

                text: I18n.qtr("Teletext")

                color: root.colors.text
            }

            ControlCheckButton {
                id: teleActivateBtn

                anchors.right: parent.right

                anchors.verticalCenter: parent.verticalCenter

                focus: true

                checked: Player.teletextEnabled

                colors: root.colors

                Navigation.parentItem: root
                Navigation.rightItem: teleTransparencyBtn
                Navigation.downItem: teleTransparencyBtn

                onCheckedChanged: Player.teletextEnabled = checked
            }
        }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right

            spacing: VLCStyle.margin_small

            Widgets.IconControlButton{
                id: teleTransparencyBtn

                enabled: teleActivateBtn.checked

                checked: Player.teletextTransparency

                iconText: VLCIcons.transparency
                text: I18n.qtr("Teletext transparency")

                colors: root.colors

                T.ToolTip.visible: (hovered || visualFocus)

                Navigation.parentItem: root
                Navigation.leftItem: teleActivateBtn
                Navigation.rightItem: telePageNumber
                Navigation.upItem: teleActivateBtn
                Navigation.downItem: indexKeyBtn

                onClicked: Player.teletextTransparency = !Player.teletextTransparency
            }

            Widgets.SpinBoxExt{
                id: telePageNumber

                Layout.fillWidth: true

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
                Navigation.upItem: teleActivateBtn
                Navigation.downItem: indexKeyBtn

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
        }

        Row {
            spacing: VLCStyle.margin_small

            Widgets.IconControlButton{
                id: indexKeyBtn

                anchors.verticalCenter: parent.verticalCenter

                width: redKeyBtn.width

                size: VLCStyle.dp(32, VLCStyle.scale)

                enabled: teleActivateBtn.checked

                iconText: VLCIcons.home
                text: I18n.qtr("Index key")

                colors: root.colors

                T.ToolTip.visible: (hovered || visualFocus)

                Navigation.parentItem: root
                Navigation.leftItem: telePageNumber
                Navigation.rightItem: redKeyBtn
                Navigation.upItem: teleTransparencyBtn

                onClicked: Player.teletextPage = Player.TELE_INDEX
            }

            TeletextColorButton {
                id: redKeyBtn

                anchors.verticalCenter: parent.verticalCenter

                enabled: teleActivateBtn.checked

                text: I18n.qtr("Red key")

                colors: root.colors
                color: root._teletextButtonColor(this, "red")

                Navigation.parentItem: root
                Navigation.leftItem: indexKeyBtn
                Navigation.rightItem: greenKeyBtn
                Navigation.upItem: teleTransparencyBtn

                onClicked: Player.teletextPage = Player.TELE_RED
            }

            TeletextColorButton {
                id: greenKeyBtn

                anchors.verticalCenter: parent.verticalCenter

                enabled: teleActivateBtn.checked

                text: I18n.qtr("Green key")

                colors: root.colors
                color: root._teletextButtonColor(this, "green")

                Navigation.parentItem: root
                Navigation.leftItem: redKeyBtn
                Navigation.rightItem: yellowKeyBtn
                Navigation.upItem: teleTransparencyBtn

                onClicked: Player.teletextPage = Player.TELE_GREEN
            }

            TeletextColorButton {
                id: yellowKeyBtn

                anchors.verticalCenter: parent.verticalCenter

                enabled: teleActivateBtn.checked

                text: I18n.qtr("Yellow key")

                colors: root.colors
                color: root._teletextButtonColor(this, "yellow")

                Navigation.parentItem: root
                Navigation.leftItem: greenKeyBtn
                Navigation.rightItem: blueKeyBtn
                Navigation.upItem: teleTransparencyBtn

                onClicked: Player.teletextPage = Player.TELE_YELLOW
            }

            TeletextColorButton {
                id: blueKeyBtn

                anchors.verticalCenter: parent.verticalCenter

                enabled: teleActivateBtn.checked

                text: I18n.qtr("Blue key")

                colors: root.colors
                color: root._teletextButtonColor(this, "blue")

                Navigation.parentItem: root
                Navigation.leftItem: yellowKeyBtn
                Navigation.upItem: teleTransparencyBtn

                onClicked: Player.teletextPage = Player.TELE_BLUE
            }
        }
    }
}
