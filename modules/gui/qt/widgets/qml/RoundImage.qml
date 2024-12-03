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

Item {
    id: root

    readonly property real implicitWidth: image.implicitHeight
    readonly property real implicitHeight: image.implicitHeight

    // Might need this as we have overridden implicit sizes as readonly
    height: root.implicitHeight
    width: root.implicitWidth

    asynchronous: true

    property alias asynchronous: image.asynchronous
    property alias source: image.source
    property alias sourceSize: image.sourceSize
    property alias status: image.status
    property alias cache: image.cache
    // fillMode is not reflected in the texture,
    // so it is not provided as an alias here

    property real radius

    // NOTE: Note the distinction between ShaderEffect and
    //       ShaderEffectSource. ShaderEffect is no different
    //       than any other item, including Image. ShaderEffectSource
    //       on the other hand breaks batching, uses video memory
    //       to store the texture of the source item that are
    //       (re)rendered in a framebuffer or offscreen surface.
    ShaderEffect {
        id: shaderEffect

        anchors.fill: parent

        visible: readyForVisibility

        readonly property bool readyForVisibility: (root.radius > 0.0) && (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

        supportsAtlasTextures: true

        blending: true

        // FIXME: Culling seems to cause issues, such as when the view is layered due to
        //        fading edge effec, this is most likely a Qt bug.
        // cullMode: ShaderEffect.BackFaceCulling

        readonly property real radius: Math.min(1.0, Math.max(root.radius / (Math.min(width, height) / 2), 0.0))
        readonly property real radiusTopRight: radius
        readonly property real radiusBottomRight: radius
        readonly property real radiusTopLeft: radius
        readonly property real radiusBottomLeft: radius

        readonly property size size: Qt.size(width, height)

        readonly property double softEdgeMin: -0.01
        readonly property double softEdgeMax:  0.01

        // QQuickImage as texture provider, no need for ShaderEffectSource.
        // In this case, we simply ask the Image to provide its texture,
        // so that we can make use of our custom shader.
        readonly property Image source: image

        fragmentShader: "qrc:///shaders/SDFAARoundedTexture.frag.qsb"
    }

    Image {
        id: image

        anchors.fill: parent

        // Image should not be visible when there is rounding and RHI shader is supported.
        // This is simply when the shader effect is invisible. However, Do not use `!shaderEffect.visible`,
        // because the root item may be invisible (`grabToImage()` call on an invisible
        // item case). In that case, shader effect would report invisible although it
        // would appear in the grabbed image.
        visible: !shaderEffect.readyForVisibility

        fillMode: Image.PreserveAspectCrop
    }
}
