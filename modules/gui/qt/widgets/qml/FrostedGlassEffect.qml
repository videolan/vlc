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
import Qt5Compat.GraphicalEffects

import VLC.Style
import VLC.Util

// This item can be used as a layer effect.
// Make sure that the sampler name is set to "source" (default).
FastBlur {
    id: root

    radius: 64

    property bool blending: false

    property color tint: "transparent"
    property real tintStrength: Qt.colorEqual(tint, "transparent") ? 0.0 : 0.7
    property real noiseStrength: 0.02
    property real exclusionStrength: 0.09

    layer.enabled: true
    layer.effect: ShaderEffect {
        readonly property color tint: root.tint
        readonly property real tintStrength: root.tintStrength
        readonly property real noiseStrength: root.noiseStrength
        readonly property real exclusionStrength: root.exclusionStrength

        cullMode: ShaderEffect.BackFaceCulling

        blending: root.blending

        fragmentShader: "qrc:///shaders/FrostedGlass.frag.qsb"
    }
}
