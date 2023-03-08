/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

FocusScope {
    id: root

    // Aliases

    default property alias contents: column.data

    property alias spacing: column.spacing

    property alias cover: cover.source

    property alias coverWidth: coverContainer.width
    property alias coverHeight: coverContainer.height

    property alias text: label.text

    property alias column: column

    Accessible.role: Accessible.Pane
    Accessible.name: I18n.qtr("Empty view")

    // Children

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Column {
        id: column

        anchors.verticalCenter: parent.verticalCenter

        width: root.width

        spacing: VLCStyle.margin_small

        Item {
            width: parent.width
            height: label.y + label.height

            Item {
                id: coverContainer

                anchors.horizontalCenter: parent.horizontalCenter

                width: VLCStyle.colWidth(1)
                height: VLCStyle.colWidth(1)

                Image {
                    id: cover

                    anchors.fill: parent

                    asynchronous: true

                    fillMode: Image.PreserveAspectFit
                }

                Widgets.ListCoverShadow {
                    anchors.fill: cover
                }
            }

            T.Label {
                id: label

                anchors.top: coverContainer.bottom

                anchors.topMargin: VLCStyle.margin_large

                width: parent.width

                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                focus: false

                wrapMode: Text.WordWrap

                color: theme.fg.primary

                font.pixelSize: VLCStyle.fontSize_xxlarge
                font.weight: Font.DemiBold
            }
        }
    }
}
