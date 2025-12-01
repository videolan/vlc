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
    property real displayMargin: (height - miniPlayer.y) + (loaderProgress.active ? loaderProgress.height : 0)

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

    // functions

    //MainDisplay behave as a PageLoader
    function loadView(path, properties, focusReason) {
        const found = stackView.loadView(path, properties, focusReason)
        if (!found)
            return

        const item = stackView.currentItem

        sourcesBanner.localMenuDelegate = Qt.binding(function () {
            return item.localMenuDelegate ?? null
        })

        // NOTE: sortMenu is declared with the SortMenu type, so when it's undefined we have to
        //       return null to avoid a QML warning.
        sourcesBanner.sortMenu = Qt.binding(function () {
            return item.sortMenu ?? null
        })

        MainCtx.hasGridListMode = Qt.binding(() => item.hasGridListMode !== undefined && item.hasGridListMode)
        MainCtx.search.available = Qt.binding(() => item.isSearchable !== undefined && item.isSearchable)
        MainCtx.sort.model = Qt.binding(function () { return item.sortModel })
        MainCtx.sort.available = Qt.binding(function () { return Helpers.isArray(item.sortModel) && item.sortModel.length > 0 })

        if (Player.hasVideoOutput && MainCtx.hasEmbededVideo)
            _showMiniPlayer = true
    }

    Component.onCompleted: {
        if (MainCtx.canShowVideoPIP)
            pipPlayerComponent.createObject(this)
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
            listed: true,
            displayText: qsTr("Home"),
            icon: VLCIcons.home,
            name: "home",
            url: MainCtx.mediaLibraryAvailable ?
                 "qrc:///qt/qml/VLC/MediaLibrary/HomeDisplay.qml" :
                 "qrc:///qt/qml/VLC/MainInterface/NoMedialibHome.qml"
        }, {
            listed: MainCtx.mediaLibraryAvailable,
            displayText: qsTr("Video"),
            icon: VLCIcons.topbar_video,
            name: "video",
            url: "qrc:///qt/qml/VLC/MediaLibrary/VideoDisplay.qml"
        }, {
            listed: MainCtx.mediaLibraryAvailable,
            displayText: qsTr("Music"),
            icon: VLCIcons.topbar_music,
            name: "music",
            url: "qrc:///qt/qml/VLC/MediaLibrary/MusicDisplay.qml"
        }, {
            listed: true,
            displayText: qsTr("Browse"),
            icon: VLCIcons.topbar_network,
            name: "network",
            url: "qrc:///qt/qml/VLC/Network/BrowseDisplay.qml"
        }, {
            listed: true,
            displayText: qsTr("Discover"),
            icon: VLCIcons.topbar_discover,
            name: "discover",
            url: "qrc:///qt/qml/VLC/Network/DiscoverDisplay.qml"
        }, {
            listed: false,
            name: "mlsettings",
            url: "qrc:///qt/qml/VLC/MediaLibrary/MLFoldersSettings.qml"
        }
    ]


    property ListModel tabModel: ListModel {
        id: tabModelid
        Component.onCompleted: {
            pageModel.forEach(function(e) {
                if (!e.listed)
                    return
                append({
                           displayText: e.displayText,
                           icon: e.icon,
                           name: e.name,
                       })
            })
        }
    }

    ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.View
    }

    Loader {
        id: voronoiSnowLoader

        z: 1.5
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

    ColumnLayout {
        id: mainColumn
        anchors.fill: parent

        Layout.minimumWidth: VLCStyle.minWindowWidth
        spacing: 0

        Navigation.parentItem: g_mainDisplay

        /* Source selection*/
        BannerSources {
            id: sourcesBanner
            z: 2
            Layout.preferredHeight: height
            Layout.minimumHeight: height
            Layout.maximumHeight: height
            Layout.fillWidth: true

            model: g_mainDisplay.tabModel

            playlistPane: playlistLoader.active ? playlistLoader.item
                                                : (playlistWindowLoader.status === Loader.Ready ? playlistWindowLoader.item.playlistView
                                                                                                : null)

            onItemClicked: (index) => {
                const name = g_mainDisplay.tabModel.get(index).name

                //don't add the ["mc"] prefix as we are only testing subviers from MainDisplay
                if (stackView.isDefaulLoadedForPath([name])) {
                    return
                }

                selectedIndex = index
                History.push(["mc", name])
            }

            Navigation.parentItem: mainColumn
            Navigation.downItem: mainRow
        }

        FocusScope {
            id: mainRow

            Layout.fillWidth: true
            Layout.fillHeight: true
            z: 0

            focus: true

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

                anchors.fill: parent

                implicitWidth: stackView.implicitWidth
                implicitHeight: stackView.implicitHeight

                color: theme.bg.primary

                layer.enabled: MainCtx.backdropBlurRequested() &&
                               (GraphicsInfo.shaderType === GraphicsInfo.RhiShader) &&
                               (miniPlayer.visible || (loaderProgress.active && loaderProgress.item.visible))

                // Blurring requires to access neighbour pixels, thus the source texture should be bigger than
                // the effect so that the effect have access to the neighbor pixels for the pixels near the
                // border, where the extra size would depend on the blur configuration. When the source is
                // static, this problem is harder to notice, but when the source is not static, such as
                // during scrolling, not considering this causes glitches in the bottom side. `PartialEffect`,
                // since 03b0de26, already provides the effect the whole source texture with a proper sub-rect,
                // so the effect here can sample the top edge neighbour pixels, but for the bottom edge we
                // need to configure the layer:
                readonly property int bottomExtension: 16
                layer.sourceRect: Qt.rect(0, 0, width, height + bottomExtension)

                Rectangle {
                    // Extension of parent rectangle for the bottom extension.
                    anchors.top: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: stackViewParent.bottomExtension
                    visible: stackViewParent.layer.enabled && (height > 0)
                    color: parent.color
                }

                layer.effect: Widgets.PartialEffect {
                    id: stackViewParentLayerEffect

                    // Setting `height` does not seem to work here. Anchoring the effect is not very nice, but it works:
                    anchors.fill: stackViewParent // WARNING: layered item is not necessarily the visual parent of its layer effect.
                    anchors.bottomMargin: -stackViewParent.bottomExtension

                    blending: stackViewParent.color.a < (1.0 - Number.EPSILON)

                    // Each pass of the blur effect also suffers from the border neighbour pixel issue mentioned
                    // above, making all the borders problematic, to a less considerable extent. For that reason,
                    // we extend both the top and the bottom edges and use viewport to prevent overdraw:
                    effectRect: Qt.rect(0,
                                        stackView.height - stackViewParent.bottomExtension,
                                        width,
                                        loaderProgress.height + miniPlayer.height + 2 * stackViewParent.bottomExtension)

                    sourceVisualRect: blending ? Qt.rect(0, 0, width, effectRect.y) : Qt.rect(0, 0, 0, 0)

                    effect: frostedGlassEffect

                    Widgets.FrostedGlassEffect {
                        id: frostedGlassEffect

                        ColorContext {
                            id: frostedTheme
                            palette: VLCStyle.palette
                            colorSet: ColorContext.Window
                        }

                        tint: frostedTheme.bg.secondary

                        // Prevent overdraw (the extension margin should not be painted):
                        viewportRect: Qt.rect(0,
                                              stackViewParent.bottomExtension,
                                              width,
                                              height - (2 * stackViewParent.bottomExtension))
                    }
                }

                Widgets.PageLoader {
                    id: stackView

                    focus: true

                    anchors.fill: parent
                    anchors.rightMargin: (playlistLoader.shown && !VLCStyle.isScreenSmall)
                                         ? playlistLoader.width
                                         : 0
                    anchors.bottomMargin: g_mainDisplay.displayMargin

                    pageModel: g_mainDisplay.pageModel

                    leftPadding: VLCStyle.applicationHorizontalMargin

                    rightPadding: playlistLoader.shown
                                  ? 0
                                  : VLCStyle.applicationHorizontalMargin

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

                    Navigation.parentItem: mainColumn
                    Navigation.upItem: sourcesBanner
                    Navigation.rightItem: playlistLoader
                    Navigation.downItem:  miniPlayer.visible ? miniPlayer : null
                }

                Rectangle {
                    // overlay for smallscreens

                    anchors.fill: parent
                    visible: VLCStyle.isScreenSmall && playlistLoader.shown
                    color: "black"
                    opacity: 0.4

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            MainCtx.playlistVisible = false
                        }

                        // Capture WheelEvents before they reach stackView
                        onWheel: (wheel) => {
                            wheel.accepted = true
                        }
                    }
                }
            }

            Loader {
                id: playlistLoader

                anchors {
                    top: parent.top
                    right: parent.right
                }

                width: 0
                height: parent.height - g_mainDisplay.displayMargin

                visible: false

                active: MainCtx.playlistDocked

                state: ((status === Loader.Ready) && MainCtx.playlistVisible) ? "expanded" : ""

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

                    implicitWidth: Math.round(VLCStyle.isScreenSmall
                                   ? g_mainDisplay.width * 0.8
                                   : Helpers.clamp(g_mainDisplay.width / resizeHandle.widthFactor,
                                                   minimumWidth,
                                                   g_mainDisplay.width / 2 + playlistLeftBorder.width / 2))

                    focus: true

                    leftPadding: playlistLeftBorder.width
                    rightPadding: VLCStyle.applicationHorizontalMargin
                    topPadding: VLCStyle.layoutTitle_top_padding
                    bottomPadding: VLCStyle.margin_normal + Math.max(VLCStyle.applicationVerticalMargin - g_mainDisplay.displayMargin, 0)

                    useAcrylic: !VLCStyle.isScreenSmall

                    Navigation.parentItem: mainColumn
                    Navigation.upItem: sourcesBanner
                    Navigation.downItem: miniPlayer.visible ? miniPlayer : null

                    Navigation.leftAction: function() {
                        stackView.currentItem.setCurrentItemFocus(Qt.TabFocusReason);
                    }

                    Navigation.cancelAction: function() {
                        MainCtx.playlistVisible = false
                        stackView.forceActiveFocus()
                    }

                    Rectangle {
                        id: playlistLeftBorder

                        parent: playlist

                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            left: parent.left
                        }

                        width: VLCStyle.border
                        color: theme.separator

                        visible: playlistLoader.shown
                    }

                    Widgets.HorizontalResizeHandle {
                        id: resizeHandle

                        property bool _inhibitMainInterfaceUpdate: false

                        parent: playlist

                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            left: parent.left
                        }

                        atRight: false
                        targetWidth: parent.width
                        sourceWidth: g_mainDisplay.width

                        visible: !VLCStyle.isScreenSmall

                        onWidthFactorChanged: {
                            if (!_inhibitMainInterfaceUpdate && visible)
                                MainCtx.setPlaylistWidthFactor(widthFactor)
                        }

                        Component.onCompleted:  _updateFromMainInterface()

                        function _updateFromMainInterface() {
                            if (widthFactor == MainCtx.playlistWidthFactor)
                                return

                            _inhibitMainInterfaceUpdate = true
                            widthFactor = MainCtx.playlistWidthFactor
                            _inhibitMainInterfaceUpdate = false
                        }

                        Connections {
                            target: MainCtx

                            function onPlaylistWidthFactorChanged() {
                                resizeHandle._updateFromMainInterface()
                            }
                        }
                    }
                }
            }
        }
    }


    Loader {
        id: loaderProgress

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: miniPlayer.top

        active: (MainCtx.mediaLibraryAvailable && MainCtx.mediaLibrary.idle === false)

        height: active ? implicitHeight : 0

        source: "qrc:///qt/qml/VLC/Widgets/ScanProgressBar.qml"

        onLoaded: {
            item.background.visible = Qt.binding(function() { return !stackViewParent.layer.enabled })

            item.leftPadding = Qt.binding(function() { return VLCStyle.margin_large + VLCStyle.applicationHorizontalMargin })
            item.rightPadding = Qt.binding(function() { return VLCStyle.margin_large + VLCStyle.applicationHorizontalMargin })
            item.bottomPadding = Qt.binding(function() { return VLCStyle.margin_small + (miniPlayer.visible ? 0 : VLCStyle.applicationVerticalMargin) })
        }
    }

    Component {
        id: pipPlayerComponent

        PIPPlayer {
            id: playerPip
            anchors {
                bottom: miniPlayer.top
                left: parent.left
                bottomMargin: VLCStyle.margin_normal
                leftMargin: VLCStyle.margin_normal + VLCStyle.applicationHorizontalMargin
            }

            width: VLCStyle.dp(320, VLCStyle.scale)
            height: VLCStyle.dp(180, VLCStyle.scale)
            z: 2
            visible: g_mainDisplay._showMiniPlayer && MainCtx.hasEmbededVideo
            enabled: g_mainDisplay._showMiniPlayer && MainCtx.hasEmbededVideo

            dragXMin: 0
            dragXMax: g_mainDisplay.width - playerPip.width
            dragYMin: sourcesBanner.y + sourcesBanner.height
            dragYMax: miniPlayer.y - playerPip.height

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

        Navigation.parentItem: mainColumn
        Navigation.upItem: mainRow
        Navigation.cancelItem:sourcesBanner
        onVisibleChanged: {
            if (!visible && miniPlayer.activeFocus)
                stackView.forceActiveFocus()
        }
    }

    Connections {
        target: Player
        function onHasVideoOutputChanged() {
            if (Player.hasVideoOutput && MainCtx.hasEmbededVideo) {
                MainCtx.requestShowPlayerView()
            } else {
                _showMiniPlayer = false;
            }
        }
    }
}
