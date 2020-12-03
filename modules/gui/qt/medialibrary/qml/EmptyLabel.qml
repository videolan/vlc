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
import QtQuick.Controls 2.4
import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper

Widgets.NavigableFocusScope {
    id: root

    property alias text: label.text
    property alias showBrowseButton: browseButton.visible
    property alias cover: cover.source
    property alias coverWidth: coverContainer.width
    property alias coverHeight: coverContainer.height

    Column {
        anchors.verticalCenter: parent.verticalCenter
        width: root.width
        spacing: VLCStyle.margin_large

        Item {
            id: coverContainer

            anchors.horizontalCenter: parent.horizontalCenter
            width: VLCStyle.colWidth(1)
            height: VLCStyle.colWidth(1)

            Image {
                id: cover

                asynchronous: true
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
            }

            Widgets.ListCoverShadow {
                anchors.fill: cover
                source: cover
            }
        }

        Label {
            id: label

            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: VLCStyle.fontSize_xxlarge
            font.weight: Font.DemiBold
            color:  VLCStyle.colors.text
            wrapMode: Text.WordWrap
            focus: false
        }

        Widgets.TabButtonExt {
            id: browseButton

            text: i18n.qtr("Browse")
            focus: true
            iconTxt: VLCIcons.topbar_network
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: history.push(["mc", "network"])
            width: VLCStyle.dp(84, VLCStyle.scale)
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)
    Keys.onReleased: {
        if (KeyHelper.matchOk(event)) {
            history.push(["mc", "network"])
        }
        defaultKeyReleaseAction(event, 0)
    }
}
