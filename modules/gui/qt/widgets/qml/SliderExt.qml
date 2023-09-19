/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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
import QtQuick.Templates as T

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers

T.Slider {
    id: control

    // helper to set colors based on given particular theme
    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.Slider

        enabled: control.enabled
        focused: control.visualFocus
        hovered: control.hovered
    }

    // colors based on different parts and states
    property color color: theme.fg.primary
    property color bgColor: theme.bg.primary

    property real handleWidth: VLCStyle.icon_xsmall

    property real radius: VLCStyle.dp(3, VLCStyle.scale)

    // when set the tooltip will follow the mouse when control is hoverred
    // else tooltip will always be shown at current value.
    property bool toolTipFollowsMouse: false

    property alias toolTip: toolTip

    // toolTipTextProvider -> function(value)
    // arg "value" is between from and to, this is "value"
    // at which pointing tool tip is currently shown
    //
    // returns the text for the given value
    property var toolTipTextProvider: function (value) {
        return value
    }

    readonly property real _tooltipX: {
        if (toolTipFollowsMouse && hoverHandler.hovered)
            return  hoverHandler.point.position.x

        return (handle.x + handle.width / 2) // handle center
    }

    // find position under given x, can be used with Slider::valueAt()
    // x is coordinate in this control's coordinate space
    function positionAt(x) {
        // taken from qt sources QQuickSlider.cpp
        // TODO: support vertical slider
        const hw = control.handle.width
        const offset = control.leftPadding + hw / 2
        const extend = control.availableWidth - hw
        return (x - offset) / extend
    }

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitHandleHeight + topPadding + bottomPadding)

    handle: Rectangle {
        x: control.leftPadding + (control.horizontal
                                  ? control.visualPosition * (control.availableWidth - width)
                                  : (control.availableWidth - width) / 2)

        y: control.topPadding + (control.horizontal
                                 ? (control.availableHeight - height) / 2
                                 : control.visualPosition * (control.availableHeight - height))

        implicitWidth: control.handleWidth
        implicitHeight: implicitWidth

        width: control.handleWidth
        height: width
        radius: width / 2

        color: control.color
        visible: (control.visualFocus || control.pressed) && control.enabled
    }

    background: Rectangle {
        x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)

        implicitWidth: control.horizontal ? VLCStyle.heightBar_xlarge : VLCStyle.heightBar_xxsmall
        implicitHeight: control.horizontal ? VLCStyle.heightBar_xxsmall : VLCStyle.heightBar_xlarge

        width: control.horizontal ? control.availableWidth : implicitWidth
        height: control.horizontal ? implicitHeight : control.availableHeight

        radius: control.radius

        color: control.bgColor
        scale: control.horizontal && control.mirrored ? -1 : 1

        Rectangle {
            y: control.horizontal ? 0 : control.visualPosition * parent.height
            width: control.horizontal ? control.position * parent.width : parent.height
            height: control.horizontal ? parent.height : control.position * parent.height

            radius: control.radius
            color: control.color
        }
    }

    HoverHandler {
        id: hoverHandler

        acceptedPointerTypes: PointerDevice.Mouse

        enabled: true

        target: background
    }

    PointingTooltip {
       id: toolTip

       z: 1 // without this tooltips get placed below root's parent popup (if any)

       pos: Qt.point(control._tooltipX, control.handle.height / 2)

       visible: hoverHandler.hovered || control.visualFocus || control.pressed

       text: {
           if (!visible) return ""

           // position is only measured till half of handle width
           // pos.x may go beyond the position at the edges
           const p = Helpers.clamp(control.positionAt(pos.x), 0.0, 1.0)
           const v = control.valueAt(p)
           return control.toolTipTextProvider(v)
       }

       //tooltip is a Popup, palette should be passed explicitly
       colorContext.palette: theme.palette
    }
}
