/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

import VLC.Style
import VLC.Util
import VLC.Widgets as Widgets

// This item can be used as a layer effect.
// Make sure that the sampler name is set to "source" (default).
Widgets.DualKawaseBlur {
    id: root

    radius: 3

    property color tint: "transparent"
    property real tintStrength: Qt.colorEqual(tint, "transparent") ? 0.0 : 0.7
    property real noiseStrength: 0.02

    // Have a semi-opaque filter blended with the source.
    // This is intended to act as both colorization (tint),
    // and exclusion effects.
    Rectangle {
        id: filter

        // Underlay for the blur effect:
        parent: root.source?.sourceItem ?? root.source

        // Since we don't use layering for the effect area anymore,
        // we need to restrict the filter to correspond to the effect
        // area instead of the whole source:
        x: root.x
        y: root.y
        width: root.width
        height: root.height

        z: 999

        visible: root.tintStrength > 0.0

        color: Qt.alpha(root.tint, root.tintStrength)
    }

    ShaderEffect {
        id: noise

        // Overlay for the blur effect:
        anchors.fill: parent
        z: 999

        visible: root.noiseStrength > 0.0

        readonly property real strength: root.noiseStrength

        fragmentShader: "qrc:///shaders/Noise.frag.qsb"
    }
}
