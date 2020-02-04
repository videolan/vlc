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

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Widgets.NavigableFocusScope {
    id: control
    height: implicitHeight
    width: implicitWidth
    implicitHeight: colLayout.implicitHeight + VLCStyle.margin_small * 2
    implicitWidth: colLayout.implicitWidth + VLCStyle.margin_large

    property alias text: txt.text
    property alias font: txt.font
    property alias color: txt.color

    property Component inlineComponent: Item {}
    property alias inlineItem: inlineComponentLoader.item

    ColumnLayout {
        id: colLayout
        anchors.fill: parent
        anchors.leftMargin: VLCStyle.margin_large
        anchors.topMargin: VLCStyle.margin_small
        anchors.bottomMargin: VLCStyle.margin_small

        RowLayout {
            id: rowLayout

            Layout.fillHeight: true
            Layout.preferredHeight: rowLayout.implicitHeight
            Layout.fillWidth: true

            Label {
                id: txt

                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignBottom

                font.pixelSize: VLCStyle.fontSize_xxlarge
                color: VLCStyle.colors.text
                font.weight: Font.Bold
                elide: Text.ElideRight
            }

            Loader {
                id: inlineComponentLoader
                Layout.preferredWidth: item.implicitWidth
                active: !!inlineComponent
                visible: active
                focus: true
                sourceComponent: inlineComponent
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: height
            height: VLCStyle.heightBar_xxxsmall

            radius: 2

            color: VLCStyle.colors.bgAlt
        }
    }


}
