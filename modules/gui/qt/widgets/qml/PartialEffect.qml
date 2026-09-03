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
import QtQuick.Window

import VLC.MainInterface
import VLC.Util

// This item can be used as a layer effect.
// The purpose of this item is to apply an effect to a partial
// area of an item without re-rendering the source item multiple
// times.
Item {
    id: root

    // source must be a texture provider Item.
    // If layer is enabled, the source (parent) will be a texture provider
    // and automatically set by Qt.
    // Some items, such as Image and ShaderEffectSource can be implicitly
    // a texture provider without creating an extra layer.
    // Make sure that the sampler name is set to "source" (default) if
    // this is used as a layer effect.
    property Item source

    // Rectangular area(s) where the effect should be applied:
    property var effectRects

    // If set false, whenever effect rects shrink, the created effects
    // will be destroyed. It is not recommended to set it to false if
    // the `effectRects` array size alternates multiple times.
    property bool persistentEffects: true

    // Not mandatory to provide, but when feasible (such as, effect is not
    // an isolated inner area), provide it for optimization. When not provided,
    // the source visual is going to cover the whole area, and if source is not
    // opaque (blending is enabled), the effect part is going to be filtered in
    // fragment shader, which is more expensive than sub-texturing by means of
    // this property. That being said, when source is opaque (blending is off),
    // then providing this may serve no purpose due to early fragment test as
    // long as the scene graph uses depth buffer (default). For that reason, it
    // is recommended to provide this only in non-opaque cases (blending is set):
    property rect sourceVisualRect

    // `index` will be provided to the effect, if it has such required property.
    // Property `inhibit` will be set on destruction, if such property exists.
    property Component effect

    property string samplerName: "source"

    // Enable blending if source or the effect is not opaque.
    // This comes with a performance penalty.
    blending: false

    property alias blending: sourceProxy.blending

    // Display scale may be a fractional size, but textures can not. Pixel aligned
    // aligns the visual size to the nearest multiple of number, depending on
    // the window/screen fraction, so that the layer texture can be displayed
    // without stretching. For example, if the display scale is 1.25, the
    // visual size would be aligned up to the nearest multiple of 4. Note that
    // using this property is going to make the visual to deviate from the
    // size intended, which may increase greatly depending on the fraction
    // of the display scale. Also note that if the QML item size is fractional
    // itself, it is ceiled regardless of the display scale or this property.
    // This setting applies to both the source and the effect visuals, but
    // is only relevant for the source visual if `sourceVisualRect` is
    // provided.
    property bool pixelAlignedForDPR: true

    property real _eDPR: MainCtx.effectiveDevicePixelRatio(Window.window) || 1.0
    readonly property int _alignNumber: pixelAlignedForDPR ? Helpers.denominatorForFloat(_eDPR) : 1

    property list<Item> _effects

    Component.onCompleted: {
        root.effectRectsChanged.connect(root, root._initializeEffects)
        root.effectChanged.connect(root, root._initializeEffects)

        _initializeEffects()
    }

    Component.onDestruction: {
        for (let i in _effects) { // for .. of is broken in Qt 6.2
            if (typeof _effects[i].inhibit === 'boolean')
                _effects[i].inhibit = true
        }
    }

    function _initializeEffects() {
        const effectRectsLength = (effectRects?.length ?? 0)

        const diff = (effectRectsLength - _effects.length)
        if (diff > 0) {
            if (effect) {
                for (let i = 0; i < diff; ++i) {
                    const index = _effects.length
                    const effectItem = effect.createObject(root, {index: index,
                                                                  visible: Qt.binding(() => (root.effectRects?.length > index)),
                                                                  x: Qt.binding(() => (root.effectRects?.length > index) ? root.effectRects[index].x : 0.0),
                                                                  y: Qt.binding(() => (root.effectRects?.length > index) ? root.effectRects[index].y : 0.0),
                                                                  width: Qt.binding(() => (root.effectRects?.length > index) ? Helpers.alignUp(root.effectRects[index].width, root._alignNumber)
                                                                                                                             : 0.0),
                                                                  height: Qt.binding(() => (root.effectRects?.length > index) ? Helpers.alignUp(root.effectRects[index].height, root._alignNumber)
                                                                                                                              : 0.0),
                                                                  blending: Qt.binding(() => root.blending)})

                    const effectAcceptsSourceRect = (typeof effectItem.sourceRect !== "undefined") // typeof `rect` is "object"
                    if (effectAcceptsSourceRect) {
                         effectItem[root.samplerName] = Qt.binding(() => effectItem.visible ? root.source : null)
                         effectItem.sourceRect = Qt.binding(() => Qt.rect(effectItem.x,
                                                                          effectItem.y,
                                                                          effectItem.width,
                                                                          effectItem.height))
                    } else {
                        const tpi = tpiComponent.createObject(effectItem, { 'effect': effectItem })
                        console.assert(tpi)
                        effectItem[root.samplerName] = Qt.binding(() => effectItem.visible ? tpi : null)
                    }

                    _effects.push(effectItem)
                }
            }
        } else if ((diff < 0) && !persistentEffects) {
            for (let i = (_effects.length - 1); i >= effectRectsLength; --i) {
                const effectItem = _effects[i]
                console.assert(effectItem)
                console.debug(root, ": destroying effect:", effectItem)
                if (typeof _effects[i].inhibit === 'boolean')
                    effectItem.inhibit = true
                effectItem.destroy()
            }

            _effects.length = effectRectsLength // Truncate the list
        }
    }

    Connections {
        target: MainCtx

        function onIntfDevicePixelRatioChanged() {
            root._eDPR = MainCtx.effectiveDevicePixelRatio(root.Window.window) || 1.0
        }
    }

    // This item displays the item with the rect denoted by effectRect
    // being transparent (only when blending is set):
    ShaderEffect {
        id: sourceProxy

        x: root.sourceVisualRect.x
        y: root.sourceVisualRect.y
        width: useSubTexture ? Helpers.alignUp(root.sourceVisualRect.width, root._alignNumber) : parent.width
        height: useSubTexture ? Helpers.alignUp(root.sourceVisualRect.height, root._alignNumber) : parent.height

        smooth: false

        // WARNING: Switching the source should be fine, but old Qt (Qt 6.2.13) seems to break the interface
        //          in this case. The indirection does not use sub-rect if it is not relevant, so in this
        //          case it would act as a dummy indirection to prevent the Qt bug.
        readonly property Item source: ((MainCtx.qtVersion() < MainCtx.qtVersionCheck(6, 4, 2)) || useSubTexture) ? sourceVisualTextureProviderIndirection : root.source

        readonly property var discardRects: {
            let ret = []

            if (!blending || useSubTexture) // If blending is not enabled, no need to make the normalization calculations
                return ret

            for (let i in root._effects) { // for .. of loop is broken in Qt 6.2
                const effect = root._effects[i]
                console.assert(effect)

                const rect = Qt.rect(effect.x / root.width,
                                     effect.y / root.height,
                                     (effect.x + effect.width) / root.width,
                                     (effect.y + effect.height) / root.height)

                if (rect.width > 0.0 && rect.height > 0.0)
                    ret.push(rect)
            }

            // We limit the size to 4 until Qt `ShaderEffect` supports forwarding arrays:
            if (ret.length > 4)
                console.debug(root, "Discarding areas in source visual that overlap the effect rectangles is currently limited to 4.")

            return ret
        }

        // QTBUG-50493, ShaderEffect does not support arrays, so for now we manually feed the values:
        readonly property rect discardRect0: discardRects[0] ?? Qt.rect(0,0,0,0)
        readonly property rect discardRect1: discardRects[1] ?? Qt.rect(0,0,0,0)
        readonly property rect discardRect2: discardRects[2] ?? Qt.rect(0,0,0,0)
        readonly property rect discardRect3: discardRects[3] ?? Qt.rect(0,0,0,0)

        readonly property bool useSubTexture: (root.sourceVisualRect.width > 0.0 && root.sourceVisualRect.height > 0.0)

        supportsAtlasTextures: true

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        fragmentShader: (discardRects.length > 0) ? "qrc:///shaders/RectFilter.frag.qsb" : ""

        TextureProviderIndirection {
            id: sourceVisualTextureProviderIndirection
            source: root.source

            // If the effect is in a isolated inner area, filtering is necessary. Otherwise, we can simply
            // use sub-texturing for the source itself as well (we already use sub-texture for the effect area).
            textureSubRect: (sourceProxy.useSubTexture) ? Qt.rect(sourceProxy.x * root._eDPR,
                                                                  sourceProxy.y * root._eDPR,
                                                                  sourceProxy.width * root._eDPR,
                                                                  sourceProxy.height * root._eDPR) : undefined
        }
    }

    Component {
        id: tpiComponent

        TextureProviderIndirection {
            id: textureProviderIndirection

            readonly property Item sourceItem: source?.sourceItem ?? source

            // In practice this is the parent item:
            required property Item effect

            source: root.source

            textureSubRect: Qt.rect(effect.x * root._eDPR,
                                    effect.y * root._eDPR,
                                    effect.width * root._eDPR,
                                    effect.height * root._eDPR)
        }
    }
}
