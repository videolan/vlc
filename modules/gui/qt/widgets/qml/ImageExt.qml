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

// NOTE: ImageExt behaves exactly like Image, except when at least one of these features are used:
//       - `PreserveAspectCrop` fill mode without requiring a clip node.
//       - Rounded rectangular shaping.
//       - Background coloring.
//       - Outlining (border).
//       - TODO: Custom padding.
// NOTE: Extra features are only available with the RHI graphics backend,
//       particularly when shaders are supported.
// NOTE: Do not use this type if none of the extra features are used.
// NOTE: This item is also a texture provider, but for technical reasons, it is through an anonymous
//       item exposed through `textureProviderItem`. Note that, postprocessing (any non-image/texture
//       level manipulation) is not going to be reflected in the texture (similar to `Image`). Unless
//       particularly specified, the extra features of `ImageExt` are postprocessing features.
Item {
    id: root

    // Implicit size used to be overridden as readonly, but that needed
    // binding width/height to the new properties which in turn caused
    // problems with layouts.
    implicitWidth: shaderEffect.readyForVisibility ? shaderEffect.implicitWidth : image.implicitWidth
    implicitHeight: shaderEffect.readyForVisibility ? shaderEffect.implicitHeight : image.implicitHeight

    // WARNING: We can not override QQuickItem's antialiasing
    //          property as readonly because Qt 6.6 marks it
    //          as `FINAL`...
    // FIXME: The shader can be generated without
    //        the define that enables antialiasing.
    //        It should be done when the build system
    //        starts supporting shader defines.
    antialiasing: true

    asynchronous: true

    property alias asynchronous: image.asynchronous
    property alias source: image.source
    property alias sourceSize: image.sourceSize
    property alias sourceClipRect: image.sourceClipRect
    property alias status: image.status
    property alias shaderStatus: shaderEffect.status
    property alias cache: image.cache

    // Normally `Image` itself is inherently a texture provider (without needing a layer), but
    // in `ImageExt` case, it is not. Obviously when `Image` is a texture provider, postprocess
    // manipulations (such as, `fillMode`, or `mirror`) would not be reflected in the texture.
    // The same applies here, any postprocessing feature such as rounding, background coloring,
    // outlining, would not be reflected in the texture. Still, we should make it possible to
    // expose a texture provider so that the texture can be accessed. Unfortunately QML does
    // not make it possible, but we can simply provide the `Image` here. I purposefully do
    // not use an alias property, because I do not want to expose the provider as an `Image`,
    // but rather as `Item`.
    // WARNING: Consumers who downcast this item to `Image` are doing this on their own
    //          discretion. It is discouraged, but not forbidden (or evil).
    readonly property Item textureProviderItem: image

    // Padding represents how much the content is shrunk. For now this is a readonly property.
    // Currently it only takes the `softEdgeMax` into calculation, as that's what the shader
    // uses to shrink to prevent "hard edges". Note that padding can only be calculated properly
    // when the shader has custom softedge support (`CUSTOM_SOFTEDGE`), currently it is used
    // at all times.
    readonly property real padding: (shaderEffect.readyForVisibility && antialiasing) ? (Math.max(shaderEffect.width, shaderEffect.height) / 4 * shaderEffect.softEdgeMax)
                                                                                      : 0.0

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
    //       able to use it. Currently, as of Qt 6.8,
    //       PreserveAspectCrop, PreserveAspectFit, and
    //       Stretch can be considered supported.
    // NOTE: If you need a more guaranteed way to preserve
    //       the aspect ratio, you can use `sourceSize` with
    //       only one dimension set. Currently `fillMode` is
    //       preferred instead of `sourceSize` because we need
    //       to have control over both width and height.
    property alias fillMode: image.fillMode

    // Unlike QQuickImage where it needs `clip: true` (clip node)
    // for `PreserveAspectCrop`, with the custom shader we do not
    // need that. This makes it feasible to use `PreserveAspectCrop`
    // in delegate, where we want to have effective batching. Note
    // that such option is still not free, because the fragment
    // shader has to do additional calculations that way.
    fillMode: Image.PreserveAspectFit

    property real radius
    property real radiusTopRight: radius
    property real radiusTopLeft: radius
    property real radiusBottomRight: radius
    property real radiusBottomLeft: radius

    property alias backgroundColor: shaderEffect.backgroundColor
    readonly property real effectiveRadius: shaderEffect.readyForVisibility ? Math.max(radiusTopRight, radiusTopLeft, radiusBottomRight, radiusBottomLeft) : 0.0
    readonly property color effectiveBackgroundColor: shaderEffect.readyForVisibility ? backgroundColor : "transparent"

    // Border:
    // NOTE: The border is an overlay for the texture (the
    //       texture does not shrink).
    // NOTE: Border uses source-over blending. Therefore if
    //       it is translucent, the image would get exposed.
    // NOTE: The unit of width is not specified. It is
    //       recommended to do only relative adjustments.
    property color borderColor: "black"
    property int borderWidth: 0
    readonly property int effectiveBorderWidth: shaderEffect.readyForVisibility ? borderWidth : 0

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

        implicitWidth: (image.status === Image.Ready) ? image.implicitWidth : 64
        implicitHeight: (image.status === Image.Ready) ? image.implicitHeight : 64

        width: ((image.status !== Image.Ready) || (image.fillMode === Image.PreserveAspectCrop)) ? root.width : image.paintedWidth
        height: ((image.status !== Image.Ready) || (image.fillMode === Image.PreserveAspectCrop)) ? root.height : image.paintedHeight

        visible: readyForVisibility

        readonly property bool readyForVisibility: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader) &&
                                                   (root.radius > 0.0 || root.borderWidth > 0 || backgroundColor.a > 0.0 || root.fillMode === Image.PreserveAspectCrop)

        smooth: root.smooth

        supportsAtlasTextures: true

        blending: true

        antialiasing: root.antialiasing

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        function normalizeRadius(radius: real) : real {
            return Math.min(1.0, Math.max(radius / (Math.min(width, height) / 2), 0.0))
        }

        readonly property real radiusTopRight: normalizeRadius(root.radiusTopRight)
        readonly property real radiusBottomRight: normalizeRadius(root.radiusBottomRight)
        readonly property real radiusTopLeft: normalizeRadius(root.radiusTopLeft)
        readonly property real radiusBottomLeft: normalizeRadius(root.radiusBottomLeft)

        property color backgroundColor: "transparent"

        readonly property size size: Qt.size(width, height)

        readonly property double softEdgeMin: -1. / Math.min(width, height)
        readonly property double softEdgeMax: -softEdgeMin

        readonly property size cropRate: {
            let ret = Qt.size(0.0, 0.0)

            // No need to calculate if PreserveAspectCrop is not used
            if (root.fillMode !== Image.PreserveAspectCrop)
                return ret

            // No need to calculate if image is not ready
            if (image.status !== Image.Ready)
                return ret

            const implicitScale = implicitWidth / implicitHeight
            const scale = width / height

            if (scale > implicitScale)
                ret.height = (implicitHeight - implicitWidth) / 2 / implicitHeight
            else if (scale < implicitScale)
                ret.width = (implicitWidth - implicitHeight) / 2 / implicitWidth

            return ret
        }

        // (2 / width) seems to be a good coefficient to make it similar to `Rectangle.border`:
        readonly property double borderRange: (image.status === Image.Ready) ? (root.borderWidth / width * 2.) : 0.0 // no need for outlining if there is no image (nothing to outline)
        readonly property color borderColor: root.borderColor

        // QQuickImage as texture provider, no need for ShaderEffectSource.
        // In this case, we simply ask the Image to provide its texture,
        // so that we can make use of our custom shader.
        readonly property Image source: image

        fragmentShader: (cropRate.width > 0.0 || cropRate.height > 0.0) || (root.borderWidth > 0) ? "qrc:///shaders/SDFAARoundedTexture_cropsupport_bordersupport.frag.qsb"
                                                                                                  : "qrc:///shaders/SDFAARoundedTexture.frag.qsb"

    }

    Image {
        id: image

        anchors.fill: parent

        smooth: root.smooth

        // Image should not be visible when there is rounding and RHI shader is supported.
        // This is simply when the shader effect is invisible. However, Do not use `!shaderEffect.visible`,
        // because the root item may be invisible (`grabToImage()` call on an invisible
        // item case). In that case, shader effect would report invisible although it
        // would appear in the grabbed image.
        visible: !shaderEffect.readyForVisibility

        // Clipping is not a big concern for rendering performance
        // with the software or openvg scene graph adaptations.
        // We can use clipping as QQuickImage suggests with
        // PreserveAspectCrop in that case:
        clip: (GraphicsInfo.shaderType !== GraphicsInfo.RhiShader) && (fillMode === Image.PreserveAspectCrop)

        antialiasing: root.antialiasing
    }
}
