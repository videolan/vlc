/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

import VLC.PlayerControls
import VLC.MainInterface
import VLC.Util
import VLC.Style
import VLC.Player
import VLC.Widgets as Widgets

VolumeWidget {
    id: root

    slider.implicitWidth: VLCStyle.dp(64, VLCStyle.scale)
    slider.opacity: 1.0
    slider.toolTipFollowsMouse: true
    slider.toolTipTextProvider: function (value) {
        return Math.round(value * 100) + "%"
    }
    slider.handle: null

    toolTipVisibilityBinding.when: false

    tickMark.visible: false

    sliderPointingToolTip.onTextChanged: {
        if ((sliderPointingToolTip.text !== volumeText.text) && !sliderPointingToolTip.visible && _hovered && !_pressed)
            toolTipVisibilityTimer.restart()
    }
    sliderPointingToolTip.visible: false

    // TODO: Implement `PointingToolTipAttached::delay`.
    Timer {
        id: toolTipVisibilityTimer

        interval: VLCStyle.delayToolTipAppear

        onTriggered: {
            root.slider.Widgets.PointingToolTipAttached.visible = true
        }
    }

    readonly property bool _hovered: (slider.hovered || mouseArea.containsMouse)

    on_HoveredChanged: {
        if (_hovered) {
            if (!_pressed) {
                toolTipVisibilityTimer.restart()
            }
        } else {
            toolTipVisibilityTimer.stop()
            slider.Widgets.PointingToolTipAttached.visible = false
        }
    }

    readonly property bool _pressed: (slider.pressed || mouseArea.pressed)
    on_PressedChanged: {
        if (_pressed) {
            toolTipVisibilityTimer.stop()
            sliderPointingToolTip.visible = false
        }
    }

    slider.background: Image {
        id: backgroundImage

        x: root.slider.leftPadding
        y: root.slider.topPadding + root.slider.availableHeight / 2 - height / 2

        width: root.slider.availableWidth
        height: root.button.height * 0.85 // Using `Slider`'s height causes polish loop

        cache: true
        fillMode: Image.Pad
        horizontalAlignment: Image.AlignLeft
        sourceSize: Qt.size(width, height)

        // WARNING: SVG does not support RGBA color.
        readonly property color opaqueBorder: (root.slider.colorContext.border.a < 1.0) ? (root.slider.colorContext.palette.isDark ? "darkgray"
                                                                                                                                   : "gray")
                                                                                        : root.slider.colorContext.border

        source: SVGColorImage.colorize("qrc:///misc/volume-slider-outside.svg")
                             .color1(opaqueBorder).uri() // foreground

        Widgets.AnimatedBackground {
            anchors.fill: parent
            anchors.margins: -VLCStyle.margin_xxsmall

            visible: (border.color.a > 0.0)
            enabled: root.slider.colorContext.initialized

            color: "transparent"

            border.color: root.slider.visualFocus ? root.slider.colorContext.visualFocus
                                                  : Qt.alpha(root.slider.colorContext.visualFocus, 0.0)
        }

        Image {
            z: -1 // Gradient is below parent

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left

            width: (parent.width * root.slider.position)

            clip: (root.slider.position < root.slider.to)

            cache: true
            fillMode: Image.Pad
            horizontalAlignment: Image.AlignLeft
            sourceSize: Qt.size(parent.width, height)

            source: Effects.url(Effects.VolumeSliderGradient, { "maximum": (root.slider.to * 100),
                                                                "sliderColors": MainCtx.volumeSliderColors,
                                                                "muted": root._player.muted })
        }

        Text {
            id: volumeText

            font.pixelSize: VLCStyle.fontSize_small
            textFormat: Text.PlainText

            x: 0
            y: -VLCStyle.margin_xxxsmall

            horizontalAlignment: Text.AlignRight

            text: {
                // the real value i.e 'Player.volume' bounds can be different
                // from bounds of this widget and we want to show the current
                // volume here, so directly use Player.volume instead of "value"

                return Math.round(Player.volume * 100) + "%"
            }

            color: backgroundImage.opaqueBorder // ###
        }
    }
}
