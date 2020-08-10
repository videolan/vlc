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
import QtGraphicalEffects 1.0

import "qrc:///style/"

Item {
    id: pointingTooltip

    // set parentWindow if you want to let tooltip not exceed window boundaries
    // if it is not set, tooltip will use mouseArea as the bounding rect.
    // Note that for now it only works with x axis.
    property var parentWindow: undefined

    property var mouseArea: undefined

    property alias text: timeMetrics.text

    // set fixedY if you want to fix y position of the tooltip
    property bool fixedY: true

    readonly property real position: xPos / mouseArea.width
    readonly property real xPos: mouseArea.mouseX
    readonly property real yPos: mouseArea.mouseY

    width: childrenRect.width
    height: childrenRect.height

    function getX() {
        var x = xPos - (pointingTooltip.width / 2)
        var diff = (x + pointingTooltip.width)

        var windowMappedX = !!parentWindow ? parentWindow.mapFromItem(mouseArea, mouseArea.x, mouseArea.y).x : undefined

        var sliderRealX = 0

        if (!!parentWindow) {
            diff -= parentWindow.width - windowMappedX
            sliderRealX = windowMappedX
        }
        else
            diff -= mouseArea.width

        if (x < -sliderRealX) {
            if (!!parentWindow)
                arrow.diff = x + windowMappedX
            else
                arrow.diff = x
            x = -sliderRealX
        }
        else if (diff > 0) {
            arrow.diff = diff
            x -= (diff)
        }
        else {
            arrow.diff = 0
        }

        return x
    }

    y: fixedY ? -(childrenRect.height) : yPos - childrenRect.height
    x: getX()

    Item {
        height: arrow.height * Math.sqrt(2)
        width: timeIndicatorRect.width

        anchors.horizontalCenter: timeIndicatorRect.horizontalCenter
        anchors.verticalCenter: timeIndicatorRect.bottom
        anchors.verticalCenterOffset: height / 2

        clip: true

        Rectangle {
            id: arrow
            width: VLCStyle.dp(10)
            height: VLCStyle.dp(10)

            anchors.centerIn: parent
            anchors.verticalCenterOffset: -(parent.height / 2)
            anchors.horizontalCenterOffset: diff

            property int diff: 0

            color: VLCStyle.colors.bgAlt

            rotation: 45

            RectangularGlow {
                anchors.fill: parent
                glowRadius: VLCStyle.dp(2)
                spread: 0.2
                color: VLCStyle.colors.glowColor
            }
        }
    }

    Rectangle {
        id: timeIndicatorRect
        width: timeMetrics.width + VLCStyle.dp(10)
        height: timeMetrics.height + VLCStyle.dp(5)

        color: VLCStyle.colors.bgAlt
        radius: VLCStyle.dp(6)

        RectangularGlow {
            anchors.fill: parent

            glowRadius: VLCStyle.dp(2)
            cornerRadius: parent.radius
            spread: 0.2

            color: VLCStyle.colors.glowColor
        }

        Text {
            anchors.fill: parent
            text: timeMetrics.text
            color: VLCStyle.colors.text

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            TextMetrics {
                id: timeMetrics
            }
        }
    }
}
