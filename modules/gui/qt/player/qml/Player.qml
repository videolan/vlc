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
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import QtQuick.Window

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player
import VLC.PlayerControls
import VLC.Dialogs
import VLC.Util

FocusScope {
    id: rootPlayer

    // Properties

    //behave like a Page
    property var pagePrefix: []

    readonly property int positionSliderY: controlBar.y + controlBar.sliderY

    readonly property string coverSource: {
        if (MainPlaylistController.currentItem.artwork &&
            MainPlaylistController.currentItem.artwork.toString())
            MainPlaylistController.currentItem.artwork
        else if (Player.hasVideoOutput)
            VLCStyle.noArtVideoCover
        else
            VLCStyle.noArtAlbumCover
    }

    // Private

    property bool _controlsUnderVideo: (MainCtx.pinVideoControls
                                        &&
                                        (MainCtx.intfMainWindow.visibility !== Window.FullScreen))

    property bool _keyPressed: false

    // Settings

    layer.enabled: (StackView.status === StackView.Deactivating || StackView.status === StackView.Activating)

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Player")

    // Events

    Component.onCompleted: MainCtx.preferHotkeys = true
    Component.onDestruction: MainCtx.preferHotkeys = false

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => {
        if (event.accepted)
            return

        _keyPressed = true

        rootPlayer.Navigation.defaultKeyAction(event)

        //unhandled keys are forwarded as hotkeys
        if (!event.accepted || controlBar.state !== "visible")
            MainCtx.sendHotkey(event.key, event.modifiers);
    }

    Keys.onReleased: (event) => {
        if (event.accepted || _keyPressed === false)
            return

        _keyPressed = false

        rootPlayer.Navigation.defaultKeyReleaseAction(event)
    }

    on_ControlsUnderVideoChanged: {
        lockUnlockAutoHide(_controlsUnderVideo)
    }

    // Functions

    function lockUnlockAutoHide(lock) {
        if (lock) {
            playerToolbarVisibilityFSM.lock()
        } else {
            playerToolbarVisibilityFSM.unlock()
        }
    }

    // Private

    function _onNavigationCancel() {
        if (MainCtx.hasEmbededVideo && !MainCtx.canShowVideoPIP) {
            MainPlaylistController.stop()
        }

        History.previous()
    }

    //we draw both the view and the window here
    ColorContext {
        id: windowTheme

        // NOTE: We force the night theme when playing a video.
        palette: (MainCtx.hasEmbededVideo && MainCtx.pinVideoControls === false)
                 ? VLCStyle.darkPalette
                 : VLCStyle.palette

        colorSet: ColorContext.Window
    }

    PlayerToolbarVisibilityFSM {
        id: playerToolbarVisibilityFSM

        onForceUnlock:{
            controlBar.forceUnlock()

            topBar.forceUnlock()
        }
    }

    PlayerPlaylistVisibilityFSM {
        id: playlistVisibility
    }

    Connections {
        target: MainCtx

        //playlist
        function onPlaylistDockedChanged() {
            playlistVisibility.updatePlaylistDocked()
        }
        function onPlaylistVisibleChanged() {
            playlistVisibility.updatePlaylistVisible()
        }
        function onHasEmbededVideoChanged() {
            playlistVisibility.updateVideoEmbed()
            playerToolbarVisibilityFSM.updateVideoEmbed()
        }
        function onAskShow() {
            playerToolbarVisibilityFSM.askShow()
        }
    }

    Loader {
        id: playerSpecializationLoader

        anchors {
            left: parent.left
            right: parent.right
            top: (MainCtx.hasEmbededVideo && rootPlayer._controlsUnderVideo) ? topBar.bottom : parent.top
            bottom: (MainCtx.hasEmbededVideo && rootPlayer._controlsUnderVideo) ? controlBar.top : parent.bottom
        }

        sourceComponent: MainCtx.hasEmbededVideo ? videoComponent : audioComponent

        property int cursorShape

        // Have padding here, so that the content (unlike background) does not go behind the top bar or the control bar:
        property real topPadding: (anchors.top === parent.top) ? topBar.height : 0
        property real bottomPadding: (anchors.bottom === parent.bottom) ? controlBar.height : 0

        Component {
            id: videoComponent

            FocusScope {
                // Video

                focus: true

                VideoSurface {
                    id: videoSurface

                    anchors.fill: parent

                    videoSurfaceProvider: MainCtx.videoSurfaceProvider

                    visible: MainCtx.hasEmbededVideo

                    focus: true

                    cursorShape: playerSpecializationLoader.cursorShape

                    onMouseMoved: {
                        //short interval for mouse events
                        if (Player.isInteractive)
                            interactiveAutoHideTimer.restart()
                        else
                            playerToolbarVisibilityFSM.mouseMove();
                    }

                    Binding on cursorShape {
                        when: playerToolbarVisibilityFSM.started
                            && !playerToolbarVisibilityFSM.isVisible
                            && !interactiveAutoHideTimer.running
                        value: Qt.BlankCursor
                    }
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right

                    implicitHeight: VLCStyle.dp(206, VLCStyle.scale)

                    opacity: topBar.opacity
                    visible: !topBarAcrylicBg.visible

                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.rgba(0, 0, 0, .8) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right

                    implicitHeight: VLCStyle.dp(206, VLCStyle.scale)

                    opacity: controlBar.opacity

                    gradient: Gradient {
                        GradientStop { position: 0; color: "transparent" }
                        GradientStop { position: .64; color: Qt.rgba(0, 0, 0, .8) }
                        GradientStop { position: 1; color: "black" }
                    }

                    visible: !(controlBar.background && controlBar.background.visible)
                }
            }
        }

        Component {
            id: audioComponent

            FocusScope {
                id: audioFocusScope
                // Audio

                focus: true

                property real topPadding: playerSpecializationLoader.topPadding
                property real bottomPadding: playerSpecializationLoader.bottomPadding

                // background image
                Rectangle {
                    focus: false
                    color: bgtheme.bg.primary
                    anchors.fill: parent

                    readonly property ColorContext colorContext: ColorContext {
                        id: bgtheme
                        colorSet: ColorContext.View
                    }

                    Widgets.BlurEffect {
                        id: blurredBackground

                        radius: 64

                        //destination aspect ratio
                        readonly property real dar: parent.width / parent.height

                        anchors.centerIn: parent
                        width: (cover.sar < dar) ? parent.width :  parent.height * cover.sar
                        height: (cover.sar < dar) ? parent.width / cover.sar :  parent.height

                        source: textureProviderItem

                        Widgets.TextureProviderItem {
                            // Texture indirection to fool Qt into not creating an implicit
                            // ShaderEffectSource because source image does not have fill mode
                            // stretch. "If needed, MultiEffect will internally generate a
                            // ShaderEffectSource as the texture source.": Qt creates a layer
                            // if source has children, source is image and does not have
                            // fill mode stretch or source size is null. In this case,
                            // we really don't need Qt to create an implicit layer.

                            // Note that this item does not create a new texture, it simply
                            // represents the source image provider.
                            id: textureProviderItem

                            // Do not set textureSubRect, because we don't want blur to be
                            // updated everytime the viewport changes. It is better to have
                            // the static source texture blurred once, and adjust the blur
                            // than to blur each time the viewport changes.

                            source: cover
                        }

                        layer.enabled: true
                        layer.samplerName: "backgroundSource"
                        layer.effect: ShaderEffect {
                            readonly property color screenColor: bgtheme.bg.primary.alpha(.55)
                            readonly property color overlayColor: Qt.tint(bgtheme.fg.primary, bgtheme.bg.primary).alpha(0.4)

                            blending: false
                            cullMode: ShaderEffect.BackFaceCulling

                            fragmentShader: "qrc:///shaders/PlayerBlurredBackground.frag.qsb"
                        }
                    }
                }

                MouseArea {
                    id: centerContent

                    readonly property ColorContext colorContext: ColorContext {
                        id: centerTheme
                        colorSet: ColorContext.View
                    }

                    anchors.fill: parent
                    anchors.topMargin: VLCStyle.margin_xsmall + audioFocusScope.topPadding
                    anchors.bottomMargin: VLCStyle.margin_xsmall + audioFocusScope.bottomPadding

                    onWheel: (wheel) => {
                        wheel.accepted = true
                        wheelToVlc.qmlWheelEvent(wheel)
                    }

                    WheelToVLCConverter {
                        id: wheelToVlc

                        onVlcWheelKey: (key) => MainCtx.sendVLCHotkey(key)
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 0

                        Item {
                            id: coverItem
                            Layout.preferredHeight: rootPlayer.height / sizeConstant
                            Layout.preferredWidth: cover.paintedWidth
                            Layout.maximumHeight: centerContent.height
                            Layout.alignment: Qt.AlignHCenter

                            readonly property real sizeConstant: 2.7182

                            Widgets.DynamicShadow {
                                anchors.centerIn: cover
                                sourceItem: cover

                                color: Qt.rgba(0, 0, 0, .18)
                                yOffset: VLCStyle.dp(24)
                                blurRadius: VLCStyle.dp(54)
                            }

                            Widgets.DynamicShadow {
                                anchors.centerIn: cover
                                sourceItem: cover

                                color: Qt.rgba(0, 0, 0, .22)
                                yOffset: VLCStyle.dp(5)
                                blurRadius: VLCStyle.dp(14)
                            }

                            Image {
                                id: cover

                                //source aspect ratio
                                readonly property real sar: paintedWidth / paintedHeight
                                readonly property int maximumWidth: Helpers.alignUp((Screen.desktopAvailableWidth * eDPR / coverItem.sizeConstant), 32)
                                readonly property int maximumHeight: Helpers.alignUp((Screen.desktopAvailableHeight * eDPR / coverItem.sizeConstant), 32)

                                readonly property int maximumSize: Math.min(maximumWidth, maximumHeight)

                                readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                source: VLCAccessImage.uri(rootPlayer.coverSource)
                                fillMode: Image.PreserveAspectFit
                                mipmap: true
                                cache: false
                                asynchronous: true

                                sourceSize: Qt.size(maximumSize, maximumSize)

                                Accessible.role: Accessible.Graphic
                                Accessible.name: qsTr("Cover")
                            }
                        }

                        Widgets.SubtitleLabel {
                            id: albumLabel

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_xxlarge

                            Binding on visible {
                                delayed: true
                                when: albumLabel.componentCompleted
                                value: centerContent.height > (albumLabel.y + albumLabel.height)
                            }

                            text: MainPlaylistController.currentItem.album
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            horizontalAlignment: Text.AlignHCenter
                            color: centerTheme.fg.primary
                            Accessible.description: qsTr("album")

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }
                        }

                        Widgets.MenuLabel {
                            id: artistLabel

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_small

                            Binding on visible {
                                delayed: true
                                when: artistLabel.componentCompleted
                                value: centerContent.height > (artistLabel.y + artistLabel.height)
                            }

                            text: MainPlaylistController.currentItem.artist
                            font.weight: Font.Light
                            horizontalAlignment: Text.AlignHCenter
                            color: centerTheme.fg.primary
                            Accessible.description: qsTr("artist")

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }
                        }

                        Widgets.NavigableRow {
                            id: audioControls

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_large

                            Binding on visible {
                                delayed: true
                                when: audioControls.componentCompleted
                                value: Player.videoTracks.count === 0 && centerContent.height > (audioControls.y + audioControls.height)
                            }

                            focus: true
                            spacing: VLCStyle.margin_xxsmall
                            Navigation.parentItem: rootPlayer
                            Navigation.upItem: topBar
                            Navigation.downItem: Player.isInteractive ? toggleControlBarButton : controlBar

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }

                            model: ObjectModel {
                                Widgets.IconToolButton {
                                    text: VLCIcons.skip_back
                                    font.pixelSize: VLCStyle.icon_audioPlayerButton
                                    onClicked: Player.jumpBwd()
                                    description: qsTr("Step back")
                                }

                                Widgets.IconToolButton {
                                    text: VLCIcons.visualization
                                    font.pixelSize: VLCStyle.icon_audioPlayerButton
                                    onClicked: Player.toggleVisualization()
                                    description: qsTr("Visualization")
                                }

                                Widgets.IconToolButton{
                                    text: VLCIcons.skip_for
                                    font.pixelSize: VLCStyle.icon_audioPlayerButton
                                    onClicked: Player.jumpFwd()
                                    description: qsTr("Step forward")
                                }
                            }
                        }
                    }

                    Widgets.SubtitleLabel {
                        id: labelVolume

                        anchors.right: parent.right
                        anchors.top: parent.top

                        anchors.rightMargin: VLCStyle.margin_normal
                        anchors.topMargin: VLCStyle.margin_xxsmall

                        visible: false

                        text: qsTr("Volume %1%").arg(Math.round(Player.volume * 100))

                        color: centerTheme.fg.primary

                        font.weight: Font.Normal

                        Connections {
                            target: Player

                            function onVolumeChanged() {
                                animationVolume.restart()
                            }
                        }

                        SequentialAnimation {
                            id: animationVolume

                            PropertyAction { target: labelVolume; property: "visible"; value: true }

                            PauseAnimation { duration: VLCStyle.duration_humanMoment }

                            PropertyAction { target: labelVolume; property: "visible"; value: false }
                        }
                    }
                }
            }
        }
    }

    TopBar {
        id: topBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        z: 1

        topMargin: VLCStyle.applicationVerticalMargin
        sideMargin: VLCStyle.applicationHorizontalMargin

        textWidth: playlistVisibility.isPlaylistVisible
                 ? rootPlayer.width - playlistpopup.width
                 : rootPlayer.width

        // NOTE: With pinned controls, the top controls are hidden when switching to
        //       fullScreen. Except when resume is visible
        visible: (MainCtx.pinVideoControls === false
                  ||
                  MainCtx.intfMainWindow.visibility !== Window.FullScreen
                  ||
                  resumeVisible)

        focus: true
        title: MainPlaylistController.currentItem.title

        pinControls: MainCtx.pinVideoControls

        showCSD: MainCtx.clientSideDecoration && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)
        showToolbar: MainCtx.hasToolbarMenu && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)
        playlistVisible: playlistVisibility.isPlaylistVisible

        Navigation.parentItem: rootPlayer
        Navigation.downItem: {
            if (playlistVisibility.isPlaylistVisible)
                return playlistpopup
            if (MainCtx.hasEmbededVideo)
                return playerSpecializationLoader
            if (Player.isInteractive)
                return toggleControlBarButton
            return controlBar
        }

        //initial state value is "", using a binding avoid animation on startup
        Binding on state {
            when: playerToolbarVisibilityFSM.started
            value: (playerToolbarVisibilityFSM.isVisible || rootPlayer._controlsUnderVideo) ? "visible" : "hidden"
        }

        onTogglePlaylistVisibility: playlistVisibility.togglePlaylistVisibility()

        onRequestLockUnlockAutoHide: (lock) => {
            rootPlayer.lockUnlockAutoHide(lock)
        }

        onBackRequested: {
            if (MainCtx.hasEmbededVideo && !MainCtx.canShowVideoPIP) {
               MainPlaylistController.stop()
            }
            MainCtx.requestShowMainView()
        }

        FadeControllerStateGroup {
            target: topBar
        }

        // TODO: Make TopBar a Control and use background
        Widgets.AcrylicBackground {
            id: topBarAcrylicBg

            z: -1

            anchors.fill: parent

            opacity: (MainCtx.intfMainWindow.visibility === Window.FullScreen && MainCtx.hasEmbededVideo) ? MainCtx.pinOpacity
                                                                                                          : 1.0

            tintColor: windowTheme.bg.primary

            visible: MainCtx.pinVideoControls
        }
    }

    Widgets.DrawerExt {
        id: playlistpopup

        anchors {
            // NOTE: When the controls are pinned we display the playqueue under the topBar.
            top: (rootPlayer._controlsUnderVideo) ? topBar.bottom
                                                  : parent.top

            right: parent.right
            bottom: parent.bottom

            bottomMargin: parent.height - rootPlayer.positionSliderY
        }

        focus: false
        edge: Widgets.DrawerExt.Edges.Right

        //initial state value is "", using a binding avoid animation on startup
        Binding on state {
            when: playlistVisibility.started
            value: playlistVisibility.isPlaylistVisible ? "visible" : "hidden"
        }

        component: PlaylistListView {
            id: playlistView

            width: Helpers.clamp(rootPlayer.width / resizeHandle.widthFactor
                                 , playlistView.minimumWidth
                                 , (rootPlayer.width + playlistView.rightPadding) / 2)
            height: playlistpopup.height

            useAcrylic: false
            focus: true

            wheelEnabled: true

            rightPadding: VLCStyle.applicationHorizontalMargin
            topPadding:  {
                if (rootPlayer._controlsUnderVideo)
                    return VLCStyle.margin_normal
                else
                    // NOTE: We increase the padding accordingly to avoid overlapping the TopBar.
                    return topBar.reservedHeight
            }

            background: Rectangle {
                color: windowTheme.bg.primary.alpha(0.8)
            }

            Navigation.parentItem: rootPlayer
            Navigation.upItem: topBar
            Navigation.downItem: Player.isInteractive ? toggleControlBarButton : controlBar
            Navigation.leftAction: closePlaylist
            Navigation.cancelAction: closePlaylist

            function closePlaylist() {
                playlistVisibility.togglePlaylistVisibility()
                if (audioControls.visible)
                    audioControls.forceActiveFocus()
                else
                    controlBar.forceActiveFocus()
            }


            Widgets.HorizontalResizeHandle {
                id: resizeHandle

                property bool _inhibitMainCtxUpdate: false

                parent: playlistView

                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    left: parent.left
                }

                atRight: false
                targetWidth: playlistpopup.width
                sourceWidth: rootPlayer.width

                onWidthFactorChanged: {
                    if (!_inhibitMainCtxUpdate)
                        MainCtx.playerPlaylistWidthFactor = widthFactor
                }

                Component.onCompleted:  _updateFromMainCtx()

                function _updateFromMainCtx() {
                    if (widthFactor == MainCtx.playerPlaylistWidthFactor)
                        return

                    _inhibitMainCtxUpdate = true
                    widthFactor = MainCtx.playerPlaylistWidthFactor
                    _inhibitMainCtxUpdate = false
                }

                Connections {
                    target: MainCtx

                    function onPlaylistWidthFactorChanged() {
                        resizeHandle._updateFromMainCtx()
                    }
                }
            }
        }
    }

    Dialogs {
        z: 10
        bgContent: rootPlayer

        anchors {
            bottom: controlBar.visible ? controlBar.top : rootPlayer.bottom
            left: parent.left
            right: parent.right

            bottomMargin: (rootPlayer._controlsUnderVideo || !controlBar.visible)
                          ? 0 : - VLCStyle.margin_large
        }
    }

    Timer {
        // NavigationBox's visibility depends on this timer
        id: interactiveAutoHideTimer
        running: false
        repeat: false
        interval: 3000
    }

    NavigationBox {
        id: navBox
        visible: Player.isInteractive && navBox.show
                    && (interactiveAutoHideTimer.running
                    || navBox.hovered || !MainCtx.hasEmbededVideo)

        x: rootPlayer.x + VLCStyle.margin_normal + VLCStyle.applicationHorizontalMargin
        y: controlBar.y - navBox.height - VLCStyle.margin_normal

        dragXMin: 0
        dragXMax: rootPlayer.width - navBox.width
        dragYMin: 0
        dragYMax: rootPlayer.height - navBox.height

        Drag.onDragStarted: (controlId) => {
            navBox.x = drag.x
            navBox.y = drag.y
        }
    }

    Connections {
        target: MainCtx
        function onNavBoxToggled() { interactiveAutoHideTimer.restart() }
    }

    Connections {
        target: rootPlayer
        function onWidthChanged() {
            if (navBox.x > navBox.dragXMax)
                navBox.x = navBox.dragXMax
        }
        function onHeightChanged() {
            if (navBox.y > navBox.dragYMax)
                navBox.y = navBox.dragYMax
        }
    }

    Widgets.ButtonExt {
        id: toggleControlBarButton
        visible: Player.isInteractive
                 && MainCtx.hasEmbededVideo
                 && !(MainCtx.pinVideoControls && !Player.fullscreen)
                 && (interactiveAutoHideTimer.running === true
                     || controlBar.state !== "hidden" || toggleControlBarButton.hovered)
        focus: true
        anchors {
            bottom: controlBar.state === "hidden" ? parent.bottom : controlBar.top
            horizontalCenter: parent.horizontalCenter
        }
        iconSize: VLCStyle.icon_large
        iconTxt: controlBar.state === "hidden" ? VLCIcons.expand_inverted : VLCIcons.expand

        Navigation.parentItem: rootPlayer
        Navigation.upItem: playlistVisibility.isPlaylistVisible ? playlistpopup : (MainCtx.hasEmbededVideo ? playerSpecializationLoader : topBar)
        Navigation.downItem: controlBar

        onClicked:{
            playerToolbarVisibilityFSM.askShow();
        }
    }

    Widgets.FloatingNotification {
        id: notif

        anchors {
            bottom: controlBar.top
            left: parent.left
            right: parent.right
            margins: VLCStyle.margin_large
        }
    }

    ControlBar {
        id: controlBar

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        hoverEnabled: true

        focus: true

        rightPadding: VLCStyle.applicationHorizontalMargin
        leftPadding: VLCStyle.applicationHorizontalMargin
        bottomPadding: VLCStyle.applicationVerticalMargin + VLCStyle.margin_xsmall

        textPosition: (MainCtx.pinVideoControls)
                      ? ControlBar.TimeTextPosition.LeftRightSlider
                      : ControlBar.TimeTextPosition.AboveSlider

        // hide right text so that it won't overlap with playlist
        showRemainingTime: (textPosition !== ControlBar.TimeTextPosition.AboveSlider)
                           || !playlistVisibility.isPlaylistVisible

        onStateChanged: {
            if (state === "visible")
                showChapterMarks()
        }

        Navigation.parentItem: rootPlayer
        Navigation.upItem: {
            if (playlistVisibility.isPlaylistVisible)
                return playlistpopup
            if (Player.isInteractive)
                return toggleControlBarButton
            if (!MainCtx.hasEmbededVideo)
                return playerSpecializationLoader
            return topBar
        }

        //initial state value is "", using a binding avoid animation on startup
        Binding on state {
            when: playerToolbarVisibilityFSM.started
            value: (playerToolbarVisibilityFSM.isVisible || rootPlayer._controlsUnderVideo) ? "visible" : "hidden"
        }

        onRequestLockUnlockAutoHide: (lock) => rootPlayer.lockUnlockAutoHide(lock)

        identifier: (Player.hasVideoOutput) ? PlayerControlbarModel.Videoplayer
                                            : PlayerControlbarModel.Audioplayer

        onHoveredChanged: rootPlayer.lockUnlockAutoHide(hovered)

        background: Rectangle {
            id: controlBarBackground

            visible: !MainCtx.hasEmbededVideo || MainCtx.pinVideoControls

            opacity: (Window.visibility === Window.FullScreen && MainCtx.hasEmbededVideo) ? MainCtx.pinOpacity
                                                                                          : ((AcrylicController.enabled || !MainCtx.hasEmbededVideo) ? 0.7 : 1.0)

            color: windowTheme.bg.primary
        }

        FadeControllerStateGroup {
            target: controlBar
        }
    }

    QmlAudioContextMenu {
        id: audioContextMenu

        ctx: MainCtx
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        enabled: !MainCtx.hasEmbededVideo

        onTapped: (eventPoint, button) => {
            if (button & Qt.RightButton) {
                audioContextMenu.popup(eventPoint.globalPosition)
            }
        }
    }

    //filter key events to keep toolbar
    //visible when user navigates within the control bar
    KeyEventFilter {
    id: filter
    target: MainCtx.intfMainWindow

        Keys.onPressed: (event) => {
            if (Player.isInteractive)
                interactiveAutoHideTimer.restart()
            else
                playerToolbarVisibilityFSM.keyboardMove()
        }
    }
}
