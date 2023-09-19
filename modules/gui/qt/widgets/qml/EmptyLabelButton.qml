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

import QtQuick
import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

EmptyLabel {
    id: root

    // Properties

    property bool _keyPressed: false

    // Aliases

    property alias button: button

    // Functions

    function onNavigate(reason) {
        History.push(["mc", "network"], reason)
    }

    // Keys

    Keys.priority: Keys.AfterItem

    Keys.onPressed: (event) => {
        _keyPressed = true

        Navigation.defaultKeyAction(event)
    }

    Keys.onReleased: (event) => {
        if (_keyPressed === false)
            return

        _keyPressed = false

        if (KeyHelper.matchOk(event))
            onNavigate(Qt.TabFocusReason)

        Navigation.defaultKeyReleaseAction(event)
    }

    // Children

    Widgets.ButtonExt {
        id: button

        anchors.horizontalCenter: parent.horizontalCenter

        width: Math.max(VLCStyle.dp(84, VLCStyle.scale), implicitWidth)

        focus: true

        text: qsTr("Browse")
        iconTxt: VLCIcons.topbar_network

        Navigation.parentItem: root

        onClicked: onNavigate(Qt.OtherFocusReason)
    }
}
