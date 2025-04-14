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
import QtQuick.Window

ShaderEffect {
    id: effect

    readonly property size windowSize: {
        if (Window.window)
            // Currently only one dimension is taken into account (to not stretch)
            // in the shader, but still provide both for now. We use window size
            // instead of item (shader effect) size because the shader uses
            // gl_FragCoord:
            return Qt.size(Window.window.width, Window.window.height)
        return Qt.size(0, 0)
    }

    property color color: Qt.rgba(0.8, 0.8, 0.8, 0.4) // snowflake color

    property real time: 0.0 // seed

    property real speed: 1.2 // speed factor

    UniformAnimator on time {
        loops: Animation.Infinite
        from: 0
        to: 50
        duration: 100000 / effect.speed
    }

    fragmentShader: "qrc:///shaders/VoronoiSnow.frag.qsb"
}
