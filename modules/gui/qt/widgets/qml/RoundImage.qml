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
    property alias sourceClipRect: image.sourceClipRect
    property alias status: image.status
    property alias cache: image.cache

    readonly property real paintedWidth: (shaderEffect.readyForVisibility) ? shaderEffect.width
                                                                           : (image.clip ? image.width : image.paintedWidth)
    readonly property real paintedHeight: (shaderEffect.readyForVisibility) ? shaderEffect.height
                                                                            : (image.clip ? image.height : image.paintedHeight)

    // NOTE: Fill mode is not guaranteed to be supported,
    //       it is supported to the extent QQuickImage
    //       provides properly filled texture, paintedWidth/Height,
    //       and applies attributes such as tiling (QSGTexture::Repeat)
    //       to the texture itself WHILE being invisible.
    //       Invisible items usually are not requested to
    //       update their paint node, so it is not clear if QQuickImage
    //       would synchronize QSGTexture attributes with its
    //       node when it is invisible (rounding is active).
    // NOTE: Experiments show that preserve aspect ratio can
    //       be supported, because QQuickImage provides
    //       appropriate painted size when it is invisible,
    //       and the generated texture is pre-filled. In
    //       the future, Qt Quick may prefer doing this
    //       within its shader, but for now, we should be
    //       able to use it.
    // NOTE: If you need a more guaranteed way to preserve
    //       the aspect ratio, you can use `sourceSize` with
    //       only one dimension set. Currently `fillMode` is
    //       preferred instead of `sourceSize` because we need
    //       to have control over both width and height.
    property alias fillMode: image.fillMode

    // We prefer preserve aspect fit by default, like in
    // playlist delegate, because preserve aspect ratio
    // crop needs `clip: true` to work properly, and it
    // it breaks batching and should not be used in a
    // delegate.
    fillMode: Image.PreserveAspectFit

    property real radius
    readonly property real effectiveRadius: (shaderEffect.readyForVisibility ?? shaderEffect.visible) ? radius : 0.0

    // NOTE: Note the distinction between ShaderEffect and
    //       ShaderEffectSource. ShaderEffect is no different
    //       than any other item, including Image. ShaderEffectSource
    //       on the other hand breaks batching, uses video memory
    //       to store the texture of the source item that are
    //       (re)rendered in a framebuffer or offscreen surface.
    ShaderEffect {
        id: shaderEffect

        anchors.alignWhenCentered: true
        anchors.centerIn: parent

        implicitWidth: image.implicitWidth
        implicitHeight: image.implicitHeight

        width: image.paintedWidth
        height: image.paintedHeight

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
    }
}
