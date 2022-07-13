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
import QtQuick.Templates 2.4 as T

import "qrc:///widgets/" as Widgets

import "qrc:///style/"

T.ToolButton {
    id: control

    property url imageSource: ""

    property bool paintOnly: false

    property size sourceSize: Qt.size(VLCStyle.icon_normal, VLCStyle.icon_normal)

    // background colors
    // NOTE: We want the background to be transparent for IconToolButton(s).
    property color backgroundColor: "transparent"
    property color backgroundColorHover: "transparent"

    property color color: VLCStyle.colors.icon
    property color colorHover: VLCStyle.colors.buttonTextHover
    property color colorHighlighted: VLCStyle.colors.accent
    property color colorDisabled: paintOnly ? color : VLCStyle.colors.textInactive
    property alias colorFocus: background.activeBorderColor


    padding: 0

    enabled: !paintOnly

    implicitWidth: control.sourceSize.width + leftPadding + rightPadding
    implicitHeight: control.sourceSize.height + topPadding + bottomPadding
    baselineOffset: contentItem.y + contentItem.baselineOffset


    Keys.priority: Keys.AfterItem
    Keys.onPressed: Navigation.defaultKeyAction(event)

    background: AnimatedBackground {
        id: background

        width: control.sourceSize.width
        height: control.sourceSize.height

        active: control.visualFocus

        backgroundColor: {
            if (control.hovered)
                return control.backgroundColorHover;
            // if base color is transparent, animation starts with black color
            else if (control.backgroundColor.a === 0)
                return VLCStyle.colors.setColorAlpha(control.backgroundColorHover, 0);
            else
                return control.backgroundColor;
        }

        foregroundColor: {
            if (control.highlighted)
                return control.colorHighlighted;
            else if (control.hovered)
                return control.colorHover;
            else if (!control.enabled)
                return control.colorDisabled;
            else
                return control.color;
        }

        activeBorderColor: VLCStyle.colors.bgFocus
    }

    contentItem: Image {
        anchors.centerIn: control

        source: control.imageSource

        fillMode: Image.PreserveAspectFit

        width: control.sourceSize.width
        height: control.sourceSize.height
        sourceSize: control.sourceSize

    }

}
