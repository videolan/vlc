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

import "qrc:///style/"

Item {
    id: control

    readonly property alias scrolling: scrollAnimation.running

    // `label`: label to scroll, don't add horizontal anchors on it
    property Text label: undefined
    property bool forceScroll: false
    property alias hoverScroll: hoverArea.enabled


    readonly property real requiredTextWidth: label.implicitWidth
    readonly property bool _needsToScroll: (label.width < requiredTextWidth)

    ToolTip.delay: VLCStyle.delayToolTipAppear
    ToolTip.visible: scrolling && hoverArea.containsMouse
    ToolTip.text: label.text

    onLabelChanged: {
        label.width = Qt.binding(function () { return Math.min(label.implicitWidth, control.width) })

        label.elide = Qt.binding(function () {
            return control.scrolling ? Text.ElideNone : Text.ElideRight
        })
    }

    MouseArea {
        id: hoverArea

        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        cursorShape: undefined
        hoverEnabled: true
    }

    SequentialAnimation {
        id: scrollAnimation

        running: (control.forceScroll || hoverArea.containsMouse) && control._needsToScroll
        loops: Animation.Infinite

        onStopped: {
            label.x = 0
        }

        PauseAnimation {
            duration: VLCStyle.duration_veryLong
        }

        SmoothedAnimation {
            target: label
            property: "x"
            from: 0
            to: label.width - control.requiredTextWidth

            maximumEasingTime: 0
            velocity: 20
        }

        PauseAnimation {
            duration: VLCStyle.duration_veryLong
        }

        PropertyAction {
            target: label
            property: "x"
            value: 0
        }
    }
}

