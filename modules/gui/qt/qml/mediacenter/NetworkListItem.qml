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
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4

import "qrc:///utils/" as Utils
import "qrc:///style/"

Rectangle {
    id: root
    signal indexClicked
    signal itemClicked(int keys, int modifier)
    signal itemDoubleClicked(int keys, int modifier)

    property alias hovered: mouse.containsMouse

    property Component cover: Item {}
    property alias line1: line1_text.text
    property alias line2: line2_text.text

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            root.itemClicked(mouse.buttons, mouse.modifiers);
        }
        onDoubleClicked: {
            root.itemDoubleClicked(mouse.buttons, mouse.modifiers);
        }
    }

    RowLayout {
        anchors.fill: parent
        Loader {
            Layout.preferredWidth: VLCStyle.icon_normal
            Layout.preferredHeight: VLCStyle.icon_normal
            sourceComponent: root.cover
        }
        Column {
            Text{
                id: line1_text
                font.bold: true
                elide: Text.ElideRight
                color: VLCStyle.colors.text
                font.pixelSize: VLCStyle.fontSize_normal
                enabled: text !== ""
            }
            Text{
                id: line2_text
                text: ""
                elide: Text.ElideRight
                color: VLCStyle.colors.text
                font.pixelSize: VLCStyle.fontSize_xsmall
                enabled: text !== ""
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Utils.ImageToolButton {
            id: indexButton
            visible: model.can_index
            Layout.preferredHeight: VLCStyle.icon_normal
            Layout.preferredWidth: VLCStyle.icon_normal
            imageSource: !model.indexed ? "qrc:///buttons/playlist/playlist_add.svg" :
                ((mouse.containsMouse || activeFocus) ? "qrc:///toolbar/clear.svg" :
                                       "qrc:///valid.svg" )
            onClicked: {
                root.indexClicked(mouse.buttons, mouse.modifiers);
            }
        }
    }
}
