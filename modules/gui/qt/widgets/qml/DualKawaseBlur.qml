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

import VLC.MainInterface
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

    readonly property bool available: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

    readonly property bool ready: us2.visible

    enum Mode {
        FourPass, // 2 downsample + 2 upsamples (3 layers/buffers)
        TwoPass // 1 downsample + 1 upsample (1 layer/buffer)
    }

    property int mode: DualKawaseBlur.Mode.FourPass

    /// <postprocess>
    // The following property must be set in order to make other properties respected:
    property bool postprocess: false

    property color tint: "transparent"
    property real tintStrength: 0.0
    property real noiseStrength: 0.0
    property real exclusionStrength: 0.0
    property color backgroundColor: "transparent"
    /// </postprocess>

    // NOTE: This property is also an optimization hint. When it is false, the
    //       intermediate buffers for the blur passes may be released (only
    //       the two intermediate layers in four pass mode, we must have one
    //       layer regardless of the mode, so optimization-wise it has no
    //       benefit in two pass mode thus should be used solely as behavior
    //       instead):
    property bool live: true

    // Do not hesitate to use an odd number for the radius, there is virtually
    // no difference between odd or even numbers due to the halfpixel trick.
    // The effective radius is always going to be a half-integer.
    property int radius: 1

    // NOTE: This property concerns the blending in the scene graph, not the internal
    //       blending used with regard to background coloring. In that case blending
    //       is always done.
    // NOTE: It seems that if SG accumulated opacity is lower than 1.0, blending is
    //       used even if it is set false here. For that reason, it should not be
    //       necessary to check for opacity (well, accumulated opacity can not be
    //       checked directly in QML anyway).
    property bool blending: (postprocess &&
                             backgroundColor.a > (1.0 - Number.EPSILON)) ? false // the result is opaque, no need for sg blending
                                                                         : _sourceIsTranslucent

    // WARNING: If texture is not valid/ready, Qt generates a transparent texture to use as source.
    readonly property bool _sourceIsTranslucent: (!sourceTextureIsValid || sourceTextureProviderObserver.hasAlphaChannel)

    // source must be a texture provider item. Some items such as `Image` and
    // `ShaderEffectSource` are inherently texture provider. Other items needs
    // layering with either `layer.enabled: true` or `ShaderEffectSource`.
    // We purposefully are not going to create a layer on behalf of the source
    // here, unlike `MultiEffect` (see `hasProxySource`), because it is impossible
    // to determine whether the new layer is actually wanted (when the source is
    // already a texture provider), and it is very trivial to have a layer when
    // it is wanted or necessary anyway.
    property Item source

    /// <debug>
    readonly property QtObject _sourceWindow: (source?.Window.window ?? null)
    function _onWindowChanged() {
        console.assert((_sourceWindow && root.Window.window) ? (_sourceWindow === root.Window.window) : true)
    }
    on_SourceWindowChanged: {
        _onWindowChanged()
    }
    Window.onWindowChanged: {
        _onWindowChanged()
    }
    /// </debug>

    // Arbitrary sub-texturing (no need to be set for atlas textures):
    // `QSGTextureView` can also be used instead of sub-texturing here.
    property rect sourceRect

    // Viewport rect allows to discard an unwanted area in effect's local coordinates.
    // Since the discard occurs at layer level, using this saves video memory.
    property rect viewportRect

    // Visual rect allows to extend the visual (us2) in effect's local coordinates.
    // Unlike viewport rect, visual rect is not relevant to the layers hence will
    // not allow saving video memory. The reason for this to exist is that when
    // viewport rect is used to save video memory, the effect may still need to
    // cover a certain area without stretching the blurred result and to keep
    // the postprocess coverage available beyond the blurred result but within
    // the effect area. A particular use case is when blurred content has large
    // margins that are empty, where in this case visual rect with clamp to edge
    // wrap mode would make the effect look as if the whole source was blurred,
    // but with the benefit of saving video memory, provided that the viewport
    // rect is set to discard the empty margins. Another use case is freely
    // adjusting the position of the visual when viewport rect is smaller than
    // the effect size, since with only viewport rect the visual is always
    // centered in the parent (effect).
    property rect visualRect
    property int visualWrapMode: TextureProviderIndirection.ClampToEdge

    // Local viewport rect is viewport rect divided by two, because it is used as the source
    // rect of last layer, which is 2x upsampled by the painter delegate (us2):
    readonly property rect _localViewportRect: ((viewportRect.width > 0 && viewportRect.height > 0)) ? Qt.rect(viewportRect.x / 2,
                                                                                                               viewportRect.y / 2,
                                                                                                               viewportRect.width / 2,
                                                                                                               viewportRect.height / 2)
                                                                                                     : Qt.rect(0, 0, 0, 0)

    // This is only supposed to be used in visual delegate (us2), maybe we move it there?
    readonly property rect _localVisualRect: ((visualRect.width > 0 && visualRect.height > 0)) ? Qt.rect(visualRect.x / 2,
                                                                                                         visualRect.y / 2,
                                                                                                         visualRect.width / 2,
                                                                                                         visualRect.height / 2)
                                                                                               : Qt.rect(0, 0, 0, 0)

    property alias sourceTextureProviderObserver: ds1.tpObserver // for accessory

    readonly property bool sourceTextureIsValid: sourceTextureProviderObserver.isValid

    onSourceTextureIsValidChanged: {
        if (root.sourceTextureIsValid) {
            if (root._queuedScheduledUpdate) {
                root._queuedScheduledUpdate = false
                root.scheduleUpdate()
            }
        }
    }

    property var /*QtWindow*/ _window: null // captured window used for chaining through `afterAnimating()`

    property bool _queuedScheduledUpdate: false

    readonly property var _comparisonVar: ({key: sourceTextureProviderObserver.comparisonKey,
                                            subRect: sourceTextureProviderObserver.normalizedTextureSubRect})
    property var _oldComparisonVar

    on_ComparisonVarChanged: {
        if (_comparisonVar.key >= 0) {
            if (_oldComparisonVar !== undefined && _oldComparisonVar !== _comparisonVar) {
                _oldComparisonVar = undefined

                // If source texture is not valid, update will be requeued in `scheduleUpdate()`.
                // That being said, a non-valid source texture should have (-1) as comparison key,
                // which we already checked here.

                if (root.funcOnNextEffectureTextureChange) {
                    root.funcOnNextEffectureTextureChange()
                }
            }
        }
    }

    // This is not respected when `live` is true, it is only for the `scheduleUpdate(true)` calls.
    property var funcOnNextEffectureTextureChange: function() {
        root.scheduleUpdate()
    }

    // When `onNextEffectiveTextureChange` is set, the update is scheduled automatically when the effective
    // texture changes, which is when the texture itself changes or the texture remains the same but
    // the sub-rect changes (such as, the new texture is a different part of the same atlas texture).
    // This behavior can be fine-tuned with the property `funcOnNextEffectureTextureChange`.
    function scheduleUpdate(onNextEffectiveTextureChange /* : bool */ = false) {
        if (live)
            return // no-op

        if (!root.available)
            return // not applicable

        if (!root.sourceTextureIsValid) {
            root._queuedScheduledUpdate = true // if source texture is not valid, delay the update until valid
            return
        }

        if (onNextEffectiveTextureChange) {
            root._oldComparisonVar = root._comparisonVar
            return
        }

        if (root._window) {
            // One possible case for this is that the mipmaps for the source texture were generated too fast, and
            // the consumer wants to update the blur to make use of the mipmaps before the blur finished chained
            // updates for the previous source texture which is the non-mipmapped version of the same texture.
            console.debug(root, "scheduleUpdate(): There is an already ongoing chained update, re-scheduling...")
            root._queuedScheduledUpdate = true
            return
        }

        root._window = root.Window.window
        ds1layer.scheduleChainedUpdate()
    }

    onLiveChanged: {
        if (live) {
            ds1layer.parent = root
            ds2layer.inhibitParent = false
        } else {
            root.scheduleUpdate() // this triggers releasing intermediate layers (when applicable)
        }
    }

    component DefaultShaderEffect : ShaderEffect {
        id: shaderEffect

        required property Item source
        readonly property int radius: root.radius

        // TODO: We could use `textureSize()` and get rid of this, but we
        //       can not because we are targeting GLSL 1.20/ESSL 1.0, even
        //       though the shader is written in GLSL 4.40:
        property size sourceTextureSize

        Binding on sourceTextureSize {
            when: root.live
            value: textureProviderObserver.nativeTextureSize
            restoreMode: Binding.RestoreNone // No need to restore
        }

        property rect normalRect // may not be necessary, but still needed to prevent warning

        property alias tpObserver: textureProviderObserver

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        // Maybe we should have vertex shader unconditionally, and calculate the half pixel there instead of fragment shader?
        vertexShader: (normalRect.width > 0.0 && normalRect.height > 0.0) ? "qrc:///shaders/SubTexture.vert.qsb"
                                                                          : ""

        supportsAtlasTextures: true

        blending: root.blending

        visible: false

        TextureProviderObserver {
            id: textureProviderObserver
            source: shaderEffect.source
            notifyAllChanges: (root.visible && root.live)
        }
    }

    component DownsamplerShaderEffect : DefaultShaderEffect {
        fragmentShader: source ? "qrc:///shaders/DualKawaseBlur_downsample.frag.qsb"
                               : "" // to prevent warning if source becomes null
    }

    component UpsamplerShaderEffect : DefaultShaderEffect {
        fragmentShader: source ? "qrc:///shaders/DualKawaseBlur_upsample.frag.qsb"
                               : "" // to prevent warning if source becomes null
    }

    component DefaultShaderEffectSource : ShaderEffectSource {
        // This is necessary to release resources even if `sourceItem` becomes null (non-live case):
        parent: sourceItem ? root : null

        visible: false
        live: root.live
        smooth: true
    }

    DownsamplerShaderEffect {
        id: ds1

        // When downsampled, we can decrease the size here so that the layer occupies less VRAM:
        width: parent.width / 2
        height: parent.height / 2

        source: root.source

        // TODO: Instead of normalizing here, we could use GLSL 1.30's `textureSize()`
        //       and normalize in the vertex shader, but we can not because we are
        //       targeting GLSL 1.20/ESSL 1.0, even though the shader is written in
        //       GLSL 4.40.
        normalRect: (root.sourceRect.width > 0.0 && root.sourceRect.height > 0.0) ? Qt.rect(root.sourceRect.x / sourceTextureSize.width,
                                                                                            root.sourceRect.y / sourceTextureSize.height,
                                                                                            root.sourceRect.width / sourceTextureSize.width,
                                                                                            root.sourceRect.height / sourceTextureSize.height)
                                                                                  : Qt.rect(0.0, 0.0, 0.0, 0.0)
    }

    DefaultShaderEffectSource {
        id: ds1layer

        sourceItem: ds1

        // Last layer for two pass mode, so use the viewport rect:
        sourceRect: (root.mode === DualKawaseBlur.Mode.TwoPass) ? root._localViewportRect : Qt.rect(0, 0, 0, 0)
        
        function scheduleChainedUpdate() {
            if (!ds1layer) // context is lost, Qt bug (reproduced with 6.2)
                return

            ds1.sourceTextureSize = ds1.tpObserver.nativeTextureSize
            if (ds1.ensurePolished)
                ds1.ensurePolished()

            // Common for both four and two pass mode:
            ds1layer.parent = root
            ds1layer.scheduleUpdate()

            if (root._window) {
                // In four pass mode, we can release the two intermediate layers:
                if (root.mode === DualKawaseBlur.Mode.FourPass) {
                    // Scheduling update must be done sequentially for each layer in
                    // a chain. It seems that each layer needs one frame for it to be
                    // used as a source in another layer, so we can not schedule
                    // update for each layer at the same time:
                    root._window.afterAnimating.connect(ds2layer, ds2layer.scheduleChainedUpdate)
                } else {
                    root._window = null
                }
            }
        }
    }

    DownsamplerShaderEffect {
        id: ds2

        // When downsampled, we can decrease the size here so that the layer occupies less VRAM:
        width: ds1.width / 2
        height: ds1.height / 2

        Binding on tpObserver.notifyAllChanges {
            when: (root.mode !== DualKawaseBlur.Mode.FourPass)
            value: false
        }

        // Qt uses reference counting, otherwise ds1layer may not be released, even if it has no parent (see `QQuickItemPrivate::derefWindow()`):
        source: ((root.mode === DualKawaseBlur.Mode.TwoPass) || !ds1layer.parent) ? null : ds1layer
    }

    DefaultShaderEffectSource {
        id: ds2layer

        // So that if the mode is two pass (this is not used), the buffer is released:
        // This is mainly relevant for switching the mode case, as initially if this was
        // never visible and was never used as texture provider, it should have never allocated
        // resources to begin with.
        sourceItem: (root.mode === DualKawaseBlur.Mode.FourPass) ? ds2 : null
        parent: (!inhibitParent && sourceItem) ? root : null // necessary to release resources even if `sourceItem` becomes null (non-live case)

        property bool inhibitParent: false

        function scheduleChainedUpdate() {
            if (!ds2layer) // context is lost, Qt bug (reproduced with 6.2)
                return

            ds2.sourceTextureSize = ds2.tpObserver.nativeTextureSize
            if (ds2.ensurePolished)
                ds2.ensurePolished()

            ds2layer.inhibitParent = false
            ds2layer.scheduleUpdate()

            if (root._window) {
                root._window.afterAnimating.disconnect(ds2layer, ds2layer.scheduleChainedUpdate)
                root._window.afterAnimating.connect(us1layer, us1layer.scheduleChainedUpdate)
            }
        }
    }

    UpsamplerShaderEffect {
        id: us1

        width: ds2.width * 2
        height: ds2.height * 2

        Binding on tpObserver.notifyAllChanges {
            when: (root.mode !== DualKawaseBlur.Mode.FourPass)
            value: false
        }

        // Qt uses reference counting, otherwise ds2layer may not be released, even if it has no parent (see `QQuickItemPrivate::derefWindow()`):
        source: ((root.mode === DualKawaseBlur.Mode.TwoPass) || !ds2layer.parent) ? null : ds2layer
    }

    DefaultShaderEffectSource {
        id: us1layer

        // So that if the mode is two pass (this is not used), the buffer is released:
        // This is mainly relevant for switching the mode case, as initially if this was
        // never visible and was never used as texture provider, it should have never allocated
        // resources to begin with.
        sourceItem: (root.mode === DualKawaseBlur.Mode.FourPass) ? us1 : null

        // Last layer for four pass mode, so use the viewport rect:
        // No need to check for the mode because this layer is not used in two pass mode anyway.
        sourceRect: root._localViewportRect

        function scheduleChainedUpdate() {
            if (!us1layer) // context is lost, Qt bug (reproduced with 6.2)
                return

            us1.sourceTextureSize = us1.tpObserver.nativeTextureSize
            if (us1.ensurePolished)
                us1.ensurePolished()

            us1layer.scheduleUpdate()

            if (root._window) {
                root._window.afterAnimating.disconnect(us1layer, us1layer.scheduleChainedUpdate)
                root._window.afterAnimating.connect(us1layer, us1layer.releaseResourcesOfIntermediateLayers)
            }
        }

        function releaseResourcesOfIntermediateLayers() {
            if (!ds1layer || !ds2layer) // context is lost, Qt bug (reproduced with 6.2)
                return

            us2.sourceTextureSize = us2.tpObserver.nativeTextureSize

            // Last layer is updated, now it is time to release the intermediate buffers:
            console.debug(root, ": releasing intermediate layers, expect the video memory consumption to drop.")

            // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling
            ds1layer.parent = null
            ds2layer.inhibitParent = true

            if (root._window) {
                root._window.afterAnimating.disconnect(us1layer, us1layer.releaseResourcesOfIntermediateLayers)
                root._window = null
            }

            if (root._queuedScheduledUpdate) {
                // Tried calling `scheduleUpdate()` before the ongoing chained updates completed.
                root._queuedScheduledUpdate = false
                root.scheduleUpdate()
            }
        }
    }

    // This rectangle is going to be visible until the blur effect is ready, so that
    // we don't expose what is beneath meanwhile.
    Rectangle {
        anchors.fill: us2

        visible: !us2.visible && root.postprocess && (root.backgroundColor.a > 0.0)

        color: root.backgroundColor
    }

    UpsamplerShaderEffect {
        id: us2

        // {us1/ds1}.size * 2, unless visual rect is used
        anchors.centerIn: useIndirection ? undefined : parent

        x: useIndirection ? root.visualRect.x : 0
        y: useIndirection ? root.visualRect.y : 0

        width: {
            if (useIndirection)
                return root.visualRect.width

            if (root.viewportRect.width > 0)
                return root.viewportRect.width

            return parent.width
        }

        height: {
            if (useIndirection)
                return root.visualRect.height

            if (root.viewportRect.height > 0)
                return root.viewportRect.height

            return parent.height
        }

        readonly property bool useIndirection: (root.visualRect.width > 0 && root.visualRect.height > 0)

        visible: tpObserver.isValid && root.available

        source: useIndirection ? textureProviderIndirection : targetSource

        readonly property Item targetSource: (root.mode === DualKawaseBlur.Mode.TwoPass) ? ds1layer : us1layer

        property alias tint: root.tint
        property alias tintStrength: root.tintStrength
        property alias noiseStrength: root.noiseStrength
        property alias exclusionStrength: root.exclusionStrength
        property alias backgroundColor: root.backgroundColor

        fragmentShader: root.postprocess ? "qrc:///shaders/DualKawaseBlur_upsample_postprocess.frag.qsb"
                                         : "qrc:///shaders/DualKawaseBlur_upsample.frag.qsb"

        property real _eDPR: MainCtx.effectiveDevicePixelRatio(Window.window) || 1.0

        Connections {
            target: MainCtx

            function onIntfDevicePixelRatioChanged() {
                us2._eDPR = MainCtx.effectiveDevicePixelRatio(us2.Window.window) || 1.0
            }
        }

        TextureProviderIndirection {
            id: textureProviderIndirection

            source: us2.targetSource

            textureSubRect: Qt.rect(0,
                                    0,
                                    root._localVisualRect.width * us2._eDPR,
                                    root._localVisualRect.height * us2._eDPR)

            horizontalWrapMode: root.visualWrapMode
            verticalWrapMode: root.visualWrapMode
        }
    }
}
