/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

RowLayout {
    id: root

    // Properties

    default property alias content: content.data

    property int preferredWidth: VLCStyle.dp(512, VLCStyle.scale)

    // Settings

    spacing: 0

    focus: true

    Navigation.leftItem: button

    // Signals

    signal backRequested

    // Children

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Item {
        Layout.preferredWidth: VLCStyle.dp(72, VLCStyle.scale)
        Layout.fillHeight: true

        Layout.topMargin: VLCStyle.margin_large

        Layout.alignment: Qt.AlignLeft | Qt.AlignTop

        Widgets.IconTrackButton {
            id: button

            anchors.horizontalCenter: parent.horizontalCenter

            text: I18n.qtr("Back")
            iconText: VLCIcons.back

            Navigation.parentItem: root
            Navigation.rightItem: content

            onClicked: root.backRequested()
        }
    }

    Rectangle {
        Layout.preferredWidth: VLCStyle.margin_xxxsmall
        Layout.fillHeight: true

        color: theme.border
    }

    FocusScope {
        id: content

        Layout.fillWidth: true
        Layout.fillHeight: true

        Layout.margins: VLCStyle.margin_large
    }
}
