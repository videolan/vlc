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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.Button {
    id: control

    // Properties

    property bool selected: false

    property bool busy: false

    property string iconTxt: ""

    property int iconSize: VLCStyle.icon_normal

    property color color: VLCStyle.colors.text

    // Aliases

    property alias backgroundColor: background.backgroundColor
    property alias foregroundColor: background.foregroundColor

    property alias colorFocus: background.activeBorderColor

    property alias iconRotation: icon.rotation

    // Settings

    width: implicitWidth
    height: implicitHeight

    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight

    padding: 0

    text: model.displayText

    font.pixelSize: VLCStyle.fontSize_normal

    // Keys

    Keys.priority: Keys.AfterItem

    Keys.onPressed: Navigation.defaultKeyAction(event)

    // Childs

    background: Widgets.AnimatedBackground {
        id: background

        height: control.height
        width: control.width

        active: visualFocus

        foregroundColor: (hovered) ? VLCStyle.colors.buttonTextHover
                                   : VLCStyle.colors.buttonBanner

        activeBorderColor: VLCStyle.colors.bgFocus
    }

    contentItem: Item {
        implicitWidth: tabRow.implicitWidth
        implicitHeight: tabRow.implicitHeight

        Row {
            id: tabRow

            anchors.fill: parent

            padding: VLCStyle.margin_xsmall
            spacing: VLCStyle.margin_xsmall

            Item {
                width: implicitWidth
                height: implicitHeight

                implicitWidth: VLCStyle.fontHeight_normal
                implicitHeight: VLCStyle.fontHeight_normal

                visible: (control.iconTxt !== "") || control.busy

                Widgets.IconLabel {
                    id: icon

                    anchors.fill: parent

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    visible: (!control.busy)

                    text: control.iconTxt

                    color: background.foregroundColor

                    font.pixelSize: control.iconSize
                }

                // FIXME: use Control.Templates
                BusyIndicator {
                    anchors.fill: parent

                    padding: 0

                    running: control.busy
                }
            }

            Widgets.ListLabel {
                text: control.text

                color: background.foregroundColor
            }
        }

        Rectangle {
            anchors.left: tabRow.left
            anchors.right: tabRow.right
            anchors.bottom: tabRow.bottom

            height: 2

            visible: control.selected

            color: "transparent"

            border.color: VLCStyle.colors.accent
        }
    }
}
