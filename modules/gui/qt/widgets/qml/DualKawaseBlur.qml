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

import VLC.Util

// This item provides the novel "Dual Kawase" effect [1], which offers a very ideal
// balance of quality and performance. It has been used by many applications since
// its introduction in 2015, including the KWin compositor. Qt 5's `FastBlur` from
// 2011, and its Qt 6 port `MultiEffect` already offers a performant blur effect,
// utilizing a similar down/up sampling trick, but it does not use the half-pixel
// trick, and does not work for textures in the atlas, or sub-textures (detaching
// or additional layer incurs an additional buffer).
// [1] SIGGRAPH 2015, "Bandwidth Efficient Rendering", Marius Bjorge (ARM).
Item {
    id: root

    implicitWidth: source ? Math.min(source.paintedWidth ?? Number.MAX_VALUE, source.width) : 0
    implicitHeight: source ? Math.min(source.paintedHeight ?? Number.MAX_VALUE, source.height) : 0

    enum Configuration {
        FourPass, // 2 downsample + 2 upsamples (3 layers/buffers)
        TwoPass // 1 downsample + 1 upsample (1 layer/buffer)
    }

    property int configuration: DualKawaseBlur.Configuration.FourPass

    // NOTE: This property is an optimization hint. When it is false, the result
    //       may be cached, and the intermediate buffers for the blur passes may
    //       be released.
    // TODO: This is pending implementation.
    property bool live: true

    // Do not hesitate to use an odd number for the radius, there is virtually
    // no difference between odd or even numbers due to the halfpixel trick.
    // The effective radius is always going to be a half-integer.
    property int radius: 1

    property bool blending: true

    // source must be a texture provider item. Some items such as `Image` and
    // `ShaderEffectSource` are inherently texture provider. Other items needs
    // layering with either `layer.enabled: true` or `ShaderEffectSource`.
    // We purposefully are not going to create a layer on behalf of the source
    // here, unlike `MultiEffect` (see `hasProxySource`), because it is impossible
    // to determine whether the new layer is actually wanted (when the source is
    // already a texture provider), and it is very trivial to have a layer when
    // it is wanted or necessary anyway.
    property Item source

    // Arbitrary sub-texturing (no need to be set for atlas textures):
    // `QSGTextureView` can also be used instead of sub-texturing here.
    property rect sourceRect

    ShaderEffect {
        id: ds1

        // When downsampled, we can decrease the size here so that the layer occupies less VRAM:
        width: parent.width / 2
        height: parent.height / 2

        readonly property Item source: root.source

        // TODO: Instead of normalizing here, we could use GLSL 1.30's `textureSize()`
        //       and normalize in the vertex shader, but we can not because we are
        //       targeting GLSL 1.20/ESSL 1.0, even though the shader is written in
        //       GLSL 4.40.
        readonly property rect normalRect: (root.sourceRect.width > 0.0 && root.sourceRect.height > 0.0) ? Qt.rect(root.sourceRect.x / sourceTextureSize.width,
                                                                                                                   root.sourceRect.y / sourceTextureSize.height,
                                                                                                                   root.sourceRect.width / sourceTextureSize.width,
                                                                                                                   root.sourceRect.height / sourceTextureSize.height)
                                                                                                         : Qt.rect(0.0, 0.0, 0.0, 0.0)

        readonly property int radius: root.radius

        // TODO: We could use `textureSize()` and get rid of this, but we
        //       can not because we are targeting GLSL 1.20/ESSL 1.0, even
        //       though the shader is written in GLSL 4.40.
        TextureProviderObserver {
            id: ds1SourceObserver
            source: ds1.source
        }

        readonly property size sourceTextureSize: ds1SourceObserver.textureSize

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        fragmentShader: "qrc:///shaders/DualKawaseBlur_downsample.frag.qsb"
        // Maybe we should have vertex shader unconditionally, and calculate the half pixel there instead of fragment shader?
        vertexShader: (normalRect.width > 0.0 && normalRect.height > 0.0) ? "qrc:///shaders/SubTexture.vert.qsb"
                                                                          : ""

        visible: false

        supportsAtlasTextures: true

        blending: root.blending
    }

    ShaderEffectSource {
        id: ds1layer

        sourceItem: ds1
        visible: false
        smooth: true
    }

    ShaderEffect {
        id: ds2

        // When downsampled, we can decrease the size here so that the layer occupies less VRAM:
        width: ds1.width / 2
        height: ds1.height / 2

        readonly property Item source: ds1layer
        property rect normalRect // not necessary here, added because of the warning
        readonly property int radius: root.radius

        // TODO: We could use `textureSize()` and get rid of this, but we
        //       can not because we are targeting GLSL 1.20/ESSL 1.0, even
        //       though the shader is written in GLSL 4.40.
        TextureProviderObserver {
            id: ds2SourceObserver
            source: ds2.source
        }

        readonly property size sourceTextureSize: ds2SourceObserver.textureSize

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        visible: false

        fragmentShader: "qrc:///shaders/DualKawaseBlur_downsample.frag.qsb"

        supportsAtlasTextures: true

        blending: root.blending
    }

    ShaderEffectSource {
        id: ds2layer

        // So that if configuration is two pass (this is not used), the buffer is released:
        // This is mainly relevant for switching configuration case, as initially if this was
        // never visible and was never used as texture provider, it should have never allocated
        // resources to begin with.
        sourceItem: (root.configuration === DualKawaseBlur.Configuration.FourPass) ? ds2 : null

        visible: false
        smooth: true
    }

    ShaderEffect {
        id: us1

        width: ds2.width * 2
        height: ds2.height * 2

        readonly property Item source: ds2layer
        property rect normalRect // not necessary here, added because of the warning
        readonly property int radius: root.radius

        // TODO: We could use `textureSize()` and get rid of this, but we
        //       can not because we are targeting GLSL 1.20/ESSL 1.0, even
        //       though the shader is written in GLSL 4.40.
        TextureProviderObserver {
            id: us1SourceObserver
            source: us1.source
        }

        readonly property size sourceTextureSize: us1SourceObserver.textureSize

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        visible: false

        fragmentShader: "qrc:///shaders/DualKawaseBlur_upsample.frag.qsb"

        supportsAtlasTextures: true

        blending: root.blending
    }

    ShaderEffectSource {
        id: us1layer

        // So that if configuration is two pass (this is not used), the buffer is released:
        // This is mainly relevant for switching configuration case, as initially if this was
        // never visible and was never used as texture provider, it should have never allocated
        // resources to begin with.
        sourceItem: (root.configuration === DualKawaseBlur.Configuration.FourPass) ? us1 : null

        visible: false
        smooth: true
    }

    ShaderEffect {
        id: us2

        anchors.fill: parent // {us1/ds1}.size * 2

        readonly property Item source: (root.configuration === DualKawaseBlur.Configuration.TwoPass) ? ds1layer : us1layer
        property rect normalRect // not necessary here, added because of the warning
        readonly property int radius: root.radius

        // TODO: We could use `textureSize()` and get rid of this, but we
        //       can not because we are targeting GLSL 1.20/ESSL 1.0, even
        //       though the shader is written in GLSL 4.40.
        TextureProviderObserver {
            id: us2SourceObserver
            source: us2.source
        }

        readonly property size sourceTextureSize: us2SourceObserver.textureSize

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        fragmentShader: "qrc:///shaders/DualKawaseBlur_upsample.frag.qsb"

        supportsAtlasTextures: true

        blending: root.blending
    }
}
