/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
    id: root

    property real leftPadding: 0
    property real topPadding: 0
    property real coverMargins: 0
    property real coverWidth: 0
    property real coverHeight: 0
    property real coverRadius: 0
    property color coverColor: VLCStyle.colors.bg

    property alias primaryVerticalOffset: primaryShadow.verticalOffset
    property alias primaryRadius: primaryShadow.radius
    property alias primarySamples: primaryShadow.samples
    property alias secondaryVerticalOffset: secondaryShadow.verticalOffset
    property alias secondaryRadius: secondaryShadow.radius
    property alias secondarySamples: secondaryShadow.samples

    readonly property real _kernalRadius: Math.max(0, Math.ceil(root.primarySamples / 2))
    property real _reference: 0

    property Component imageComponent: ShaderEffectSource {
        sourceItem: container
        live: true
        x: - root._kernalRadius + root.leftPadding
        y: - root._kernalRadius + root.primaryVerticalOffset + root.topPadding
        width: container.width
        height: container.height
        hideSource: true

        Component.onCompleted: ++root._reference;
        Component.onDestruction: --root._reference;
    }

    Item {
        id: container

        // if imageComponent is used with invisible container, generated shadows are too dark
        // another possible fix is to set DropShadow::cached = false, but that has performance penalty
        visible: root._reference > 0

        width: baseRect.width + 2 * root._kernalRadius
        height: baseRect.height + 2 * root._kernalRadius

        Rectangle {
            id: baseRect

            x: root._kernalRadius + root.coverMargins
            y: root._kernalRadius - root.primaryVerticalOffset + root.coverMargins
            width: root.coverWidth - root.coverMargins * 2
            height: root.coverHeight - root.coverMargins * 2
            radius: root.coverRadius
            color: root.coverColor
        }

        DropShadow {
            id: primaryShadow

            anchors.fill: baseRect
            source: baseRect
            horizontalOffset: 0
            spread: 0
            color: Qt.rgba(0, 0, 0, .22)
            samples: 1 + radius * 2
            cached: true
        }

        DropShadow {
            id: secondaryShadow

            anchors.fill: baseRect
            source: baseRect
            horizontalOffset: 0
            spread: 0
            color: Qt.rgba(0, 0, 0, .18)
            samples: 1 + radius * 2
            cached: true
        }
    }
}
