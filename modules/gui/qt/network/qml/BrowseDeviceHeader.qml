/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

FocusScope {
    id: root

    // Properties

    /* required */ property var view

    // Aliases

    property alias text: label.text

    property alias label: label
    property alias button: button

    // Signals

    signal clicked(int reason)

    // Settings

    width: view.width
    height: label.height

    Navigation.navigable: button.visible

    // Children

    RowLayout {
        id: row

        anchors.fill: parent

        anchors.leftMargin: root.view.contentMargin
        anchors.rightMargin: anchors.leftMargin

        Widgets.SubtitleLabel {
            id: label

            Layout.fillWidth: true

            topPadding: VLCStyle.margin_large
            bottomPadding: VLCStyle.margin_normal
        }

        Widgets.TextToolButton {
            id: button

            Layout.preferredWidth: implicitWidth

            focus: true

            text: I18n.qtr("See All")

            font.pixelSize: VLCStyle.fontSize_large

            Navigation.parentItem: root

            onClicked: root.clicked(focusReason)
        }
    }
}
