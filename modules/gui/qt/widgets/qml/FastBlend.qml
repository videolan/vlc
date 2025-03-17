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

// Premultiplied color allows making use of various blending modes (with restrictions)
// without adjusting the pipeline state. This is not magic, but simple mathematics.
// For this to work, the graphics pipeline blend mode is assumed to be source-over
// which is the default mode of Qt Scene Graph.
ShaderEffect {
    id: root

    enum Mode {
        // default (S + D * (1 - S.a)), no restriction:
        SourceOver,
        // Additive (S + D), no restriction:
        Additive,
        // Multiply (S * D), only grayscale:
        Multiply,
        // Screen / Inverse Multiply (S + D - S * D), only grayscale:
        Screen
    }

    property int mode: FastBlend.Mode.SourceOver

    property color color
    readonly property real grayscale: {
        if (mode !== FastBlend.Mode.Multiply && mode !== FastBlend.Mode.Screen)
            return 0.0 // no need to calculate
        // Color to gray, `qGray()` algorithm:
        return (root.color.r * 11 + root.color.g * 16 + root.color.b * 5) / 32
    }

    blending: true
    supportsAtlasTextures: true // irrelevant for now. Do we want to support texture here? It should be trivial.

    z: 99 // this is assumed to be the source, which means that it needs to be on top of the destination

    fragmentShader: {
        switch (mode) {
        case FastBlend.Mode.Additive:
            return "qrc:///shaders/FastBlend_additive.frag.qsb"
        case FastBlend.Mode.Multiply:
            return "qrc:///shaders/FastBlend_multiply.frag.qsb"
        case FastBlend.Mode.Screen:
            return "qrc:///shaders/FastBlend_screen.frag.qsb"
        default:
            return "qrc:///shaders/FastBlend.frag.qsb"
        }
    }
}
