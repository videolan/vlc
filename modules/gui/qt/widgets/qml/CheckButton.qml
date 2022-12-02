/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1

import "qrc:///style/"

T.Switch {
    id: root

    // Style

    property int animationDuration: VLCStyle.duration_long

    property color color: (checked) ? theme.bg.secondary
                                    : theme.bg.primary

    property color colorHandle: (checked) ? theme.fg.secondary
                                          : theme.fg.primary


    property color colorBorder:  (checked) ? "transparent"
                                           : theme.border

    // Private

    property bool _update: true

    // Settings

    width : VLCStyle.checkButton_width
    height: VLCStyle.checkButton_height

    T.ToolTip.visible: (T.ToolTip.text && (hovered || visualFocus))

    T.ToolTip.text: text

    T.ToolTip.delay: VLCStyle.delayToolTipAppear

    // Keys

    Keys.priority: Keys.AfterItem

    Keys.onPressed: Navigation.defaultKeyAction(event)

    // Events

    onWidthChanged: {
        animation.stop()

        if (checked)
            handle.x = handle.drag.maximumX
        else
            handle.x = 0
    }

    onCheckedChanged: {
        if (_update === false)
            return

        var from = handle.x
        var to

        if (checked)
            to = handle.drag.maximumX
        else
            to = 0

        _animate(from, to)
    }

    // Functions

    // Private

    function _applyX(x) {
        var from = handle.x
        var to

        _update = false

        if (x < width / 2) {
            to = 0

            checked = false
        } else {
            to = handle.drag.maximumX

            checked = true
        }

        _update = true

        _animate(from, to)
    }

    function _animate(from, to) {
        if (from === to)
            return

        animation.from = from
        animation.to   = to

        animation.restart()
    }

    // Animations

    NumberAnimation {
        id: animation

        target: handle

        property: "x"

        duration: VLCStyle.duration_short

        easing.type: Easing.OutQuad
    }

    // Children

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Switch

        enabled: root.enabled
        focused: root.visualFocus
        hovered: root.hovered
        pressed: root.down
    }

    background: AnimatedBackground {
        active: root.visualFocus
        animate: theme.initialized
        activeBorderColor: theme.visualFocus
    }

    indicator: Rectangle {
        anchors.fill: parent

        anchors.margins: VLCStyle.checkButton_margins

        radius: height

        color: root.color

        border.color: root.colorBorder
        border.width: VLCStyle.border

        MouseArea {
            id: handle

            width: Math.round(root.height - VLCStyle.checkButton_margins * 2)
            height: width

            drag.target: handle
            drag.axis  : Drag.XAxis

            drag.minimumX: 0
            drag.maximumX: parent.width - width

            onClicked: root.toggle()

            // NOTE: We update the position when the drag has ended.
            drag.onActiveChanged: if (drag.active === false) root._applyX(x + width / 2)

            Rectangle {
                anchors.fill: parent

                anchors.margins: VLCStyle.checkButton_handle_margins

                radius: height

                color: root.colorHandle
            }
        }
    }
}
