/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * Author: Prince Gupta <guptaprince8832@gmail.com>
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
import QtQuick.Templates as T
import QtQml.Models

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Playlist as Playlist
import VLC.Util

Item {
    id: dragItem

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    readonly property int coverSize: VLCStyle.icon_dragItem

    property var indexes: []

    // FIXME: This should not be required.
    //        Qt does not pick the right method overload.
    //        So we provide this additional information
    //        in order to pick the right method.
    property bool indexesFlat: false

    property string defaultCover: VLCStyle.noArtAlbumCover

    property string defaultText: qsTr("Unknown")

    // function(index, data) - returns cover for the index in the model in the form {artwork: <string> (file-name), fallback: <string> (file-name)}
    property var coverProvider: null

    // Optional, for now used by the default `getTextureProvider()`.
    // Mandatory for `MLDragItem`, as `MLDragItem` retrieves the model through the view.
    property Item view // can not use `ItemView` because of `ExpandGridView`

    // Optional, function that takes index and returns the texture provider:
    property var getTextureProvider: function(index) {
        // Texture provider is not going to be available at all times, particularly
        // for the cases when the item is not available in the view (large model,
        // select all case)

        // FIXME: `call(null, index)` is to prevent the following warning:
        //         > Calling C++ methods with 'this' objects different from the one they
        //         > were retrieved from is broken, due to historical reasons. The original
        //         > object is used as 'this' object. You can allow the given 'this' object
        //         > to be used by setting 'pragma NativeMethodBehavior: AcceptThisObject'
        return dragItem.view?.itemAtIndex.call(null, index)?.artworkTextureProvider ?? null
    }

    // string => role
    property string coverRole: "cover"

    property real padding: VLCStyle.margin_xsmall
    property real margins: VLCStyle.margin_large

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    signal requestData(var indexes, var resolve, var reject)
    signal requestInputItems(var indexes, var data, var resolve, var reject)

    function coversXPos(index) {
        return VLCStyle.margin_small + (coverSize / 1.5) * index + margins;
    }

    /**
      * @return {Promise} Promise object of the input items
      */
    function getSelectedInputItem() {
        if (_inputItems)
            return Promise.resolve(dragItem._inputItems)
        else if (dragItem._dropPromise)
            return dragItem._dropPromise
        else
            dragItem._dropPromise = new Promise((resolve, reject) => {
                dragItem._dropCallback = resolve
                dragItem._dropFailedCallback = reject
            })
            return dragItem._dropPromise
    }

    //---------------------------------------------------------------------------------------------
    // Private

    readonly property int _maxCovers: 10

    readonly property int _indexesSize: !!indexes ? indexes.length : 0

    readonly property int _displayedCoversCount: Math.min(_indexesSize, _maxCovers + 1)

    property var _inputItems

    property var _data: []

    property var _covers: []

    property int _currentRequest: 0

    property int _grabImageRequest: 0

    property bool _pendingNativeDragStart: false

    property var _dropPromise: null
    property var _dropCallback: null
    property var _dropFailedCallback: null

    //---------------------------------------------------------------------------------------------
    // Implementation
    //---------------------------------------------------------------------------------------------

    parent: T.Overlay.overlay

    visible: false

    Drag.dragType: Drag.None

    Drag.hotSpot.x: - VLCStyle.dragDelta + margins

    Drag.hotSpot.y: - VLCStyle.dragDelta + margins

    width: padding * 2
           + coversXPos(_displayedCoversCount - 1) + coverSize + VLCStyle.margin_small
           + subtitleLabel.width
           + margins /* one margins comes from `coversXPos()` */

    height: coverSize + padding * 2 + margins * 2

    enabled: false

    function _setData(data) {
        console.assert(data.length === indexes.length)
        _data = data

        const covers = []
        let mimeData = []

        for (let i in indexes) {
            if (covers.length === _maxCovers)
                break

            const element = data[i]
            const cover = _getCover(indexes[i], element)
            if (!cover)
                continue

            covers.push(cover)

            const url = element.url ?? element.mrl
            if (url)
                mimeData.push(url)
        }

        if (covers.length === 0)
            covers.push({
                artwork: "",
                fallback: dragItem.defaultCover
            })

        _covers = covers

        if (mimeData.length > 0) {
            Drag.mimeData = MainCtx.urlListToMimeData(mimeData)
        }
    }

    function _setInputItems(inputItems) {
        if (!Helpers.isArray(inputItems) || inputItems.length === 0) {
            console.warn("can't convert items to input items");
            dragItem._inputItems = null
            return
        }

        dragItem._inputItems = inputItems
    }

    function _getCover(index, data) {
        console.assert(dragItem.coverRole)
        if (!!dragItem.coverProvider)
            return dragItem.coverProvider(index, data)
        else
            return {
                artwork: data[dragItem.coverRole] || dragItem.defaultCover,
                fallback: dragItem.defaultCover,
                textureProvider: dragItem.getTextureProvider ? dragItem.getTextureProvider(index) : null
            }
    }

    function _startNativeDrag() {
        if (!_pendingNativeDragStart)
            return

        _pendingNativeDragStart = false

        const requestId = ++dragItem._grabImageRequest

        const s = dragItem.grabToImage(function (result) {
            if (requestId !== dragItem._grabImageRequest
                    || fsmDragInactive.active)
                return

            dragItem.Drag.imageSource = result.url
            dragItem.Drag.startDrag()
        })

        if (!s) {
            // reject all pending requests
            ++dragItem._grabImageRequest

            dragItem.Drag.imageSource = ""
            dragItem.Drag.startDrag()
        }
    }

    //NoRole because I'm not sure we need this to be accessible
    //can drag items be considered Tooltip ? or is another class better suited
    Accessible.role: Accessible.NoRole
    Accessible.name: qsTr("drag item")

    Drag.onActiveChanged: {
        if (Drag.active) {
            // reject all pending requests
            ++dragItem._grabImageRequest

            fsm.startDrag()
        } else {
            fsm.stopDrag()
        }
    }

    Timer {
        // used to start the drag if it's taking too much time to load data
        id: nativeDragStarter

        interval: 50
        running: _pendingNativeDragStart
        onTriggered: {
            dragItem._startNativeDrag()
        }
    }

    FSM {
        id: fsm

        signal startDrag()
        signal stopDrag()

        signal allImagesAreLoaded()

        //internal signals
        signal resolveData(var requestId, var indexes)
        signal resolveInputItems(var requestId, var indexes)
        signal resolveFailed()

        signalMap: ({
            startDrag: startDrag,
            stopDrag: stopDrag,
            resolveData: resolveData,
            resolveInputItems: resolveInputItems,
            resolveFailed: resolveFailed,
            allImagesAreLoaded: allImagesAreLoaded
        })

        initialState: fsmDragInactive

        FSMState {
            id: fsmDragInactive

            function enter() {
                _pendingNativeDragStart = false

                _covers = []
                _data = []
            }

            transitions: ({
                startDrag: fsmDragActive,
                resolveInputItems: {
                    guard: (requestId, items) => requestId === dragItem._currentRequest,
                    action: (requestId, items) => {
                        if (dragItem._dropCallback) {
                            dragItem._dropCallback(items)
                        }
                    }
                },
                resolveFailed: {
                    action: () => {
                        if (dragItem._dropFailedCallback) {
                            dragItem._dropFailedCallback()
                        }
                    }
                }
            })
        }

        FSMState {
            id: fsmDragActive

            initialState: fsmRequestData

            function enter() {
                dragItem._dropPromise = null
                dragItem._dropFailedCallback = null
                dragItem._dropCallback = null
                dragItem._inputItems = undefined
            }

            function exit() {
                _pendingNativeDragStart = false
            }

            transitions: ({
                stopDrag: fsmDragInactive
            })

            FSMState {
                id: fsmRequestData

                function enter() {
                    const requestId = ++dragItem._currentRequest
                    dragItem.requestData(
                        dragItem.indexes,
                        (data) => fsm.resolveData(requestId, data),
                        fsm.resolveFailed)
                }

                transitions: ({
                    resolveData: {
                        guard: (requestId, data) => requestId === dragItem._currentRequest,
                        action: (requestId, data) => {
                            dragItem._setData(data)
                            _pendingNativeDragStart = true
                        },
                        target: fsmRequestInputItem
                    },
                    resolveFailed: fsmLoadingFailed
                })
            }

            FSMState {
                id: fsmRequestInputItem

                function enter() {
                    const requestId = ++dragItem._currentRequest
                    dragItem.requestInputItems(
                        dragItem.indexes, _data,
                        (items) => { fsm.resolveInputItems(requestId, items) },
                        fsm.resolveFailed)
                }

                transitions: ({
                    resolveInputItems: {
                        guard: (requestId, items) => requestId === dragItem._currentRequest,
                        action: (requestId, items) => {
                            dragItem._setInputItems(items)
                        },
                        target: fsmWaitingForImages,
                    },
                    resolveFailed: fsmLoadingFailed
                })
            }

            FSMState {
                id: fsmWaitingForImages

                transitions: ({
                    allImagesAreLoaded: {
                        target: fsmLoadingDone
                    }
                })

                function enter() {
                    if (coverRepeater.notReadyCount === 0) {
                        // By the time the state changes
                        // the images might have been
                        // already loaded:
                        fsm.allImagesAreLoaded()
                    }
                }
            }

            FSMState {
                id: fsmLoadingDone

                function enter() {
                    dragItem._startNativeDrag()

                    if (dragItem._dropCallback) {
                        dragItem._dropCallback(dragItem._inputItems)
                    }
                    dragItem._dropPromise = null
                    dragItem._dropCallback = null
                    dragItem._dropFailedCallback = null
                }
            }

            FSMState {
                id: fsmLoadingFailed
                function enter() {
                    _pendingNativeDragStart = false

                    if (dragItem._dropFailedCallback) {
                        dragItem._dropFailedCallback()
                    }
                    dragItem._dropPromise = null
                    dragItem._dropCallback = null
                    dragItem._dropFailedCallback = null
                }
            }
        }
    }

    Rectangle {
        /* background */
        anchors.fill: parent
        anchors.margins: dragItem.margins

        color: fsmLoadingFailed.active ? theme.bg.negative : theme.bg.primary
        border.color: theme.border
        border.width: VLCStyle.dp(1, VLCStyle.scale)
        radius: VLCStyle.dp(6, VLCStyle.scale)

        Widgets.RoundedRectangleShadow {
            blending: false // offscreen stacked below everything, no need for blending even though it is not opaque

            // Blur radius can not be greater than (margin / compensationFactor), as in that case it would need bigger
            // size than the item size to compensate. If you want bigger blur radius, either decrease the compensation
            // factor which can lead to visible clipping, or increase the margin:
            blurRadius: (dragItem.margins / compensationFactor)

            yOffset: VLCStyle.dp(4, VLCStyle.scale)

            color: Qt.rgba(0, 0, 0, .2)

            compensationFactor: 4.0 // Large compensation factor is to prevent clipping and strong shadow at the same time
        }
    }

    Repeater {
        id: coverRepeater

        readonly property var _model: dragItem._covers

        property int notReadyCount

        on_ModelChanged: {
            // Repeater signals model and count change after it reloads the items.
            // So we need to adjust the ready count before that:
            notReadyCount = _model.length
            model = _model
        }

        onNotReadyCountChanged: {
            if (notReadyCount === 0) {
                // All the images are loaded, don't wait anymore
                fsm.allImagesAreLoaded()
            }
        }

        Item {
            required property var modelData
            required property int index

            x: dragItem.coversXPos(index)
            anchors.verticalCenter: parent.verticalCenter
            width: dragItem.coverSize
            height: dragItem.coverSize

            Widgets.ImageExt {
                id: artworkCover

                anchors.centerIn: parent
                width: coverSize
                height: coverSize
                radius: coverRepeater.count > 1 ? dragItem.coverSize : 0.0
                source: modelData.artwork ?? ""
                sourceSize: dragItem.imageSourceSize ?? Qt.size(width * eDPR, height * eDPR)
                textureProviderItem: modelData?.textureProvider ?? null

                backgroundColor: theme.bg.primary
                borderWidth: VLCStyle.dp(1, VLCStyle.scale)
                borderColor: theme.border
                fillMode: Image.PreserveAspectCrop

                // FIXME: Qt bug, asynchronous + texture provider + custom shader does not work properly with `grabToImage()`:
                asynchronous: false

                readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)
                property bool _triggerReadiness: false

                on_TriggerReadinessChanged: {
                    // If it was already true (readiness already signalled), do not decrease the counter.
                    // This handler is only called when the property changes.
                    if (_triggerReadiness) {
                        coverRepeater.notReadyCount -= 1
                    }
                }

                readonly property var _combinedStatus: [status, shaderStatus]

                on_CombinedStatusChanged: {
                    // Qt `ShaderEffect` documentation states:
                    // > When runtime compilation is not in use and the shader properties
                    // > refer to files with bytecode, the status is always Compiled.
                    // However this is not correct, the status is reported to be "uncompiled" initially.
                    // And sometimes the status remains "uncompiled" even when the shader is in use, for
                    // that reason I only care about `ShaderEffect.Error` here and not `Compiled`.

                    if (shaderStatus === ShaderEffect.Error) {
                        _triggerReadiness = true // Not much to do in this case, shader could not be loaded.
                        return
                    }

                    if (status === Image.Error) {
                        const fallbackSource = modelData.fallback ?? defaultCover
                        if (source === fallbackSource) {
                            _triggerReadiness = true // Not much to do in this case either, fallback image could not be loaded.
                        } else {
                            source = fallbackSource
                        }
                    } else if (status === Image.Ready /* && shaderStatus === ShaderEffect.Compiled */) {
                        // FIXME: When Qt starts to report `ShaderEffect.Compiled` properly, start using it.
                        _triggerReadiness = true // Only in this case the image is loaded and shown.
                    }
                }

                DefaultShadow {

                }

                // FIXME: Qt bug (observed 6.2 and 6.8): The image does not get rendered without this.
                Rectangle {
                    z: -1

                    anchors.centerIn: parent

                    width: 1
                    height: 1

                    opacity: 0.01

                    color: "black"
                }
            }
        }
    }

    Rectangle {
        id: extraCovers

        x: dragItem.coversXPos(_maxCovers)
        anchors.verticalCenter: parent.verticalCenter
        width: dragItem.coverSize
        height: dragItem.coverSize
        radius: dragItem.coverSize
        visible: dragItem._indexesSize > dragItem._maxCovers
        color: theme.bg.secondary
        border.width: VLCStyle.dp(1, VLCStyle.scale)
        border.color: theme.border

        Widgets.MenuLabel {
            anchors.fill: parent

            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: VLCStyle.fontSize_small

            color: theme.accent

            text: "+" + (dragItem._indexesSize - dragItem._maxCovers)
        }

        DefaultShadow {

        }
    }


    MenuCaption {
        id: subtitleLabel

        anchors.verticalCenter: parent.verticalCenter
        x: dragItem.coversXPos(_displayedCoversCount - 1) + dragItem.coverSize + VLCStyle.margin_small

        visible: text && text !== ""
        text: qsTr("%1 selected").arg(dragItem._indexesSize)
        color: theme.fg.secondary
    }

}
