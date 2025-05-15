/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

// This item uses Evan Wallace's Fast Rounded Rectangle Shadow (CC0).
// This is considered a better approach than any texture based approach (whether GPU or CPU generated).
// Regarding non-texture based solutions, it has the advantage over Qt 6.9's `RectangularShadow` or
// Qt 5's `RectangularGlow` that this uses proper approximation of gaussian blur instead of relying on
// `smoothstep()`, which I assume is good for glow effect but not for shadows.
ShaderEffect {
    implicitWidth: parent ? Math.min((parent.paintedWidth ?? Number.MAX_VALUE) - Math.ceil(parent.padding ?? 0) * 2, parent.width) + (blurRadius * compensationFactor * 2)
                          : 0
    implicitHeight: parent ? Math.min((parent.paintedHeight ?? Number.MAX_VALUE) - Math.ceil(parent.padding ?? 0) * 2, parent.height) + (blurRadius * compensationFactor * 2)
                           : 0

    z: -1

    anchors.centerIn: parent
    anchors.horizontalCenterOffset: xOffset
    anchors.verticalCenterOffset: yOffset

    // Any of these properties can be freely animated:
    property real blurRadius: 0.0
    property color color

    // These are only respected when anchoring is used to center the item:
    property real xOffset
    property real yOffset

    // Currently different xRadius/yRadius is not supported.
    property real radius: (parent ? (parent.effectiveRadius ?? parent.radius) : 0) ?? 0

    readonly property size size: Qt.size(width, height)

    // Increase the compensation factor if clipping occurs, but make it as small as possible
    // to prevent overlapping shadows (breaks batching) and to decrease the shader coverage:
    property real compensationFactor: 2.0

    // Do not paint in the non-compensated inner area - only makes sense if there is compensation:
    property bool hollow: false

    blending: true

    supportsAtlasTextures: true // irrelevant, but nevertheless...

    // cullMode: ShaderEffect.BackFaceCulling // problematic with item layers

    fragmentShader: hollow ? "qrc:///shaders/RoundedRectangleShadow_hollow.frag.qsb"
                           : "qrc:///shaders/RoundedRectangleShadow.frag.qsb"
}
