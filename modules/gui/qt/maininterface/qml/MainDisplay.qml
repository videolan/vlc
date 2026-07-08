/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Controls
import QtQuick.Layouts

import VLC.Style
import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player

import VLC.Util
import VLC.Dialogs

FocusScope {
    id: g_mainDisplay

    // Properties

    property bool hasMiniPlayer: miniPlayer.visible

    // NOTE: The main view must be above the indexing bar and the mini player.
    property real displayMargin: (height - miniPlayer.y) +
                                 (loaderUpdatePane.active ? loaderUpdatePane.height : 0)

    //MainDisplay behave as a PageLoader
    property alias pagePrefix: stackView.pagePrefix

    readonly property int positionSliderY: {
        var size = miniPlayer.y + miniPlayer.sliderY

        if (MainCtx.pinVideoControls)
            return size - VLCStyle.margin_xxxsmall
        else
            return size
    }

    property bool _showMiniPlayer: false

    property bool _showCSD: MainCtx.clientSideDecoration
        && !(MainCtx.intfMainWindow.visibility === Window.FullScreen)

    // functions

    //MainDisplay behave as a PageLoader
    function loadView(path, properties, focusReason) {
        const found = stackView.loadView(path, properties, focusReason)
        if (!found)
            return

        if (Player.hasVideoOutput && MainCtx.hasEmbededVideo)
            _showMiniPlayer = true
    }

    function loadCurrentHistoryView(focusReason) {
        loadView(History.viewPath, History.viewProp, focusReason)
        contextSaver.restore(History.viewPath)
    }

    Component.onCompleted: {
        if (MainCtx.canShowVideoPIP) {
            pipPlayerComponent.createObject(this)
        } else {
            if (MainCtx.hasEmbededVideo)
                MainPlaylistController.stop()
        }

        if (History.previousEmpty) {
            History.update(["home"])
        }
        loadCurrentHistoryView(Qt.OtherFocusReason)
    }

    Navigation.cancelAction: function() {
        History.previous(Qt.BacktabFocusReason)
    }

    Keys.onPressed: (event) => {
        if (KeyHelper.matchSearch(event)) {
            MainCtx.search.askShow()
            event.accepted = true
        }
        //unhandled keys are forwarded as hotkeys
        if (!event.accepted)
            MainCtx.sendHotkey(event.key, event.modifiers);
    }

    readonly property var pageModel: [
        {
            name: "home",
            url: MainCtx.mediaLibraryAvailable ?
                 "qrc:///qt/qml/VLC/MediaLibrary/HomeDisplay.qml" :
                 "qrc:///qt/qml/VLC/MainInterface/NoMedialibHome.qml"
        }, {
            name: "video",
            url: "qrc:///qt/qml/VLC/MediaLibrary/VideoDisplay.qml"
        }, {
            name: "music",
            url: "qrc:///qt/qml/VLC/MediaLibrary/MusicDisplay.qml"
        }, {
            name: "network",
            url: "qrc:///qt/qml/VLC/Network/BrowseDisplay.qml"
        }, {
            name: "discover",
            url: "qrc:///qt/qml/VLC/Network/DiscoverDisplay.qml"
        }, {
            name: "mlsettings",
            url: "qrc:///qt/qml/VLC/MediaLibrary/MLFoldersSettings.qml"
        }
    ]

    PinchHandler {
        id: interfaceScalePinchHandler

        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.TouchPad

        minimumPointCount: 2
        maximumPointCount: 2

        target: null

        // Qt version check in disguise, only available starting with 6.5:
        enabled: !!scaleAxis?.activeValueChanged

        // Default in QPlatformTheme::defaultThemeHint is 10
        dragThreshold: ((Application.styleHints?.startDragDistance ?? 10) * 4)

        property double accumulatedDelta: 0.0

        function adjustScaleFactor() {
            // We don't want too fine change, since changing it requires reloading
            // all images from disk:
            if (accumulatedDelta >= 0.5) {
                MainCtx.setIntfUserScaleFactor(+((MainCtx.getIntfUserScaleFactor() * accumulatedDelta).toFixed(2)))
                accumulatedDelta = 0.0
            }
        }

        Component.onCompleted: {
            if (interfaceScalePinchHandler?.scaleAxis?.activeValueChanged) {
                scaleAxis.activeValueChanged.connect(interfaceScalePinchHandler, onScaleAxisActiveValueChanged)
            }
        }

        function onScaleAxisActiveValueChanged(delta) {
            if (delta < 0.25)
                return

            accumulatedDelta += delta

            // Naive compression:
            Qt.callLater(interfaceScalePinchHandler.adjustScaleFactor)
        }

        onActiveChanged: {
            if (!active) {
                accumulatedDelta = 0.0
            }
        }
    }

    ModelSortSettingHandler {
        id: contextSaver
    }

    Connections {
        target: MainCtx.sort

        function onCriteriaChanged(criteria) {
            contextSaver.save(History.viewPath)
        }

        function onOrderChanged(order) {
            contextSaver.save(History.viewPath)
        }
    }

    Connections {
        target: History
        function onNavigate(focusReason) {
            loadCurrentHistoryView(focusReason)
        }
    }

    ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.View
    }

    Loader {
        id: voronoiSnowLoader

        z: 3.5
        source: "qrc:///qt/qml/VLC/Widgets/VoronoiSnow.qml"
        anchors.fill: parent
        active: false

        function toggleActive() {
            voronoiSnowLoader.active = !voronoiSnowLoader.active
        }

        Component.onCompleted: {
            if (MainCtx.useXmasCone()) {
                MainCtx.kc_pressed.connect(voronoiSnowLoader.toggleActive)
            }
        }
    }

    MenuTopbar {
        id: menuTopbar
        z: 6

        visible: MainCtx.hasToolbarMenu
        enabled: visible

        anchors {
            top: parent.top
            right: parent.right
            left: parent.left
        }

        plListView: {
            if (playlistLoader.active)
                return playlistLoader.item
            else if (playlistWindowLoader.status === Loader.Ready)
                return playlistWindowLoader.item.playlistView
            else
                return null
        }
    }

    LocalTopbar {
        id: localTopbar
        z: 5

        anchors {
            top: menuTopbar.visible ? menuTopbar.bottom : parent.top
            left: parent.left
            right: parent.right
        }

        leftPadding: VLCStyle.applicationHorizontalMargin
        rightPadding: VLCStyle.applicationHorizontalMargin
        topPadding: menuTopbar.visible ? 0 : VLCStyle.applicationVerticalMargin

        plListView: {
            if (playlistLoader.active)
                return playlistLoader.item
            else if (playlistWindowLoader.status === Loader.Ready)
                return playlistWindowLoader.item.playlistView
            else
                return null
        }

        navigationVisible: pannelVisiblity.showNavigation
        playqueueVisible: pannelVisiblity.showPlayqueue

        Navigation.parentItem: g_mainDisplay
        Navigation.upItem: menuTopbar
        Navigation.downItem: mainRow

        onToggleNavigationVisibility: pannelVisiblity.toggleNavigationVisibility()
        onTogglePlayqueueVisibility:  pannelVisiblity.togglePlayqueueVisibility()
    }

    FocusScope {
        id: mainRow

        anchors {
            top: localTopbar.bottom
            right: parent.right
            left: parent.left
            bottom: parent.bottom
        }

        z: 0

        Navigation.parentItem: g_mainDisplay

        Rectangle {
            id: stackViewParent

            // This rectangle is used to display the effect in
            // the area of miniplayer background.
            // We can not directly apply the effect on the
            // view because its size is limited and the effect
            // should exceed the size. Also, it is beneficial
            // to have a rectangle here because if the background
            // is transparent we would lose subpixel font rendering
            // support.

            z: 1

            anchors.fill: parent

            implicitWidth: stackView.implicitWidth
            implicitHeight: stackView.implicitHeight

            color: theme.bg.primary

            layer.enabled: MainCtx.backdropBlurRequested() &&
                           (GraphicsInfo.shaderType === GraphicsInfo.RhiShader) &&
                           (miniPlayer.visible || !!loaderUpdatePane.item?.visible)

            // Blurring requires to access neighbour pixels, thus the source texture should be bigger than
            // the effect so that the effect have access to the neighbor pixels for the pixels near the
            // border, where the extra size would depend on the blur configuration. When the source is
            // static, this problem is harder to notice, but when the source is not static, such as
            // during scrolling, not considering this causes glitches in the bottom side. `PartialEffect`,
            // since 03b0de26, already provides the effect the whole source texture with a proper sub-rect,
            // so the effect here can sample the top edge neighbour pixels, but for the bottom edge we
            // need to configure the layer:
            readonly property int edgeExtension: 16
            // The layer width is smaller than the item width, this is intentional because we do not want
            // to include the area that playqueue occupies in the layer since it is empty. Note that we
            // still want to do layering here, because even though the layer is smaller than the item
            // size, what we display is covered by the item size. The effect, which needs to cover the
            // item width is going to respect the empty area due to clamp to edge behavior, so we don't
            // need to use background coloring. It is currently a todo to further reduce video memory
            // consumption by covering the effect for only the area of interest, currently the blur
            // effect does not support having an extension area for postprocessing.
            layer.sourceRect: Qt.rect(stackView.x - edgeExtension,
                                      0,
                                      Helpers.alignUp(stackView.width + (2 * edgeExtension), alignNumber),
                                      Helpers.alignUp(height + edgeExtension, alignNumber))

            property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window) || 1.0
            readonly property int alignNumber: Helpers.denominatorForFloat(eDPR)

            Connections {
                target: MainCtx

                function onIntfDevicePixelRatioChanged() {
                    stackViewParent.eDPR = MainCtx.effectiveDevicePixelRatio(stackViewParent.Window.window) || 1.0
                }
            }

            Rectangle {
                // Extension of parent rectangle for edge and fractional scale alignment extension.
                anchors.fill: parent
                anchors.margins: -(stackViewParent.edgeExtension + fractionalScaleExtensionSize)

                // With fractional scale, if we align up the layer size to make sure the size is
                // an integer, we also need to extend here because the background must cover the
                // extension area. 8 should be enough for the fractions that we care (.25, .5, .75).
                // Note that the background extension here does not consume additional video memory.
                readonly property real fractionalScaleExtensionSize: (stackViewParent.alignNumber > 1 ? 8.0 : 0.0)

                visible: stackViewParent.layer.enabled && (height > 0 && width > 0)
                // We can't simply adjust the color because the color might not be opaque,
                // so we use border instead. Note that border is always placed inside the
                // rectangle:
                border.width: -anchors.margins
                border.color: parent.color
                color: "transparent"
            }

            layer.effect: Widgets.PartialEffect {
                id: stackViewParentLayerEffect

                // Setting `height` does not seem to work here. Anchoring the effect is not very nice, but it works:
                anchors.fill: stackViewParent // WARNING: layered item is not necessarily the visual parent of its layer effect.
                anchors.bottomMargin: (stackViewParent.height - stackViewParent.layer.sourceRect.height)
                // Layer width is limited to `stackView` width to save memory, in `PartialEffect` the source visual uses the
                // size of the `PartialEffect` unless `sourceVisualRect` is used, so we define the boundary in `PartialEffect`
                // for the source visual here. The effect rect exceeds the boundaries of `PartialEffect` due to this (see
                // `effectRect`), which is not particularly nice, but there is not much we can do about that here without
                // using `sourceVisualRect`, and saving video memory is considered more important:
                anchors.rightMargin: (stackViewParent.width - stackViewParent.layer.sourceRect.width - stackViewParent.layer.sourceRect.x)
                anchors.leftMargin: stackViewParent.layer.sourceRect.x

                blending: stackViewParent.color.a < (1.0 - Number.EPSILON)

                // Each pass of the blur effect also suffers from the border neighbour pixel issue mentioned
                // above, making all the borders problematic, to a less considerable extent. For that reason,
                // we extend both the top and the bottom edges and use viewport to prevent overdraw:
                effectRect: Qt.rect(-stackView.x,
                                    stackView.height - stackViewParent.edgeExtension,
                                    stackViewParent.width + 2 * stackViewParent.edgeExtension,
                                    loaderUpdatePane.height + miniPlayer.height + 2 * stackViewParent.edgeExtension)

                // WARNING: We are not using `sourceVisualRect` because it is not trivial to guarantee that
                //          the visual (`ShaderEffect`) scene graph sizing and sub-texturing are synchronized.
                //          This can cause the visual to deviate from 1:1 representation of the texture
                //          momentarily, leading to sizing glitches during initialization and animations.

                effect: frostedGlassEffect

                Widgets.FrostedGlassEffect {
                    id: frostedGlassEffect

                    ColorContext {
                        id: frostedTheme
                        palette: VLCStyle.palette
                        colorSet: ColorContext.Window
                    }

                    backgroundColor: (ready ? "transparent" : stackViewParent.color)
                    tint: frostedTheme.bg.secondary

                    // Prevent overdraw (the extension margin should not be painted).
                    // This also saves video memory compared to solely using visual rect.
                    // Note that viewport rect does not cover edge extension in y-axis
                    // but covers edge extension in x-axis because we don't want to
                    // have artifacts with regard to clamp-to-edge texture extension
                    // in x-axis (relevant with delegate background coloring in list
                    // view mode). Since we don't do texture extension in y-axis, we
                    // don't need to cover the edge extension in y-axis, which allows
                    // us to save some video memory (as opposed to `visualRect`).
                    viewportRect: Qt.rect(stackView.x,
                                          stackViewParent.edgeExtension,
                                          stackView.width + (2 * stackViewParent.edgeExtension),
                                          height - (2 * stackViewParent.edgeExtension))

                    visualRect: (stackView.width < stackViewParent.width) ? Qt.rect(stackViewParent.edgeExtension,
                                                                                    viewportRect.y,
                                                                                    width - (2 * stackViewParent.edgeExtension),
                                                                                    viewportRect.height)
                                                                          : Qt.rect(0, 0, 0, 0)
                }
            }

            Widgets.PageLoader {
                id: stackView

                focus: true

                anchors.fill: parent
                anchors.leftMargin: sidebar.width
                anchors.rightMargin: (playlistLoader.shown && !VLCStyle.isScreenSmall)
                                     ? playlistLoader.width
                                     : 0
                anchors.bottomMargin: g_mainDisplay.displayMargin

                pageModel: g_mainDisplay.pageModel

                leftPadding: sidebar.visible  ? 0 : VLCStyle.applicationHorizontalMargin

                rightPadding: playlistLoader.shown ? 0 : VLCStyle.applicationHorizontalMargin

                // Top-left corner rounding:
                Widgets.AcrylicBackground {
                    id: topLeftCornerBackground

                    z: 99
                    color: sidebar.background.color
                    visible: sidebar.visible && (stackViewTopLeftRadiusRectangle.topLeftRadius !== undefined)

                    anchors.left: stackView.left
                    anchors.leftMargin: -1
                    anchors.top: stackView.top

                    // (-1) is to prevent seams
                    width: stackViewTopLeftRadiusRectangle.width - 1
                    height: stackViewTopLeftRadiusRectangle.height - 1

                    Rectangle {
                        id: stackViewTopLeftRadiusRectangle

                        width: (stackViewTopLeftRadiusRectangle.topLeftRadius * 2)
                        height: (stackViewTopLeftRadiusRectangle.topLeftRadius * 2)

                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.leftMargin: 1

                        color: stackViewParent.color

                        property real implicitTopLeftRadius: VLCStyle.mainView_topLeftRadius

                        Component.onCompleted: {
                            if (stackViewTopLeftRadiusRectangle.topLeftRadius !== undefined) // TODO: Directly set when minimum Qt is 6.7
                                stackViewTopLeftRadiusRectangle.topLeftRadius = Qt.binding(() => stackViewTopLeftRadiusRectangle.implicitTopLeftRadius)
                        }
                    }
                }

                onCurrentItemChanged: {
                    if (currentItem) {
                        {
                            // Main pages need to compensate for the mini player:

                            if (currentItem.displayMarginEnd !== undefined)
                                currentItem.displayMarginEnd = Qt.binding(() => { return g_mainDisplay.displayMargin })

                            if (currentItem.enableEndFade !== undefined)
                                currentItem.enableEndFade = Qt.binding(() => { return (g_mainDisplay.hasMiniPlayer === false) })
                        }
                    }
                }

                Navigation.parentItem: g_mainDisplay
                Navigation.upItem: localTopbar
                Navigation.rightItem: playlistLoader
                Navigation.leftItem: sidebar
                Navigation.downItem: loaderUpdatePane
            }
        }

        Rectangle {
            // overlay for smallscreens
            z: 2

            anchors.fill: parent
            visible: VLCStyle.isScreenSmall && (playlistLoader.shown || (pannelVisiblity.showNavigation && sidebar.visible))
            color: "black"
            opacity: 0.4

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onClicked:  pannelVisiblity.hideVisiblePanels()

                // Capture WheelEvents before they reach stackView
                onWheel: (wheel) => {
                    wheel.accepted = true
                }
            }
        }

        SideNavigationPane {
            id: sidebar

            z: 3

            anchors {
                top : parent.top
                left: parent.left
            }

            width: 0

            visible: false

            height: parent.height - g_mainDisplay.displayMargin

            property int maximumWidth: Math.max(minimumWidth, (g_mainDisplay.width + sidebarResizeHandle.width) / 3)

            topPadding: 0
            leftPadding: 0
            rightPadding: sidebarResizeHandle.visualBorder.width
            bottomPadding: VLCStyle.applicationVerticalMargin + VLCStyle.margin_small

            safeAreaLeftMargin: VLCStyle.applicationHorizontalMargin

            useAcrylic: !VLCStyle.isScreenSmall

            onItemClicked: (modelUri) => {
                if (stackView.isDefaulLoadedForPath([...modelUri]) ||
                    !!modelUri.length && History.exactMatch(History.viewPath, modelUri)) {
                    stackView.positionContentAtBeginning()
                    return
                }

                History.push(modelUri)
            }

            implicitWidth: Math.round(Helpers.clamp(MainCtx.navigationPanel.width, minimumWidth, maximumWidth))

            Navigation.parentItem: g_mainDisplay
            Navigation.upItem: localTopbar
            Navigation.downItem: loaderUpdatePane
            Navigation.rightItem: stackView.currentItem

            state: pannelVisiblity.showNavigation ? "expanded" : ""

            onVisibleChanged: {
                if (!visible) {
                    stackView.focus = true
                }
            }

            Component.onCompleted: {
                Qt.callLater(() => { sidebarTransition.enabled = true; })
            }

            states: State {
                name: "expanded"
                PropertyChanges {
                    target: sidebar
                    width: sidebar.implicitWidth
                    visible: true
                }
            }

            transitions: Transition {
                id: sidebarTransition
                enabled: false

                from: ""; to: "expanded";
                reversible: true

                SequentialAnimation {
                    PropertyAction { property: "visible" }

                    NumberAnimation {
                        property: "width"
                        duration: VLCStyle.duration_short
                        easing.type: Easing.InOutSine
                    }
                }
            }

            PaneResizeHandle {
                id: sidebarResizeHandle

                parent: sidebar
                target: sidebar

                panelObject: MainCtx.navigationPanel
                atRight: true

                visualBorder.visible: !topLeftCornerBackground.visible

                minimumWidth: sidebar.minimumWidth
                maximumWidth: sidebar.maximumWidth

                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                }

            }
        }

        Loader {
            id: playlistLoader

            z: 3
            anchors {
                top: parent.top
                right: parent.right
            }

            width: 0

            visible: false
            height: parent.height - g_mainDisplay.displayMargin

            active: MainCtx.playqueuePanel.docked

            state: ((status === Loader.Ready) && pannelVisiblity.showPlayqueue) ? "expanded" : ""

            readonly property bool shown: (status === Loader.Ready) && item.visible

            onVisibleChanged: {
                if (!visible) {
                    stackView.focus = true
                }
            }

            Component.onCompleted: {
                Qt.callLater(() => { playlistTransition.enabled = true; })
            }

            states: State {
                name: "expanded"
                PropertyChanges {
                    target: playlistLoader
                    width: playlistLoader.implicitWidth
                    visible: true
                }
            }

            transitions: Transition {
                id: playlistTransition
                enabled: false

                from: ""; to: "expanded";
                reversible: true

                SequentialAnimation {
                    PropertyAction { property: "visible" }

                    NumberAnimation {
                        property: "width"
                        duration: VLCStyle.duration_short
                        easing.type: Easing.InOutSine
                    }
                }
            }

            sourceComponent: PlaylistPane {
                id: playlist

                property int maximumWidth: Math.max(minimumWidth, (g_mainDisplay.width + playqueueResizeHandle.width)
                                                    / (pannelVisiblity.showNavigation ? 3 : 2))

                implicitWidth: Math.round(Helpers.clamp(MainCtx.playqueuePanel.width, minimumWidth, maximumWidth))

                focus: true

                leftPadding: playqueueResizeHandle.visualBorder.width
                rightPadding: VLCStyle.applicationHorizontalMargin
                bottomPadding: VLCStyle.margin_normal + Math.max(VLCStyle.applicationVerticalMargin - g_mainDisplay.displayMargin, 0)
                topPadding: VLCStyle.layoutTitle_top_padding

                useAcrylic: !VLCStyle.isScreenSmall

                Navigation.parentItem: g_mainDisplay
                Navigation.upItem: localTopbar
                Navigation.downItem: loaderUpdatePane
                Navigation.leftItem: stackView.currentItem

                Navigation.cancelAction: function() {
                    MainCtx.playqueuePanel.visible = false
                    stackView.forceActiveFocus()
                }

                PaneResizeHandle {
                    id: playqueueResizeHandle

                    parent: playlist
                    target: playlist

                    minimumWidth: playlist.minimumWidth
                    maximumWidth: playlist.maximumWidth

                    panelObject: MainCtx.playqueuePanel
                    atRight: false

                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        left: parent.left
                    }
                }
            }
        }
    }

    Loader {
        id: loaderUpdatePane

        z: 2

        // WARNING: Object name is used from C++ side to acknowledge the modern update pane is loaded,
        //          if C++ side does not get this acknowledgement, the old update dialog is shown as
        //          fallback.
        objectName: "updatePaneLoader"

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: miniPlayer.top

        readonly property bool updateModelIsAvailable: (typeof UpdateModel !== 'undefined')

        active: updateModelIsAvailable && (shouldShow || height > 0.0)

        focus: !!item

        readonly property bool shouldShow: {
            if (!updateModelIsAvailable)
                return false

            if (UpdateModel.explicitCheck) {
                return (UpdateModel.updateStatus !== UpdateModel.Unchecked)
            } else {
                switch (UpdateModel.updateStatus) {
                    case UpdateModel.Unchecked:
                    case UpdateModel.Checking:
                    case UpdateModel.UpToDate:
                        return false
                    default:
                        return true
                }
            }
        }

        // This property can be used to enable/disable the animation:
        property alias toggleAnimation: heightBehavior.enabled

        function dismiss() {
            height = 0.0
        }

        onShouldShowChanged: {
            if (shouldShow)
                height = Qt.binding(() => { return implicitHeight })
        }

        onActiveChanged: {
            if (updateModelIsAvailable && !active) {
                UpdateModel.resetStatus()
                UpdateModel.explicitCheck = undefined // Resets the property
            }
        }

        clip: (height < implicitHeight)

        Behavior on height {
            id: heightBehavior

            NumberAnimation {
                easing.type: Easing.InOutSine
                duration: VLCStyle.duration_long
            }
        }

        source: "qrc:///qt/qml/VLC/MainInterface/UpdatePane.qml"

        Navigation.parentItem: g_mainDisplay
        Navigation.upItem: mainRow
        Navigation.downItem: miniPlayer
        Navigation.navigable: (active && height > 0.0)

        onLoaded: {            
            item.background.visible = Qt.binding(function() { return !stackViewParent.layer.enabled })

            item.leftPadding = Qt.binding(function() { return VLCStyle.margin_large + VLCStyle.applicationHorizontalMargin })
            item.rightPadding = Qt.binding(function() { return VLCStyle.margin_large + VLCStyle.applicationHorizontalMargin })
            item.bottomPadding = Qt.binding(function() { return VLCStyle.margin_small + (miniPlayer.visible ? 0
                                                                                                            : VLCStyle.applicationVerticalMargin) })

            item.dismissRequested.connect(loaderUpdatePane, loaderUpdatePane.dismiss)

            // We have height animation here, we don't want animation inside the item in addition to that:
            item.animations = Qt.binding(function() { return !loaderUpdatePane.toggleAnimation })

            item.Navigation.parentItem = loaderUpdatePane

            item.focus = true // Loader itself is a focus scope, so we need this
        }
    }

    //track the visiblity state of the side panels
    //FIXME do we want proper state machine?
    Item {
        id: pannelVisiblity
        property bool showNavigation: false
        property bool showPlayqueue: false

        Component.onCompleted: {
            onIsScreenSmallChanged()
        }

        onShowNavigationChanged: {
            if (VLCStyle.isScreenSmall && pannelVisiblity.showPlayqueue && MainCtx.playqueuePanel.docked && pannelVisiblity.showNavigation) {
                pannelVisiblity.showPlayqueue = false
            }
        }

        onShowPlayqueueChanged: {
            if (VLCStyle.isScreenSmall && pannelVisiblity.showPlayqueue && MainCtx.playqueuePanel.docked && pannelVisiblity.showNavigation) {
                pannelVisiblity.showNavigation = false
            }
        }

        function hideVisiblePanels() {
            pannelVisiblity.showNavigation = false
            MainCtx.navigationPanel.visible = false
            if (MainCtx.playqueuePanel.docked) {
                pannelVisiblity.showPlayqueue = false
                MainCtx.playqueuePanel.visible = false
            }
        }

        function toggleNavigationVisibility() {
            showNavigation = !showNavigation
            MainCtx.navigationPanel.visible = showNavigation
        }

        function togglePlayqueueVisibility() {
            showPlayqueue = !showPlayqueue
            MainCtx.playqueuePanel.visible = showPlayqueue
        }

        function onIsScreenSmallChanged() {
            //when screen becomes small hide side panels
            if (VLCStyle.isScreenSmall) {
                pannelVisiblity.showNavigation = false
                pannelVisiblity.showPlayqueue = MainCtx.playqueuePanel.visible
            } else {
                //reshow the navigation panels to original state
                pannelVisiblity.showNavigation = MainCtx.navigationPanel.visible
                pannelVisiblity.showPlayqueue =  MainCtx.playqueuePanel.visible
            }
        }

        Connections {
            target: VLCStyle

            function onIsScreenSmallChanged() {
                pannelVisiblity.onIsScreenSmallChanged()
            }
        }

        Connections {
            target: MainCtx.playqueuePanel

            function onVisibleChanged() {
                pannelVisiblity.showPlayqueue = MainCtx.playqueuePanel.visible
            }
        }

        Connections {
            target: MainCtx.navigationPanel

            function onVisibleChanged() {
                pannelVisiblity.showNavigation = MainCtx.navigationPanel.visible
            }
        }
    }

    Component {
        id: pipPlayerComponent

        PIPPlayer {
            id: playerPip
            anchors {
                bottom: loaderUpdatePane.top
                left: parent.left
                bottomMargin: VLCStyle.margin_normal
                leftMargin: VLCStyle.margin_normal + VLCStyle.applicationHorizontalMargin
            }

            width: VLCStyle.dp(320, VLCStyle.scale)
            height: VLCStyle.dp(180, VLCStyle.scale)
            z: 4
            visible: g_mainDisplay._showMiniPlayer && MainCtx.hasEmbededVideo
            enabled: g_mainDisplay._showMiniPlayer && MainCtx.hasEmbededVideo

            dragXMin: 0
            dragXMax: g_mainDisplay.width - playerPip.width
            dragYMin: localTopbar.y + localTopbar.height
            dragYMax: loaderUpdatePane.y - playerPip.height

            //keep the player visible on resize
            Connections {
                target: g_mainDisplay
                function onWidthChanged() {
                    if (playerPip.x > playerPip.dragXMax)
                        playerPip.x = playerPip.dragXMax
                }
                function onHeightChanged() {
                    if (playerPip.y > playerPip.dragYMax)
                        playerPip.y = playerPip.dragYMax
                }
            }
        }
    }

    Dialogs {
        z: 10
        bgContent: g_mainDisplay

        anchors {
            bottom: miniPlayer.visible ? miniPlayer.top : parent.bottom
            left: parent.left
            right: parent.right
        }
    }

    Widgets.FloatingNotification {
        id: notif
        z: 11

        anchors {
            bottom: miniPlayer.top
            left: parent.left
            right: parent.right
            margins: VLCStyle.margin_large
        }
    }

    MiniPlayer {
        id: miniPlayer

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        z: 3

        horizontalPadding: VLCStyle.applicationHorizontalMargin
        bottomPadding: VLCStyle.applicationVerticalMargin + VLCStyle.margin_xsmall

        background.visible: !stackViewParent.layer.enabled

        Navigation.parentItem: g_mainDisplay
        Navigation.upItem: loaderUpdatePane

        onVisibleChanged: {
            if (!visible && miniPlayer.activeFocus)
                stackView.forceActiveFocus()
        }
    }

    Connections {
        target: Player
        function onHasVideoOutputChanged() {
            if (Player.hasVideoOutput && MainCtx.hasEmbededVideo) {
                MainCtx.playerView = true
            } else {
                _showMiniPlayer = false;
            }
        }
    }

    component PaneResizeHandle: Item {
        id: paneResizeHandle
        required property Item target
        required property QtObject panelObject
        property alias atRight: resizeHandle.atRight

        property alias visualBorder: visualBorder

        property alias minimumWidth: resizeHandle.minimumWidth
        property alias maximumWidth: resizeHandle.maximumWidth

        implicitWidth: resizeHandle.width

        Rectangle {
            id: visualBorder

            anchors {
                top: parent.top
                bottom: parent.bottom
                left: resizeHandle.atRight ? undefined : parent.left
                right: resizeHandle.atRight ? parent.right: undefined
            }

            width: VLCStyle.border
            color: theme.separator
        }

        Widgets.HorizontalResizeHandle {
            id: resizeHandle

            property bool _inhibitMainInterfaceUpdate: false

            anchors {
                top: parent.top
                bottom: parent.bottom
                left: resizeHandle.atRight ? undefined : parent.left
                right: resizeHandle.atRight ? parent.right: undefined
            }

            atRight: false
            currentWidth: target.width

            visible: !VLCStyle.isScreenSmall

            onRequestedWidthChanged: {
                if (!_inhibitMainInterfaceUpdate)
                    paneResizeHandle.panelObject.width = requestedWidth
            }

            Component.onCompleted:  _updateFromMainInterface()

            function _updateFromMainInterface() {
                if (requestedWidth === paneResizeHandle.panelObject.width)
                    return

                _inhibitMainInterfaceUpdate = true
                requestedWidth = paneResizeHandle.panelObject.width
                _inhibitMainInterfaceUpdate = false
            }

            Connections {
                target: paneResizeHandle.panelObject

                function onWidthChanged() {
                    resizeHandle._updateFromMainInterface()
                }
            }
        }
    }

    MouseArea {
        /// handles mouse navigation buttons
        z:9
        anchors.fill: parent
        acceptedButtons: Qt.BackButton
        cursorShape: undefined
        onClicked: History.previous()
    }
}
